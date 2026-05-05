/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>

#include "AI/Providers/Provider.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace AI {

class SseEventReader;

/**
 * @brief Real OpenAI Chat Completions streaming Reply.
 *
 * Posts a JSON body to https://api.openai.com/v1/chat/completions with
 * stream:true, parses Server-Sent-Events (each line a JSON delta with
 * a final `data: [DONE]` sentinel), and translates OpenAI's
 * choices[0].delta.{content, tool_calls} stream into the provider-neutral
 * Reply signals.
 */
class OpenAIReply : public Reply {
  Q_OBJECT

public:
  OpenAIReply(QNetworkAccessManager& nam,
              const QString& apiKey,
              const QByteArray& requestBody,
              QObject* parent = nullptr);

  OpenAIReply(QNetworkAccessManager& nam,
              const QString& endpointUrl,
              const QString& authHeader,
              const QString& apiKey,
              const QByteArray& requestBody,
              const QString& providerLabel,
              QObject* parent = nullptr);

  void abort() override;

private:
  /** @brief Per-tool-call accumulator keyed by the streamed `index`. */
  struct ToolCallState {
    QString id;
    QString name;
    QByteArray arguments;
    bool emitted;
  };

  void onSseEvent(const QString& name, const QJsonObject& data);
  void onSseError(const QString& reason);
  void onReplyReadyRead();
  void onReplyFinished();

  void processChoiceDelta(const QJsonObject& choice);
  void emitPendingToolCalls();
  void finishOk();
  void finishWithError(const QString& message);

private:
  void issueRequest();

private:
  QNetworkAccessManager& m_nam;
  QString m_endpointUrl;
  QString m_authHeader;
  QString m_apiKey;
  QString m_providerLabel;
  QByteArray m_requestBody;
  QNetworkReply* m_reply;
  SseEventReader* m_sse;
  QHash<int, ToolCallState> m_toolCalls;
  QString m_finishReason;
  bool m_finished;
};

}  // namespace AI
