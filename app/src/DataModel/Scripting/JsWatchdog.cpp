/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2026 Alex Spataru
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

#include "DataModel/Scripting/JsWatchdog.h"

#include <QDebug>

//--------------------------------------------------------------------------------------------------
// Construction
//--------------------------------------------------------------------------------------------------

/**
 * @brief Builds a watchdog that interrupts the given engine after budgetMs.
 */
DataModel::JsWatchdog::JsWatchdog(QJSEngine* engine, int budgetMs, QString tag)
  : m_engine(engine), m_tag(std::move(tag)), m_lastTimedOut(false)
{
  Q_ASSERT(engine != nullptr);
  Q_ASSERT(budgetMs > 0);

  m_timer.setSingleShot(true);
  m_timer.setInterval(budgetMs);
  QObject::connect(&m_timer, &QTimer::timeout, [this]() { m_engine->setInterrupted(true); });
}

//--------------------------------------------------------------------------------------------------
// Watchdog-protected calls
//--------------------------------------------------------------------------------------------------

/**
 * @brief Calls fn(args) under the watchdog with no explicit `this`.
 */
QJSValue DataModel::JsWatchdog::call(QJSValue& fn, const QJSValueList& args)
{
  Q_ASSERT(fn.isCallable());
  Q_ASSERT(m_engine != nullptr);

  m_lastTimedOut = false;
  m_engine->setInterrupted(false);
  m_timer.start();
  const auto result = fn.call(args);
  m_timer.stop();

  if (m_engine->isInterrupted()) [[unlikely]] {
    m_engine->setInterrupted(false);
    m_lastTimedOut = true;
    qWarning().noquote() << "[JsWatchdog]" << (m_tag.isEmpty() ? QStringLiteral("script") : m_tag)
                         << "timed out after" << m_timer.interval() << "ms -- interrupted";
  }

  return result;
}

/**
 * @brief Calls fn.callWithInstance(thisObj, args) under the watchdog.
 */
QJSValue DataModel::JsWatchdog::call(QJSValue& fn, QJSValue thisObj, const QJSValueList& args)
{
  Q_ASSERT(fn.isCallable());
  Q_ASSERT(m_engine != nullptr);

  m_lastTimedOut = false;
  m_engine->setInterrupted(false);
  m_timer.start();
  const auto result = fn.callWithInstance(thisObj, args);
  m_timer.stop();

  if (m_engine->isInterrupted()) [[unlikely]] {
    m_engine->setInterrupted(false);
    m_lastTimedOut = true;
    qWarning().noquote() << "[JsWatchdog]" << (m_tag.isEmpty() ? QStringLiteral("script") : m_tag)
                         << "timed out after" << m_timer.interval() << "ms -- interrupted";
  }

  return result;
}

//--------------------------------------------------------------------------------------------------
// Configuration
//--------------------------------------------------------------------------------------------------

/**
 * @brief Returns the current watchdog budget in milliseconds.
 */
int DataModel::JsWatchdog::budgetMs() const noexcept
{
  return m_timer.interval();
}

/**
 * @brief Updates the watchdog budget; takes effect on the next call().
 */
void DataModel::JsWatchdog::setBudgetMs(int ms) noexcept
{
  Q_ASSERT(ms > 0);
  m_timer.setInterval(ms);
}

/**
 * @brief Returns true when the most recent call() was interrupted by timeout.
 */
bool DataModel::JsWatchdog::lastCallTimedOut() const noexcept
{
  return m_lastTimedOut;
}
