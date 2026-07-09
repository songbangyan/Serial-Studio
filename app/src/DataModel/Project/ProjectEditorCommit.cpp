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

#include <cmath>
#include <memory>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QTimer>

#include "DataModel/FrameBuilder.h"
#include "DataModel/ProjectEditor.h"
#include "DataModel/ProjectModel.h"
#include "IO/Checksum.h"
#include "IO/ConnectionManager.h"
#include "Misc/IconEngine.h"
#include "Misc/Translator.h"
#include "Misc/Utilities.h"
#include "SerialStudio.h"

#ifdef BUILD_COMMERCIAL
#  include "MQTT/Publisher.h"
#  include "MQTT/PublisherScriptEditor.h"
#endif
#include "ProjectEditorItemIds.h"
#include "ProjectEditorShared.h"

/**
 * @brief Applies a source-title edit and syncs the tree-item cache.
 */
void DataModel::ProjectEditor::handleSourceTitleChange(QStandardItem* item)
{
  const QString newTitle = item->data(EditableValue).toString();
  if (m_selectedSource.title == newTitle)
    return;

  m_selectedSource.title = newTitle;
  m_projectModelRef.updateSourceTitle(m_selectedSource.sourceId, newTitle, false);

  for (auto it = m_sourceItems.begin(); it != m_sourceItems.end(); ++it) {
    if (it.value().sourceId != m_selectedSource.sourceId)
      continue;

    auto* treeItem = it.key();
    treeItem->setText(newTitle);
    treeItem->setData(newTitle, TreeViewText);
    m_sourceItems[treeItem].title = newTitle;
    break;
  }

  Q_EMIT selectedTextChanged();
}

/**
 * @brief Applies a bus-type edit and rebuilds the source form once contexts are ready.
 */
void DataModel::ProjectEditor::handleSourceBusTypeChange(QStandardItem* item)
{
  const int busType = item->data(EditableValue).toInt();
  m_projectModelRef.updateSourceBusType(m_selectedSource.sourceId, busType);
  m_selectedSource.busType = busType;
  auto conn                = std::make_shared<QMetaObject::Connection>();
  *conn                    = connect(
    &m_connectionManager,
    &IO::ConnectionManager::contextsRebuilt,
    this,
    [this, conn] {
      disconnect(*conn);
      buildSourceModel(m_selectedSource);
    },
    Qt::QueuedConnection);
}

/**
 * @brief Applies a live-driver property edit and rebuilds the form on transport mode changes.
 */
void DataModel::ProjectEditor::handleSourcePropertyChange(QStandardItem* item)
{
  const QString key   = item->data(ParameterKey).toString();
  const QVariant val  = item->data(EditableValue);
  IO::HAL_Driver* drv = m_connectionManager.driverForEditing(m_selectedSource.sourceId);
  if (drv)
    drv->setDriverProperty(key, val);

  m_projectModelRef.captureSourceSettings(m_selectedSource.sourceId);

  static const QStringList kModeKeys = {
    QStringLiteral("socketTypeIndex"),
    QStringLiteral("protocolIndex"),
  };
  if (kModeKeys.contains(key))
    buildSourceModel(m_selectedSource);
}

/**
 * @brief Dispatches source form edits to ProjectModel or the live driver.
 */
void DataModel::ProjectEditor::onSourceItemChanged(QStandardItem* item)
{
  if (!item)
    return;

  const int id = item->data(ParameterType).toInt();

  if (id == kSourceView_Title) {
    handleSourceTitleChange(item);
    return;
  }

  if (id == kSourceView_BusType) {
    handleSourceBusTypeChange(item);
    return;
  }

  if (id == kSourceView_Property) {
    handleSourcePropertyChange(item);
    return;
  }

  DataModel::Source updated = m_selectedSource;
  switch (static_cast<SourceItem>(id)) {
    case kSourceView_FrameDetection:
    case kSourceView_HexadecimalSequence:
      handleSourceFrameDetectionChange(item, updated);
      break;
    case kSourceView_FrameStartSequence:
    case kSourceView_FrameEndSequence:
      handleSourceFrameStartEndChange(item, updated);
      break;
    case kSourceView_FrameDecoder:
    case kSourceView_ChecksumFunction:
      handleSourceDecoderChecksumChange(item, updated);
      break;
    default:
      break;
  }
}

