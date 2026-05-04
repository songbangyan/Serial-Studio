/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2026 Alex Spataru
 *
 * Pro feature -- requires the Serial Studio Commercial License.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#ifdef BUILD_COMMERCIAL

#  include "UI/Widgets/PainterContext.h"

#  include <cmath>
#  include <QDir>
#  include <QFileInfo>
#  include <QFontMetricsF>
#  include <QImage>
#  include <QImageReader>
#  include <QPixmap>
#  include <QRegularExpression>
#  include <QStringList>

#  include "Misc/CommonFonts.h"

/**
 * @brief Maps Canvas2D line-cap names to Qt::PenCapStyle.
 */
[[nodiscard]] static Qt::PenCapStyle mapLineCap(const QString& cap)
{
  if (cap == QLatin1String("round"))
    return Qt::RoundCap;

  if (cap == QLatin1String("square"))
    return Qt::SquareCap;

  return Qt::FlatCap;
}

/**
 * @brief Maps Qt::PenCapStyle back to its Canvas2D name.
 */
[[nodiscard]] static QString unmapLineCap(Qt::PenCapStyle cap)
{
  switch (cap) {
    case Qt::RoundCap:
      return QStringLiteral("round");
    case Qt::SquareCap:
      return QStringLiteral("square");
    case Qt::FlatCap:
    default:
      return QStringLiteral("butt");
  }
}

/**
 * @brief Maps Canvas2D line-join names to Qt::PenJoinStyle.
 */
[[nodiscard]] static Qt::PenJoinStyle mapLineJoin(const QString& join)
{
  if (join == QLatin1String("round"))
    return Qt::RoundJoin;

  if (join == QLatin1String("bevel"))
    return Qt::BevelJoin;

  return Qt::MiterJoin;
}

/**
 * @brief Maps Qt::PenJoinStyle back to its Canvas2D name.
 */
[[nodiscard]] static QString unmapLineJoin(Qt::PenJoinStyle join)
{
  switch (join) {
    case Qt::RoundJoin:
      return QStringLiteral("round");
    case Qt::BevelJoin:
      return QStringLiteral("bevel");
    case Qt::MiterJoin:
    default:
      return QStringLiteral("miter");
  }
}

//--------------------------------------------------------------------------------------------------
// Construction
//--------------------------------------------------------------------------------------------------

/**
 * @brief Resolves a CSS-style font family to a real installed family.
 *
 * Maps the generic CSS aliases (sans-serif, serif, monospace, cursive,
 * fantasy, system-ui) onto Serial Studio's CommonFonts so QFontDatabase
 * doesn't have to populate the platform-wide alias table -- a 40-50 ms
 * stall on first paint that QFontDatabase warns about. Real family names
 * pass through untouched.
 */
[[nodiscard]] static QString resolveFontFamily(const QString& family)
{
  const QString trimmed = family.trimmed();
  if (trimmed.isEmpty())
    return Misc::CommonFonts::instance().widgetFontFamily();

  const QString lower = trimmed.toLower();
  if (lower == QLatin1String("sans-serif") || lower == QLatin1String("system-ui")
      || lower == QLatin1String("ui-sans-serif"))
    return Misc::CommonFonts::instance().widgetFontFamily();

  if (lower == QLatin1String("monospace") || lower == QLatin1String("ui-monospace"))
    return Misc::CommonFonts::instance().monoFont().family();

  if (lower == QLatin1String("serif") || lower == QLatin1String("ui-serif")
      || lower == QLatin1String("cursive") || lower == QLatin1String("fantasy"))
    return Misc::CommonFonts::instance().uiFont().family();

  return trimmed;
}

/**
 * @brief Initialises the context with sensible Canvas2D defaults.
 */
Widgets::PainterContext::PainterContext(QObject* parent)
  : QObject(parent), m_painter(nullptr), m_width(0.0), m_height(0.0)
{
  const QString defaultFam = Misc::CommonFonts::instance().widgetFontFamily();

  m_state.fillBrush      = QBrush(Qt::black);
  m_state.strokePen      = QPen(Qt::black, 1.0);
  m_state.fillSpec       = QStringLiteral("#000000");
  m_state.strokeSpec     = QStringLiteral("#000000");
  m_state.font           = QFont(defaultFam, 10);
  m_state.fontSpecCached = QStringLiteral("10px ") + defaultFam;
  m_state.textAlign      = QStringLiteral("start");
  m_state.textBaseline   = QStringLiteral("alphabetic");
  m_state.globalAlpha    = 1.0;
}

//--------------------------------------------------------------------------------------------------
// Frame lifecycle
//--------------------------------------------------------------------------------------------------

