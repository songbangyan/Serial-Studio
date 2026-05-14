/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020–2025 Alex Spataru <https://aspatru.com>
 *
 * This file is part of the proprietary feature set of Serial Studio
 * and is licensed under the Serial Studio Commercial License.
 *
 * Redistribution, modification, or use of this file in any form
 * is permitted only under the terms of a valid commercial license
 * obtained from the author.
 *
 * This file may NOT be used in any build distributed under the
 * GNU General Public License (GPL) unless explicitly authorized
 * by a separate commercial agreement.
 *
 * For license terms, see:
 * https://github.com/Serial-Studio/Serial-Studio/blob/master/LICENSE.md
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

// clang-format off
#include <QtMqtt>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTimer>
#include <QVariantMap>
// clang-format on

#include "DataModel/Frame.h"
#include "DataModel/FrameConsumer.h"
#include "IO/HAL_Driver.h"

namespace MQTT {

/**
 * @brief Snapshot of every field needed by the worker thread to (re)configure the broker.
 */
struct BrokerConfig {
  bool enabled                              = false;
  bool sslEnabled                           = false;
  bool cleanSession                         = true;
  bool publishNotifications                 = false;
  int mode                                  = 0;
  int peerVerifyDepth                       = 10;
  quint16 port                              = 1883;
  quint16 keepAlive                         = 60;
  QMqttClient::ProtocolVersion mqttVersion  = QMqttClient::MQTT_5_0;
  QSsl::SslProtocol sslProtocol             = QSsl::SecureProtocols;
  QSslSocket::PeerVerifyMode peerVerifyMode = QSslSocket::AutoVerifyPeer;
  QString clientId;
  QString hostname;
  QString username;
  QString password;
  QString topicBase;
  QString notificationTopic;
  QList<QSslCertificate> caCertificates;
};

/**
 * @brief Raw RX-bytes payload paired with the device id and capture timestamp.
 */
struct TimestampedRawBytes {
  int deviceId;
  IO::CapturedDataPtr data;
};

class Publisher;

/**
 * @brief Background worker that owns the QMqttClient and performs broker I/O off-main.
 */
class PublisherWorker : public DataModel::FrameConsumerWorker<DataModel::TimestampedFramePtr> {
  Q_OBJECT

signals:
  void brokerStateChanged(int state);
  void brokerErrorOccurred(const QString& message);
  void testConnectionFinished(bool ok, const QString& detail);

public:
  PublisherWorker(moodycamel::ReaderWriterQueue<DataModel::TimestampedFramePtr>* frameQueue,
                  std::atomic<bool>* enabled,
                  std::atomic<size_t>* queueSize,
                  moodycamel::ReaderWriterQueue<TimestampedRawBytes>* rawQueue,
                  std::atomic<int>* mode);
  ~PublisherWorker() override;

  void processData() override;
  void closeResources() override;
  [[nodiscard]] bool isResourceOpen() const override;

