/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru
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

import QtQuick

import SerialStudio

QtObject {
  id: root

  //
  // Optional host refs; items whose refs are absent stay hidden via their visible: guards.
  //
  property var taskBar: null
  property bool allowFullScreen: true
  property bool allowExternalWindow: true

  //
  // Window-owner actions raised by tool items that need a host to fulfil them.
  //
  signal fullScreenRequested()
  signal externalWindowRequested()

  //
  // Returns visible actions matching `filter`; gating lives only in each item's visible field.
  //
  function items(filter) {
    var list = [
      {
        name: qsTr("Auto Layout"),
        icon: "qrc:/icons/start/auto-layout.svg",
        visible: root.taskBar !== null && !(app.runtimeMode && Cpp_UI_Dashboard.frozen),
        run: function() {
          root.taskBar.windowManager.autoLayoutEnabled = !root.taskBar.windowManager.autoLayoutEnabled
        }
      },
      {
        name: qsTr("Full Screen"),
        icon: "qrc:/icons/start/full-screen.svg",
        visible: root.allowFullScreen,
        run: function() { root.fullScreenRequested() }
      },
      {
        name: qsTr("Add External Window"),
        icon: "qrc:/icons/start/external-window.svg",
        visible: root.allowExternalWindow,
        run: function() { root.externalWindowRequested() }
      },
      {
        name: qsTr("Console"),
        icon: "qrc:/icons/start/console.svg",
        visible: !app.runtimeMode,
        run: function() { Cpp_UI_Dashboard.terminalEnabled = !Cpp_UI_Dashboard.terminalEnabled }
      },
      {
        name: qsTr("Notifications"),
        icon: "qrc:/icons/start/notifications.svg",
        visible: Cpp_CommercialBuild,
        run: function() {
          Cpp_UI_Dashboard.notificationLogEnabled = !Cpp_UI_Dashboard.notificationLogEnabled
        }
      },
      {
        name: qsTr("Clock"),
        icon: "qrc:/icons/start/clock.svg",
        visible: true,
        run: function() {
          Cpp_UI_Dashboard.clockEnabled = !Cpp_UI_Dashboard.clockEnabled
        }
      },
      {
        name: qsTr("Stopwatch"),
        icon: "qrc:/icons/start/stopwatch.svg",
        visible: true,
        run: function() {
          Cpp_UI_Dashboard.stopwatchEnabled = !Cpp_UI_Dashboard.stopwatchEnabled
        }
      },
      {
        name: qsTr("Preferences"),
        icon: "qrc:/icons/start/settings.svg",
        visible: !app.runtimeMode,
        run: function() { app.showSettingsDialog() }
      },
      {
        name: qsTr("Help Center"),
        icon: "qrc:/icons/start/help.svg",
        visible: true,
        run: function() { app.showHelpCenter() }
      },
      {
        name: qsTr("Sessions"),
        icon: "qrc:/icons/start/sessions.svg",
        visible: Cpp_CommercialBuild
                 && (!app.runtimeMode || Cpp_Sessions_Export.exportEnabled),
        run: function() { app.showDatabaseExplorer() }
      },
      {
        name: qsTr("File Transmission"),
        icon: "qrc:/icons/taskbar/file-transmission.svg",
        visible: Cpp_CommercialBuild
                 && (!app.runtimeMode || Cpp_IO_FileTransmission.runtimeAccessAllowed),
        run: function() { app.showFileTransmission() }
      },
      {
        name: qsTr("AI Assistant"),
        icon: "qrc:/icons/taskbar/ai.svg",
        visible: Cpp_CommercialBuild && !app.runtimeMode,
        run: function() { app.showAIAssistant() }
      }
    ]

    var f = (filter || "").trim().toLowerCase()
    var out = []
    for (var i = 0; i < list.length; ++i) {
      if (!list[i].visible)
        continue

      if (f.length > 0 && list[i].name.toLowerCase().indexOf(f) === -1)
        continue

      out.push(list[i])
    }

    return out
  }
}
