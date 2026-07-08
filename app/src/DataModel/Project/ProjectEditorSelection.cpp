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
// Private slot: view management
//--------------------------------------------------------------------------------------------------

/**
 * @brief Transitions the editor to the given view.
 */
void DataModel::ProjectEditor::setCurrentView(const DataModel::ProjectEditor::CurrentView view)
{
  if (m_suppressViewChange) [[unlikely]]
    return;

  if (m_currentView == view)
    return;

  m_currentView = view;
  Q_EMIT currentViewChanged();
  Q_EMIT selectedTextChanged();
}

/**
 * @brief Toggles the suppression latch used by the diagram's context-menu actions.
 */
void DataModel::ProjectEditor::setSuppressViewChange(bool suppress) noexcept
{
  m_suppressViewChange = suppress;
}

//--------------------------------------------------------------------------------------------------
// Private slot: tree selection changed
//--------------------------------------------------------------------------------------------------

/**
 * @brief Refreshes the selected-source snapshot and switches to the parser view.
 */
bool DataModel::ProjectEditor::selectSourceParserItem(QStandardItem* item)
{
  if (!m_sourceParserItems.contains(item))
    return false;

  const auto cached      = m_sourceParserItems.value(item);
  const auto& srcs       = m_projectModelRef.sources();
  DataModel::Source live = cached;
  for (const auto& s : srcs) {
    if (s.sourceId == cached.sourceId) {
      live = s;
      break;
    }
  }
  m_selectedSource = live;
  setCurrentView(SourceFrameParserView);
  Q_EMIT selectedSourceFrameParserCodeChanged();
  Q_EMIT sourceModelChanged();
  return true;
}

/**
 * @brief Switches the form to the SourceView for the clicked source item.
 */
bool DataModel::ProjectEditor::selectSourceItem(QStandardItem* item)
{
  if (!m_sourceItems.contains(item))
    return false;

  const auto cached      = m_sourceItems.value(item);
  const auto& srcs       = m_projectModelRef.sources();
  DataModel::Source live = cached;
  for (const auto& s : srcs) {
    if (s.sourceId == cached.sourceId) {
      live = s;
      break;
    }
  }

  if (m_currentView == SourceView && live.sourceId == m_selectedSource.sourceId)
    return true;

  setCurrentView(SourceView);
  buildSourceModel(live);
  return true;
}

/**
 * @brief Switches the form to the GroupView for the clicked group item.
 */
bool DataModel::ProjectEditor::selectGroupItem(QStandardItem* item)
{
  if (!m_groupItems.contains(item))
    return false;

  auto& pm              = m_projectModelRef;
  const auto cached     = m_groupItems.value(item);
  const auto& groups    = pm.groups();
  DataModel::Group live = cached;
  if (cached.groupId >= 0 && static_cast<size_t>(cached.groupId) < groups.size())
    live = groups[cached.groupId];

  pm.setSelectedGroup(live);
  setCurrentView(GroupView);
  buildGroupModel(live);
  return true;
}

/**
 * @brief Routes the Groups root and group folder items to their navigable views.
 */
bool DataModel::ProjectEditor::selectGroupFolderItem(QStandardItem* item)
{
  if (item == m_groupsRootItem) {
    setCurrentView(GroupsView);
    return true;
  }

  if (m_groupFolderItems.contains(item)) {
    const int fid = m_groupFolderItems.value(item);
    if (m_selectedGroupFolderId != fid) {
      m_selectedGroupFolderId = fid;
      Q_EMIT selectedGroupFolderIdChanged();
    }
    setCurrentView(GroupFolderView);
    return true;
  }

  return false;
}

/**
 * @brief Switches the form to the DatasetView for the clicked dataset item.
 */