/**
 * @brief Applies a frame-detection-method or hex-delimiter edit and rebuilds the source form.
 */
void DataModel::ProjectEditor::handleSourceFrameDetectionChange(QStandardItem* item,
                                                                DataModel::Source& updated)
{
  const int id  = item->data(ParameterType).toInt();
  const int sid = m_selectedSource.sourceId;

  if (id == kSourceView_FrameDetection) {
    const int idx = item->data(EditableValue).toInt();
    if (idx < 0 || idx >= m_frameDetectionMethodsValues.size())
      return;

    updated.frameDetection = static_cast<int>(m_frameDetectionMethodsValues.at(idx));
  } else {
    updated.hexadecimalDelimiters = item->data(EditableValue).toBool();
  }

  m_projectModelRef.updateSource(sid, updated);
  m_selectedSource = updated;

  buildSourceModel(m_selectedSource);
}

/**
 * @brief Applies a frame start/end delimiter edit to the source.
 */
void DataModel::ProjectEditor::handleSourceFrameStartEndChange(QStandardItem* item,
                                                               DataModel::Source& updated)
{
  const int id  = item->data(ParameterType).toInt();
  const int sid = m_selectedSource.sourceId;

  if (id == kSourceView_FrameStartSequence)
    updated.frameStart = item->data(EditableValue).toString();
  else
    updated.frameEnd = item->data(EditableValue).toString();

  m_projectModelRef.updateSource(sid, updated, false);
  m_selectedSource = updated;
}

/**
 * @brief Applies a decoder-method or checksum-algorithm edit to the source.
 */
void DataModel::ProjectEditor::handleSourceDecoderChecksumChange(QStandardItem* item,
                                                                 DataModel::Source& updated)
{
  const int id  = item->data(ParameterType).toInt();
  const int sid = m_selectedSource.sourceId;

  if (id == kSourceView_FrameDecoder) {
    updated.decoderMethod = item->data(EditableValue).toInt();
    m_projectModelRef.updateSource(sid, updated);
    m_selectedSource = updated;
    return;
  }

  const auto checksums = IO::availableChecksums();
  const int checksumId = item->data(EditableValue).toInt();
  if (checksumId < 0 || checksumId >= checksums.size())
    return;

  updated.checksumAlgorithm = checksums.at(checksumId);
  m_projectModelRef.updateSource(sid, updated);
  m_selectedSource = updated;
}

//--------------------------------------------------------------------------------------------------
// Private slot: item changed handlers
//--------------------------------------------------------------------------------------------------

/**
 * @brief Propagates group form edits to ProjectModel and the tree.
 */
