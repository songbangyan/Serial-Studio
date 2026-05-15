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
#include <optional>
#include <QJsonArray>
#include <QJsonObject>

#include "API/CommandRegistry.h"
#include "API/EnumLabels.h"
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
 * @brief Returns a hint on how to unlock @a wtype on @a group, or empty when none applies.
 */
[[nodiscard]] static QString unlockHint(int wtype, int gid)
{
  switch (wtype) {
    case SerialStudio::DashboardPlot:
      return QStringLiteral(" To enable widgetType=9 (Plot), set DatasetOption "
                            "bit 1 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<currentBitfield|1>} or "
                            "project.dataset.update{groupId:%1, datasetId:<n>, "
                            "graph:true}.")
        .arg(gid);
    case SerialStudio::DashboardFFT:
      return QStringLiteral(" To enable widgetType=7 (FFT), set DatasetOption "
                            "bit 2 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<currentBitfield|2>} or "
                            "project.dataset.update{groupId:%1, datasetId:<n>, "
                            "fft:true}.")
        .arg(gid);
    case SerialStudio::DashboardBar:
      return QStringLiteral(" To enable widgetType=10 (Bar), set DatasetOption "
                            "bit 4 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<bitfield with bit 4>} "
                            "(clears Gauge/Compass).")
        .arg(gid);
    case SerialStudio::DashboardGauge:
      return QStringLiteral(" To enable widgetType=11 (Gauge), set DatasetOption "
                            "bit 8 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<bitfield with bit 8>} "
                            "(clears Bar/Compass).")
        .arg(gid);
    case SerialStudio::DashboardCompass:
      return QStringLiteral(" To enable widgetType=12 (Compass), set DatasetOption "
                            "bit 16 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<bitfield with bit 16>} "
                            "(clears Bar/Gauge).")
        .arg(gid);
    case SerialStudio::DashboardLED:
      return QStringLiteral(" To enable widgetType=8 (LED), set DatasetOption "
                            "bit 32 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<currentBitfield|32>} or "
                            "project.dataset.update{groupId:%1, datasetId:<n>, "
                            "led:true}.")
        .arg(gid);
#ifdef BUILD_COMMERCIAL
    case SerialStudio::DashboardWaterfall:
      return QStringLiteral(" To enable widgetType=17 (Waterfall), set DatasetOption "
                            "bit 64 via project.dataset.setOptions{groupId:%1, "
                            "datasetId:<n>, options:<currentBitfield|64>} or "
                            "project.dataset.update{groupId:%1, datasetId:<n>, "
                            "waterfall:true}.")
        .arg(gid);
#endif
    default:
      return QStringLiteral(" widgetType=%1 is a group-shape widget -- change "
                            "the group's widget string via project.group.update "
                            "instead of a dataset option.")
        .arg(wtype);
  }
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

/**
 * @brief Resolves widgetType from int or slug; returns -1 when slug is unknown.
 */
[[nodiscard]] static int resolveWidgetType(const QJsonValue& wtypeJson)
{
  if (wtypeJson.isString())
    return API::EnumLabels::dashboardWidgetFromSlug(wtypeJson.toString());

  return wtypeJson.toInt();
}

/**
 * @brief Validates wtype is compatible with the group; returns empty optional on success.
 */
[[nodiscard]] static std::optional<QString> validateGroupCompatibility(
  const DataModel::Group& group, int wtype, int gid)
{
  const auto compatible = compatibleWidgetTypes(group);
  if (compatible.contains(wtype))
    return std::nullopt;

  QStringList compat_strs;
  for (int v : compatible)
    compat_strs.append(QString::number(v));

  return QStringLiteral("widgetType=%1 is not compatible with group %2 ('%3'). "
                        "Compatible widgetTypes for this group: [%4].%5")
    .arg(wtype)
    .arg(gid)
    .arg(group.title)
    .arg(compat_strs.join(QStringLiteral(", ")))
    .arg(unlockHint(wtype, gid));
}

/**
 * @brief Picks the next free relativeIndex for (widgetType, groupId) on the workspace.
 */
[[nodiscard]] static int nextFreeRelativeIndex(const std::vector<DataModel::Workspace>& wsList,
                                               int wid,
                                               int wtype,
                                               int gid)
{
  const auto wsIt = std::find_if(
    wsList.begin(), wsList.end(), [wid](const auto& ws) { return ws.workspaceId == wid; });
  if (wsIt == wsList.end())
    return 0;

  QSet<int> taken;
  for (const auto& ref : wsIt->widgetRefs)
    if (ref.widgetType == wtype && ref.groupId == gid)
      taken.insert(ref.relativeIndex);

  int relIndex = 0;
  while (taken.contains(relIndex))
    ++relIndex;

  return relIndex;
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
    {   QStringLiteral("widgetType"),
     QStringLiteral("string|integer"),
     QStringLiteral("Widget kind. PREFERRED form: a string slug -- 'plot', 'fft', "
     "'bar', 'gauge', 'compass', 'led', 'datagrid', 'multiplot', "
     "'accelerometer', 'gyroscope', 'gps', 'plot3d', 'imageview' (Pro), "
     "'output-panel' (Pro), 'notification-log' (Pro), 'waterfall' (Pro), "
     "'painter' (Pro). Integer DashboardWidget enum still accepted for "
     "back-compat (1=DataGrid, 2=MultiPlot, ... 9=Plot, 10=Bar, 11=Gauge, "
     "12=Compass) but the strings are unambiguous and forwards-compatible.")                },
    {      QStringLiteral("groupId"),
     QStringLiteral("integer"),
     QStringLiteral("groupId from project.groups.list. Use group.id, NOT the array index.") },
    {QStringLiteral("relativeIndex"),
     QStringLiteral("integer"),
     QStringLiteral("OPTIONAL. Per-(widgetType,groupId) deduplication counter; NOT a "
     "dataset index. Omit to auto-assign the next free slot. Pass an "
     "explicit value only when you are reproducing a prior layout.")                        }
  });
  registry.registerCommand(
    QStringLiteral("project.workspace.addWidget"),
    QStringLiteral("Pin a visualization tile onto a workspace. The tile renders the "
                   "dashboard widget of the given widgetType fed by the given groupId. "
                   "MANDATORY pre-flight: call project.group.list (for groupId + "
                   "compatibleWidgetTypes) and project.workspace.list (for workspaceId) "
                   "BEFORE calling this command. Calling it without that information "
                   "produces validation errors. If your widgetType isn't in the group's "
                   "compatibleWidgetTypes, first flip the matching dataset option via "
                   "project.dataset.setOption / project.dataset.setOptions / "
                   "project.dataset.update {graph|fft|led|waterfall}, then re-list and "
                   "call addWidget."),
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
    entry[QStringLiteral("icon")]        = SerialStudio::normalizeIconPath(ws.icon);
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

  const int wid = params.value(QStringLiteral("workspaceId")).toInt();
  const int gid = params.value(QStringLiteral("groupId")).toInt();

  // widgetType accepts either an integer (DashboardWidget enum) or a string slug
  const QJsonValue wtypeJson = params.value(QStringLiteral("widgetType"));
  const int wtype            = resolveWidgetType(wtypeJson);
  if (wtype < 0)
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("Unknown widgetType slug '%1'. Use one of: plot, fft, bar, gauge, "
                     "compass, led, datagrid, multiplot, accelerometer, gyroscope, gps, "
                     "plot3d, terminal, imageview, output-panel, notification-log, "
                     "waterfall, painter.")
        .arg(wtypeJson.toString()));

  const bool hasRelIndex = params.contains(QStringLiteral("relativeIndex"));
  int relIndex           = hasRelIndex ? params.value(QStringLiteral("relativeIndex")).toInt()
                                       : nextFreeRelativeIndex(pm.editorWorkspaces(), wid, wtype, gid);
  const bool relIndexAutoAssigned = !hasRelIndex && relIndex != 0;

  // Reject DashboardTerminal / DashboardNoWidget with actionable errors
  if (wtype == SerialStudio::DashboardTerminal)
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("widgetType=0 is DashboardTerminal/Console, not a workspace tile. "
                     "Common cause: confusing the DatasetOption bitflag (used by "
                     "project.dataset.setOptions: 1=Plot, 2=FFT, 4=Bar, 8=Gauge, "
                     "16=Compass, 32=LED, 64=Waterfall) with the DashboardWidget enum "
                     "(used here: 9=Plot, 7=FFT, 10=Bar, 11=Gauge, 12=Compass, 8=LED, "
                     "17=Waterfall, 1=DataGrid, 2=MultiPlot, 5=GPS). The numbers do not "
                     "line up. Pick a value from the group's compatibleWidgetTypes "
                     "(see project.group.list)."));

  if (wtype == SerialStudio::DashboardNoWidget)
    return CommandResponse::makeError(
      id,
      ErrorCode::InvalidParam,
      QStringLiteral("widgetType=13 is DashboardNoWidget (a sentinel value, "
                     "not a tile). Pick a real visualization type from the "
                     "group's compatibleWidgetTypes "
                     "(see project.group.list)."));

  const auto& wsList = pm.editorWorkspaces();
  const auto exists  = std::any_of(
    wsList.begin(), wsList.end(), [wid](const auto& ws) { return ws.workspaceId == wid; });
  if (!exists)
    return CommandResponse::makeError(
      id, ErrorCode::InvalidParam, QStringLiteral("Workspace not found: %1").arg(wid));

  const auto& groups  = DataModel::ProjectModel::instance().groups();
  const auto group_it = std::find_if(
    groups.begin(), groups.end(), [gid](const DataModel::Group& g) { return g.groupId == gid; });
  if (group_it == groups.end())
    return CommandResponse::makeError(id,
                                      ErrorCode::InvalidParam,
                                      QStringLiteral("Group not found: %1. Use group.id from "
                                                     "project.group.list, not the array index.")
                                        .arg(gid));

  if (const auto err = validateGroupCompatibility(*group_it, wtype, gid))
    return CommandResponse::makeError(id, ErrorCode::InvalidParam, *err);

  pm.addWidgetToWorkspace(wid, wtype, gid, relIndex);

  QJsonObject result;
  result[QStringLiteral("workspaceId")]               = wid;
  result[QStringLiteral("widgetType")]                = wtype;
  result[QStringLiteral("widgetTypeSlug")]            = API::EnumLabels::dashboardWidgetSlug(wtype);
  result[QStringLiteral("groupId")]                   = gid;
  result[QStringLiteral("relativeIndex")]             = relIndex;
  result[QStringLiteral("relativeIndexAutoAssigned")] = relIndexAutoAssigned;
  result[QStringLiteral("added")]                     = true;
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
  const int gid      = params.value(QStringLiteral("groupId")).toInt();
  const int relIndex = params.value(QStringLiteral("relativeIndex")).toInt();

  // widgetType accepts integer or string slug
  int wtype                  = -1;
  const QJsonValue wtypeJson = params.value(QStringLiteral("widgetType"));
  if (wtypeJson.isString()) {
    wtype = API::EnumLabels::dashboardWidgetFromSlug(wtypeJson.toString());
    if (wtype < 0)
      return CommandResponse::makeError(
        id,
        ErrorCode::InvalidParam,
        QStringLiteral("Unknown widgetType slug '%1'.").arg(wtypeJson.toString()));
  } else {
    wtype = wtypeJson.toInt();
  }

  pm.removeWidgetFromWorkspace(wid, wtype, gid, relIndex);

  QJsonObject result;
  result[QStringLiteral("workspaceId")]    = wid;
  result[QStringLiteral("widgetType")]     = wtype;
  result[QStringLiteral("widgetTypeSlug")] = API::EnumLabels::dashboardWidgetSlug(wtype);
  result[QStringLiteral("groupId")]        = gid;
  result[QStringLiteral("relativeIndex")]  = relIndex;
  result[QStringLiteral("removed")]        = true;
  return CommandResponse::makeSuccess(id, result);
}
