/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru
 *
 * This file is dual-licensed:
 *
 * - Under the GNU GPLv3 (or later) for builds that exclude Pro modules.
 * - Under the Serial Studio Commercial License for builds that include
 *   any Pro functionality.
 *
 * You must comply with the terms of one of these licenses, depending
 * on your use case.
 *
 * For GPL terms, see <https://www.gnu.org/licenses/gpl-3.0.html>
 * For commercial terms, see LICENSE_COMMERCIAL.md in the project root.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-SerialStudio-Commercial
 */

#include "UI/Widgets/FFTPlot.h"

#include <algorithm>

#include "UI/Dashboard.h"
#include "UI/Widgets/FFTWindow.h"
#include "UI/Widgets/PlotLogScale.h"

//--------------------------------------------------------------------------------------------------
// Log-axis display constants
//--------------------------------------------------------------------------------------------------

static constexpr int kMaxFftSamples     = 262144;
static constexpr int kLogRenderPoints   = 2048;
static constexpr float kSpectrumFloorDb = -100.0f;
static constexpr float kSpectrumEpsSq   = 1e-24f;

//--------------------------------------------------------------------------------------------------
// Constructor & initialization
//--------------------------------------------------------------------------------------------------

/**
 * @brief Constructs a new FFTPlot widget.
 */