void DataModel::ProjectEditor::onGroupItemChanged(QStandardItem* item)
{
  if (!item)
    return;

  const auto id      = static_cast<GroupItem>(item->data(ParameterType).toInt());
  const auto value   = item->data(EditableValue);
  auto& pm           = m_projectModelRef;
  const auto groupId = m_selectedGroup.groupId;

  if (id == kGroupView_Title) {
    if (!applyGroupTitleEdit(value.toString(), groupId))
      return;

    Q_EMIT editableOptionsChanged();
    return;
  }

  if (id == kGroupView_Source) {
    applyGroupSourceEdit(value.toInt(), groupId);
    Q_EMIT editableOptionsChanged();
    return;
  }

  if (id == kGroupView_Widget) {
    if (!applyGroupWidgetEdit(value.toInt(), groupId))
      return;

    Q_EMIT editableOptionsChanged();
    return;
  }

  if (id == kGroupView_xAxis) {
    const int xAxisId = (value.toInt() == 1) ? kXAxisSamples : kXAxisTime;
    for (auto& dataset : m_selectedGroup.datasets)
      dataset.xAxisId = xAxisId;

    pm.updateGroup(groupId, m_selectedGroup);
    Q_EMIT editableOptionsChanged();
    return;
  }

  if (id == kGroupView_WebUrl) {
    m_selectedGroup.webViewUrl = value.toString();
    pm.updateGroup(groupId, m_selectedGroup, false);
    Q_EMIT editableOptionsChanged();
    return;
  }

#ifdef BUILD_COMMERCIAL
  if (id == kGroupView_ImgMode) {
    if (applyGroupImgModeEdit(value.toInt(), groupId))
      return;

    Q_EMIT editableOptionsChanged();
    return;
  }

  if (id == kGroupView_ImgStart) {
    m_selectedGroup.imgStartSequence = value.toString();
    pm.updateGroup(groupId, m_selectedGroup, false);
  }

  if (id == kGroupView_ImgEnd) {
    m_selectedGroup.imgEndSequence = value.toString();
    pm.updateGroup(groupId, m_selectedGroup, false);
  }
#endif

  Q_EMIT editableOptionsChanged();
}

/**
 * @brief Applies a group-title edit; returns false when the title is unchanged.
 */
bool DataModel::ProjectEditor::applyGroupTitleEdit(const QString& newTitle, int groupId)
{
  if (m_selectedGroup.title == newTitle)
    return false;

  m_selectedGroup.title = newTitle;
  m_projectModelRef.updateGroup(groupId, m_selectedGroup, false);

  for (auto it = m_groupItems.begin(); it != m_groupItems.end(); ++it) {
    if (it.value().groupId != groupId)
      continue;

    auto* treeItem = it.key();
    treeItem->setText(newTitle);
    treeItem->setData(newTitle, TreeViewText);
    m_groupItems[treeItem].title = newTitle;
    break;
  }

  Q_EMIT selectedTextChanged();
  return true;
}

/**
 * @brief Re-routes the group (and its datasets) to the source at the given combobox index.
 */
void DataModel::ProjectEditor::applyGroupSourceEdit(int srcIdx, int groupId)
{
  const auto& sources = m_projectModelRef.sources();
  if (srcIdx < 0 || srcIdx >= static_cast<int>(sources.size()))
    return;

  m_selectedGroup.sourceId = sources[srcIdx].sourceId;
  for (auto& ds : m_selectedGroup.datasets)
    ds.sourceId = m_selectedGroup.sourceId;

  m_projectModelRef.updateGroup(groupId, m_selectedGroup, true);
}

/**
 * @brief Applies a group-widget change; returns false when the change is rejected.
 */
bool DataModel::ProjectEditor::applyGroupWidgetEdit(int widgetIdx, int groupId)
{
  const auto keys = m_groupWidgets.keys();
  if (widgetIdx < 0 || widgetIdx >= keys.size())
    return false;

  const auto widgetStr = keys.at(widgetIdx);

  static const QMap<QString, SerialStudio::GroupWidget> kWidgetEnumMap = {
    {"accelerometer", SerialStudio::Accelerometer},
    {    "multiplot",     SerialStudio::MultiPlot},
    {         "gyro",     SerialStudio::Gyroscope},
    {          "map",           SerialStudio::GPS},
    {     "datagrid",      SerialStudio::DataGrid},
    {       "plot3d",        SerialStudio::Plot3D},
    {        "image",     SerialStudio::ImageView},
    {      "painter",       SerialStudio::Painter},
    {      "webview",       SerialStudio::WebView},
    {             "", SerialStudio::NoGroupWidget},
  };

  const auto widget = kWidgetEnumMap.value(widgetStr, SerialStudio::NoGroupWidget);
  if (m_projectModelRef.setGroupWidget(groupId, widget)) {
    m_selectedGroup.widget = widgetStr;
    return true;
  }

  QTimer::singleShot(0, this, [this, groupId] {
    buildTreeModel();
    for (auto g = m_groupItems.begin(); g != m_groupItems.end(); ++g) {
      if (g.value().groupId != groupId)
        continue;

      if (m_selectionModel)
        m_selectionModel->setCurrentIndex(g.key()->index(), QItemSelectionModel::ClearAndSelect);

      break;
    }
  });

  return false;
}