/**
 * @brief Binds a QPainter for the duration of a single user paint() call.
 */
void Widgets::PainterContext::beginFrame(QPainter* painter, qreal width, qreal height)
{
  if (!painter) [[unlikely]]
    return;

  m_painter = painter;
  m_width   = width;
  m_height  = height;
  m_path    = QPainterPath();

  m_painter->setRenderHint(QPainter::Antialiasing, true);
  m_painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  m_painter->setBrush(m_state.fillBrush);
  m_painter->setPen(m_state.strokePen);
  m_painter->setFont(m_state.font);
  m_painter->setOpacity(m_state.globalAlpha);
}

/**
 * @brief Releases the active QPainter binding.
 */
void Widgets::PainterContext::endFrame()
{
  m_painter = nullptr;
  m_width   = 0.0;
  m_height  = 0.0;
}

/**
 * @brief Updates the project directory used to resolve relative drawImage paths.
 */
void Widgets::PainterContext::setProjectDirectory(const QString& dir)
{
  m_projectDir = dir;
}

//--------------------------------------------------------------------------------------------------
// Style getters
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the current fill style spec string.
 */
QString Widgets::PainterContext::fillStyle() const
{
  return m_state.fillSpec;
}

/**
 * @brief Returns the current stroke style spec string.
 */
QString Widgets::PainterContext::strokeStyle() const
{
  return m_state.strokeSpec;
}

/**
 * @brief Returns the current line width in pixels.
 */
qreal Widgets::PainterContext::lineWidth() const
{
  return m_state.strokePen.widthF();
}

/**
 * @brief Returns the current line cap style as a Canvas2D name.
 */
QString Widgets::PainterContext::lineCap() const
{
  return unmapLineCap(m_state.strokePen.capStyle());
}

/**
 * @brief Returns the current line join style as a Canvas2D name.
 */
QString Widgets::PainterContext::lineJoin() const
{
  return unmapLineJoin(m_state.strokePen.joinStyle());
}

/**
 * @brief Returns the cached font spec string.
 */
QString Widgets::PainterContext::fontSpec() const
{
  return m_state.fontSpecCached;
}

/**
 * @brief Returns the current text horizontal alignment.
 */
QString Widgets::PainterContext::textAlign() const
{
  return m_state.textAlign;
}

/**
 * @brief Returns the current text vertical baseline.
 */
QString Widgets::PainterContext::textBaseline() const
{
  return m_state.textBaseline;
}

/**
 * @brief Returns the current global alpha multiplier.
 */
qreal Widgets::PainterContext::globalAlpha() const
{
  return m_state.globalAlpha;
}

//--------------------------------------------------------------------------------------------------
// Style setters
//--------------------------------------------------------------------------------------------------

/**
 * @brief Updates the fill style from a CSS-like color spec.
 */
void Widgets::PainterContext::setFillStyle(const QString& spec)
{
  const QColor c = parseColor(spec);
  if (!c.isValid())
    return;

  m_state.fillSpec  = spec;
  m_state.fillBrush = QBrush(c);

  if (m_painter)
    m_painter->setBrush(m_state.fillBrush);
}

/**
 * @brief Updates the stroke style from a CSS-like color spec.
 */
void Widgets::PainterContext::setStrokeStyle(const QString& spec)
{
  const QColor c = parseColor(spec);
  if (!c.isValid())
    return;

  m_state.strokeSpec = spec;
  m_state.strokePen.setColor(c);

  if (m_painter)
    m_painter->setPen(m_state.strokePen);
}

/**
 * @brief Updates the line width.
 */
void Widgets::PainterContext::setLineWidth(qreal w)
{
  if (w <= 0.0)
    return;

  m_state.strokePen.setWidthF(w);

  if (m_painter)
    m_painter->setPen(m_state.strokePen);
}

/**
 * @brief Updates the line cap style.
 */
void Widgets::PainterContext::setLineCap(const QString& cap)
{
  m_state.strokePen.setCapStyle(mapLineCap(cap));

  if (m_painter)
    m_painter->setPen(m_state.strokePen);
}

/**
 * @brief Updates the line join style.
 */
void Widgets::PainterContext::setLineJoin(const QString& join)
{
  m_state.strokePen.setJoinStyle(mapLineJoin(join));

  if (m_painter)
    m_painter->setPen(m_state.strokePen);
}

/**
 * @brief Updates the active font from a CSS-like font shorthand.
 */
void Widgets::PainterContext::setFontSpec(const QString& spec)
{
  const QFont f          = parseFontSpec(spec);
  m_state.font           = f;
  m_state.fontSpecCached = spec;

  if (m_painter)
    m_painter->setFont(m_state.font);
}

