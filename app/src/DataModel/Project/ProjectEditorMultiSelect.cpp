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

//--------------------------------------------------------------------------------------------------
// Multi-selection aggregate editing
//--------------------------------------------------------------------------------------------------

/**
 * @brief Detects a homogeneous multi-selection (>=2 items of one editable kind) and switches to the
 *        aggregate MultiSelectionView. Returns true when it took over the selection.
 */
bool DataModel::ProjectEditor::tryMultiSelection()
{
  if (!m_selectionModel || !m_treeModel)
    return false;

  int kind = KindNone;
  QSet<qint64> seen;
  QVector<QPair<int, int>> items;
  const auto indexes = m_selectionModel->selectedIndexes();
  for (const auto& idx : indexes) {
    if (!idx.isValid() || idx.column() != 0)
      continue;

    const auto key =
      (static_cast<qint64>(idx.row()) << 32) ^ reinterpret_cast<qint64>(idx.internalPointer());
    if (seen.contains(key))
      continue;

    seen.insert(key);

    const int k = m_treeModel->data(idx, TreeItemKind).toInt();
    if (k != KindDataset && k != KindOutputWidget)
      return false;

    if (kind == KindNone)
      kind = k;
    else if (kind != k)
      return false;

    const int id     = m_treeModel->data(idx, TreeItemId).toInt();
    const int parent = m_treeModel->data(idx, TreeItemParentId).toInt();
    items.append(qMakePair(parent, id));
  }

  if (items.size() < 2)
    return false;

  if (m_currentView == MultiSelectionView && m_batchKind == static_cast<ItemKind>(kind)
      && m_batchItems == items)
    return true;

  m_batchKind  = static_cast<ItemKind>(kind);
  m_batchItems = items;
  setCurrentView(MultiSelectionView);
  buildMultiSelectionModel();
  Q_EMIT currentViewChanged();
  return true;
}

/**
 * @brief Builds the aggregate form model for the current multi-selection's kind.
 */
void DataModel::ProjectEditor::buildMultiSelectionModel()
{
  if (m_batchKind == KindDataset)
    buildMultiDatasetModel();
  else if (m_batchKind == KindOutputWidget)
    buildMultiOutputWidgetModel();
}

/**
 * @brief Extracts a ParameterType -> EditableValue map for a dataset via a throwaway form model,
 *        used to decide which aggregate fields agree across the selection.
 */
QHash<int, QVariant> DataModel::ProjectEditor::datasetEditValues(const DataModel::Dataset& dataset)
{
  CustomModel tmp;
  addGeneralSection(&tmp, dataset);
  addPlotSection(&tmp, dataset);
  addFFTSection(&tmp, dataset);
  addWidgetSection(&tmp, dataset);
  addLEDSection(&tmp, dataset);

  QHash<int, QVariant> out;
  const int rows = tmp.rowCount();
  for (int r = 0; r < rows; ++r) {
    auto* it = tmp.item(r);
    if (!it)
      continue;

    const auto pt = it->data(ParameterType);
    if (pt.isValid())
      out.insert(pt.toInt(), it->data(EditableValue));
  }

  return out;
}

/**
 * @brief Builds the dataset aggregate form: rows from the first selection member, identity fields
 *        greyed, shared fields marked "Mixed" when the selection disagrees. Fans edits out on
 *        change via onMultiSelectionItemChanged.
 */
