/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020–2025 Alex Spataru
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

#include "API/CommandRegistry.h"

#include <QJsonArray>

//--------------------------------------------------------------------------------------------------
// Closest-match suggestion helpers
//--------------------------------------------------------------------------------------------------

/**
 * @brief Levenshtein distance between two strings, capped for speed.
 */
static int editDistance(const QString& a, const QString& b)
{
  const int la = a.size();
  const int lb = b.size();
  if (la == 0)
    return lb;

  if (lb == 0)
    return la;

  QVector<int> prev(lb + 1);
  QVector<int> curr(lb + 1);
  for (int j = 0; j <= lb; ++j)
    prev[j] = j;

  for (int i = 1; i <= la; ++i) {
    curr[0] = i;
    for (int j = 1; j <= lb; ++j) {
      const int cost = (a.at(i - 1).toLower() == b.at(j - 1).toLower()) ? 0 : 1;
      curr[j]        = std::min({curr[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost});
    }
    std::swap(prev, curr);
  }
  return prev[lb];
}

/**
 * @brief Lower-is-better score: prefers names sharing the dotted prefix.
 */
static int similarityScore(const QString& want, const QString& have)
{
  const auto wantParts = want.split(QLatin1Char('.'));
  const auto haveParts = have.split(QLatin1Char('.'));

  int sharedSegments = 0;
  const int minSeg   = std::min(wantParts.size(), haveParts.size());
  for (int i = 0; i < minSeg; ++i)
    if (wantParts[i].compare(haveParts[i], Qt::CaseInsensitive) == 0)
      sharedSegments += 1;
    else
      break;

  const int dist = editDistance(want, have);

  // Sharing a dotted prefix is worth a lot: each shared segment cancels 6 edits
  return dist - (sharedSegments * 6);
}

//--------------------------------------------------------------------------------------------------
// Singleton access
//--------------------------------------------------------------------------------------------------

/**
 * @brief Gets the singleton instance of the CommandRegistry
 */
API::CommandRegistry& API::CommandRegistry::instance()
{
  static CommandRegistry singleton;
  return singleton;
}

//--------------------------------------------------------------------------------------------------
// Command registration
//--------------------------------------------------------------------------------------------------

/**
 * @brief Register a new command handler
 */
void API::CommandRegistry::registerCommand(const QString& name,
                                           const QString& description,
                                           CommandFunction handler)
{
  CommandDefinition def;
  def.name        = name;
  def.description = description;
  def.handler     = std::move(handler);
  m_commands.insert(name, def);
}

/**
 * @brief Register a new command handler with a JSON Schema for MCP tool metadata
 */
void API::CommandRegistry::registerCommand(const QString& name,
                                           const QString& description,
                                           const QJsonObject& inputSchema,
                                           CommandFunction handler)
{
  CommandDefinition def;
  def.name        = name;
  def.description = description;
  def.inputSchema = inputSchema;
  def.handler     = std::move(handler);
  m_commands.insert(name, def);
}

//--------------------------------------------------------------------------------------------------
// Command lookup & execution
//--------------------------------------------------------------------------------------------------

/**
 * @brief Check if a command is registered
 */
bool API::CommandRegistry::hasCommand(const QString& name) const
{
  return m_commands.contains(name);
}

/**
 * @brief Execute a registered command
 */
API::CommandResponse API::CommandRegistry::execute(const QString& name,
                                                   const QString& id,
                                                   const QJsonObject& params)
{
  if (!hasCommand(name))
    return buildUnknownCommandResponse(name, id);

  try {
    auto response = m_commands[name].handler(id, params);
    attachErrorMetadata(name, response);
    return response;
  } catch (const std::exception& e) {
    return CommandResponse::makeError(
      id, ErrorCode::ExecutionError, QStringLiteral("Command execution failed: %1").arg(e.what()));
  } catch (...) {
    return CommandResponse::makeError(
      id, ErrorCode::ExecutionError, QStringLiteral("Command execution failed: unknown exception"));
  }
}

/**
 * @brief Builds a "Unknown command" error response with did_you_mean suggestions.
 */
API::CommandResponse API::CommandRegistry::buildUnknownCommandResponse(const QString& name,
                                                                       const QString& id) const
{
  QVector<QPair<int, QString>> ranked;
  ranked.reserve(m_commands.size());
  for (auto it = m_commands.cbegin(); it != m_commands.cend(); ++it)
    ranked.append({similarityScore(name, it.key()), it.key()});

  std::sort(
    ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

  QJsonArray suggestions;
  const int kMax = std::min<int>(5, ranked.size());
  for (int i = 0; i < kMax; ++i) {
    QJsonObject hint;
    hint[QStringLiteral("name")]        = ranked[i].second;
    hint[QStringLiteral("description")] = m_commands[ranked[i].second].description;
    suggestions.append(hint);
  }

  QJsonObject data;
  data[QStringLiteral("did_you_mean")] = suggestions;
  data[QStringLiteral("hint")] =
    QStringLiteral("Use meta.listCommands to browse the full surface, then meta.describeCommand "
                   "for the exact schema before invoking.");

  return CommandResponse::makeError(
    id, ErrorCode::UnknownCommand, QStringLiteral("Unknown command: %1").arg(name), data);
}

/**
 * @brief Attaches inputSchema + structured category to a failed response.
 */
void API::CommandRegistry::attachErrorMetadata(const QString& name, CommandResponse& response) const
{
  if (response.success)
    return;

  // Attach inputSchema on validation errors for self-correction
  const bool isValidation =
    response.errorCode == ErrorCode::MissingParam || response.errorCode == ErrorCode::InvalidParam;
  if (isValidation && !m_commands[name].inputSchema.isEmpty()) {
    auto data = response.errorData;
    if (!data.contains(QStringLiteral("inputSchema")))
      data[QStringLiteral("inputSchema")] = m_commands[name].inputSchema;

    if (!data.contains(QStringLiteral("commandName")))
      data[QStringLiteral("commandName")] = name;

    response.errorData = data;
  }

  // Attach structured error category (handler-set values win)
  if (response.errorData.contains(QStringLiteral("category")))
    return;

  auto data                        = response.errorData;
  data[QStringLiteral("category")] = classifyErrorCategory(response);
  response.errorData               = data;
}

/**
 * @brief Returns a structured error category for a failed CommandResponse.
 */
QString API::CommandRegistry::classifyErrorCategory(const CommandResponse& response)
{
  if (response.errorCode == ErrorCode::MissingParam || response.errorCode == ErrorCode::InvalidParam
      || response.errorCode == ErrorCode::InvalidJson)
    return QStringLiteral("validation_failed");

  if (response.errorCode == ErrorCode::UnknownCommand)
    return QStringLiteral("unknown_command");

  // Heuristic message scan (unhappy path only)
  const auto msg = response.errorMessage.toLower();
  if (msg.contains(QStringLiteral("commercial")) || msg.contains(QStringLiteral("pro license"))
      || msg.contains(QStringLiteral("requires a pro")))
    return QStringLiteral("license_required");

  if (msg.contains(QStringLiteral("not connected"))
      || msg.contains(QStringLiteral("connection lost"))
      || msg.contains(QStringLiteral("device not")))
    return QStringLiteral("connection_lost");

  if (msg.contains(QStringLiteral("compile")) || msg.contains(QStringLiteral("syntax"))
      || msg.contains(QStringLiteral("script error"))
      || msg.contains(QStringLiteral("define parse"))
      || msg.contains(QStringLiteral("define paint"))
      || msg.contains(QStringLiteral("define transform")))
    return QStringLiteral("script_compile_failed");

  if (msg.contains(QStringLiteral("busy")) || msg.contains(QStringLiteral("in progress"))
      || msg.contains(QStringLiteral("already running")))
    return QStringLiteral("bus_busy");

  if (msg.contains(QStringLiteral("no such file")) || msg.contains(QStringLiteral("not found"))
      || msg.contains(QStringLiteral("does not exist")))
    return QStringLiteral("file_not_found");

  if (msg.contains(QStringLiteral("permission")) || msg.contains(QStringLiteral("denied"))
      || msg.contains(QStringLiteral("not allowed")) || msg.contains(QStringLiteral("blocked")))
    return QStringLiteral("permission_denied");

  return QStringLiteral("execution_error");
}

//--------------------------------------------------------------------------------------------------
// Command metadata
//--------------------------------------------------------------------------------------------------

/**
 * @brief Get a sorted list of all available command names
 */
QStringList API::CommandRegistry::availableCommands() const
{
  QStringList names = m_commands.keys();
  names.sort();
  return names;
}

/**
 * @brief Get direct access to all command definitions
 */
const QMap<QString, API::CommandDefinition>& API::CommandRegistry::commands() const
{
  return m_commands;
}
