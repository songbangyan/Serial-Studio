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

import QtQuick
import QtGraphs
import QtQuick.Layouts
import QtQuick.Controls

import SerialStudio

import "../"
import "../../Dialogs" as Dialogs

Item {
  id: root

  clip: true

  //
  // Widget data inputs
  //
  required property color color
  required property var windowRoot
  required property PlotModel model
  required property string widgetId

  //
  // Window flags
  //
  property bool hasToolbar: true

  //
  // Custom properties
  //
  readonly property var interpolationModes: [SerialStudio.InterpolationNone,
                                             SerialStudio.InterpolationLinear,
                                             SerialStudio.InterpolationZoh,
                                             SerialStudio.InterpolationStem]
  property int interpolationMode: SerialStudio.InterpolationLinear
  property bool showAreaUnderPlot: false

  //
  // User-controlled visibility preferences (persisted, ANDed with size thresholds)
  //
  property bool userShowXLabel: true
  property bool userShowYLabel: true

  //
  // Set downsample size based on widget size & zoom factor
  //
  function setDownsampleFactor()
  {
    const z = plot.zoom
    model.dataW = plot.plotArea.width * z
    model.dataH = plot.plotArea.height * z
  }

  //
  // Sync model width/height with widget, then restore persisted settings
  //
  Component.onCompleted: {
    root.setDownsampleFactor()

    const s = Cpp_JSON_ProjectModel.widgetSettings(widgetId)

    if (s["interpolationMode"] !== undefined)
      root.interpolationMode = root.normalizeInterpolationMode(s["interpolationMode"])
    else if (s["interpolate"] !== undefined)
      root.interpolationMode = s["interpolate"]
        ? SerialStudio.InterpolationLinear
        : SerialStudio.InterpolationNone

    if (root.model)
      root.model.interpolationMode = root.interpolationMode

    if (s["showAreaUnderPlot"] !== undefined)
      root.showAreaUnderPlot = s["showAreaUnderPlot"]

    if (root.interpolationMode === SerialStudio.InterpolationNone
      || root.interpolationMode === SerialStudio.InterpolationStem)
      root.showAreaUnderPlot = false

    if (s["userShowXLabel"] !== undefined)
      root.userShowXLabel = s["userShowXLabel"]

    if (s["userShowYLabel"] !== undefined)
      root.userShowYLabel = s["userShowYLabel"]
  }

  //
  // Enable/disable features depending on window size, ANDed with user preferences
  //
  onWidthChanged: updateWidgetOptions()
  onHeightChanged: updateWidgetOptions()
  onInterpolationModeChanged: {
    scatterSeries.clear()
    upperSeries.clear()
    lowerSeries.clear()
  }
  function updateWidgetOptions() {
    plot.yLabelVisible = root.userShowYLabel && (root.width >= 196)
    plot.xLabelVisible = root.userShowXLabel && (root.height >= (196 * 2/3))
    root.hasToolbar = (root.width >= toolbar.implicitWidth) && (root.height >= 220)
  }

  function normalizeInterpolationMode(value) {
    const idx = root.interpolationModes.indexOf(value)
    return idx >= 0 ? value : SerialStudio.InterpolationLinear
  }

  function nextInterpolationMode() {
    const idx = root.interpolationModes.indexOf(root.interpolationMode)
    return root.interpolationModes[(idx + 1) % root.interpolationModes.length]
  }

  function modeLabel() {
    if (root.interpolationMode === SerialStudio.InterpolationNone)
      return qsTr("None")
    if (root.interpolationMode === SerialStudio.InterpolationZoh)
      return qsTr("ZOH")
    if (root.interpolationMode === SerialStudio.InterpolationStem)
      return qsTr("Stem")
    return qsTr("Linear")
  }

  //
  // Axis range configuration dialog
  //
  Dialogs.AxisRangeDialog {
    id: axisRangeDialog
  }

  //
  // Update curve at 24 Hz
  //
  Connections {
    target: Cpp_Misc_TimerEvents

    function onUiTimeout() {
      if (root.visible && root.model) {
        if (root.interpolationMode === SerialStudio.InterpolationNone) {
          root.model.draw(scatterSeries)
        } else {
          root.model.draw(upperSeries)

          if (root.showAreaUnderPlot) {
            lowerSeries.clear()
            lowerSeries.append(root.model.minX, 0)
            lowerSeries.append(root.model.maxX, 0)
          }
        }
      }
    }
  }

  //
  // Add toolbar
  //
  RowLayout {
    id: toolbar

    spacing: 4
    visible: root.hasToolbar
    height: root.hasToolbar ? 48 : 0

    anchors {
      leftMargin: 8
      top: parent.top
      left: parent.left
      right: parent.right
    }

    DashboardToolButton {
      onClicked: {
        root.interpolationMode = root.nextInterpolationMode()
        if (root.model)
          root.model.interpolationMode = root.interpolationMode

        if (root.interpolationMode === SerialStudio.InterpolationNone
          || root.interpolationMode === SerialStudio.InterpolationStem)
          root.showAreaUnderPlot = false

        Cpp_JSON_ProjectModel.saveWidgetSetting(widgetId,
                                                "interpolationMode",
                                                root.interpolationMode)
        Cpp_JSON_ProjectModel.saveWidgetSetting(widgetId, "showAreaUnderPlot", root.showAreaUnderPlot)
      }
      checked: root.interpolationMode !== SerialStudio.InterpolationNone
      ToolTip.text: qsTr("Interpolation: %1").arg(root.modeLabel())
      icon.source: root.interpolationMode === SerialStudio.InterpolationNone
             ? "qrc:/icons/dashboard-buttons/interpolate-off.svg"
             : "qrc:/icons/dashboard-buttons/interpolate-on.svg"
    }

    DashboardToolButton {
      onClicked: {
        root.showAreaUnderPlot = !root.showAreaUnderPlot
        Cpp_JSON_ProjectModel.saveWidgetSetting(widgetId, "showAreaUnderPlot", root.showAreaUnderPlot)
      }
       enabled: root.interpolationMode !== SerialStudio.InterpolationNone
         && root.interpolationMode !== SerialStudio.InterpolationStem
      opacity: enabled ? 1 : 0.5
      checked: root.showAreaUnderPlot
      ToolTip.text: qsTr("Show Area Under Plot")
      icon.source: "qrc:/icons/dashboard-buttons/area.svg"
    }

    Rectangle {
      implicitWidth: 1
      implicitHeight: 24
      color: Cpp_ThemeManager.colors["widget_border"]
    }

    DashboardToolButton {
      onClicked: {
        root.userShowXLabel = !root.userShowXLabel
        root.updateWidgetOptions()
        Cpp_JSON_ProjectModel.saveWidgetSetting(widgetId, "userShowXLabel", root.userShowXLabel)
      }
      checked: root.userShowXLabel
      ToolTip.text: qsTr("Show X Axis Label")
      icon.source: "qrc:/icons/dashboard-buttons/x.svg"
    }

    DashboardToolButton {
      onClicked: {
        root.userShowYLabel = !root.userShowYLabel
        root.updateWidgetOptions()
        Cpp_JSON_ProjectModel.saveWidgetSetting(widgetId, "userShowYLabel", root.userShowYLabel)
      }
      checked: root.userShowYLabel
      ToolTip.text: qsTr("Show Y Axis Label")
      icon.source: "qrc:/icons/dashboard-buttons/y.svg"
    }

    Rectangle {
      implicitWidth: 1
      implicitHeight: 24
      color: Cpp_ThemeManager.colors["widget_border"]
    }

    DashboardToolButton {
      checked: plot.showCrosshairs
      ToolTip.text: qsTr("Show Crosshair")
      onClicked: plot.showCrosshairs = !plot.showCrosshairs
      icon.source: "qrc:/icons/dashboard-buttons/crosshair.svg"
    }

    DashboardToolButton {
      checked: !model.running
      ToolTip.text: model.running ? qsTr("Pause") : qsTr("Resume")
      icon.source: model.running?
                     "qrc:/icons/dashboard-buttons/pause.svg" :
                     "qrc:/icons/dashboard-buttons/resume.svg"
      onClicked: model.running = !model.running
    }

    DashboardToolButton {
      onClicked: {
        plot.xAxis.pan = 0
        plot.yAxis.pan = 0
        plot.xAxis.zoom = 1
        plot.yAxis.zoom = 1
        plot.xMin = Qt.binding(function() { return root.model.minX })
        plot.xMax = Qt.binding(function() { return root.model.maxX })
        plot.yMin = Qt.binding(function() { return root.model.minY })
        plot.yMax = Qt.binding(function() { return root.model.maxY })
      }
      opacity: enabled ? 1 : 0.5
      ToolTip.text: qsTr("Reset View")
      icon.source: "qrc:/icons/dashboard-buttons/return.svg"
      enabled: plot.xAxis.zoom !== 1 || plot.yAxis.zoom !== 1
    }

    DashboardToolButton {
      ToolTip.text: qsTr("Axis Range Settings")
      icon.source: "qrc:/icons/toolbar/settings.svg"
      onClicked: axisRangeDialog.openDialog(plot, root.model)
    }

    Item {
      Layout.fillWidth: true
    }
  }

  //
  // Plot widget
  //
  PlotWidget {
    id: plot

    anchors {
      margins: 8
      left: parent.left
      right: parent.right
      top: toolbar.bottom
      bottom: parent.bottom
    }

    xMin: root.model.minX
    xMax: root.model.maxX
    yMin: root.model.minY
    yMax: root.model.maxY
    curveColors: [root.color]
    xLabel: root.model.xLabel
    yLabel: root.model.yLabel
    mouseAreaEnabled: windowRoot.focused
    xAxis.tickInterval: plot.xTickInterval
    yAxis.tickInterval: plot.yTickInterval

    onZoomChanged: root.setDownsampleFactor()
    onWidthChanged: root.setDownsampleFactor()
    onHeightChanged: root.setDownsampleFactor()

    Connections {
      target: root.windowRoot
      function onFocusedChanged() {
        plot.mouseAreaEnabled = root.windowRoot.focused
      }
    }

    Component.onCompleted: {
      graph.addSeries(areaSeries)
      graph.addSeries(upperSeries)
      graph.addSeries(lowerSeries)
      graph.addSeries(scatterSeries)
    }

    ScatterSeries {
      id: scatterSeries

      visible: root.interpolationMode === SerialStudio.InterpolationNone
      pointDelegate: Rectangle {
        width: 2
        height: 2
        radius: 1
        color: root.color
      }
    }

    LineSeries {
      id: upperSeries

      width: 2
      visible: root.interpolationMode !== SerialStudio.InterpolationNone
    }

    LineSeries {
      id: lowerSeries

      width: 0
      visible: false
    }

    AreaSeries {
      id: areaSeries

      upperSeries: upperSeries
      lowerSeries: lowerSeries
      borderColor: "transparent"
      visible: root.showAreaUnderPlot
           && root.interpolationMode !== SerialStudio.InterpolationNone
           && root.interpolationMode !== SerialStudio.InterpolationStem
      color: Qt.rgba(root.color.r, root.color.g, root.color.b, 0.2)
    }
  }
}
