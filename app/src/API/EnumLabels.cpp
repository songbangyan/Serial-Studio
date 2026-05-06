/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru
 *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-SerialStudio-Commercial
 */

#include "API/EnumLabels.h"

#include <QStringList>

//--------------------------------------------------------------------------------------------------
// BusType
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a short slug for a BusType value.
 */
QString API::EnumLabels::busTypeSlug(int value)
{
  switch (static_cast<SerialStudio::BusType>(value)) {
    case SerialStudio::BusType::UART:
      return QStringLiteral("uart");
    case SerialStudio::BusType::Network:
      return QStringLiteral("network");
    case SerialStudio::BusType::BluetoothLE:
      return QStringLiteral("bluetooth-le");
#ifdef BUILD_COMMERCIAL
    case SerialStudio::BusType::Audio:
      return QStringLiteral("audio");
    case SerialStudio::BusType::ModBus:
      return QStringLiteral("modbus");
    case SerialStudio::BusType::CanBus:
      return QStringLiteral("canbus");
    case SerialStudio::BusType::RawUsb:
      return QStringLiteral("usb");
    case SerialStudio::BusType::HidDevice:
      return QStringLiteral("hid");
    case SerialStudio::BusType::Process:
      return QStringLiteral("process");
#endif
  }
  return QStringLiteral("unknown");
}

/**
 * @brief Returns a human-friendly label for a BusType value.
 */
QString API::EnumLabels::busTypeLabel(int value)
{
  switch (static_cast<SerialStudio::BusType>(value)) {
    case SerialStudio::BusType::UART:
      return QStringLiteral("UART (serial port)");
    case SerialStudio::BusType::Network:
      return QStringLiteral("Network (TCP/UDP)");
    case SerialStudio::BusType::BluetoothLE:
      return QStringLiteral("Bluetooth LE");
#ifdef BUILD_COMMERCIAL
    case SerialStudio::BusType::Audio:
      return QStringLiteral("Audio input");
    case SerialStudio::BusType::ModBus:
      return QStringLiteral("Modbus");
    case SerialStudio::BusType::CanBus:
      return QStringLiteral("CAN bus");
    case SerialStudio::BusType::RawUsb:
      return QStringLiteral("USB (libusb)");
    case SerialStudio::BusType::HidDevice:
      return QStringLiteral("HID");
    case SerialStudio::BusType::Process:
      return QStringLiteral("Process I/O");
#endif
  }
  return QStringLiteral("Unknown");
}

//--------------------------------------------------------------------------------------------------
// FrameDetection
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a short slug for a FrameDetection value.
 */
QString API::EnumLabels::frameDetectionSlug(int value)
{
  switch (static_cast<SerialStudio::FrameDetection>(value)) {
    case SerialStudio::EndDelimiterOnly:
      return QStringLiteral("end-delimiter");
    case SerialStudio::StartAndEndDelimiter:
      return QStringLiteral("start-end-delimiter");
    case SerialStudio::NoDelimiters:
      return QStringLiteral("no-delimiters");
    case SerialStudio::StartDelimiterOnly:
      return QStringLiteral("start-delimiter");
  }
  return QStringLiteral("unknown");
}

/**
 * @brief Returns a human-friendly label for a FrameDetection value.
 */
QString API::EnumLabels::frameDetectionLabel(int value)
{
  switch (static_cast<SerialStudio::FrameDetection>(value)) {
    case SerialStudio::EndDelimiterOnly:
      return QStringLiteral("End delimiter only (split on frameEnd)");
    case SerialStudio::StartAndEndDelimiter:
      return QStringLiteral("Start and end delimiters (split between frameStart and frameEnd)");
    case SerialStudio::NoDelimiters:
      return QStringLiteral("No delimiters (raw byte stream)");
    case SerialStudio::StartDelimiterOnly:
      return QStringLiteral("Start delimiter only");
  }
  return QStringLiteral("Unknown");
}

//--------------------------------------------------------------------------------------------------
// DecoderMethod
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a short slug for a DecoderMethod value.
 */
QString API::EnumLabels::decoderMethodSlug(int value)
{
  switch (static_cast<SerialStudio::DecoderMethod>(value)) {
    case SerialStudio::PlainText:
      return QStringLiteral("plain-text");
    case SerialStudio::Hexadecimal:
      return QStringLiteral("hex");
    case SerialStudio::Base64:
      return QStringLiteral("base64");
    case SerialStudio::Binary:
      return QStringLiteral("binary");
  }
  return QStringLiteral("unknown");
}

/**
 * @brief Returns a human-friendly label for a DecoderMethod value.
 */
QString API::EnumLabels::decoderMethodLabel(int value)
{
  switch (static_cast<SerialStudio::DecoderMethod>(value)) {
    case SerialStudio::PlainText:
      return QStringLiteral("Plain text (UTF-8)");
    case SerialStudio::Hexadecimal:
      return QStringLiteral("Hexadecimal-encoded ASCII");
    case SerialStudio::Base64:
      return QStringLiteral("Base64-encoded ASCII");
    case SerialStudio::Binary:
      return QStringLiteral("Raw binary bytes");
  }
  return QStringLiteral("Unknown");
}

//--------------------------------------------------------------------------------------------------
// OperationMode
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a short slug for an OperationMode value.
 */