void DataModel::ProjectEditor::buildMultiDatasetModel()
{
  auto& pm           = m_projectModelRef;
  const auto& groups = pm.groups();

  QVector<DataModel::Dataset> sel;
  sel.reserve(static_cast<qsizetype>(m_batchItems.size()));
  for (const auto& pr : m_batchItems) {
    const int gid = pr.first, dsid = pr.second;
    if (gid < 0 || static_cast<size_t>(gid) >= groups.size())
      continue;

    for (const auto& d : groups[gid].datasets)
      if (d.datasetId == dsid) {
        sel.append(d);
        break;
      }
  }

  if (sel.isEmpty())
    return;

  QVector<QHash<int, QVariant>> maps;
  maps.reserve(sel.size());
  for (const auto& d : sel)
    maps.append(datasetEditValues(d));

  if (m_datasetModel) {
    m_datasetModel->disconnect(this);
    m_datasetModel->deleteLater();
  }

  m_selectedDataset = sel.first();
  m_datasetModel    = new CustomModel(this);

  addGeneralSection(m_datasetModel, m_selectedDataset);
  addPlotSection(m_datasetModel, m_selectedDataset);
  addFFTSection(m_datasetModel, m_selectedDataset);
  addWidgetSection(m_datasetModel, m_selectedDataset);
  addLEDSection(m_datasetModel, m_selectedDataset);

  const int rows = m_datasetModel->rowCount();
  for (int r = 0; r < rows; ++r) {
    auto* it = m_datasetModel->item(r);
    if (!it)
      continue;

    const auto ptVar = it->data(ParameterType);
    if (!ptVar.isValid())
      continue;

    const int wt         = it->data(WidgetType).toInt();
    const bool intWidget = (wt == ComboBox || wt == CheckBox || wt == AutoIntField);
    const QVariant blank = intWidget ? QVariant(-1) : QVariant(QString());

    const int pt = ptVar.toInt();
    if (pt == kDatasetView_Title || pt == kDatasetView_Index || pt == kDatasetView_Alias) {
      it->setEditable(false);
      it->setData(false, Active);
      it->setData(blank, EditableValue);
      it->setData(tr("(multiple)"), PlaceholderValue);
      continue;
    }

    const QVariant first = maps.first().value(pt);
    bool mixed           = false;
    for (int i = 1; i < maps.size(); ++i)
      if (maps[i].contains(pt) && maps[i].value(pt) != first) {
        mixed = true;
        break;
      }

    if (mixed) {
      it->setData(blank, EditableValue);
      it->setData(tr("Mixed"), PlaceholderValue);
    }
  }

  connect(m_datasetModel,
          &CustomModel::itemChanged,
          this,
          &DataModel::ProjectEditor::onMultiSelectionItemChanged);

  Q_EMIT datasetModelChanged();
  Q_EMIT datasetOptionsChanged();
}

/**
 * @brief Maps an OutputWidget's editable fields to a ParameterType -> value hash, used to decide
 *        which aggregate rows agree across the selection.
 */
QHash<int, QVariant> DataModel::ProjectEditor::outputWidgetEditValues(
  const DataModel::OutputWidget& widget)
{
  QHash<int, QVariant> out;
  out.insert(kOutputWidget_Title, widget.title);
  out.insert(kOutputWidget_Icon, widget.icon);
  out.insert(kOutputWidget_MonoIcon, widget.monoIcon);
  out.insert(kOutputWidget_MinValue, widget.minValue);
  out.insert(kOutputWidget_MaxValue, widget.maxValue);
  out.insert(kOutputWidget_StepSize, widget.stepSize);
  out.insert(kOutputWidget_InitialValue, widget.initialValue);
  out.insert(kOutputWidget_TxEncoding, widget.txEncoding);
  return out;
}

/**
 * @brief Builds the output-widget aggregate form: rows from the first selection member, the
 * per-item title greyed, shared fields marked "Mixed" when the selection disagrees. Fans edits out
 * on change via onMultiSelectionItemChanged.
 */
