/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#include "AI/ToolDispatcher.h"

#include <QFile>
#include <QUuid>

#include "AI/CommandRegistry.h"
#include "AI/Logging.h"
#include "API/CommandProtocol.h"
#include "API/CommandRegistry.h"
#include "Misc/JsonValidator.h"

//--------------------------------------------------------------------------------------------------
// Construction
//--------------------------------------------------------------------------------------------------

/** @brief Creates a tool dispatcher with the given Qt parent. */
AI::ToolDispatcher::ToolDispatcher(QObject* parent) : QObject(parent) {}

//--------------------------------------------------------------------------------------------------
// Catalog
//--------------------------------------------------------------------------------------------------

/** @brief Returns AI-tool catalog derived from API::CommandRegistry, minus Blocked entries. */
QJsonArray AI::ToolDispatcher::availableTools(const QString& category) const
{
  QJsonArray tools;
  const auto& commands = API::CommandRegistry::instance().commands();
  const auto& aiReg    = AI::CommandRegistry::instance();

  for (auto it = commands.constBegin(); it != commands.constEnd(); ++it) {
    const auto& def = it.value();
    if (aiReg.safetyOf(def.name) == Safety::Blocked)
      continue;

    if (!category.isEmpty() && !def.name.startsWith(category))
      continue;

    QJsonObject tool;
    tool[QStringLiteral("name")]        = def.name;
    tool[QStringLiteral("description")] = def.description;
    tool[QStringLiteral("inputSchema")] = def.inputSchema;
    tools.append(tool);
  }

  return tools;
}

/** @brief Returns a compact name+description list of every command, optionally
 *         filtered by prefix. Useful as a discovery tool for the AI. */
QJsonObject AI::ToolDispatcher::listCommands(const QString& prefix) const
{
  const auto& commands = API::CommandRegistry::instance().commands();
  const auto& aiReg    = AI::CommandRegistry::instance();

  QJsonArray entries;
  for (auto it = commands.constBegin(); it != commands.constEnd(); ++it) {
    const auto& def = it.value();
    if (aiReg.safetyOf(def.name) == Safety::Blocked)
      continue;

    if (!prefix.isEmpty() && !def.name.startsWith(prefix))
      continue;

    QJsonObject row;
    row[QStringLiteral("name")]        = def.name;
    row[QStringLiteral("description")] = def.description;
    entries.append(row);
  }

  QJsonObject reply;
  reply[QStringLiteral("ok")]       = true;
  reply[QStringLiteral("count")]    = entries.size();
  reply[QStringLiteral("commands")] = entries;
  return reply;
}

/** @brief Returns the metadata block for a single command, or an empty object. */
QJsonObject AI::ToolDispatcher::describeCommand(const QString& name) const
{
  const auto& commands = API::CommandRegistry::instance().commands();
  const auto it        = commands.constFind(name);
  if (it == commands.constEnd())
    return {};

  if (AI::CommandRegistry::instance().safetyOf(name) == Safety::Blocked)
    return {};

  QJsonObject desc;
  desc[QStringLiteral("name")]        = it.value().name;
  desc[QStringLiteral("description")] = it.value().description;
  desc[QStringLiteral("inputSchema")] = it.value().inputSchema;
  return desc;
}

//--------------------------------------------------------------------------------------------------
// Dispatch
//--------------------------------------------------------------------------------------------------

