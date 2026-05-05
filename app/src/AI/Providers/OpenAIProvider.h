/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <functional>

#include "AI/Providers/Provider.h"

class QNetworkAccessManager;

namespace AI {

/**
 * @brief Callable returning the API key for the active provider.
 */
using KeyGetter = std::function<QString()>;

/**
 * @brief OpenAI Chat Completions adapter (first-slice stub).
 */
class OpenAIProvider : public Provider {
public:
  OpenAIProvider(QNetworkAccessManager& nam, KeyGetter keyGetter);

  [[nodiscard]] QString displayName() const override;
  [[nodiscard]] QString keyVendorUrl() const override;
  [[nodiscard]] QStringList availableModels() const override;
  [[nodiscard]] QString defaultModel() const override;
  [[nodiscard]] QString modelDisplayName(const QString& modelId) const override;

  [[nodiscard]] Reply* sendMessage(const QJsonArray& history, const QJsonArray& tools) override;

private:
  [[nodiscard]] static QJsonArray translateHistory(const QJsonArray& history,
                                                   const QString& systemText);
  [[nodiscard]] static QJsonArray translateTools(const QJsonArray& tools);
  static void translateBlocks(const QJsonArray& blocks,
                              QString& textAccumulator,
                              QJsonArray& toolCalls,
                              QJsonArray& toolResultMessages);

  QNetworkAccessManager& m_nam;
  KeyGetter m_keyGetter;
};

}  // namespace AI