void DataModel::ProjectEditor::buildMultiOutputWidgetModel()
{
  auto& pm           = m_projectModelRef;
  const auto& groups = pm.groups();

  QVector<DataModel::OutputWidget> sel;
  sel.reserve(static_cast<qsizetype>(m_batchItems.size()));
  for (const auto& pr : m_batchItems) {
    const int gid = pr.first, wid = pr.second;
    if (gid < 0 || static_cast<size_t>(gid) >= groups.size())
      continue;

    for (const auto& w : groups[gid].outputWidgets)
      if (w.widgetId == wid) {
        sel.append(w);
        break;
      }
  }

  if (sel.isEmpty())
    return;

  QVector<QHash<int, QVariant>> maps;
  maps.reserve(sel.size());
  for (const auto& w : sel)
    maps.append(outputWidgetEditValues(w));

  if (m_outputWidgetModel) {
    m_outputWidgetModel->disconnect(this);
    m_outputWidgetModel->deleteLater();
  }

  m_selectedOutputWidget = sel.first();
  m_outputWidgetModel    = new CustomModel(this);

  buildOutputWidgetCommonRows(m_selectedOutputWidget);
  buildOutputWidgetTransmitRow(m_selectedOutputWidget);
  buildOutputWidgetValueRows(m_selectedOutputWidget);

  const int rows = m_outputWidgetModel->rowCount();
  for (int r = 0; r < rows; ++r) {
    auto* it = m_outputWidgetModel->item(r);
    if (!it)
      continue;

    const auto ptVar = it->data(ParameterType);
    if (!ptVar.isValid())
      continue;

    const int wt         = it->data(WidgetType).toInt();
    const bool intWidget = (wt == ComboBox || wt == CheckBox);
    const QVariant blank = intWidget ? QVariant(-1) : QVariant(QString());

    const int pt = ptVar.toInt();
    if (pt == kOutputWidget_Title) {
      it->setEditable(false);
      it->setData(false, Active);
      it->setData(blank, EditableValue);
      it->setData(tr("(multiple)"), PlaceholderValue);
      continue;
    }

    const QVariant first = maps.first().value(pt);
    bool mixed           = false;
    for (int i = 1; i < maps.size(); ++i)
      if (maps[i].contains(pt) && maps[i].value(pt) != first) {
        mixed = true;
        break;
      }

    if (mixed) {
      it->setData(blank, EditableValue);
      it->setData(tr("Mixed"), PlaceholderValue);
    }
  }

  connect(m_outputWidgetModel,
          &CustomModel::itemChanged,
          this,
          &DataModel::ProjectEditor::onMultiSelectionItemChanged);

  Q_EMIT outputWidgetModelChanged();
}

/**
 * @brief Fans a single aggregate-form field edit out across every selected item, as one modified
 *        state and one autosave, then rebuilds the aggregate model to refresh common/mixed.
 */
void DataModel::ProjectEditor::onMultiSelectionItemChanged(QStandardItem* item)
{
  if (!item || m_batchApplying)
    return;

  if (m_batchKind == KindOutputWidget) {
    fanOutputWidgetSelectionEdit(item);
    return;
  }

  if (m_batchKind != KindDataset)
    return;

  const auto idInt = static_cast<DatasetItem>(item->data(ParameterType).toInt());
  const auto value = item->data(EditableValue);
  const int vIdx   = value.toInt();
  if (idInt == kDatasetView_Widget && (vIdx < 0 || vIdx >= m_datasetWidgets.size()))
    return;

  if (idInt == kDatasetView_Plot && (vIdx < 0 || vIdx >= m_plotOptions.size()))
    return;

  if (idInt == kDatasetView_FFT_Samples && (vIdx < 0 || vIdx >= m_fftSamples.size()))
    return;

  if (idInt == kDatasetView_FFT_Window && (vIdx < 0 || vIdx >= m_fftWindowValues.size()))
    return;

  auto& pm = m_projectModelRef;

  QVector<DataModel::Dataset> sel;
  QVector<QPair<int, int>> ids;
  {
    const auto& groups = pm.groups();
    for (const auto& pr : m_batchItems) {
      const int gid = pr.first, dsid = pr.second;
      if (gid < 0 || static_cast<size_t>(gid) >= groups.size())
        continue;

      for (const auto& d : groups[gid].datasets)
        if (d.datasetId == dsid) {
          sel.append(d);
          ids.append(pr);
          break;
        }
    }
  }

  pm.setAutoSaveSuspended(true);
  m_batchApplying = true;
  for (int i = 0; i < sel.size(); ++i) {
    DataModel::Dataset ds = sel[i];
    onDatasetCommonItemChanged(item, ds);
    onDatasetWidgetItemChanged(item, ds);
    onDatasetRangeItemChanged(item, ds);
    onDatasetFftItemChanged(item, ds);
    onDatasetFlagItemChanged(item, ds);
    pm.updateDataset(ids[i].first, ids[i].second, ds, false);
  }

  m_batchApplying = false;
  pm.setAutoSaveSuspended(false);

  buildMultiDatasetModel();
  pm.flushAutoSave();
}

