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

#include "API/Handlers/WorkspacesHandler.h"

#include <algorithm>
#include <QJsonArray>
#include <QJsonObject>

#include "API/CommandRegistry.h"
#include "API/SchemaBuilder.h"
#include "AppState.h"
#include "DataModel/Frame.h"
#include "DataModel/ProjectModel.h"
#include "SerialStudio.h"

//--------------------------------------------------------------------------------------------------
// Local helpers
//--------------------------------------------------------------------------------------------------

/**
 * @brief Locates a workspace by id in ProjectModel's list.
 */
[[nodiscard]] static auto findWorkspace(const std::vector<DataModel::Workspace>& ws, int wid)
{
  return std::find_if(ws.begin(), ws.end(), [wid](const auto& w) { return w.workspaceId == wid; });
}

/**
 * @brief Returns true when the project model is in ProjectFile mode and ready
 *        to accept workspace mutations.
 */
[[nodiscard]] static bool inProjectFileMode()
{
  return AppState::instance().operationMode() == SerialStudio::ProjectFile;
}

/**
 * @brief Serialises a workspace's widget refs as a QJsonArray of objects.
 */
[[nodiscard]] static QJsonArray refsToJson(const std::vector<DataModel::WidgetRef>& refs)
{
  QJsonArray arr;
  for (const auto& r : refs) {
    QJsonObject entry;
    entry[QStringLiteral("widgetType")]    = r.widgetType;
    entry[QStringLiteral("groupId")]       = r.groupId;
    entry[QStringLiteral("relativeIndex")] = r.relativeIndex;
    arr.append(entry);
  }

  return arr;
}

/**
 * @brief Returns the deduped DashboardWidget enums a group can render.
 */
[[nodiscard]] static QList<int> compatibleWidgetTypes(const DataModel::Group& group)
{
  QList<int> out;
  const auto group_widget = static_cast<int>(SerialStudio::getDashboardWidget(group));
  if (group_widget != SerialStudio::DashboardNoWidget)
    out.append(group_widget);

  for (const auto& ds : group.datasets) {
    for (const auto w : SerialStudio::getDashboardWidgets(ds)) {
      const auto v = static_cast<int>(w);
      if (!out.contains(v))
        out.append(v);
    }
  }

  return out;
}

//--------------------------------------------------------------------------------------------------
// Command registration
//--------------------------------------------------------------------------------------------------

/**
 * @brief Register all workspace-related commands with the registry.
 */
void API::Handlers::WorkspacesHandler::registerCommands()
{
  registerWorkspaceCrudCommands();
  registerCustomizeCommands();
  registerWidgetRefCommands();
}

/**
 * @brief Register list / get / add / delete / rename / autoGenerate commands.
 */
void API::Handlers::WorkspacesHandler::registerWorkspaceCrudCommands()
{
  auto& registry   = CommandRegistry::instance();
  const auto empty = API::emptySchema();

  registry.registerCommand(QStringLiteral("project.workspace.list"),
                           QStringLiteral("List all workspaces with widget counts"),
                           empty,
                           &list);
  registry.registerCommand(
    QStringLiteral("project.workspace.get"),
    QStringLiteral("Return widget refs for a workspace (params: id)"),
    API::makeSchema({
      {QStringLiteral("id"), QStringLiteral("integer"), QStringLiteral("Workspace id")}
  }),
    &get);
  registry.registerCommand(
    QStringLiteral("project.workspace.add"),
    QStringLiteral("Create a new workspace (params: title=\"Workspace\"). Returns new id."),
    API::makeSchema(
      {
  },
      {{QStringLiteral("title"), QStringLiteral("string"), QStringLiteral("Workspace title")}}),
    &add);
  registry.registerCommand(
    QStringLiteral("project.workspace.delete"),
    QStringLiteral("Delete a workspace (params: id)"),
    API::makeSchema({
      {QStringLiteral("id"), QStringLiteral("integer"), QStringLiteral("Workspace id")}
  }),
    &remove);
  registry.registerCommand(
    QStringLiteral("project.workspace.rename"),
    QStringLiteral("Rename a workspace (params: id, title)"),
    API::makeSchema({
      {   QStringLiteral("id"), QStringLiteral("integer"),        QStringLiteral("Workspace id")},
      {QStringLiteral("title"),  QStringLiteral("string"), QStringLiteral("New workspace title")}
  }),
    &rename);
  registry.registerCommand(
    QStringLiteral("project.workspace.update"),
    QStringLiteral("Patch workspace fields (params: id; optional title, icon)"),
    API::makeSchema(
      {
        {QStringLiteral("id"), QStringLiteral("integer"), QStringLiteral("Workspace id")}
  },
      {{QStringLiteral("title"),
        QStringLiteral("string"),
        QStringLiteral("New workspace title (optional)")},
       {QStringLiteral("icon"),
        QStringLiteral("string"),
        QStringLiteral("New workspace icon, e.g. 'qrc:/icons/panes/overview.svg' (optional)")}}),
    &update);
  registry.registerCommand(
    QStringLiteral("project.workspace.autoGenerate"),
    QStringLiteral(
      "Materialise synthetic workspaces into the customised set. No-op if already customised."),
    empty,
    &autoGenerate);
}

