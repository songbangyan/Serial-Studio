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

#pragma once

#include <chrono>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QKeyEvent>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace CSV {
/**
 * @brief CSV player that re-plays a CSV file as if it were live data.
 */
class Player : public QObject {
  // clang-format off
  Q_OBJECT
  Q_PROPERTY(bool isOpen
             READ isOpen
             NOTIFY openChanged)
  Q_PROPERTY(double progress
             READ progress
             NOTIFY timestampChanged)
  Q_PROPERTY(double frameCount
             READ frameCount
             NOTIFY playerStateChanged)
  Q_PROPERTY(double framePosition
             READ framePosition
             NOTIFY timestampChanged)
  Q_PROPERTY(bool isPlaying
             READ isPlaying
             NOTIFY playerStateChanged)
  Q_PROPERTY(const QString& timestamp
             READ timestamp
             NOTIFY timestampChanged)
  // clang-format on

signals:
  void openChanged();
  void timestampChanged();
  void playerStateChanged();

private:
  explicit Player();
  Player(Player&&)                 = delete;
  Player(const Player&)            = delete;
  Player& operator=(Player&&)      = delete;
  Player& operator=(const Player&) = delete;

public:
  [[nodiscard]] static Player& instance();

  [[nodiscard]] bool isOpen() const;
  [[nodiscard]] double progress() const;
  [[nodiscard]] bool isPlaying() const;
  [[nodiscard]] int frameCount() const;
  [[nodiscard]] int framePosition() const;

  [[nodiscard]] QString filename() const;
  [[nodiscard]] const QString& timestamp() const;

public slots:
  void play();
  void pause();
  void toggle();
  void openFile();
  void closeFile();
  void nextFrame();
  void previousFrame();
  void openFile(const QString& filePath);
  void setProgress(const double progress);

private slots:
  void updateData();
  void performSeekTick();
  void performSeekSettle();

private:
  void sendHeaderFrame();
  void updateTimestampDisplay();
  void processFrameBatch(int startFrame, int endFrame);
  void parseCsvRows(QTextStream& stream);
  void initializeTimestamps();
  bool recomputeMsUntilNext(qint64& msUntilNext);

private:
  bool promptUserForDateTimeOrInterval();
  void generateDateTimeForRows(int interval);
  void convertColumnToDateTime(int columnIndex);

  QDateTime getDateTime(int row);
  QDateTime getDateTime(const QString& cell);
  double getTimestampSeconds(int row);
  double getTimestampSeconds(const QString& cell);
  [[nodiscard]] QString formatTimestamp(double seconds) const;

  QByteArray getFrame(const int row);

  const QString getCellValue(const int row, const int column, bool& error);

protected:
  bool eventFilter(QObject* obj, QEvent* event) override;
  bool handleKeyPress(QKeyEvent* keyEvent);

private:
  void buildReplayLayout();
  void injectFrame(const QByteArray& frame);
  void injectRow(int row);
  void anchorSteadyBase(int row);
  void buildSeekWindow(int startRow,
                       int endRow,
                       QVector<double>& times,
                       QHash<qint64, QVector<double>>& series);
  void buildDateTimeSecondsCache();
  [[nodiscard]] int seekWindowStartRow(int target);
  [[nodiscard]] double rowSecondsSinceStart(int row);
  [[nodiscard]] std::chrono::steady_clock::time_point rowSteadyTimestamp(int row);

private:
  int m_framePos;
  bool m_playing;
  bool m_multiSource;
  QFile m_csvFile;
  QString m_timestamp;
  QList<QStringList> m_csvData;

  QElapsedTimer m_elapsedTimer;
  QDateTime m_startTimestamp;
  double m_startTimestampSeconds;
  bool m_useHighPrecisionTimestamps;
  QVector<double> m_timestampCache;
  QVector<double> m_dateTimeSecondsCache;

  double m_steadyBaseRowSeconds;
  std::chrono::steady_clock::time_point m_steadyBase;

  QTimer m_seekTimer;
  QTimer m_settleTimer;
  QHash<qint64, int> m_seekColumnByKey;

  QMap<int, QVector<int>> m_sourceColumnsByIndex;
};
}  // namespace CSV
