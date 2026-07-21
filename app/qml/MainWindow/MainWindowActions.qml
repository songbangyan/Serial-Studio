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
  // Toolbar actions matching `filter`; visible fields mirror the toolbar's runtime-mode gating.
  //
  function items(filter) {
    var tb = "qrc:/icons/toolbar/"
    var list = [
      {
        name: qsTr("Project Editor"),
        icon: tb + "project-setup.svg",
        visible: !app.runtimeMode,
        run: function() { app.showProjectEditor() }
      },
      {
        name: qsTr("Open Project"),
        icon: tb + "open-project.svg",
        visible: !app.runtimeMode,
        run: function() {
          Cpp_AppState.operationMode = SerialStudio.ProjectFile
          Cpp_JSON_ProjectModel.openJsonFile()
        }
      },
      {
        name: qsTr("Open CSV"),
        icon: tb + "csv.svg",
        visible: !app.runtimeMode,
        run: function() { Cpp_CSV_Player.openFile() }
      },
      {
        name: qsTr("Open MDF4"),
        icon: tb + "mf4.svg",
        visible: !app.runtimeMode,
        run: function() { Cpp_MDF4_Player.openFile() }
      },
      {
        name: qsTr("Deploy Operator App"),
        icon: tb + "deploy.svg",
        visible: !app.runtimeMode,
        run: function() { app.showShortcutGenerator() }
      },
      {
        name: qsTr("Extensions"),
        icon: tb + "extensions-small.svg",
        visible: true,
        run: function() { app.showExtensionManager() }
      },
      {
        name: qsTr("Examples"),
        icon: tb + "examples.svg",
        visible: true,
        run: function() { app.showExamplesBrowser() }
      },
      {
        name: qsTr("About"),
        icon: tb + "about.svg",
        visible: true,
        run: function() { app.showAboutDialog() }
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