QString API::EnumLabels::operationModeSlug(int value)
{
  switch (static_cast<SerialStudio::OperationMode>(value)) {
    case SerialStudio::ProjectFile:
      return QStringLiteral("project-file");
    case SerialStudio::ConsoleOnly:
      return QStringLiteral("console-only");
    case SerialStudio::QuickPlot:
      return QStringLiteral("quick-plot");
  }
  return QStringLiteral("unknown");
}

/**
 * @brief Returns a human-friendly label for an OperationMode value.
 */
QString API::EnumLabels::operationModeLabel(int value)
{
  switch (static_cast<SerialStudio::OperationMode>(value)) {
    case SerialStudio::ProjectFile:
      return QStringLiteral("Project File (full dashboard with parser)");
    case SerialStudio::ConsoleOnly:
      return QStringLiteral("Console Only (raw terminal, no dashboard)");
    case SerialStudio::QuickPlot:
      return QStringLiteral("Quick Plot (auto CSV plotting)");
  }
  return QStringLiteral("Unknown");
}

//--------------------------------------------------------------------------------------------------
// GroupWidget
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a short slug for a GroupWidget value.
 */
QString API::EnumLabels::groupWidgetSlug(int value)
{
  switch (static_cast<SerialStudio::GroupWidget>(value)) {
    case SerialStudio::DataGrid:
      return QStringLiteral("data-grid");
    case SerialStudio::Accelerometer:
      return QStringLiteral("accelerometer");
    case SerialStudio::Gyroscope:
      return QStringLiteral("gyroscope");
    case SerialStudio::GPS:
      return QStringLiteral("gps");
    case SerialStudio::MultiPlot:
      return QStringLiteral("multi-plot");
    case SerialStudio::NoGroupWidget:
      return QStringLiteral("none");
    case SerialStudio::Plot3D:
      return QStringLiteral("plot-3d");
    case SerialStudio::ImageView:
      return QStringLiteral("image-view");
    case SerialStudio::Painter:
      return QStringLiteral("painter");
  }
  return QStringLiteral("unknown");
}

/**
 * @brief Returns a human-friendly label for a GroupWidget value.
 */
QString API::EnumLabels::groupWidgetLabel(int value)
{
  switch (static_cast<SerialStudio::GroupWidget>(value)) {
    case SerialStudio::DataGrid:
      return QStringLiteral("Data grid (table)");
    case SerialStudio::Accelerometer:
      return QStringLiteral("Accelerometer (3-axis bar)");
    case SerialStudio::Gyroscope:
      return QStringLiteral("Gyroscope (3-axis dial)");
    case SerialStudio::GPS:
      return QStringLiteral("GPS map");
    case SerialStudio::MultiPlot:
      return QStringLiteral("Multi-plot (overlaid line chart)");
    case SerialStudio::NoGroupWidget:
      return QStringLiteral("No group widget");
    case SerialStudio::Plot3D:
      return QStringLiteral("3D plot");
    case SerialStudio::ImageView:
      return QStringLiteral("Image view");
    case SerialStudio::Painter:
      return QStringLiteral("Painter (custom canvas)");
  }
  return QStringLiteral("Unknown");
}

//--------------------------------------------------------------------------------------------------
// DatasetWidget
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a short slug for a DatasetWidget value.
 */
QString API::EnumLabels::datasetWidgetSlug(int value)
{
  switch (static_cast<SerialStudio::DatasetWidget>(value)) {
    case SerialStudio::Bar:
      return QStringLiteral("bar");
    case SerialStudio::Gauge:
      return QStringLiteral("gauge");
    case SerialStudio::Compass:
      return QStringLiteral("compass");
    case SerialStudio::NoDatasetWidget:
      return QStringLiteral("none");
  }
  return QStringLiteral("unknown");
}

/**
 * @brief Returns a human-friendly label for a DatasetWidget value.
 */
QString API::EnumLabels::datasetWidgetLabel(int value)
{
  switch (static_cast<SerialStudio::DatasetWidget>(value)) {
    case SerialStudio::Bar:
      return QStringLiteral("Bar (level meter)");
    case SerialStudio::Gauge:
      return QStringLiteral("Gauge (analog dial)");
    case SerialStudio::Compass:
      return QStringLiteral("Compass");
    case SerialStudio::NoDatasetWidget:
      return QStringLiteral("No dataset widget");
  }
  return QStringLiteral("Unknown");
}

//--------------------------------------------------------------------------------------------------
// DatasetOption
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns a comma-separated list of dataset option flags set in value.
 */
QString API::EnumLabels::datasetOptionsLabel(int value)
{
  QStringList parts;
  if (value & SerialStudio::DatasetPlot)
    parts.append(QStringLiteral("plot"));

  if (value & SerialStudio::DatasetFFT)
    parts.append(QStringLiteral("FFT"));

  if (value & SerialStudio::DatasetBar)
    parts.append(QStringLiteral("bar"));

  if (value & SerialStudio::DatasetGauge)
    parts.append(QStringLiteral("gauge"));

  if (value & SerialStudio::DatasetCompass)
    parts.append(QStringLiteral("compass"));

  if (value & SerialStudio::DatasetLED)
    parts.append(QStringLiteral("LED"));

  if (value & SerialStudio::DatasetWaterfall)
    parts.append(QStringLiteral("waterfall"));

  if (parts.isEmpty())
    return QStringLiteral("generic");

  return parts.join(QStringLiteral(", "));
}