/**
 * @brief Sets the horizontal anchor used by fillText/strokeText.
 */
void Widgets::PainterContext::setTextAlign(const QString& align)
{
  m_state.textAlign = align;
}

/**
 * @brief Sets the vertical baseline used by fillText/strokeText.
 */
void Widgets::PainterContext::setTextBaseline(const QString& baseline)
{
  m_state.textBaseline = baseline;
}

/**
 * @brief Updates the global alpha multiplier (clamped to [0, 1]).
 */
void Widgets::PainterContext::setGlobalAlpha(qreal a)
{
  m_state.globalAlpha = qBound(0.0, a, 1.0);

  if (m_painter)
    m_painter->setOpacity(m_state.globalAlpha);
}

//--------------------------------------------------------------------------------------------------
// State stack + transforms
//--------------------------------------------------------------------------------------------------

/**
 * @brief Pushes the current QPainter state onto its stack.
 */
void Widgets::PainterContext::save()
{
  if (!active())
    return;

  m_painter->save();
}

/**
 * @brief Pops the QPainter state stack.
 */
void Widgets::PainterContext::restore()
{
  if (!active())
    return;

  m_painter->restore();
}

/**
 * @brief Translates the painter origin by (x, y).
 */
void Widgets::PainterContext::translate(qreal x, qreal y)
{
  if (!active())
    return;

  m_painter->translate(x, y);
}

/**
 * @brief Rotates the painter by the given angle in radians.
 */
void Widgets::PainterContext::rotate(qreal radians)
{
  if (!active())
    return;

  m_painter->rotate(qRadiansToDegrees(radians));
}

/**
 * @brief Scales the painter by (sx, sy).
 */
void Widgets::PainterContext::scale(qreal sx, qreal sy)
{
  if (!active())
    return;

  m_painter->scale(sx, sy);
}

/**
 * @brief Resets the painter transform to identity.
 */
void Widgets::PainterContext::resetTransform()
{
  if (!active())
    return;

  m_painter->resetTransform();
}

//--------------------------------------------------------------------------------------------------
// Paths
//--------------------------------------------------------------------------------------------------

/**
 * @brief Starts a new path, discarding any previous subpaths.
 */
void Widgets::PainterContext::beginPath()
{
  m_path = QPainterPath();
}

/**
 * @brief Closes the current subpath with a line to its starting point.
 */
void Widgets::PainterContext::closePath()
{
  m_path.closeSubpath();
}

/**
 * @brief Moves the path cursor to (x, y) without drawing.
 */
void Widgets::PainterContext::moveTo(qreal x, qreal y)
{
  m_path.moveTo(x, y);
}

/**
 * @brief Adds a straight line segment from the cursor to (x, y).
 */
void Widgets::PainterContext::lineTo(qreal x, qreal y)
{
  m_path.lineTo(x, y);
}

/**
 * @brief Adds a closed rectangle subpath at (x, y) with size (w, h).
 */
void Widgets::PainterContext::rect(qreal x, qreal y, qreal w, qreal h)
{
  m_path.addRect(x, y, w, h);
}

/**
 * @brief Adds a circular arc to the path (Canvas2D semantics).
 */
void Widgets::PainterContext::arc(
  qreal x, qreal y, qreal r, qreal startRad, qreal endRad, bool counterClockwise)
{
  if (r <= 0.0)
    return;

  // Wrap into Canvas2D direction (CW = positive, CCW = negative); Qt sweep is the negation.
  constexpr qreal kTau = 2.0 * M_PI;
  const qreal raw      = endRad - startRad;
  qreal sweepRad;
  if (std::abs(raw) >= kTau)
    sweepRad = counterClockwise ? -kTau : kTau;
  else {
    sweepRad = std::fmod(raw, kTau);
    if (!counterClockwise && sweepRad < 0.0)
      sweepRad += kTau;
    else if (counterClockwise && sweepRad > 0.0)
      sweepRad -= kTau;
  }

  const qreal startDeg = -qRadiansToDegrees(startRad);
  const qreal sweepDeg = -qRadiansToDegrees(sweepRad);
  const QRectF box(x - r, y - r, 2.0 * r, 2.0 * r);
  m_path.arcTo(box, startDeg, sweepDeg);
}

/**
 * @brief Adds a quadratic Bezier segment from the cursor.
 */
void Widgets::PainterContext::quadraticCurveTo(qreal cpx, qreal cpy, qreal x, qreal y)
{
  m_path.quadTo(cpx, cpy, x, y);
}

/**
 * @brief Adds a cubic Bezier segment from the cursor.
 */