/**
 * @brief Register customizeWorkspaces flag get/set commands.
 */
void API::Handlers::WorkspacesHandler::registerCustomizeCommands()
{
  auto& registry = CommandRegistry::instance();

  registry.registerCommand(QStringLiteral("project.workspace.getCustomizeMode"),
                           QStringLiteral("Return the customizeWorkspaces flag"),
                           API::emptySchema(),
                           &customizeGet);
  registry.registerCommand(QStringLiteral("project.workspace.setCustomizeMode"),
                           QStringLiteral("Flip the customizeWorkspaces flag (params: enabled)"),
                           API::makeSchema({
                             {QStringLiteral("enabled"),
                              QStringLiteral("boolean"),
                              QStringLiteral("Enable (true) or disable (false)")}
  }),
                           &customizeSet);
}

/**
 * @brief Register widget-ref add/remove commands.
 */
void API::Handlers::WorkspacesHandler::registerWidgetRefCommands()
{
  auto& registry = CommandRegistry::instance();

  // SerialStudio::DashboardWidget enums valid as workspace tiles (no Terminal/NoWidget)
  const QJsonArray kWidgetTypeValues = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
#ifdef BUILD_COMMERCIAL
    14,
    15,
    16,
    17,
    18,
#endif
  };

  const auto addSchema = API::makeSchema({
    {  QStringLiteral("workspaceId"),
     QStringLiteral("integer"),
     QStringLiteral("Workspace id from project.workspaces.list (workspace IDs are >= 1000)")},
    API::enumProp(QStringLiteral("widgetType"),
                  QStringLiteral("DashboardWidget enum: 1=DataGrid, "
                                 "2=MultiPlot, 3=Accelerometer, "
                                 "4=Gyroscope, 5=GPS, 6=Plot3D, "
                                 "7=FFT, 8=LED, 9=Plot, 10=Bar, "
                                 "11=Gauge, 12=Compass, "
                                 "14=ImageView (Pro), "
                                 "15=OutputPanel (Pro), "
                                 "16=NotificationLog (Pro), "
                                 "17=Waterfall (Pro), "
                                 "18=Painter (Pro). DO NOT pass 0 -- "
                                 "that is Terminal, not a tile."),
                  kWidgetTypeValues),
    {      QStringLiteral("groupId"),
     QStringLiteral("integer"),
     QStringLiteral("groupId from project.groups.list. Use group.id, NOT the array index.") },
    {QStringLiteral("relativeIndex"),
     QStringLiteral("integer"),
     QStringLiteral("Almost always 0. This is a per-(widgetType,groupId) "
     "deduplication counter -- it is NOT a dataset index. Pass 0 "
     "unless you are intentionally adding a second tile with the "
     "same widgetType and groupId to the same workspace.")                                  }
  });
  registry.registerCommand(QStringLiteral("project.workspace.addWidget"),
                           QStringLiteral("Pin a visualization tile onto a workspace. The tile "
                                          "renders the dashboard widget of the given widgetType "
                                          "fed by the given groupId. Read project.groups.list "
                                          "and project.workspaces.list first to get valid IDs "
                                          "and the group's compatibleWidgetTypes."),
                           addSchema,
                           &widgetAdd);

  const auto removeSchema = API::makeSchema({
    {  QStringLiteral("workspaceId"),QStringLiteral("integer"),QStringLiteral("Workspace id")                                                                },
    {   QStringLiteral("widgetType"),
     QStringLiteral("integer"),
     QStringLiteral("SerialStudio.DashboardWidget enum")                                          },
    {      QStringLiteral("groupId"), QStringLiteral("integer"), QStringLiteral("Source group id")},
    {QStringLiteral("relativeIndex"),
     QStringLiteral("integer"),
     QStringLiteral("Relative widget index")                                                      }
  });
  registry.registerCommand(
    QStringLiteral("project.workspace.removeWidget"),
    QStringLiteral("Remove a widget ref (params: workspaceId, widgetType, groupId, relativeIndex)"),
    removeSchema,
    &widgetRemove);
}

