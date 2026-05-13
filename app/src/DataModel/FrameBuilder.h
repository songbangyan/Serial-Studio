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

#include <lua.h>

#include <chrono>
#include <map>
#include <QDeadlineTimer>
#include <QJSEngine>
#include <QJSValue>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QVariant>

#include "DataModel/DataTable.h"
#include "DataModel/Frame.h"
#include "IO/HAL_Driver.h"
#include "SerialStudio.h"

namespace DataModel {

/**
 * @brief Assembles a DataModel::Frame from raw I/O bytes and distributes it to the dashboard and
 * export workers.
 */
class FrameBuilder : public QObject {
  // clang-format off
  Q_OBJECT
  // clang-format on

signals:
  void jsonFileMapChanged();
  void frameChanged(const DataModel::Frame& frame);

private:
  explicit FrameBuilder();
  FrameBuilder(FrameBuilder&&)                 = delete;
  FrameBuilder(const FrameBuilder&)            = delete;
  FrameBuilder& operator=(FrameBuilder&&)      = delete;
  FrameBuilder& operator=(const FrameBuilder&) = delete;

public:
  [[nodiscard]] static FrameBuilder& instance();

  [[nodiscard]] const DataModel::Frame& frame() const noexcept;
  [[nodiscard]] const DataModel::Frame& quickPlotFrame() const noexcept;

  void injectTableApiLua(lua_State* L);
  void injectTableApiJS(QJSEngine* js);
  void refreshTableStoreFromProjectModel();

public slots:
  void setupExternalConnections();
  void syncFromProjectModel();
  void registerQuickPlotHeaders(const QStringList& headers);

  void hotpathRxFrame(const IO::CapturedDataPtr& data);
  void hotpathRxSourceFrame(int sourceId, const IO::CapturedDataPtr& data);

private slots:
  void onSourceRemoved();
  void onConnectedChanged();

private:
  void parseProjectFrame(const IO::CapturedDataPtr& data);
  void parseProjectFrame(int sourceId, const IO::CapturedDataPtr& data);
  void parseQuickPlotFrame(const IO::CapturedDataPtr& data);
  void buildQuickPlotFrame(const QStringList& channels);
  void buildQuickPlotAudioFrame(const QStringList& channels);

  void decodeProjectChannels(int sourceId,
                             bool applyPerSourceOverride,
                             const IO::CapturedDataPtr& data,
                             QList<QStringList>& outChannels);
  [[nodiscard]] SerialStudio::DecoderMethod resolveDecoderMethod(int sourceId,
                                                                 bool applyPerSourceOverride) const;
  [[nodiscard]] DataModel::Frame& ensureSourceFrame(int sourceId);
  void applyDatasetValues(DataModel::Frame& frame, const QStringList& channels, int sourceId);
  void applyDatasetValue(Dataset& dataset,
                         const QString* channelData,
                         int channelCount,
                         int sourceId);

  void hotpathTxFrame(const DataModel::TimestampedFramePtr& frame);
  void publishSourceTemplateFrame(const DataModel::Source& src);

  struct TransformEngine {
    lua_State* luaState = nullptr;
    QJSEngine* jsEngine = nullptr;
    std::map<int, int> luaRefs;
    std::map<int, QJSValue> jsRefs;
    QDeadlineTimer luaDeadline{QDeadlineTimer::Forever};
  };

  static constexpr int kTransformWatchdogMs     = 100;
  static constexpr int kTransformHookInstrCount = 10000;

  // Parser-load circuit breaker. Tracks wall-clock time spent in the user
  // script across a rolling 1-second window; if the parser consumes more
  // than kParseBudgetWarnLimitMs (80% of the window), incoming frames are
  // dropped until the window resets. Protects the GUI thread from being
  // starved by very high-rate sources (e.g. audio at 48 kHz fanning out
  // one parse() call per sample).
  static constexpr int kParseBudgetWindowMs    = 1000;
  static constexpr int kParseBudgetWarnLimitMs = 800;

  using BudgetClock = std::chrono::steady_clock;
  [[nodiscard]] bool parseBudgetSkipFrame();
  void parseBudgetAccount(BudgetClock::time_point startedAt);
  void parseBudgetReset() noexcept;

  static void transformLuaWatchdogHook(lua_State* L, lua_Debug* ar);

  struct TransformEntry {
    int uniqueId;
    QString code;
  };

  void compileTransforms();
  void compileTransformsLua(TransformEngine& engine, const std::vector<TransformEntry>& entries);
  void compileTransformsJS(TransformEngine& engine, const std::vector<TransformEntry>& entries);
  void destroyTransformEngines();
  [[nodiscard]] QVariant applyTransform(int sourceId,
                                        int language,
                                        int uniqueId,
                                        const QVariant& rawValue);
  [[nodiscard]] QVariant applyTransformLua(TransformEngine& engine,
                                           int uniqueId,
                                           const QVariant& rawValue);
  [[nodiscard]] QVariant applyTransformJs(TransformEngine& engine,
                                          int uniqueId,
                                          const QVariant& rawValue);
  void initializeTableStore();

  struct EngineKey {
    int sourceId;
    int language;

    bool operator<(const EngineKey& other) const noexcept
    {
      return sourceId < other.sourceId || (sourceId == other.sourceId && language < other.language);
    }
  };

private:
  std::map<EngineKey, TransformEngine> m_transformEngines;
  QTimer m_jsTransformWatchdog;
  DataModel::DataTableStore m_tableStore;

  DataModel::Frame m_frame;
  DataModel::Frame m_quickPlotFrame;

  QMap<int, DataModel::Frame> m_sourceFrames;

  int m_quickPlotChannels;
  bool m_quickPlotHasHeader;
  QStringList m_quickPlotChannelNames;
  QStringList m_channelScratch;

  BudgetClock::time_point m_parseBudgetWindowStart;
  qint64 m_parseBudgetUsedNs;
  bool m_parseBudgetSkipping;
  bool m_parseBudgetWarned;
};

}  // namespace DataModel