void Widgets::PainterContext::bezierCurveTo(
  qreal c1x, qreal c1y, qreal c2x, qreal c2y, qreal x, qreal y)
{
  m_path.cubicTo(c1x, c1y, c2x, c2y, x, y);
}

//--------------------------------------------------------------------------------------------------
// Path consumers
//--------------------------------------------------------------------------------------------------

/**
 * @brief Fills the current path with the active fill style.
 */
void Widgets::PainterContext::fill()
{
  if (!active())
    return;

  m_painter->fillPath(m_path, m_state.fillBrush);
}

/**
 * @brief Strokes the current path with the active stroke style.
 */
void Widgets::PainterContext::stroke()
{
  if (!active())
    return;

  m_painter->strokePath(m_path, m_state.strokePen);
}

/**
 * @brief Sets the current path as the clipping region.
 */
void Widgets::PainterContext::clip()
{
  if (!active())
    return;

  m_painter->setClipPath(m_path, Qt::IntersectClip);
}

//--------------------------------------------------------------------------------------------------
// Rectangle convenience
//--------------------------------------------------------------------------------------------------

/**
 * @brief Fills the rectangle (x, y, w, h) with the active fill style.
 */
void Widgets::PainterContext::fillRect(qreal x, qreal y, qreal w, qreal h)
{
  if (!active())
    return;

  m_painter->fillRect(QRectF(x, y, w, h), m_state.fillBrush);
}

/**
 * @brief Strokes the rectangle (x, y, w, h) with the active stroke style.
 */
void Widgets::PainterContext::strokeRect(qreal x, qreal y, qreal w, qreal h)
{
  if (!active())
    return;

  QPainterPath p;
  p.addRect(x, y, w, h);
  m_painter->strokePath(p, m_state.strokePen);
}

/**
 * @brief Clears the rectangle (x, y, w, h) to transparent.
 */
void Widgets::PainterContext::clearRect(qreal x, qreal y, qreal w, qreal h)
{
  if (!active())
    return;

  const auto previous = m_painter->compositionMode();
  m_painter->setCompositionMode(QPainter::CompositionMode_Source);
  m_painter->fillRect(QRectF(x, y, w, h), Qt::transparent);
  m_painter->setCompositionMode(previous);
}

//--------------------------------------------------------------------------------------------------
// Text
//--------------------------------------------------------------------------------------------------

/**
 * @brief Draws filled text at (x, y) using the active font and fill style.
 */
void Widgets::PainterContext::fillText(const QString& text, qreal x, qreal y)
{
  if (!active() || text.isEmpty())
    return;

  m_painter->save();
  m_painter->setPen(QPen(m_state.fillBrush.color()));
  m_painter->setFont(m_state.font);
  const QPointF origin = alignTextOrigin(text, x, y);
  m_painter->drawText(origin, text);
  m_painter->restore();
}

/**
 * @brief Draws stroked text at (x, y) using the active font and stroke style.
 */
void Widgets::PainterContext::strokeText(const QString& text, qreal x, qreal y)
{
  if (!active() || text.isEmpty())
    return;

  QPainterPath path;
  const QPointF origin = alignTextOrigin(text, x, y);
  path.addText(origin, m_state.font, text);
  m_painter->strokePath(path, m_state.strokePen);
}

/**
 * @brief Returns the rendered width of `text` under the current font.
 */
qreal Widgets::PainterContext::measureTextWidth(const QString& text) const
{
  return QFontMetricsF(m_state.font).horizontalAdvance(text);
}

//--------------------------------------------------------------------------------------------------
// Images
//--------------------------------------------------------------------------------------------------

/**
 * @brief Draws an image at (x, y) at its native size.
 */
void Widgets::PainterContext::drawImage(const QString& src, qreal x, qreal y)
{
  if (!active())
    return;

  const QString resolved = resolveImagePath(src);
  if (resolved.isEmpty())
    return;

  const QImage img(resolved);
  if (img.isNull())
    return;

  m_painter->drawImage(QPointF(x, y), img);
}

/**
 * @brief Draws an image scaled to (w, h) at (x, y).
 */
void Widgets::PainterContext::drawImageScaled(
  const QString& src, qreal x, qreal y, qreal w, qreal h)
{
  if (!active() || w <= 0.0 || h <= 0.0)
    return;

  const QString resolved = resolveImagePath(src);
  if (resolved.isEmpty())
    return;

  const QImage img(resolved);
  if (img.isNull())
    return;

  m_painter->drawImage(QRectF(x, y, w, h), img);
}

//--------------------------------------------------------------------------------------------------
// Geometry getters
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the current canvas width in logical pixels.
 */