#ifdef BUILD_COMMERCIAL
/**
 * @brief Applies an image-mode edit; returns true when handled (caller skips Q_EMIT).
 */
bool DataModel::ProjectEditor::applyGroupImgModeEdit(int modeIdx, int groupId)
{
  const QStringList kImgModeValues = {QStringLiteral("autodetect"), QStringLiteral("manual")};
  if (modeIdx < 0 || modeIdx >= kImgModeValues.size())
    return false;

  m_selectedGroup.imgDetectionMode = kImgModeValues.at(modeIdx);
  m_projectModelRef.updateGroup(groupId, m_selectedGroup);
  buildGroupModel(m_selectedGroup);
  return true;
}
#endif

/**
 * @brief Handles edits to the action form model.
 */
void DataModel::ProjectEditor::onActionItemChanged(QStandardItem* item)
{
  if (!item)
    return;

  static QStringList eolKeys;
  if (eolKeys.isEmpty())
    for (auto i = m_eolSequences.begin(); i != m_eolSequences.end(); ++i)
      eolKeys.append(i.key());

  const auto id    = item->data(ParameterType);
  const auto value = item->data(EditableValue);

  switch (static_cast<ActionItem>(id.toInt())) {
    case kActionView_Title:
      m_selectedAction.title = value.toString();
      break;
    case kActionView_Data:
      m_selectedAction.txData = value.toString();
      break;
    case kActionView_EOL: {
      const int eolIdx = value.toInt();
      if (eolIdx < 0 || eolIdx >= eolKeys.size())
        return;

      m_selectedAction.eolSequence = eolKeys.at(eolIdx);
      break;
    }
    case kActionView_Icon:
      m_selectedAction.icon = value.toString();
      Q_EMIT actionModelChanged();
      break;
    case kActionView_Binary:
      m_selectedAction.binaryData = value.toBool();
      buildActionModel(m_selectedAction);
      break;
    case kActionView_TxEncoding:
      m_selectedAction.txEncoding = value.toInt();
      break;
    case kActionView_SourceId: {
      const auto& sources = m_projectModelRef.sources();
      const int srcIdx    = value.toInt();
      if (srcIdx >= 0 && srcIdx < static_cast<int>(sources.size()))
        m_selectedAction.sourceId = sources[srcIdx].sourceId;

      break;
    }
    case kActionView_AutoExecute:
      m_selectedAction.autoExecuteOnConnect = value.toBool();
      break;
    case kActionView_TimerMode:
      m_selectedAction.timerMode = static_cast<DataModel::TimerMode>(value.toInt());
      buildActionModel(m_selectedAction);
      break;
    case kActionView_TimerInterval:
      m_selectedAction.timerIntervalMs = value.toInt();
      break;
    case kActionView_RepeatCount:
      m_selectedAction.repeatCount = qMax(1, value.toInt());
      break;
    default:
      break;
  }

  auto& pm            = m_projectModelRef;
  const auto actionId = m_selectedAction.actionId;
  pm.setSelectedAction(m_selectedAction);
  pm.updateAction(actionId, m_selectedAction, false);

  if (static_cast<ActionItem>(id.toInt()) == kActionView_Title) {
    const auto newTitle = value.toString();
    for (auto it = m_actionItems.begin(); it != m_actionItems.end(); ++it) {
      if (it.value().actionId != actionId)
        continue;

      auto* treeItem = it.key();
      treeItem->setText(newTitle);
      treeItem->setData(newTitle, TreeViewText);
      m_actionItems[treeItem].title = newTitle;
      break;
    }

    Q_EMIT selectedTextChanged();
  } else {
    for (auto it = m_actionItems.begin(); it != m_actionItems.end(); ++it) {
      if (it.value().actionId == actionId) {
        m_actionItems[it.key()] = m_selectedAction;
        break;
      }
    }
  }
}

