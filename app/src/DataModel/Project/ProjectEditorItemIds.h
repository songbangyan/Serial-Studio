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

#pragma once

//--------------------------------------------------------------------------------------------------
// Private enums to track which item the user selected/modified
//--------------------------------------------------------------------------------------------------

// clang-format off

/**
 * @brief Identifies the project-root tree item.
 */
typedef enum { kRootItem } TopLevelItem;

/**
 * @brief Form-field identifiers for the project configuration view.
 */
typedef enum {
  kProjectView_Title
} ProjectItem;

/**
 * @brief Form-field identifiers for the dataset view.
 */
typedef enum {
  kDatasetView_Title,
  kDatasetView_Index,
  kDatasetView_Units,
  kDatasetView_Widget,
  kDatasetView_FFT,
  kDatasetView_Waterfall,
  kDatasetView_WaterfallYAxis,
  kDatasetView_LED,
  kDatasetView_LED_High,
  kDatasetView_Plot,
  kDatasetView_FFTMin,
  kDatasetView_FFTMax,
  kDatasetView_PltMin,
  kDatasetView_PltMax,
  kDatasetView_WgtMin,
  kDatasetView_WgtMax,
  kDatasetView_FFT_Samples,
  kDatasetView_FFT_SamplingRate,
  kDatasetView_FFT_Window,
  kDatasetView_xAxis,
  kDatasetView_Overview,
  kDatasetView_HideOnDashboard,
  kDatasetView_TransformCode,
  kDatasetView_Virtual,
  kDatasetView_DisplayTickCount,
  kDatasetView_DisplayFormat,
  kDatasetView_DecimalPoints,
} DatasetItem;

/**
 * @brief Form-field identifiers for the action view.
 */
typedef enum {
  kActionView_Title,
  kActionView_Icon,
  kActionView_EOL,
  kActionView_Data,
  kActionView_Binary,
  kActionView_TxEncoding,
  kActionView_SourceId,
  kActionView_AutoExecute,
  kActionView_TimerMode,
  kActionView_TimerInterval,
  kActionView_RepeatCount
} ActionItem;

/**
 * @brief Form-field identifiers for the group view.
 */
typedef enum {
  kGroupView_Title,
  kGroupView_Widget,
  kGroupView_Source,
  kGroupView_xAxis,
  kGroupView_WebUrl,
  kGroupView_ImgMode,
  kGroupView_ImgStart,
  kGroupView_ImgEnd,
  kGroupView_Columns,
  kGroupView_Dataset
} GroupItem;

/**
 * @brief Form-field identifiers for the source view.
 */
typedef enum {
  kSourceView_Title,
  kSourceView_BusType,
  kSourceView_Property,
  kSourceView_FrameDetection,
  kSourceView_HexadecimalSequence,
  kSourceView_FrameStartSequence,
  kSourceView_FrameEndSequence,
  kSourceView_FrameDecoder,
  kSourceView_ChecksumFunction
} SourceItem;

/**
 * @brief Form-field identifiers for the output-widget view.
 */
typedef enum {
  kOutputWidget_Title,
  kOutputWidget_Icon,
  kOutputWidget_MonoIcon,
  kOutputWidget_Type,
  kOutputWidget_MinValue,
  kOutputWidget_MaxValue,
  kOutputWidget_StepSize,
  kOutputWidget_InitialValue,
  kOutputWidget_TransmitFunction,
  kOutputWidget_TxEncoding
} OutputWidgetItem;

/**
 * @brief Form-field identifiers for the MQTT publisher view.
 */
typedef enum {
  kMqttPublisher_Enabled,
  kMqttPublisher_Mode,
  kMqttPublisher_PublishFrequency,
  kMqttPublisher_TopicBase,
  kMqttPublisher_ScriptTopic,
  kMqttPublisher_ScriptCode,
  kMqttPublisher_PublishNotifications,
  kMqttPublisher_NotificationTopic,
  kMqttPublisher_Hostname,
  kMqttPublisher_Port,
  kMqttPublisher_CustomClientId,
  kMqttPublisher_ClientId,
  kMqttPublisher_Username,
  kMqttPublisher_Password,
  kMqttPublisher_MqttVersion,
  kMqttPublisher_KeepAlive,
  kMqttPublisher_CleanSession,
  kMqttPublisher_SslEnabled,
  kMqttPublisher_SslProtocol,
  kMqttPublisher_PeerVerifyMode,
  kMqttPublisher_PeerVerifyDepth,
} MqttPublisherItem;

// clang-format on