  [[nodiscard]] QString errorString(QMqttClient::ClientError error) const;

public slots:
  void bootstrap();
  void applyBrokerConfig(const MQTT::BrokerConfig& cfg);
  void openBroker();
  void closeBroker();
  void publishNotificationOnWorker(const QString& topic, const QByteArray& payload);
  void publishCustomOnWorker(const QString& topic, const QByteArray& payload, int qos, bool retain);
  void runTestConnection();

protected:
  void processItems(const std::vector<DataModel::TimestampedFramePtr>& items) override;

private slots:
  void onClientStateChanged(QMqttClient::ClientState state);
  void onClientErrorChanged(QMqttClient::ClientError error);

private:
  void publishFrameAsJson(const DataModel::Frame& frame);
  static QString describeMqttError(QMqttClient::ClientError error);

private:
  BrokerConfig m_cfg;
  QMqttClient* m_client;
  QSslConfiguration m_sslConfiguration;
  QByteArray m_rawBatchBuffer;
  moodycamel::ReaderWriterQueue<TimestampedRawBytes>* m_rawQueue;
  std::atomic<int>* m_mode;
};

/**
 * @brief Per-project MQTT publisher; broadcasts frames, raw bytes and notifications.
 */
class Publisher : public DataModel::FrameConsumer<DataModel::TimestampedFramePtr> {
  // clang-format off
  Q_OBJECT
  Q_PROPERTY(bool enabled
             READ enabled
             WRITE setEnabled
             NOTIFY configurationChanged)
  Q_PROPERTY(bool sslEnabled
             READ sslEnabled
             WRITE setSslEnabled
             NOTIFY configurationChanged)
  Q_PROPERTY(bool isConnected
             READ isConnected
             NOTIFY connectedChanged)
  Q_PROPERTY(bool cleanSession
             READ cleanSession
             WRITE setCleanSession
             NOTIFY configurationChanged)
  Q_PROPERTY(bool publishNotifications
             READ publishNotifications
             WRITE setPublishNotifications
             NOTIFY configurationChanged)
  Q_PROPERTY(int mode
             READ mode
             WRITE setMode
             NOTIFY configurationChanged)
  Q_PROPERTY(int peerVerifyDepth
             READ peerVerifyDepth
             WRITE setPeerVerifyDepth
             NOTIFY configurationChanged)
  Q_PROPERTY(int publishFrequency
             READ publishFrequency
             WRITE setPublishFrequency
             NOTIFY configurationChanged)
  Q_PROPERTY(quint8 mqttVersion
             READ mqttVersion
             WRITE setMqttVersion
             NOTIFY configurationChanged)
  Q_PROPERTY(quint8 sslProtocol
             READ sslProtocol
             WRITE setSslProtocol
             NOTIFY configurationChanged)
  Q_PROPERTY(quint8 peerVerifyMode
             READ peerVerifyMode
             WRITE setPeerVerifyMode
             NOTIFY configurationChanged)
  Q_PROPERTY(quint16 port
             READ port
             WRITE setPort
             NOTIFY configurationChanged)
  Q_PROPERTY(quint16 keepAlive
             READ keepAlive
             WRITE setKeepAlive
             NOTIFY configurationChanged)
  Q_PROPERTY(QString clientId
             READ clientId
             WRITE setClientId
             NOTIFY configurationChanged)
  Q_PROPERTY(QString hostname
             READ hostname
             WRITE setHostname
             NOTIFY configurationChanged)
  Q_PROPERTY(QString username
             READ username
             WRITE setUsername
             NOTIFY configurationChanged)
  Q_PROPERTY(QString password
             READ password
             WRITE setPassword
             NOTIFY configurationChanged)
  Q_PROPERTY(QString topicBase
             READ topicBase
             WRITE setTopicBase
             NOTIFY configurationChanged)
  Q_PROPERTY(QString notificationTopic
             READ notificationTopic
             WRITE setNotificationTopic
             NOTIFY configurationChanged)
  Q_PROPERTY(QStringList modes
             READ modes
             CONSTANT)
  Q_PROPERTY(QStringList mqttVersions
             READ mqttVersions
             CONSTANT)
  Q_PROPERTY(QStringList sslProtocols
             READ sslProtocols
             CONSTANT)
  Q_PROPERTY(QStringList peerVerifyModes
             READ peerVerifyModes
             CONSTANT)
  // clang-format on

public:
  /**
   * @brief Publisher output modes.
   */
  enum Mode {
    DashboardData = 0,
    RawRxData     = 1,
  };
  Q_ENUM(Mode)

  static constexpr int kMinPublishHz     = 1;
  static constexpr int kMaxPublishHz     = 30;
  static constexpr int kDefaultPublishHz = 10;

signals:
  void connectedChanged();
  void configurationChanged();

private:
  explicit Publisher();
  ~Publisher() override;
  Publisher(Publisher&&)                 = delete;
  Publisher(const Publisher&)            = delete;
  Publisher& operator=(Publisher&&)      = delete;
  Publisher& operator=(const Publisher&) = delete;

public:
  [[nodiscard]] static Publisher& instance();

  [[nodiscard]] bool enabled() const noexcept;
  [[nodiscard]] bool sslEnabled() const noexcept;
  [[nodiscard]] bool isConnected() const;
  [[nodiscard]] bool cleanSession() const noexcept;
  [[nodiscard]] bool publishNotifications() const noexcept;

  [[nodiscard]] int mode() const noexcept;
  [[nodiscard]] int peerVerifyDepth() const noexcept;
  [[nodiscard]] int publishFrequency() const noexcept;

  [[nodiscard]] quint8 mqttVersion() const noexcept;
  [[nodiscard]] quint8 sslProtocol() const noexcept;
  [[nodiscard]] quint8 peerVerifyMode() const noexcept;
  [[nodiscard]] quint16 port() const noexcept;
  [[nodiscard]] quint16 keepAlive() const noexcept;

  [[nodiscard]] QString clientId() const;
  [[nodiscard]] QString hostname() const;
  [[nodiscard]] QString username() const;
  [[nodiscard]] QString password() const;
  [[nodiscard]] QString topicBase() const;
  [[nodiscard]] QString notificationTopic() const;

  [[nodiscard]] const QStringList& modes() const;
  [[nodiscard]] const QStringList& mqttVersions() const;
  [[nodiscard]] const QStringList& sslProtocols() const;
  [[nodiscard]] const QStringList& peerVerifyModes() const;

  [[nodiscard]] QJsonObject toJson() const;

public slots:
  void setupExternalConnections();
  void applyProjectConfig(const QJsonObject& cfg);
  void resetProjectConfig();
  void testConnection();
  void addCaCertificates();
  void regenerateClientId();