/**
 * @brief Dispatches project-level form edits to ProjectModel.
 */
void DataModel::ProjectEditor::onProjectItemChanged(QStandardItem* item)
{
  if (!item)
    return;

  const auto id    = item->data(ParameterType);
  const auto value = item->data(EditableValue);
  auto& pm         = m_projectModelRef;

  switch (static_cast<ProjectItem>(id.toInt())) {
    case kProjectView_Title:
      pm.setTitle(value.toString());
      break;
    default:
      break;
  }

  pm.setModified(true);
}

/**
 * @brief Applies edits to dataset identity rows (title, index, units, transform code).
 */
void DataModel::ProjectEditor::onDatasetCommonItemChanged(QStandardItem* item,
                                                          DataModel::Dataset& dataset)
{
  const auto id    = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);

  switch (id) {
    case kDatasetView_Title:
      dataset.title = value.toString();
      break;
    case kDatasetView_Index:
      dataset.index = value.toInt();
      break;
    case kDatasetView_Units:
      dataset.units = value.toString();
      break;
    case kDatasetView_TransformCode:
      dataset.transformCode = value.toString();
      break;
    default:
      break;
  }
}

/**
 * @brief Applies widget/plot/virtual selector edits and triggers a form rebuild.
 */
void DataModel::ProjectEditor::onDatasetWidgetItemChanged(QStandardItem* item,
                                                          DataModel::Dataset& dataset)
{
  static QStringList datasetWidgetKeys;
  static QList<QPair<bool, bool>> plotOptionKeys;

  if (datasetWidgetKeys.isEmpty())
    for (auto i = m_datasetWidgets.begin(); i != m_datasetWidgets.end(); ++i)
      datasetWidgetKeys.append(i.key());

  if (plotOptionKeys.isEmpty())
    for (auto i = m_plotOptions.begin(); i != m_plotOptions.end(); ++i)
      plotOptionKeys.append(i.key());

  const auto id    = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);

  if (id == kDatasetView_Widget) {
    const int widgetIdx = value.toInt();
    if (widgetIdx < 0 || widgetIdx >= datasetWidgetKeys.size())
      return;

    dataset.widget = datasetWidgetKeys.at(widgetIdx);
    if (dataset.widget == "compass") {
      dataset.wgtMin = 0;
      dataset.wgtMax = 360;
      dataset.alarmBands.clear();
    }

    if (!m_batchApplying)
      buildDatasetModel(dataset);

    return;
  }

  if (id == kDatasetView_Plot) {
    const int plotIdx = value.toInt();
    if (plotIdx < 0 || plotIdx >= plotOptionKeys.size())
      return;

    dataset.plt = plotOptionKeys.at(plotIdx).first;
    dataset.log = plotOptionKeys.at(plotIdx).second;
    if (!m_batchApplying)
      buildDatasetModel(dataset);

    return;
  }

  if (id == kDatasetView_DisplayFormat) {
    static QStringList formatKeys;
    if (formatKeys.isEmpty())
      for (auto i = m_displayFormats.begin(); i != m_displayFormats.end(); ++i)
        formatKeys.append(i.key());

    const int formatIdx = value.toInt();
    if (formatIdx < 0 || formatIdx >= formatKeys.size())
      return;

    dataset.displayFormat = formatKeys.at(formatIdx);
    return;
  }

  if (id == kDatasetView_Virtual) {
    dataset.virtual_ = value.toBool();

    for (auto it = m_datasetItems.begin(); it != m_datasetItems.end(); ++it) {
      if (it.value().groupId == dataset.groupId && it.value().datasetId == dataset.datasetId) {
        it.key()->setData(dataset.virtual_, TreeViewVirtual);
        break;
      }
    }

    if (m_batchApplying)
      return;

    const int uid = dataset.uniqueId;
    QTimer::singleShot(0, this, [this, uid] {
      if (m_selectedDataset.uniqueId == uid)
        buildDatasetModel(m_selectedDataset);
    });
  }
}

