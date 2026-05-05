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
#include <QString>
#include <QStringList>

namespace AI {

/**
 * @brief Builds the system-prompt blocks fed to the LLM each turn.
 *
 * The output is split into three text blocks. The first two never change
 * within a session and carry @c cache_control: ephemeral so Anthropic's
 * prompt cache can fingerprint them; the third (live project state) is
 * outside the cache so it can update between turns without invalidating
 * the cached prefix.
 */
class ContextBuilder {
public:
  [[nodiscard]] static QString roleBlock();
  [[nodiscard]] static QString scriptingDocsBlock();
  [[nodiscard]] static QString liveProjectStateBlock();
  [[nodiscard]] static QString scriptingDocFor(const QString& kind);

  [[nodiscard]] static QStringList howToTasks();
  [[nodiscard]] static QString howToRecipe(const QString& task);

  [[nodiscard]] static QJsonArray buildSystemArray(bool includeScriptingDocs = true);
};

}  // namespace AI