  void setEnabled(const bool enabled);
  void setMode(const int mode);
  void setSslEnabled(const bool enabled);
  void setCleanSession(const bool cleanSession);
  void setPublishNotifications(const bool publish);
  void setPeerVerifyDepth(const int depth);
  void setPublishFrequency(const int hz);
  void setMqttVersion(const quint8 version);
  void setSslProtocol(const quint8 protocol);
  void setPeerVerifyMode(const quint8 verifyMode);
  void setPort(const quint16 port);
  void setKeepAlive(const quint16 keepAlive);
  void setClientId(const QString& id);
  void setHostname(const QString& hostname);
  void setUsername(const QString& username);
  void setPassword(const QString& password);
  void setTopicBase(const QString& topic);
  void setNotificationTopic(const QString& topic);

  void hotpathTxFrame(const DataModel::TimestampedFramePtr& frame);
  void hotpathTxRawBytes(int deviceId, const IO::CapturedDataPtr& data);
  void onNotificationPosted(const QVariantMap& event);

  qint64 mqttPublish(const QString& topic,
                     const QByteArray& payload,
                     int qos     = 0,
                     bool retain = false);

protected:
  DataModel::FrameConsumerWorkerBase* createWorker() override;

private slots:
  void onWorkerBrokerStateChanged(int state);
  void onWorkerBrokerError(const QString& message);
  void onWorkerTestConnectionFinished(bool ok, const QString& detail);

private:
  [[nodiscard]] bool licenseValid() const;
  void markConfigChanged();
  void scheduleSyncToWorker();
  void syncToWorker();
  void applyTimerInterval();
  [[nodiscard]] BrokerConfig snapshotConfig() const;

private:
  bool m_enabled;
  bool m_sslEnabled;
  bool m_publishNotifications;
  bool m_cleanSession;
  bool m_inApply;
  bool m_skipNextSync;
  bool m_savingToProjectModel;
  bool m_reportConnectionErrors;
  int m_mode;
  int m_peerVerifyDepth;
  int m_publishFrequencyHz;

  QMqttClient::ProtocolVersion m_protocolVersion;
  QSsl::SslProtocol m_sslProtocol;
  QSslSocket::PeerVerifyMode m_peerVerifyMode;
  quint16 m_port;
  quint16 m_keepAlive;

  QString m_clientId;
  QString m_hostname;
  QString m_username;
  QString m_password;
  QString m_topicBase;
  QString m_notificationTopic;

  QList<QSslCertificate> m_caCertificates;

  QMap<QString, QSsl::SslProtocol> m_sslProtocols;
  QMap<QString, QMqttClient::ProtocolVersion> m_mqttVersions;
  QMap<QString, QSslSocket::PeerVerifyMode> m_peerVerifyModes;

  QTimer m_syncTimer;
  moodycamel::ReaderWriterQueue<TimestampedRawBytes> m_rawBytesQueue;
  alignas(64) std::atomic<int> m_workerMode;
  alignas(64) std::atomic<bool> m_isConnected;

  static constexpr int kSyncDebounceMs = 200;

  static constexpr QLatin1StringView kKeyEnabled{"enabled"};
  static constexpr QLatin1StringView kKeyMode{"mode"};
  static constexpr QLatin1StringView kKeyPublishNotifications{"publishNotifications"};
  static constexpr QLatin1StringView kKeyPublishFrequency{"publishFrequency"};
  static constexpr QLatin1StringView kKeyTopicBase{"topicBase"};
  static constexpr QLatin1StringView kKeyNotificationTopic{"notificationTopic"};
  static constexpr QLatin1StringView kKeyHostname{"hostname"};
  static constexpr QLatin1StringView kKeyPort{"port"};
  static constexpr QLatin1StringView kKeyClientId{"clientId"};
  static constexpr QLatin1StringView kKeyUsername{"username"};
  static constexpr QLatin1StringView kKeyPassword{"password"};
  static constexpr QLatin1StringView kKeyCleanSession{"cleanSession"};
  static constexpr QLatin1StringView kKeyKeepAlive{"keepAlive"};
  static constexpr QLatin1StringView kKeyMqttVersion{"mqttVersion"};
  static constexpr QLatin1StringView kKeySslEnabled{"sslEnabled"};
  static constexpr QLatin1StringView kKeySslProtocol{"sslProtocol"};
  static constexpr QLatin1StringView kKeyPeerVerifyMode{"peerVerifyMode"};
  static constexpr QLatin1StringView kKeyPeerVerifyDepth{"peerVerifyDepth"};
};

}  // namespace MQTT

Q_DECLARE_METATYPE(MQTT::BrokerConfig)