bool DataModel::ProjectEditor::selectDatasetItem(QStandardItem* item)
{
  if (!m_datasetItems.contains(item))
    return false;

  auto& pm                = m_projectModelRef;
  const auto cached       = m_datasetItems.value(item);
  const auto& groups      = pm.groups();
  DataModel::Dataset live = cached;
  if (cached.groupId >= 0 && static_cast<size_t>(cached.groupId) < groups.size()) {
    for (const auto& d : groups[cached.groupId].datasets) {
      if (d.datasetId == cached.datasetId) {
        live = d;
        break;
      }
    }
  }

  pm.setSelectedDataset(live);
  setCurrentView(DatasetView);
  buildDatasetModel(live);
  return true;
}

/**
 * @brief Switches the form to the ActionView for the clicked action item.
 */
bool DataModel::ProjectEditor::selectActionItem(QStandardItem* item)
{
  if (!m_actionItems.contains(item))
    return false;

  auto& pm               = m_projectModelRef;
  const auto cached      = m_actionItems.value(item);
  const auto& actions    = pm.actions();
  DataModel::Action live = cached;
  for (const auto& a : actions) {
    if (a.actionId == cached.actionId) {
      live = a;
      break;
    }
  }

  pm.setSelectedAction(live);
  setCurrentView(ActionView);
  buildActionModel(live);
  return true;
}

/**
 * @brief Switches the form to the OutputWidgetView for the clicked widget item.
 */
bool DataModel::ProjectEditor::selectOutputWidgetItem(QStandardItem* item)
{
  if (!m_outputWidgetItems.contains(item))
    return false;

  auto& pm                     = m_projectModelRef;
  const auto cached            = m_outputWidgetItems.value(item);
  const auto& groups           = pm.groups();
  DataModel::OutputWidget live = cached;
  if (cached.groupId >= 0 && static_cast<size_t>(cached.groupId) < groups.size()) {
    for (const auto& w : groups[cached.groupId].outputWidgets) {
      if (w.widgetId == cached.widgetId) {
        live = w;
        break;
      }
    }
  }

  pm.setSelectedOutputWidget(live);
  setCurrentView(OutputWidgetView);
  buildOutputWidgetModel(live);
  return true;
}

/**
 * @brief Routes selections under the data-tables tree branch.
 */
bool DataModel::ProjectEditor::selectDataTableItem(QStandardItem* item)
{
  if (item == m_tablesRootItem) {
    setCurrentView(DataTablesView);
    return true;
  }

  if (item == m_systemDatasetsItem) {
    setCurrentView(SystemDatasetsView);
    return true;
  }

  if (m_userTableItems.contains(item)) {
    const auto name = m_userTableItems.value(item);
    if (m_selectedUserTable != name) {
      m_selectedUserTable = name;
      Q_EMIT selectedUserTableChanged();
    }
    setCurrentView(UserTableView);
    return true;
  }

  if (m_tableFolderItems.contains(item)) {
    const int fid = m_tableFolderItems.value(item);
    if (m_selectedTableFolderId != fid) {
      m_selectedTableFolderId = fid;
      Q_EMIT selectedTableFolderIdChanged();
    }
    setCurrentView(TableFolderView);
    return true;
  }

  return false;
}

/**
 * @brief Routes selections under the workspaces tree branch.
 */
bool DataModel::ProjectEditor::selectWorkspaceTreeItem(QStandardItem* item)
{
  if (item == m_workspacesRootItem) {
    setCurrentView(WorkspacesView);
    return true;
  }

  if (m_workspaceItems.contains(item)) {
    const int wid = m_workspaceItems.value(item);
    if (m_selectedWorkspaceId != wid) {
      m_selectedWorkspaceId = wid;
      Q_EMIT selectedWorkspaceIdChanged();
    }
    setCurrentView(WorkspaceView);
    return true;
  }

  if (m_workspaceFolderItems.contains(item)) {
    const int fid = m_workspaceFolderItems.value(item);
    if (m_selectedFolderId != fid) {
      m_selectedFolderId = fid;
      Q_EMIT selectedFolderIdChanged();
    }
    setCurrentView(WorkspaceFolderView);
    return true;
  }

  return false;
}

/**
 * @brief Routes selection of the single MQTT Publisher tree node.
 */