//--------------------------------------------------------------------------------------------------
// Queries
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the workspace list with id, title, icon, and widget count.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::list(const QString& id,
                                                            const QJsonObject& params)
{
  Q_UNUSED(params)

  const auto& pm         = DataModel::ProjectModel::instance();
  const auto& workspaces = pm.activeWorkspaces();

  QJsonArray arr;
  for (const auto& ws : workspaces) {
    QJsonObject entry;
    entry[QStringLiteral("id")]          = ws.workspaceId;
    entry[QStringLiteral("title")]       = ws.title;
    entry[QStringLiteral("icon")]        = ws.icon;
    entry[QStringLiteral("widgetCount")] = static_cast<int>(ws.widgetRefs.size());
    arr.append(entry);
  }

  QJsonObject result;
  result[QStringLiteral("workspaces")]       = arr;
  result[QStringLiteral("count")]            = static_cast<int>(workspaces.size());
  result[QStringLiteral("customizeEnabled")] = pm.customizeWorkspaces();
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Returns widget refs for a single workspace.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::get(const QString& id,
                                                           const QJsonObject& params)
{
  if (!params.contains(QStringLiteral("id")))
    return CommandResponse::makeError(
      id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: id"));

  const int wid = params.value(QStringLiteral("id")).toInt();

  const auto& workspaces = DataModel::ProjectModel::instance().activeWorkspaces();
  const auto it          = findWorkspace(workspaces, wid);
  if (it == workspaces.end())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace not found: %1").arg(wid));

  QJsonObject result;
  result[QStringLiteral("id")]      = it->workspaceId;
  result[QStringLiteral("title")]   = it->title;
  result[QStringLiteral("icon")]    = it->icon;
  result[QStringLiteral("widgets")] = refsToJson(it->widgetRefs);
  return CommandResponse::makeSuccess(id, result);
}

//--------------------------------------------------------------------------------------------------
// Mutations
//--------------------------------------------------------------------------------------------------

/**
 * @brief Creates a new workspace; returns the assigned id.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::add(const QString& id,
                                                           const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  const QString title = params.value(QStringLiteral("title")).toString(QStringLiteral("Workspace"));

  const int newId = DataModel::ProjectModel::instance().addWorkspace(title);

  QJsonObject result;
  result[QStringLiteral("id")]    = newId;
  result[QStringLiteral("title")] = title;
  result[QStringLiteral("added")] = true;
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Deletes a workspace. No-op if id not found.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::remove(const QString& id,
                                                              const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  if (!params.contains(QStringLiteral("id")))
    return CommandResponse::makeError(
      id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: id"));

  auto& pm = DataModel::ProjectModel::instance();
  if (!pm.customizeWorkspaces())
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("customizeWorkspaces is off; call project.workspaces.customize.set first"));

  const int wid = params.value(QStringLiteral("id")).toInt();
  pm.deleteWorkspace(wid);

  QJsonObject result;
  result[QStringLiteral("id")]      = wid;
  result[QStringLiteral("deleted")] = true;
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Renames a workspace. No-op if id not found.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::rename(const QString& id,
                                                              const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  if (!params.contains(QStringLiteral("id")))
    return CommandResponse::makeError(
      id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: id"));

  if (!params.contains(QStringLiteral("title")))
    return CommandResponse::makeError(
      id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: title"));

  auto& pm = DataModel::ProjectModel::instance();
  if (!pm.customizeWorkspaces())
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("customizeWorkspaces is off; call project.workspaces.customize.set first"));

  const int wid          = params.value(QStringLiteral("id")).toInt();
  const QString newTitle = params.value(QStringLiteral("title")).toString();
  if (newTitle.trimmed().isEmpty())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace title cannot be empty"));

  pm.renameWorkspace(wid, newTitle);

  QJsonObject result;
  result[QStringLiteral("id")]      = wid;
  result[QStringLiteral("title")]   = newTitle;
  result[QStringLiteral("renamed")] = true;
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Patches workspace title and/or icon by id.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::update(const QString& id,
                                                              const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  if (!params.contains(QStringLiteral("id")))
    return CommandResponse::makeError(
      id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: id"));

  auto& pm = DataModel::ProjectModel::instance();
  if (!pm.customizeWorkspaces())
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("customizeWorkspaces is off; call project.workspace.setCustomizeMode first"));

  const int wid       = params.value(QStringLiteral("id")).toInt();
  const bool setTitle = params.contains(QStringLiteral("title"));
  const bool setIcon  = params.contains(QStringLiteral("icon"));
  if (!setTitle && !setIcon)
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Provide at least one of: title, icon"));

  QString title;
  QString icon;
  if (setTitle) {
    title = params.value(QStringLiteral("title")).toString();
    if (title.trimmed().isEmpty())
      return CommandResponse::makeError(
        id, ErrorCode::InvalidParam, QStringLiteral("Workspace title cannot be empty"));
  }
  if (setIcon)
    icon = params.value(QStringLiteral("icon")).toString();

  pm.updateWorkspace(wid, title, icon, setTitle, setIcon);

  QJsonObject result;
  result[QStringLiteral("id")] = wid;
  if (setTitle)
    result[QStringLiteral("title")] = title;

  if (setIcon)
    result[QStringLiteral("icon")] = icon;

  result[QStringLiteral("updated")] = true;
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Materialises the synthetic auto-workspaces into customized state.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::autoGenerate(const QString& id,
                                                                    const QJsonObject& params)
{
  Q_UNUSED(params)

  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  const int firstId = DataModel::ProjectModel::instance().autoGenerateWorkspaces();

  QJsonObject result;
  result[QStringLiteral("firstWorkspaceId")] = firstId;
  result[QStringLiteral("generated")]        = (firstId != -1);
  return CommandResponse::makeSuccess(id, result);
}

//--------------------------------------------------------------------------------------------------
// Customize flag
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the current value of the customizeWorkspaces flag.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::customizeGet(const QString& id,
                                                                    const QJsonObject& params)
{
  Q_UNUSED(params)

  QJsonObject result;
  result[QStringLiteral("enabled")] = DataModel::ProjectModel::instance().customizeWorkspaces();
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Flips the customizeWorkspaces flag.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::customizeSet(const QString& id,
                                                                    const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  if (!params.contains(QStringLiteral("enabled")))
    return CommandResponse::makeError(
      id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: enabled"));

  const bool enabled = params.value(QStringLiteral("enabled")).toBool();
  DataModel::ProjectModel::instance().setCustomizeWorkspaces(enabled);

  QJsonObject result;
  result[QStringLiteral("enabled")] = enabled;
  result[QStringLiteral("updated")] = true;
  return CommandResponse::makeSuccess(id, result);
}

//--------------------------------------------------------------------------------------------------
// Widget ref mutations
//--------------------------------------------------------------------------------------------------

/**
 * @brief Attaches a widget ref to a workspace.
 */
API::CommandResponse API::Handlers::WorkspacesHandler::widgetAdd(const QString& id,
                                                                 const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  const QStringList required{
    QStringLiteral("workspaceId"),
    QStringLiteral("widgetType"),
    QStringLiteral("groupId"),
    QStringLiteral("relativeIndex"),
  };

  for (const auto& key : required)
    if (!params.contains(key))
      return CommandResponse::makeError(
        id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: %1").arg(key));

  auto& pm = DataModel::ProjectModel::instance();
  if (!pm.customizeWorkspaces())
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("customizeWorkspaces is off; call project.workspaces.customize.set first"));

  const int wid      = params.value(QStringLiteral("workspaceId")).toInt();
  const int wtype    = params.value(QStringLiteral("widgetType")).toInt();
  const int gid      = params.value(QStringLiteral("groupId")).toInt();
  const int relIndex = params.value(QStringLiteral("relativeIndex")).toInt();

  // Reject DashboardTerminal / DashboardNoWidget with actionable errors
  if (wtype == SerialStudio::DashboardTerminal)
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("widgetType=0 is DashboardTerminal, not a workspace "
                     "tile. Pick a real visualization type from the "
                     "group's compatibleWidgetTypes "
                     "(see project.groups.list)."));

  if (wtype == SerialStudio::DashboardNoWidget)
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("widgetType=13 is DashboardNoWidget. Pick a real "
                     "visualization type."));

  // Validate the target workspace exists; reject stale IDs explicitly
  const auto& wsList = pm.editorWorkspaces();
  const auto exists  = std::any_of(
    wsList.begin(), wsList.end(), [wid](const auto& ws) { return ws.workspaceId == wid; });
  if (!exists)
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace not found: %1").arg(wid));

  // Validate widgetType matches one the target group can render
  const auto& groups  = DataModel::ProjectModel::instance().groups();
  const auto group_it = std::find_if(
    groups.begin(), groups.end(), [gid](const DataModel::Group& g) { return g.groupId == gid; });
  if (group_it == groups.end())
    return CommandResponse::makeError(id,
                                      ErrorCode::InvalidParam,
                                      QStringLiteral("Group not found: %1. Use group.id from "
                                                     "project.group.list, not the array index.")
                                        .arg(gid));

  const auto compatible = compatibleWidgetTypes(*group_it);
  if (!compatible.contains(wtype)) {
    QStringList compat_strs;
    for (int v : compatible)
      compat_strs.append(QString::number(v));

    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("widgetType=%1 is not compatible with group %2 "
                     "('%3'). Compatible widgetTypes for this group: "
                     "[%4]. Either pick one of those, or change the "
                     "group's widget / dataset flags first.")
        .arg(wtype)
        .arg(gid)
        .arg(group_it->title)
        .arg(compat_strs.join(QStringLiteral(", "))));
  }

  pm.addWidgetToWorkspace(wid, wtype, gid, relIndex);

  QJsonObject result;
  result[QStringLiteral("workspaceId")]   = wid;
  result[QStringLiteral("widgetType")]    = wtype;
  result[QStringLiteral("groupId")]       = gid;
  result[QStringLiteral("relativeIndex")] = relIndex;
  result[QStringLiteral("added")]         = true;
  return CommandResponse::makeSuccess(id, result);
}

