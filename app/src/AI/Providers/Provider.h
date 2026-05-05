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

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

namespace AI {

/**
 * @brief Streamed reply handle returned by Provider::sendMessage.
 *
 * Heap-allocated, owned by the caller. Concrete providers inherit and
 * emit signals from their network slots; the caller deletes via
 * deleteLater() once finished/errorOccurred fires.
 */
class Reply : public QObject {
  Q_OBJECT

public:
  explicit Reply(QObject* parent = nullptr) : QObject(parent) {}

  ~Reply() override = default;

  virtual void abort() = 0;

signals:
  void partialText(const QString& chunk);
  void partialThinking(const QString& chunk);
  void toolCallRequested(const QString& callId,
                         const QString& toolName,
                         const QJsonObject& arguments);
  void cacheStatsAvailable(int readTokens, int createdTokens);
  void finished();
  void errorOccurred(const QString& message);
};

/**
 * @brief Abstract chat-completion backend (Anthropic, OpenAI, Gemini).
 *
 * Implementations own no state beyond a non-owning QNetworkAccessManager
 * reference and a key-getter functor. They produce Reply* instances per
 * sendMessage call; ownership transfers to the caller.
 */
class Provider {
public:
  Provider()                           = default;
  virtual ~Provider()                  = default;
  Provider(Provider&&)                 = delete;
  Provider(const Provider&)            = delete;
  Provider& operator=(Provider&&)      = delete;
  Provider& operator=(const Provider&) = delete;

  [[nodiscard]] virtual QString displayName() const  = 0;
  [[nodiscard]] virtual QString keyVendorUrl() const = 0;

  [[nodiscard]] virtual QStringList availableModels() const = 0;
  [[nodiscard]] virtual QString defaultModel() const        = 0;

  /**
   * @brief Returns a user-friendly label for a model id (e.g. "Claude Haiku 4.5").
   *
   * Default implementation returns the id verbatim; providers override
   * to give human-friendly names while the canonical id continues to be
   * what gets sent to the API.
   */
  [[nodiscard]] virtual QString modelDisplayName(const QString& modelId) const { return modelId; }

  [[nodiscard]] QString currentModel() const
  {
    return m_currentModel.isEmpty() ? defaultModel() : m_currentModel;
  }

  void setCurrentModel(const QString& model)
  {
    if (model.isEmpty() || !availableModels().contains(model))
      return;

    m_currentModel = model;
  }

  [[nodiscard]] virtual Reply* sendMessage(const QJsonArray& history, const QJsonArray& tools) = 0;

protected:
  QString m_currentModel;
};

}  // namespace AI