qreal Widgets::PainterContext::width() const noexcept
{
  return m_width;
}

/**
 * @brief Returns the current canvas height in logical pixels.
 */
qreal Widgets::PainterContext::height() const noexcept
{
  return m_height;
}

//--------------------------------------------------------------------------------------------------
// Internal helpers
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns true while a QPainter is bound for the current frame.
 */
bool Widgets::PainterContext::active() const noexcept
{
  return m_painter != nullptr;
}

/**
 * @brief Parses a CSS-like color spec into a QColor.
 */
QColor Widgets::PainterContext::parseColor(const QString& spec) const
{
  const QString trimmed = spec.trimmed();
  if (trimmed.isEmpty())
    return QColor();

  return QColor::fromString(trimmed);
}

/**
 * @brief Parses a "<size>px <family>" font shorthand into a QFont.
 *
 * Accepts forms like "12px sans-serif", "bold 14px Arial",
 * "italic 700 16px monospace". Generic family aliases route through
 * resolveFontFamily() to pick a real installed family from CommonFonts.
 */
QFont Widgets::PainterContext::parseFontSpec(const QString& spec) const
{
  static const QRegularExpression sizeRe(QStringLiteral("(\\d+(?:\\.\\d+)?)\\s*px"));

  bool bold   = false;
  bool italic = false;
  qreal size  = 10.0;

  const auto sizeMatch = sizeRe.match(spec);
  if (sizeMatch.hasMatch())
    size = sizeMatch.captured(1).toDouble();

  const QString lower = spec.toLower();
  if (lower.contains(QLatin1String("italic")))
    italic = true;

  if (lower.contains(QLatin1String("bold")) || lower.contains(QLatin1String(" 700"))
      || lower.contains(QLatin1String(" 800")) || lower.contains(QLatin1String(" 900")))
    bold = true;

  // Family is whatever comes after the px size token; empty falls back to the dashboard widget font
  QString family;
  if (sizeMatch.hasMatch()) {
    const int sizeEnd  = sizeMatch.capturedEnd();
    const QString tail = spec.mid(sizeEnd).trimmed();
    const QStringList tokens =
      tail.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    if (!tokens.isEmpty())
      family = tokens.first();
  }

  QFont f(resolveFontFamily(family), 10);
  f.setPointSizeF(size * 0.75);
  f.setBold(bold);
  f.setItalic(italic);

  return f;
}

/**
 * @brief Resolves a drawImage source path through the sandbox.
 *
 * Allowed roots: qrc:/ resources and the project file directory tree.
 * Anything else returns an empty string and the caller silently skips
 * the draw. Canonical-path resolution defeats `..` escapes.
 */
QString Widgets::PainterContext::resolveImagePath(const QString& src) const
{
  if (src.isEmpty())
    return QString();

  if (src.startsWith(QLatin1String("qrc:/")) || src.startsWith(QLatin1String(":/")))
    return src.startsWith(QLatin1String("qrc:")) ? src.mid(3) : src;

  if (src.contains(QLatin1String("://")))
    return QString();

  if (m_projectDir.isEmpty())
    return QString();

  const QFileInfo info(src);
  const QString canonical = info.exists() ? info.canonicalFilePath() : QString();
  if (canonical.isEmpty())
    return QString();

  const QString projCanon = QFileInfo(m_projectDir).canonicalFilePath();
  if (projCanon.isEmpty())
    return QString();

  if (canonical == projCanon || canonical.startsWith(projCanon + QLatin1Char('/')))
    return canonical;

  return QString();
}

/**
 * @brief Translates Canvas2D textAlign/textBaseline into a QPainter origin.
 */
QPointF Widgets::PainterContext::alignTextOrigin(const QString& text, qreal x, qreal y) const
{
  qreal dx = 0.0;
  qreal dy = 0.0;

  const QFontMetricsF fm(m_state.font);
  const qreal w = fm.horizontalAdvance(text);

  if (m_state.textAlign == QLatin1String("center"))
    dx = -w * 0.5;
  else if (m_state.textAlign == QLatin1String("right") || m_state.textAlign == QLatin1String("end"))
    dx = -w;

  if (m_state.textBaseline == QLatin1String("top")
      || m_state.textBaseline == QLatin1String("hanging"))
    dy = fm.ascent();
  else if (m_state.textBaseline == QLatin1String("middle"))
    dy = fm.ascent() * 0.5 - fm.descent() * 0.5;
  else if (m_state.textBaseline == QLatin1String("bottom"))
    dy = -fm.descent();

  return QPointF(x + dx, y + dy);
}

#endif  // BUILD_COMMERCIAL
