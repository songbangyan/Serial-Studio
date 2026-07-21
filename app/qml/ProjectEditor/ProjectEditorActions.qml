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
  // Host window providing the backup-recovery dialog; the item stays hidden without it.
  //
  property var editorWindow: null

  //
  // Returns visible project actions matching `filter`; gating lives only in each item's visible
  // field, mirroring the Project Editor toolbar's own rules.
  //
  function items(filter) {
    //
    // Wraps addGroup with its localized default name so palette + toolbar stay in sync.
    //
    function addGroup(name, widget) {
      return function() { Cpp_JSON_ProjectModel.addGroup(name, widget) }
    }

    function addDataset(widget) {
      return function() { Cpp_JSON_ProjectModel.addDataset(widget) }
    }

    function addOutput(control) {
      return function() { Cpp_JSON_ProjectModel.addOutputControl(control) }
    }

    var pe = "qrc:/icons/project-editor/toolbar/"
    var list = [
      { name: qsTr("New Project"), icon: pe + "new.svg", visible: true,
        run: function() { Cpp_JSON_ProjectModel.newJsonFile() } },
      { name: qsTr("Open Project"), icon: pe + "open.svg", visible: true,
        run: function() { Cpp_JSON_ProjectModel.openJsonFile() } },
      { name: qsTr("Save Project"), icon: pe + "save.svg",
        visible: Cpp_JSON_ProjectModel.canSave,
        run: function() { Cpp_JSON_ProjectModel.saveJsonFile(false) } },
      { name: qsTr("Save Project As"), icon: pe + "save-as.svg",
        visible: Cpp_JSON_ProjectModel.canSave,
        run: function() { Cpp_JSON_ProjectModel.saveJsonFile(true) } },
      { name: qsTr("Import Protobuf Schema"), icon: pe + "protobuf.svg", visible: true,
        run: function() { Cpp_JSON_ProtoImporter.importProto() } },
      { name: qsTr("Restore Backup"), icon: pe + "recover-backup.svg",
        visible: root.editorWindow !== null,
        run: function() { root.editorWindow.showBackupRecovery() } },
      { name: qsTr("Lock Project Editor"), icon: pe + "lock.svg", visible: true,
        run: function() { Cpp_JSON_ProjectModel.lockProject() } },
      { name: qsTr("Add Device"), icon: pe + "add-device.svg", visible: Cpp_CommercialBuild,
        run: function() { Cpp_JSON_ProjectModel.addSource() } },
      { name: qsTr("Add Group"), icon: pe + "add-group.svg", visible: true,
        run: addGroup(qsTr("Dataset Container"), SerialStudio.NoGroupWidget) },
      { name: qsTr("Add Image View"), icon: pe + "image.svg", visible: true,
        run: addGroup(qsTr("Image View"), SerialStudio.ImageView) },
      { name: qsTr("Add Web View"), icon: pe + "add-webview.svg", visible: true,
        run: addGroup(qsTr("Web View"), SerialStudio.WebView) },
      { name: qsTr("Add Painter"), icon: pe + "add-painter.svg", visible: true,
        run: addGroup(qsTr("Painter Widget"), SerialStudio.Painter) },
      { name: qsTr("Add Data Table"), icon: pe + "add-datagrid.svg", visible: true,
        run: addGroup(qsTr("Data Grid"), SerialStudio.DataGrid) },
      { name: qsTr("Add Multi-Plot"), icon: pe + "add-multiplot.svg", visible: true,
        run: addGroup(qsTr("Multiple Plot"), SerialStudio.MultiPlot) },
      { name: qsTr("Add 3D Plot"), icon: pe + "add-plot3d.svg", visible: true,
        run: addGroup(qsTr("3D Plot"), SerialStudio.Plot3D) },
      { name: qsTr("Add Accelerometer"), icon: pe + "add-accelerometer.svg", visible: true,
        run: addGroup(qsTr("Accelerometer"), SerialStudio.Accelerometer) },
      { name: qsTr("Add Gyroscope"), icon: pe + "add-gyroscope.svg", visible: true,
        run: addGroup(qsTr("Gyroscope"), SerialStudio.Gyroscope) },
      { name: qsTr("Add GPS Map"), icon: pe + "add-gps.svg", visible: true,
        run: addGroup(qsTr("GPS Map"), SerialStudio.GPS) },
      { name: qsTr("Add Dataset"), icon: pe + "add-dataset.svg", visible: true,
        run: addDataset(SerialStudio.DatasetGeneric) },
      { name: qsTr("Add Plot"), icon: pe + "add-plot.svg", visible: true,
        run: addDataset(SerialStudio.DatasetPlot) },
      { name: qsTr("Add FFT Plot"), icon: pe + "add-fft.svg", visible: true,
        run: addDataset(SerialStudio.DatasetFFT) },
      { name: qsTr("Add Gauge"), icon: pe + "add-gauge.svg", visible: true,
        run: addDataset(SerialStudio.DatasetGauge) },
      { name: qsTr("Add Level Indicator"), icon: pe + "add-bar.svg", visible: true,
        run: addDataset(SerialStudio.DatasetBar) },
      { name: qsTr("Add Compass"), icon: pe + "add-compass.svg", visible: true,
        run: addDataset(SerialStudio.DatasetCompass) },
      { name: qsTr("Add LED Indicator"), icon: pe + "add-led.svg", visible: true,
        run: addDataset(SerialStudio.DatasetLED) },
      { name: qsTr("Add Action"), icon: pe + "add-action.svg", visible: true,
        run: function() { Cpp_JSON_ProjectModel.addAction() } },
      { name: qsTr("Add Output Panel"), icon: pe + "add-output-panel.svg", visible: true,
        run: function() { Cpp_JSON_ProjectModel.addOutputPanel() } },
      { name: qsTr("Add Output Slider"), icon: pe + "add-output-slider.svg", visible: true,
        run: addOutput(SerialStudio.OutputSlider) },
      { name: qsTr("Add Output Toggle"), icon: pe + "add-output-toggle.svg", visible: true,
        run: addOutput(SerialStudio.OutputToggle) },
      { name: qsTr("Add Output Knob"), icon: pe + "add-output-knob.svg", visible: true,
        run: addOutput(SerialStudio.OutputKnob) },
      { name: qsTr("Add Output Text Field"), icon: pe + "add-output-textfield.svg",
        visible: true, run: addOutput(SerialStudio.OutputTextField) },
      { name: qsTr("Add Output Button"), icon: pe + "add-output-button.svg", visible: true,
        run: addOutput(SerialStudio.OutputButton) }
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