/**
 * @brief Applies edits to dataset numeric range/limit fields (plot, widget, alarm, x-axis).
 */
void DataModel::ProjectEditor::onDatasetRangeItemChanged(QStandardItem* item,
                                                         DataModel::Dataset& dataset)
{
  const auto id    = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);

  switch (id) {
    case kDatasetView_xAxis: {
      const auto xUids = m_projectModelRef.xDataSourceUniqueIds();
      const int pos    = value.toInt();
      dataset.xAxisId  = (pos >= 0 && pos < xUids.size()) ? xUids.at(pos) : kXAxisTime;
      break;
    }
    case kDatasetView_PltMin:
      dataset.pltMin = SerialStudio::toDouble(value);
      break;
    case kDatasetView_PltMax:
      dataset.pltMax = SerialStudio::toDouble(value);
      break;
    case kDatasetView_WgtMin:
      dataset.wgtMin = SerialStudio::toDouble(value);
      break;
    case kDatasetView_WgtMax:
      dataset.wgtMax = SerialStudio::toDouble(value);
      break;
    case kDatasetView_DisplayTickCount:
      dataset.displayTickCount = qMax(0, value.toInt());
      break;
    case kDatasetView_DecimalPoints:
      dataset.decimalPoints = qMax(-1, value.toInt());
      break;
    default:
      break;
  }
}

/**
 * @brief Applies edits to FFT-related dataset fields (samples, sampling rate, min/max, axis).
 */
void DataModel::ProjectEditor::onDatasetFftItemChanged(QStandardItem* item,
                                                       DataModel::Dataset& dataset)
{
  const auto id    = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);

  switch (id) {
    case kDatasetView_FFTMin:
      dataset.fftMin = SerialStudio::toDouble(value);
      break;
    case kDatasetView_FFTMax:
      dataset.fftMax = SerialStudio::toDouble(value);
      break;
    case kDatasetView_WaterfallYAxis: {
      const auto yUids       = m_projectModelRef.yWaterfallSourceUniqueIds();
      const int pos          = value.toInt();
      dataset.waterfallYAxis = (pos >= 0 && pos < yUids.size()) ? yUids.at(pos) : 0;
      break;
    }
    case kDatasetView_FFT_Samples: {
      const int sampleIdx = value.toInt();
      if (sampleIdx < 0 || sampleIdx >= m_fftSamples.size())
        return;

      dataset.fftSamples = m_fftSamples.at(sampleIdx).toInt();
      break;
    }
    case kDatasetView_FFT_Window: {
      const int windowIdx = value.toInt();
      if (windowIdx < 0 || windowIdx >= m_fftWindowValues.size())
        return;

      dataset.fftWindow = m_fftWindowValues.at(windowIdx);
      break;
    }
    case kDatasetView_FFT_SamplingRate:
      dataset.fftSamplingRate = value.toInt();
      break;
    default:
      break;
  }
}

/**
 * @brief Applies dataset boolean flag edits (FFT, waterfall, LED, alarm-enabled, ledHigh).
 */