/** @brief Validates args and forwards to API::CommandRegistry honoring AI safety tags. */
QJsonObject AI::ToolDispatcher::executeCommand(const QString& name,
                                               const QJsonObject& args,
                                               bool autoConfirmSafe)
{
  // Validate the args envelope
  Misc::JsonValidator::Limits limits;
  limits.maxFileSize  = 1 * 1024 * 1024;
  limits.maxDepth     = 32;
  limits.maxArraySize = 1000;

  if (!Misc::JsonValidator::validateStructure(QJsonValue(args), limits)) {
    qCWarning(serialStudioAI) << "Tool args validation failed for" << name;
    QJsonObject reply;
    reply[QStringLiteral("ok")]    = false;
    reply[QStringLiteral("error")] = QStringLiteral("args_validation_failed");
    return reply;
  }

  // Honor safety tags
  const auto safety = AI::CommandRegistry::instance().safetyOf(name);
  if (safety == Safety::Blocked) {
    qCWarning(serialStudioAI) << "Tool execution blocked:" << name;
    QJsonObject reply;
    reply[QStringLiteral("ok")]    = false;
    reply[QStringLiteral("error")] = QStringLiteral("blocked");
    return reply;
  }

  if (safety == Safety::Confirm && !autoConfirmSafe) {
    Q_EMIT confirmationRequested(name, args);
    QJsonObject reply;
    reply[QStringLiteral("ok")]    = false;
    reply[QStringLiteral("error")] = QStringLiteral("awaiting_confirmation");
    return reply;
  }

  // Forward to API::CommandRegistry
  const auto callId   = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto response = API::CommandRegistry::instance().execute(name, callId, args);

  QJsonObject reply;
  reply[QStringLiteral("ok")] = response.success;
  if (response.success)
    reply[QStringLiteral("result")] = response.result;
  else {
    QJsonObject error;
    error[QStringLiteral("code")]    = response.errorCode;
    error[QStringLiteral("message")] = response.errorMessage;
    reply[QStringLiteral("error")]   = error;
  }
  return reply;
}

//--------------------------------------------------------------------------------------------------
// Context (placeholders for the next slice)
//--------------------------------------------------------------------------------------------------

/** @brief Returns the result.result field of a safe command, or an empty object. */
static QJsonObject runSafeCommand(const QString& name)
{
  const auto callId   = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto response = API::CommandRegistry::instance().execute(name, callId, {});
  if (!response.success)
    return {};

  return response.result;
}

/** @brief Returns project structure assembled from a curated set of safe list commands. */
QJsonObject AI::ToolDispatcher::getProjectState() const
{
  static const QStringList kSafeListCommands = {
    QStringLiteral("project.groups.list"),
    QStringLiteral("project.datasets.list"),
    QStringLiteral("project.actions.list"),
    QStringLiteral("project.source.list"),
    QStringLiteral("project.tables.list"),
    QStringLiteral("project.workspaces.list"),
    QStringLiteral("project.parser.getCode"),
    QStringLiteral("project.frameParser.getConfig"),
  };

  QJsonObject state;
  for (const auto& name : kSafeListCommands) {
    if (!API::CommandRegistry::instance().hasCommand(name))
      continue;

    state.insert(name, runSafeCommand(name));
  }

  return state;
}

/** @brief Returns the markdown reference body for the given scripting kind. */
QJsonObject AI::ToolDispatcher::getScriptingDocs(const QString& kind) const
{
  static const QStringList kAllowed = {
    QStringLiteral("frame_parser_js"),
    QStringLiteral("frame_parser_lua"),
    QStringLiteral("transform_js"),
    QStringLiteral("transform_lua"),
    QStringLiteral("output_widget_js"),
    QStringLiteral("painter_js"),
  };

  QJsonObject reply;
  if (!kAllowed.contains(kind)) {
    reply[QStringLiteral("ok")]    = false;
    reply[QStringLiteral("error")] = QStringLiteral("unknown_kind");
    reply[QStringLiteral("known")] = QJsonArray::fromStringList(kAllowed);
    return reply;
  }

  QFile file(QStringLiteral(":/ai/docs/%1.md").arg(kind));
  if (!file.open(QIODevice::ReadOnly)) {
    reply[QStringLiteral("ok")]    = false;
    reply[QStringLiteral("error")] = QStringLiteral("doc_not_found");
    return reply;
  }

  const auto body = QString::fromUtf8(file.readAll());
  file.close();

  reply[QStringLiteral("ok")]   = true;
  reply[QStringLiteral("kind")] = kind;
  reply[QStringLiteral("body")] = body;
  return reply;
}