Widgets::FFTPlot::FFTPlot(const int index, QQuickItem* parent)
  : QQuickItem(parent)
  , m_dashboard(UI::Dashboard::instance())
  , m_logX(false)
  , m_size(0)
  , m_index(index)
  , m_samplingRate(0)
  , m_ballistics(false)
  , m_releaseMs(300)
  , m_releaseAlpha(1.0f)
  , m_dataW(0)
  , m_dataH(0)
  , m_minX(0)
  , m_maxX(0)
  , m_minY(0)
  , m_maxY(0)
  , m_center(0)
  , m_halfRange(1)
  , m_scaleIsValid(false)
  , m_windowType(SerialStudio::FFTWindowBlackmanHarris)
  , m_interpolationMode(SerialStudio::InterpolationLinear)
  , m_plan(nullptr)
{
  if (VALIDATE_WIDGET(SerialStudio::DashboardFFT, m_index)) {
    const auto& dataset      = GET_DATASET(SerialStudio::DashboardFFT, m_index);
    const int clampedSamples = qBound(8, dataset.fftSamples, kMaxFftSamples);
    m_size                   = 1 << static_cast<int>(std::log2(clampedSamples));
    m_samplingRate           = qMax(1, dataset.fftSamplingRate);
    m_windowType             = static_cast<SerialStudio::FFTWindow>(dataset.fftWindow);
    m_minX                   = 0;
    m_maxY                   = 0;
    m_minY                   = -100;
    m_maxX                   = m_samplingRate / 2;
    m_logX                   = dataset.fftLogX;
    m_ballistics             = dataset.fftBallistics;
    m_releaseMs              = qBound(50, dataset.fftBallisticsRelease, 5000);
    if (m_logX) {
      rebuildLogBinTable();
      applyLogFrequencyBounds();
    }

    m_samples.resize(m_size);
    m_fftOutput.resize(m_size);
    m_window.resize(m_size);
    Widgets::fillFftWindow(m_windowType, m_window.data(), static_cast<unsigned int>(m_size));

    m_plan = kiss_fft_alloc(m_size, 0, nullptr, nullptr);
    if (!m_plan) {
      qWarning() << "FFT plan allocation failed for size:" << m_size;
      return;
    }

    double minVal = dataset.fftMin;
    double maxVal = dataset.fftMax;
    if (std::isfinite(minVal) && std::isfinite(maxVal)) {
      if (maxVal < minVal)
        std::swap(minVal, maxVal);

      if (maxVal - minVal > 0.0) {
        m_scaleIsValid = true;
        m_center       = (maxVal + minVal) * 0.5;
        m_halfRange    = qMax(1e-12, (maxVal - minVal) * 0.5);
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------
// Data dimension getters
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the size of the down-sampled X axis data.
 */
int Widgets::FFTPlot::dataW() const noexcept
{
  return m_dataW;
}

/**
 * @brief Returns the size of the down-sampled Y axis data.
 */
int Widgets::FFTPlot::dataH() const noexcept
{
  return m_dataH;
}

//--------------------------------------------------------------------------------------------------
// Axis range getters
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the minimum X-axis value.
 */
double Widgets::FFTPlot::minX() const noexcept
{
  return m_minX;
}

/**
 * @brief Returns the maximum X-axis value.
 */
double Widgets::FFTPlot::maxX() const noexcept
{
  return m_maxX;
}

/**
 * @brief Returns the minimum Y-axis value.
 */
double Widgets::FFTPlot::minY() const noexcept
{
  return m_minY;
}

/**
 * @brief Returns the maximum Y-axis value.
 */
double Widgets::FFTPlot::maxY() const noexcept
{
  return m_maxY;
}

/**
 * @brief Returns true when the frequency axis renders in log10 space.
 */
bool Widgets::FFTPlot::logX() const noexcept
{
  return m_logX;
}

//--------------------------------------------------------------------------------------------------
// State queries
//--------------------------------------------------------------------------------------------------

/**
 * @brief Checks whether plot data updates are currently active.
 */
bool Widgets::FFTPlot::running() const noexcept
{
  return m_dashboard.fftPlotRunning(m_index);
}

/**
 * @brief Returns the current interpolation mode.
 */
SerialStudio::InterpolationMode Widgets::FFTPlot::interpolationMode() const noexcept
{
  return m_interpolationMode;
}

//--------------------------------------------------------------------------------------------------
// Rendering
//--------------------------------------------------------------------------------------------------

/**
 * @brief Draws the FFT data on the given QLineSeries.
 */
void Widgets::FFTPlot::draw(QXYSeries* series)
{
  if (series) {
    updateData();
    const auto* data = &m_data;
    if (m_interpolationMode == SerialStudio::InterpolationZoh
        || m_interpolationMode == SerialStudio::InterpolationStem) {
      updateInterpolatedData();
      data = &m_renderData;
    }

    series->replace(*data);
    Q_EMIT series->update();
  }
}

//--------------------------------------------------------------------------------------------------
// Property setters
//--------------------------------------------------------------------------------------------------

/**
 * @brief Updates the size of the down-sampled X axis data.
 */
void Widgets::FFTPlot::setDataW(const int width)
{
  if (m_dataW != width) {
    m_dataW = width;
    updateData();

    Q_EMIT dataSizeChanged();
  }
}

/**
 * @brief Updates the size of the down-sampled Y axis data.
 */
void Widgets::FFTPlot::setDataH(const int height)
{
  if (m_dataH != height) {
    m_dataH = height;
    updateData();

    Q_EMIT dataSizeChanged();
  }
}

/**
 * @brief Enables or disables plot data updates.
 */
void Widgets::FFTPlot::setRunning(const bool enabled)
{
  m_dashboard.setFFTPlotRunning(m_index, enabled);
  Q_EMIT runningChanged();
}

/**
 * @brief Updates the interpolation mode used by the FFT plot.
 */
void Widgets::FFTPlot::setInterpolationMode(SerialStudio::InterpolationMode mode)
{
  SerialStudio::InterpolationMode resolved;
  switch (mode) {
    case SerialStudio::InterpolationNone:
    case SerialStudio::InterpolationLinear:
    case SerialStudio::InterpolationZoh:
    case SerialStudio::InterpolationStem:
      resolved = mode;
      break;
    default:
      resolved = SerialStudio::InterpolationLinear;
      break;
  }

  if (m_interpolationMode == resolved)
    return;

  m_interpolationMode = resolved;
  Q_EMIT interpolationModeChanged();
}

//--------------------------------------------------------------------------------------------------
// Data updates
//--------------------------------------------------------------------------------------------------

/**
 * @brief Rebuilds the FFT plan and window when the input size changes.
 */
bool Widgets::FFTPlot::rebuildFftPlan(int newSize)
{
  m_size = newSize;

  m_window.resize(m_size);
  m_samples.resize(m_size);
  m_fftOutput.resize(m_size);

  Widgets::fillFftWindow(m_windowType, m_window.data(), static_cast<unsigned int>(m_size));

  if (m_plan) {
    kiss_fft_free(m_plan);
    m_plan = nullptr;
  }

  m_plan = kiss_fft_alloc(m_size, 0, nullptr, nullptr);
  if (!m_plan) {
    qWarning() << "FFT plan allocation failed for size:" << m_size;
    return false;
  }

  if (m_logX) {
    rebuildLogBinTable();
    applyLogFrequencyBounds();
  }

  return true;
}

/**
 * @brief Maps the frequency axis bounds into log10 space: the lower bound sits on the
 *        first FFT bin (the closest a log axis gets to 0 Hz, so nothing the analysis
 *        captures is cropped), the upper bound on Nyquist.
 */
void Widgets::FFTPlot::applyLogFrequencyBounds()
{
  Q_ASSERT(m_size > 0);
  Q_ASSERT(m_samplingRate > 0);

  const double freqStep = static_cast<double>(m_samplingRate) / qMax(1, m_size);
  m_minX                = LogScale::clampedLog10(freqStep);
  m_maxX                = LogScale::clampedLog10(m_samplingRate * 0.5, freqStep);
}

/**
 * @brief Rebuilds the cached log10 position of every FFT bin (bin 0 clamps onto bin 1's
 *        position, the closest a log axis gets to DC) and sizes the render buffers for
 *        the interpolated log curve -- steady state stays allocation-free.
 */
void Widgets::FFTPlot::rebuildLogBinTable()
{
  Q_ASSERT(m_size > 0);
  Q_ASSERT(m_samplingRate > 0);

  const int spectrumSize = m_size / 2;
  const double freqStep  = static_cast<double>(m_samplingRate) / m_size;
  m_logBinX.resize(static_cast<std::size_t>(spectrumSize));
  m_pchipSlope.resize(static_cast<std::size_t>(spectrumSize));
  for (int i = 0; i < spectrumSize; ++i) {
    const double freq = i * freqStep;
    m_logBinX[static_cast<std::size_t>(i)] =
      static_cast<float>(LogScale::clampedLog10(freq, freqStep));
  }

  if (m_xData.size() != static_cast<std::size_t>(kLogRenderPoints)) {
    m_xData.resize(kLogRenderPoints);
    m_xData.clear();
    m_yData.resize(kLogRenderPoints);
    m_yData.clear();
  }
}

/**
 * @brief Converts the FFT output to smoothed display dB per bin (shared 1/N^2 power
 *        norm, 3-bin boxcar, then the optional ballistics envelope) into m_binDb.
 */
void Widgets::FFTPlot::computeBinSpectrum(const int spectrumSize)
{
  constexpr int halfWindow = 1;
  Q_ASSERT(spectrumSize > 0);
  Q_ASSERT(m_fftOutput.size() >= static_cast<std::size_t>(spectrumSize));

  static thread_local std::vector<float> dbCache;
  if (dbCache.size() < static_cast<size_t>(spectrumSize))
    dbCache.resize(spectrumSize);

  const float normFactor = static_cast<float>(m_size) * static_cast<float>(m_size);
  const float invNorm    = 1.0f / normFactor;
  for (int i = 0; i < spectrumSize; ++i) {
    const float re    = m_fftOutput[i].r;
    const float im    = m_fftOutput[i].i;
    const float power = std::max((re * re + im * im) * invNorm, kSpectrumEpsSq);
    dbCache[i]        = std::max(10.0f * std::log10(power), kSpectrumFloorDb);
  }

  if (m_binDb.size() != static_cast<std::size_t>(spectrumSize))
    m_binDb.resize(static_cast<std::size_t>(spectrumSize));

  if (m_ballistics && m_displayDb.size() != static_cast<std::size_t>(spectrumSize))
    resetBallistics(spectrumSize);

  updateBallisticsAlpha();

  for (int i = 0; i < spectrumSize; ++i) {
    const int minIdx = std::max(0, i - halfWindow);
    const int maxIdx = std::min(spectrumSize - 1, i + halfWindow);

    float sum = 0.0f;
    for (int k = minIdx; k <= maxIdx; ++k)
      sum += dbCache[k];

    const float smoothedDB               = sum / static_cast<float>(maxIdx - minIdx + 1);
    m_binDb[static_cast<std::size_t>(i)] = applyBallistics(static_cast<std::size_t>(i), smoothedDB);
  }
}

/**
 * @brief Pushes the per-bin display spectrum on the linear frequency axis (the
 *        pre-existing rendering, unchanged in shape).
 */
void Widgets::FFTPlot::emitLinearSpectrum(const int spectrumSize)
{
  Q_ASSERT(spectrumSize > 0);
  Q_ASSERT(m_binDb.size() == static_cast<std::size_t>(spectrumSize));

  if (m_xData.size() != static_cast<size_t>(spectrumSize)) {
    m_xData.resize(spectrumSize);
    m_xData.clear();
    m_yData.resize(spectrumSize);
    m_yData.clear();
  }

  const float freqStep = static_cast<float>(m_samplingRate) / static_cast<float>(qMax(1, m_size));
  for (int i = 0; i < spectrumSize; ++i) {
    m_xData.push(static_cast<float>(i) * freqStep);
    m_yData.push(m_binDb[static_cast<std::size_t>(i)]);
  }
}

/**
 * @brief Renders the log-axis curve the Ableton way: a monotone cubic (Fritsch-Carlson
 *        PCHIP) through the bins in log-x space, resampled on a uniform log grid, so the
 *        sparse low decades draw as smooth hills instead of angular segments. Monotone
 *        interpolation never overshoots, so peaks stay honest.
 */
void Widgets::FFTPlot::buildLogRenderCurve(const int spectrumSize)
{
  const int first = 1;
  const int last  = spectrumSize - 1;
  Q_ASSERT(spectrumSize >= 4);
  Q_ASSERT(m_logBinX.size() == static_cast<std::size_t>(spectrumSize));

  const float* xs = m_logBinX.data();
  const float* ys = m_binDb.data();
  float* slope    = m_pchipSlope.data();

  float hPrev  = xs[first + 1] - xs[first];
  float dPrev  = (ys[first + 1] - ys[first]) / hPrev;
  slope[first] = dPrev;
  for (int i = first + 1; i < last; ++i) {
    const float h = xs[i + 1] - xs[i];
    const float d = (ys[i + 1] - ys[i]) / h;
    if (dPrev * d <= 0.0f)
      slope[i] = 0.0f;
    else {
      const float w1 = 2.0f * h + hPrev;
      const float w2 = h + 2.0f * hPrev;
      slope[i]       = (w1 + w2) / (w1 / dPrev + w2 / d);
    }

    hPrev = h;
    dPrev = d;
  }
  slope[last] = dPrev;

  const float x0 = xs[first];
  const float x1 = xs[last];
  const float dx = (x1 - x0) / static_cast<float>(kLogRenderPoints - 1);
  int seg        = first;
  for (int j = 0; j < kLogRenderPoints; ++j) {
    const float x = x0 + static_cast<float>(j) * dx;
    // code-verify off
    while (seg + 1 < last && xs[seg + 1] < x)
      ++seg;
    // code-verify on

    const float h   = xs[seg + 1] - xs[seg];
    const float t   = qBound(0.0f, (x - xs[seg]) / h, 1.0f);
    const float t2  = t * t;
    const float t3  = t2 * t;
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float h11 = t3 - t2;
    const float y =
      h00 * ys[seg] + h10 * h * slope[seg] + h01 * ys[seg + 1] + h11 * h * slope[seg + 1];

    m_xData.push(x);
    m_yData.push(y);
  }
}

/**
 * @brief Refreshes the wall-clock release coefficient once per displayed frame; the
 *        first frame after a reset jumps straight to the fresh value (alpha = 1).
 */
void Widgets::FFTPlot::updateBallisticsAlpha()
{
  if (!m_ballistics)
    return;

  if (!m_ballisticsClock.isValid()) {
    m_ballisticsClock.start();
    m_releaseAlpha = 1.0f;
    return;
  }

  const double dt  = m_ballisticsClock.nsecsElapsed() * 1e-9;
  const double tau = m_releaseMs * 1e-3;
  m_ballisticsClock.restart();
  m_releaseAlpha = static_cast<float>(1.0 - std::exp(-dt / tau));
}

/**
 * @brief Resets the per-bin display state to the dB floor and invalidates the release
 *        clock, so the next frame attacks cleanly instead of decaying from stale bins.
 */
void Widgets::FFTPlot::resetBallistics(const int bins)
{
  Q_ASSERT(bins > 0);
  m_displayDb.assign(static_cast<std::size_t>(bins), kSpectrumFloorDb);
  m_ballisticsClock.invalidate();
}

/**
 * @brief Display-only envelope per emitted bin: instant attack (peaks never
 *        under-read), exponential wall-clock release toward lower fresh values.
 */
float Widgets::FFTPlot::applyBallistics(const std::size_t idx, const float freshDb)
{
  if (!m_ballistics)
    return freshDb;

  Q_ASSERT(idx < m_displayDb.size());
  float& shown = m_displayDb[idx];
  shown        = freshDb >= shown ? freshDb : shown + (freshDb - shown) * m_releaseAlpha;
  return shown;
}

/**
 * @brief Updates the FFT data. The plan is sized from the ring's capacity (the configured FFT
 *        size), never its fill level, so a filling ring cannot thrash plan reallocation; the
 *        unfilled tail is zero-padded instead.
 */
void Widgets::FFTPlot::updateData()
{
  static thread_local DSP::DownsampleWorkspace ws;

  if (!isEnabled())
    return;

  if (!VALIDATE_WIDGET(SerialStudio::DashboardFFT, m_index))
    return;

  const auto& data  = m_dashboard.fftData(m_index);
  const int newSize = static_cast<int>(data.capacity());
  if (newSize != m_size && !rebuildFftPlan(newSize))
    return;

  if (newSize <= 0)
    return;

  if (!m_plan)
    return;

  const int avail = static_cast<int>(std::min(data.size(), static_cast<std::size_t>(m_size)));

  const double* in       = data.raw();
  std::size_t idx        = data.frontIndex();
  const std::size_t mask = data.storageMask();
  const double offset    = m_scaleIsValid ? -m_center : 0.0;
  const double scale     = m_scaleIsValid ? (1.0 / m_halfRange) : 1.0;
  for (int i = 0; i < avail; ++i) {
    const double raw = in[idx];
    const float v    = std::isfinite(raw) ? static_cast<float>((raw + offset) * scale) : 0.0f;
    m_samples[i].r   = v * m_window[i];
    m_samples[i].i   = 0.0f;
    idx              = (idx + 1) & mask;
  }

  for (int i = avail; i < m_size; ++i) {
    m_samples[i].r = 0.0f;
    m_samples[i].i = 0.0f;
  }

  kiss_fft(m_plan, m_samples.data(), m_fftOutput.data());
  const int spectrumSize = m_size / 2;
  computeBinSpectrum(spectrumSize);
  if (m_logX)
    buildLogRenderCurve(spectrumSize);
  else
    emitLinearSpectrum(spectrumSize);

  DSP::downsampleMonotonic(m_xData, m_yData, m_dataW, m_dataH, m_data, &ws);
}

/**
 * @brief Rebuilds the render data for ZOH or stem interpolation modes.
 */
void Widgets::FFTPlot::updateInterpolatedData()
{
  const int n = m_data.size();

  if (m_interpolationMode == SerialStudio::InterpolationZoh) {
    if (n < 2) {
      m_renderData.resize(n);
      if (n == 1)
        m_renderData.data()[0] = m_data.constData()[0];

      return;
    }

    m_renderData.resize(2 * n - 1);
    QPointF* out      = m_renderData.data();
    const QPointF* in = m_data.constData();
    out[0]            = in[0];
    for (int i = 1; i < n; ++i) {
      out[2 * i - 1] = QPointF(in[i].x(), in[i - 1].y());
      out[2 * i]     = in[i];
    }
    return;
  }

  if (m_interpolationMode == SerialStudio::InterpolationStem) {
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
    const double base     = m_minY;

    m_renderData.resize(3 * n);
    QPointF* out      = m_renderData.data();
    const QPointF* in = m_data.constData();
    for (int i = 0; i < n; ++i) {
      out[3 * i]     = in[i];
      out[3 * i + 1] = QPointF(in[i].x(), base);
      out[3 * i + 2] = QPointF(kNan, kNan);
    }
  }
}