/**
 * @brief Detaches a widget ref matching (widgetType, groupId, relativeIndex).
 */
API::CommandResponse API::Handlers::WorkspacesHandler::widgetRemove(const QString& id,
                                                                    const QJsonObject& params)
{
  if (!inProjectFileMode())
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace mutations require ProjectFile mode"));

  const QStringList required{
    QStringLiteral("workspaceId"),
    QStringLiteral("widgetType"),
    QStringLiteral("groupId"),
    QStringLiteral("relativeIndex"),
  };

  for (const auto& key : required)
    if (!params.contains(key))
      return CommandResponse::makeError(
        id, ErrorCode::MissingParam, QStringLiteral("Missing required parameter: %1").arg(key));

  auto& pm = DataModel::ProjectModel::instance();
  if (!pm.customizeWorkspaces())
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("customizeWorkspaces is off; call project.workspaces.customize.set first"));

  const int wid      = params.value(QStringLiteral("workspaceId")).toInt();
  const int wtype    = params.value(QStringLiteral("widgetType")).toInt();
  const int gid      = params.value(QStringLiteral("groupId")).toInt();
  const int relIndex = params.value(QStringLiteral("relativeIndex")).toInt();

  pm.removeWidgetFromWorkspace(wid, wtype, gid, relIndex);

  QJsonObject result;
  result[QStringLiteral("workspaceId")]   = wid;
  result[QStringLiteral("widgetType")]    = wtype;
  result[QStringLiteral("groupId")]       = gid;
  result[QStringLiteral("relativeIndex")] = relIndex;
  result[QStringLiteral("removed")]       = true;
  return CommandResponse::makeSuccess(id, result);
}
