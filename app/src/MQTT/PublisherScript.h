/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2026 Alex Spataru <https://aspatru.com>
 *
 * This file is part of the proprietary feature set of Serial Studio
 * and is licensed under the Serial Studio Commercial License.
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <memory>
#include <QByteArray>
#include <QJSEngine>
#include <QJSValue>
#include <QString>

#include "DataModel/Scripting/JsWatchdog.h"

struct lua_State;

namespace MQTT {

/**
 * @brief Worker-thread JS/Lua engine that runs the user-supplied mqtt(frame) callback.
 */
class PublisherScript {
public:
  enum Language {
    JavaScript = 0,
    Lua        = 1,
  };

  PublisherScript();
  ~PublisherScript();

  PublisherScript(const PublisherScript&)            = delete;
  PublisherScript& operator=(const PublisherScript&) = delete;

  [[nodiscard]] bool compile(const QString& source, int language, QString& errorOut);
  [[nodiscard]] bool isLoaded() const noexcept;
  [[nodiscard]] int currentLanguage() const noexcept;

  [[nodiscard]] bool run(const QByteArray& frame, QByteArray& payloadOut, QString& errorOut);

  void reset();

private:
  static constexpr int kRuntimeWatchdogMs = 500;

  void destroyLua();
  void resetJs();

  int m_language;
  bool m_loaded;

  // JS engine state (always allocated; lazy-initialized on first JS compile).
  std::unique_ptr<QJSEngine> m_jsEngine;
  std::unique_ptr<DataModel::JsWatchdog> m_jsWatchdog;
  QJSValue m_jsFunction;

  // Lua engine state.
  lua_State* m_luaState;
};

}  // namespace MQTT