void DataModel::ProjectEditor::onDatasetFlagItemChanged(QStandardItem* item,
                                                        DataModel::Dataset& dataset)
{
  const auto id    = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);

  bool reshape = false;
  switch (id) {
    case kDatasetView_FFT:
      dataset.fft = value.toBool();
      reshape     = true;
      break;
    case kDatasetView_Waterfall:
      dataset.waterfall = value.toBool();
      reshape           = true;
      break;
    case kDatasetView_LED:
      dataset.led = value.toBool();
      reshape     = true;
      break;
    case kDatasetView_LED_High:
      dataset.ledHigh = SerialStudio::toDouble(value);
      break;
    case kDatasetView_HideOnDashboard:
      dataset.hideOnDashboard = value.toBool();
      break;
    default:
      break;
  }

  if (reshape && !m_batchApplying)
    buildDatasetModel(dataset);
}

/**
 * @brief Commits the result of the AlarmBandsEditor dialog into the currently-selected dataset.
 */
void DataModel::ProjectEditor::commitAlarmBands(const QVariantList& bands)
{
  m_selectedDataset.alarmBands.clear();
  m_selectedDataset.alarmBands.reserve(bands.size());
  for (const auto& v : bands) {
    const auto m = v.toMap();
    DataModel::AlarmBand band;
    band.min   = SerialStudio::toDouble(m.value(QStringLiteral("min")));
    band.max   = SerialStudio::toDouble(m.value(QStringLiteral("max")));
    band.blink = m.value(QStringLiteral("blink"), false).toBool();
    band.color = m.value(QStringLiteral("color")).toString().simplified();
    band.label = m.value(QStringLiteral("label")).toString().simplified();
    const int sev =
      m.value(QStringLiteral("severity"), static_cast<int>(DataModel::AlarmSeverity::Warning))
        .toInt();
    band.severity = static_cast<DataModel::AlarmSeverity>(qBound(0, sev, 3));
    if (band.max > band.min)
      m_selectedDataset.alarmBands.push_back(std::move(band));
  }

  auto& pm = m_projectModelRef;
  pm.updateDataset(
    m_selectedDataset.groupId, m_selectedDataset.datasetId, m_selectedDataset, false);
  buildDatasetModel(m_selectedDataset);
}

/**
 * @brief Dispatches dataset form edits to ProjectModel, rebuilding only on tree-visible changes.
 */
void DataModel::ProjectEditor::onDatasetItemChanged(QStandardItem* item)
{
  if (!item)
    return;

  const auto idInt = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);

  if (idInt == kDatasetView_Widget) {
    const int widgetIdx = value.toInt();
    if (widgetIdx < 0 || widgetIdx >= m_datasetWidgets.size())
      return;
  }
  if (idInt == kDatasetView_Plot) {
    const int plotIdx = value.toInt();
    if (plotIdx < 0 || plotIdx >= m_plotOptions.size())
      return;
  }
  if (idInt == kDatasetView_FFT_Samples) {
    const int sampleIdx = value.toInt();
    if (sampleIdx < 0 || sampleIdx >= m_fftSamples.size())
      return;
  }
  if (idInt == kDatasetView_FFT_Window) {
    const int windowIdx = value.toInt();
    if (windowIdx < 0 || windowIdx >= m_fftWindowValues.size())
      return;
  }

  onDatasetCommonItemChanged(item, m_selectedDataset);
  onDatasetWidgetItemChanged(item, m_selectedDataset);
  onDatasetRangeItemChanged(item, m_selectedDataset);
  onDatasetFftItemChanged(item, m_selectedDataset);
  onDatasetFlagItemChanged(item, m_selectedDataset);

  auto& pm             = m_projectModelRef;
  const auto groupId   = m_selectedDataset.groupId;
  const auto datasetId = m_selectedDataset.datasetId;

  if (idInt == kDatasetView_Title) {
    const auto newTitle = m_selectedDataset.title;
    pm.updateDataset(groupId, datasetId, m_selectedDataset, false);

    for (auto it = m_datasetItems.begin(); it != m_datasetItems.end(); ++it) {
      if (it.value().groupId != groupId || it.value().datasetId != datasetId)
        continue;

      auto* treeItem = it.key();
      treeItem->setText(newTitle);
      treeItem->setData(newTitle, TreeViewText);
      m_datasetItems[treeItem].title = newTitle;
      break;
    }

    Q_EMIT selectedTextChanged();
    Q_EMIT datasetOptionsChanged();
    Q_EMIT editableOptionsChanged();
    return;
  }

  const bool rebuildTree = (idInt == kDatasetView_Index);
  pm.updateDataset(groupId, datasetId, m_selectedDataset, rebuildTree);
  if (!rebuildTree)
    syncDatasetItemCache(groupId, datasetId);

  Q_EMIT datasetOptionsChanged();
  Q_EMIT editableOptionsChanged();
}

