/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * This file is part of the proprietary feature set of Serial Studio
 * and is licensed under the Serial Studio Commercial License.
 *
 * Redistribution, modification, or use of this file in any form
 * is permitted only under the terms of a valid commercial license
 * obtained from the author.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "AI/Providers/Provider.h"

class QNetworkAccessManager;

namespace AI {

/**
 * @brief Local OpenAI-compatible provider (Ollama, llama.cpp, LM Studio, vLLM).
 *
 * Most local model runners expose an OpenAI-compatible Chat Completions
 * endpoint at /v1/chat/completions and a model list at /v1/models. This
 * provider reuses the OpenAI translation helpers and the OpenAIReply
 * streaming pipe, with a user-configurable base URL persisted in
 * QSettings under "ai/local/baseUrl".
 *
 * Default base URL is http://localhost:11434/v1 (Ollama). Users can
 * point at other compatible servers (e.g. http://localhost:1234/v1 for
 * LM Studio, http://localhost:8080/v1 for llama.cpp's llama-server).
 *
 * The model list is queried live from <baseUrl>/models on construction
 * and on baseUrl changes, with a static fallback so the UI is never
 * empty when the local server is offline.
 */
class LocalProvider : public QObject, public Provider {
  Q_OBJECT

public:
  explicit LocalProvider(QNetworkAccessManager& nam);

  [[nodiscard]] QString displayName() const override;
  [[nodiscard]] QString keyVendorUrl() const override;
  [[nodiscard]] QStringList availableModels() const override;
  [[nodiscard]] QString defaultModel() const override;
  [[nodiscard]] QString modelDisplayName(const QString& modelId) const override;

  void setCurrentModel(const QString& model) override;

  [[nodiscard]] Reply* sendMessage(const QJsonArray& history, const QJsonArray& tools) override;

  [[nodiscard]] QString baseUrl() const;
  void setBaseUrl(const QString& url);
  void refreshModels();

  static QString defaultBaseUrl();

signals:
  void modelsChanged();

private:
  [[nodiscard]] QString chatEndpoint() const;
  [[nodiscard]] QString modelsEndpoint() const;
  void loadCachedModels();
  void persistCachedModels() const;

private:
  QNetworkAccessManager& m_nam;
  mutable QSettings m_settings;
  QString m_baseUrl;
  QStringList m_models;
};

}  // namespace AI