bool DataModel::ProjectEditor::selectMqttPublisherItem(QStandardItem* item)
{
  if (item != m_mqttPublisherItem || item == nullptr)
    return false;

  buildMqttPublisherModel();
  setCurrentView(MqttPublisherView);
  return true;
}

/**
 * @brief Switches to the control-script view when its tree node is selected.
 */
bool DataModel::ProjectEditor::selectControlScriptItem(QStandardItem* item)
{
  if (item != m_controlScriptItem || item == nullptr)
    return false;

  setCurrentView(ControlScriptView);
  return true;
}

/**
 * @brief Switches the active editor view based on the newly selected tree item.
 */
void DataModel::ProjectEditor::onCurrentSelectionChanged(const QModelIndex& current,
                                                         const QModelIndex& previous)
{
  (void)previous;

  if (!m_treeModel)
    return;

  if (tryMultiSelection())
    return;

  m_batchKind = KindNone;
  m_batchItems.clear();

  if (!current.isValid())
    return;

  auto* item = m_treeModel->itemFromIndex(current);
  if (!item)
    return;

  if (selectSourceParserItem(item))
    return;

  if (selectSourceItem(item))
    return;

  if (selectGroupItem(item))
    return;

  if (selectGroupFolderItem(item))
    return;

  if (selectDatasetItem(item))
    return;

  if (selectActionItem(item))
    return;

  if (selectOutputWidgetItem(item))
    return;

  if (selectDataTableItem(item))
    return;

  if (selectWorkspaceTreeItem(item))
    return;

  if (selectMqttPublisherItem(item))
    return;

  if (selectControlScriptItem(item))
    return;

  if (m_rootItems.contains(item)) {
    setCurrentView(ProjectView);
    buildProjectModel();
  }
}

/**
 * @brief Selects the source item with the given sourceId in the tree.
 */
void DataModel::ProjectEditor::selectSource(int sourceId)
{
  if (!m_selectionModel)
    return;

  for (auto it = m_sourceItems.begin(); it != m_sourceItems.end(); ++it) {
    if (it.value().sourceId == sourceId) {
      m_selectionModel->setCurrentIndex(it.key()->index(), QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
}

/**
 * @brief Selects the group item with the given groupId in the tree.
 */
void DataModel::ProjectEditor::selectGroup(int groupId)
{
  if (!m_selectionModel)
    return;

  for (auto it = m_groupItems.begin(); it != m_groupItems.end(); ++it) {
    if (it.value().groupId == groupId) {
      m_selectionModel->setCurrentIndex(it.key()->index(), QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
}

/**
 * @brief Selects the dataset item (groupId, datasetId) in the tree.
 */
void DataModel::ProjectEditor::selectDataset(int groupId, int datasetId)
{
  if (!m_selectionModel)
    return;

  for (auto it = m_datasetItems.begin(); it != m_datasetItems.end(); ++it) {
    if (it.value().groupId == groupId && it.value().datasetId == datasetId) {
      m_selectionModel->setCurrentIndex(it.key()->index(), QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
}

/**
 * @brief Selects the action item with the given actionId in the tree.
 */
void DataModel::ProjectEditor::selectAction(int actionId)
{
  if (!m_selectionModel)
    return;

  for (auto it = m_actionItems.begin(); it != m_actionItems.end(); ++it) {
    if (it.value().actionId == actionId) {
      m_selectionModel->setCurrentIndex(it.key()->index(), QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
}

/**
 * @brief Selects the Frame Parser tree node for the given source.
 */
void DataModel::ProjectEditor::selectFrameParser(int sourceId)
{
  displayFrameParserView(sourceId);
}

/**
 * @brief Selects the output widget (groupId, widgetId) in the tree.
 */
void DataModel::ProjectEditor::selectOutputWidget(int groupId, int widgetId)
{
  if (!m_selectionModel)
    return;

  for (auto it = m_outputWidgetItems.begin(); it != m_outputWidgetItems.end(); ++it) {
    if (it.value().groupId == groupId && it.value().widgetId == widgetId) {
      m_selectionModel->setCurrentIndex(it.key()->index(), QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
}