/**
 * @brief Refreshes the cached dataset record bound to the matching tree item.
 */
void DataModel::ProjectEditor::syncDatasetItemCache(int groupId, int datasetId)
{
  for (auto it = m_datasetItems.begin(); it != m_datasetItems.end(); ++it) {
    if (it.value().groupId != groupId || it.value().datasetId != datasetId)
      continue;

    m_datasetItems[it.key()] = m_selectedDataset;
    break;
  }
}

/**
 * @brief Handles changes to output widget form fields.
 */
void DataModel::ProjectEditor::onOutputWidgetItemChanged(QStandardItem* item)
{
  if (!item)
    return;

  const auto id    = item->data(ParameterType);
  const auto value = item->data(EditableValue);

  switch (static_cast<OutputWidgetItem>(id.toInt())) {
    case kOutputWidget_Title:
      m_selectedOutputWidget.title = value.toString();
      break;
    case kOutputWidget_Icon:
      m_selectedOutputWidget.icon = value.toString();
      break;
    case kOutputWidget_MonoIcon:
      m_selectedOutputWidget.monoIcon = value.toBool();
      break;
    case kOutputWidget_Type: {
      const auto newType = static_cast<DataModel::OutputWidgetType>(value.toInt());
      if (m_selectedOutputWidget.type != newType) {
        m_selectedOutputWidget.type = newType;
        buildOutputWidgetModel(m_selectedOutputWidget);
      }
      break;
    }
    case kOutputWidget_MinValue:
      m_selectedOutputWidget.minValue = SerialStudio::toDouble(value);
      break;
    case kOutputWidget_MaxValue:
      m_selectedOutputWidget.maxValue = SerialStudio::toDouble(value);
      break;
    case kOutputWidget_StepSize:
      m_selectedOutputWidget.stepSize = SerialStudio::toDouble(value);
      break;
    case kOutputWidget_InitialValue:
      m_selectedOutputWidget.initialValue = SerialStudio::toDouble(value);
      break;
    case kOutputWidget_TransmitFunction:
      m_selectedOutputWidget.transmitFunction = value.toString();
      break;
    case kOutputWidget_TxEncoding:
      m_selectedOutputWidget.txEncoding = value.toInt();
      break;
  }

  if (static_cast<OutputWidgetItem>(id.toInt()) == kOutputWidget_Title) {
    const auto newTitle = value.toString();
    for (auto it = m_outputWidgetItems.begin(); it != m_outputWidgetItems.end(); ++it) {
      if (it.value().groupId == m_selectedOutputWidget.groupId
          && it.value().widgetId == m_selectedOutputWidget.widgetId) {
        it.key()->setData(newTitle, TreeViewText);
        m_outputWidgetItems[it.key()].title = newTitle;
        Q_EMIT selectedTextChanged();
        break;
      }
    }
  } else {
    for (auto it = m_outputWidgetItems.begin(); it != m_outputWidgetItems.end(); ++it) {
      if (it.value().groupId == m_selectedOutputWidget.groupId
          && it.value().widgetId == m_selectedOutputWidget.widgetId) {
        m_outputWidgetItems[it.key()] = m_selectedOutputWidget;
        break;
      }
    }
  }

  m_projectModelRef.updateOutputWidget(
    m_selectedOutputWidget.groupId, m_selectedOutputWidget.widgetId, m_selectedOutputWidget, false);
}