/**
 * @brief Fans one aggregate-form field edit out across every selected output widget, as one
 * modified state and one autosave, then rebuilds the aggregate model to refresh common/mixed.
 */
void DataModel::ProjectEditor::fanOutputWidgetSelectionEdit(QStandardItem* item)
{
  auto& pm = m_projectModelRef;

  QVector<DataModel::OutputWidget> sel;
  QVector<QPair<int, int>> ids;
  {
    const auto& groups = pm.groups();
    for (const auto& pr : m_batchItems) {
      const int gid = pr.first, wid = pr.second;
      if (gid < 0 || static_cast<size_t>(gid) >= groups.size())
        continue;

      for (const auto& w : groups[gid].outputWidgets)
        if (w.widgetId == wid) {
          sel.append(w);
          ids.append(pr);
          break;
        }
    }
  }

  pm.setAutoSaveSuspended(true);
  m_batchApplying = true;
  for (int i = 0; i < sel.size(); ++i) {
    DataModel::OutputWidget widget = sel[i];
    applyOutputWidgetField(item, widget);
    pm.updateOutputWidget(ids[i].first, ids[i].second, widget, false);
  }

  m_batchApplying = false;
  pm.setAutoSaveSuspended(false);

  buildMultiOutputWidgetModel();
  pm.flushAutoSave();
}

/**
 * @brief Applies a dataset visualization toggle (plot/FFT/waterfall/widget/LED) to every dataset in
 *        the current multi-selection, as one modified state and one autosave.
 */
void DataModel::ProjectEditor::changeDatasetOptionForSelection(int option, bool checked)
{
  if (m_batchKind != KindDataset)
    return;

  const auto opt = static_cast<SerialStudio::DatasetOption>(option);
  auto& pm       = m_projectModelRef;

  QVector<DataModel::Dataset> sel;
  QVector<QPair<int, int>> ids;
  {
    const auto& groups = pm.groups();
    for (const auto& pr : m_batchItems) {
      const int gid = pr.first, dsid = pr.second;
      if (gid < 0 || static_cast<size_t>(gid) >= groups.size())
        continue;

      for (const auto& d : groups[gid].datasets)
        if (d.datasetId == dsid) {
          sel.append(d);
          ids.append(pr);
          break;
        }
    }
  }

  pm.setAutoSaveSuspended(true);
  for (int i = 0; i < sel.size(); ++i) {
    DataModel::Dataset ds = sel[i];
    switch (opt) {
      case SerialStudio::DatasetPlot:
        ds.plt = checked;
        break;
      case SerialStudio::DatasetFFT:
        ds.fft = checked;
        break;
      case SerialStudio::DatasetBar:
        ds.widget = checked ? QStringLiteral("bar") : QString();
        break;
      case SerialStudio::DatasetGauge:
        ds.widget = checked ? QStringLiteral("gauge") : QString();
        break;
      case SerialStudio::DatasetCompass:
        ds.widget = checked ? QStringLiteral("compass") : QString();
        break;
      case SerialStudio::DatasetMeter:
        ds.widget = checked ? QStringLiteral("meter") : QString();
        break;
      case SerialStudio::DatasetLED:
        ds.led = checked;
        break;
      case SerialStudio::DatasetWaterfall:
        ds.waterfall = checked;
        break;
      default:
        break;
    }

    pm.updateDataset(ids[i].first, ids[i].second, ds, false);
  }
  pm.setAutoSaveSuspended(false);

  buildMultiDatasetModel();
  Q_EMIT datasetOptionsChanged();
  pm.flushAutoSave();
}
