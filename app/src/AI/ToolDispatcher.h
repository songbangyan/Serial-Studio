/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * This file is part of the proprietary feature set of Serial Studio
 * and is licensed under the Serial Studio Commercial License.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace AI {

/**
 * @brief Bridges AI tool calls to the in-process API::CommandRegistry.
 *
 * Filters and gates the API surface using AI::CommandRegistry safety
 * tags: Blocked entries are denied, Confirm entries emit a signal and
 * return "awaiting_confirmation", Safe entries (or Confirm with
 * autoConfirmSafe=true) execute synchronously via
 * API::CommandRegistry::execute().
 */
class ToolDispatcher : public QObject {
  Q_OBJECT

public:
  explicit ToolDispatcher(QObject* parent = nullptr);

  [[nodiscard]] QJsonArray availableTools(const QString& category = {}) const;
  [[nodiscard]] QJsonObject listCommands(const QString& prefix = {}) const;
  [[nodiscard]] QJsonObject describeCommand(const QString& name) const;
  [[nodiscard]] QJsonObject executeCommand(const QString& name,
                                           const QJsonObject& args,
                                           bool autoConfirmSafe);
  [[nodiscard]] QJsonObject getProjectState() const;
  [[nodiscard]] QJsonObject getScriptingDocs(const QString& kind) const;

signals:
  void confirmationRequested(const QString& name, const QJsonObject& args);
};

}  // namespace AI
