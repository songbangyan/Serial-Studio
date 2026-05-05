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

/** @brief Functor that returns the current API key (or empty when unset). */
using KeyGetter = std::function<QString()>;

/**
 * @brief Anthropic Messages API adapter (first-slice stub).
 *
 * Holds a non-owning QNetworkAccessManager reference and a key-getter
 * functor. sendMessage returns a Reply that errors on the next event
 * loop tick with "Provider backend not yet implemented".
 */
class AnthropicProvider : public Provider {
public:
  AnthropicProvider(QNetworkAccessManager& nam, KeyGetter keyGetter);

  [[nodiscard]] QString displayName() const override;
  [[nodiscard]] QString keyVendorUrl() const override;
  [[nodiscard]] QStringList availableModels() const override;
  [[nodiscard]] QString defaultModel() const override;
  [[nodiscard]] QString modelDisplayName(const QString& modelId) const override;

  [[nodiscard]] Reply* sendMessage(const QJsonArray& history, const QJsonArray& tools) override;

private:
  QNetworkAccessManager& m_nam;
  KeyGetter m_keyGetter;
};

}  // namespace AI
