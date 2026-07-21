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
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl

import SerialStudio
import SerialStudio.UI as SS_Ui

import "../../../Widgets" as Widgets

Item {
  id: root

  z: 5000
  visible: false
  anchors.fill: parent
  onVisibleChanged: if (visible) Qt.callLater(_search.forceActiveFocus)

  required property SS_Ui.TaskBar taskBar

  //
  // Breadcrumb of folder levels; each frame is { text, nodes, folderId }. folderId -1 is the root.
  //
  property var levelStack: []

  //
  // Ordered categories { title, items } for rendering, plus a flat list for keyboard navigation.
  //
  property var sections: []
  property var displayNodes: []
  property int currentIndex: -1

  readonly property var currentNodes:
      levelStack.length > 0 ? levelStack[levelStack.length - 1].nodes : []

  readonly property int currentFolderId:
      levelStack.length > 0 ? levelStack[levelStack.length - 1].folderId : -1

  readonly property bool searching: _search.text.trim().length > 0
  readonly property int columns: Math.max(1, Math.floor((panel.width - 64) / 152))

  readonly property var toolDefs: [
    { "tool": "terminal", "text": qsTr("Console"), "icon": "qrc:/icons/start/console.svg",
      "keywords": [qsTr("Terminal")] },
    { "tool": "notificationLog", "text": qsTr("Notifications"),
      "icon": "qrc:/icons/start/notifications.svg" },
    { "tool": "clock", "text": qsTr("Clock"), "icon": "qrc:/icons/start/clock.svg" },
    { "tool": "stopwatch", "text": qsTr("Stopwatch"), "icon": "qrc:/icons/start/stopwatch.svg" }
  ]

  //
  // True when a tool matches the query by its label or any translated synonym (e.g. "terminal"
  // finds the Console). Keeps discovery forgiving of the name users reach for first.
  //
  function toolMatches(td, query) {
    if (td.text.toLowerCase().indexOf(query) >= 0)
      return true

    const kw = td.keywords || []
    for (let i = 0; i < kw.length; ++i)
      if (kw[i].toLowerCase().indexOf(query) >= 0)
        return true

    return false
  }

  function open() {
    _search.text = ""
    const tree = taskBar ? taskBar.workspaceTree() : []
    root.levelStack = [{ text: qsTr("Workspaces"), nodes: tree, folderId: -1 }]
    root.recompute()
    root.visible = true
    _search.forceActiveFocus()
  }

  function close() {
    if (taskBar)
      taskBar.searchFilter = ""

    root.visible = false
  }

  function toggle() {
    if (root.visible)
      root.close()
    else
      root.open()
  }

  //
  // Flattens every workspace leaf, tagging each with its full folder path (e.g. DAQ / ATAM / Sum).
  //
  function flattenWorkspaces(nodes, prefix, out) {
    for (let i = 0; i < nodes.length; ++i) {
      const node = nodes[i]
      const path = prefix.length > 0 ? (prefix + " / " + node.text) : node.text
      if (node.isFolder)
        root.flattenWorkspaces(node.children, path, out)
      else
        out.push({ id: node.id, text: node.text, icon: node.icon, isFolder: false, fullPath: path })
    }
  }

  //
  // Flattens every folder at any depth, tagging each with its full path, so search can match and
  // drill into folders (not just workspace leaves). Children are kept intact for enterFolder().
  //
  function flattenFolders(nodes, prefix, out) {
    for (let i = 0; i < nodes.length; ++i) {
      const node = nodes[i]
      if (!node.isFolder)
        continue

      const path = prefix.length > 0 ? (prefix + " / " + node.text) : node.text
      out.push({ id: node.id, text: node.text, icon: node.icon, isFolder: true,
                 children: node.children, fullPath: path })
      root.flattenFolders(node.children, path, out)
    }
  }

  function activateTool(tool) {
    if (tool === "terminal")
      Cpp_UI_Dashboard.terminalEnabled = true
    else if (tool === "notificationLog")
      Cpp_UI_Dashboard.notificationLogEnabled = true
    else if (tool === "clock")
      Cpp_UI_Dashboard.clockEnabled = true
    else if (tool === "stopwatch")
      Cpp_UI_Dashboard.stopwatchEnabled = true
  }

  //
  // Rebuilds the categorized sections and the flat navigation list. Browsing (empty query) shows
  // the current folder level plus an "add workspace" cell; searching splits results by category.
  //
  function recompute() {
    const query = _search.text.trim().toLowerCase()
    if (taskBar)
      taskBar.searchFilter = _search.text.trim()

    let secs = []
    if (query.length > 0) {
      const tree = taskBar ? taskBar.workspaceTree() : []

      let ws = []
      root.flattenWorkspaces(tree, "", ws)
      ws = ws.filter(n => n.text.toLowerCase().indexOf(query) >= 0)

      let folders = []
      root.flattenFolders(tree, "", folders)
      folders = folders.filter(n => n.text.toLowerCase().indexOf(query) >= 0)

      let groups = []
      let widgets = []
      const wr = taskBar ? taskBar.searchResults : []
      for (let i = 0; i < wr.length; ++i) {
        const w = wr[i]
        const node = {
          "id": w.windowId,
          "isWidget": true,
          "isFolder": false,
          "text": w.widgetName,
          "icon": w.widgetIcon,
          "groupId": w.groupId
        }
        if (w.isGroupWidget)
          groups.push(node)
        else
          widgets.push(node)
      }

      let tools = []
      for (let t = 0; t < root.toolDefs.length; ++t) {
        const td = root.toolDefs[t]
        if (root.toolMatches(td, query))
          tools.push({ "isTool": true, "isFolder": false, "tool": td.tool,
                       "text": td.text, "icon": td.icon })
      }

      if (folders.length > 0)
        secs.push({ "title": qsTr("Folders"), "items": folders })

      if (ws.length > 0)
        secs.push({ "title": qsTr("Workspaces"), "items": ws })

      if (groups.length > 0)
        secs.push({ "title": qsTr("Groups"), "items": groups })

      if (widgets.length > 0)
        secs.push({ "title": qsTr("Widgets"), "items": widgets })

      if (tools.length > 0)
        secs.push({ "title": qsTr("Tools"), "items": tools })
    } else {
      const addCell = {
        "id": -1,
        "isAdd": true,
        "isFolder": false,
        "text": qsTr("Add Workspace"),
        "icon": "qrc:/icons/buttons/add-workspace.svg"
      }
      secs.push({ "title": "", "items": root.currentNodes.concat([addCell]) })
    }

    let flat = []
    for (let s = 0; s < secs.length; ++s)
      for (let k = 0; k < secs[s].items.length; ++k) {
        secs[s].items[k]._idx = flat.length
        flat.push(secs[s].items[k])
      }

    root.sections = secs
    root.displayNodes = flat
    root.currentIndex = -1
  }

  //
  // Slide the result grid in from the given direction: +1 (forward, from the right) on drill-in,
  // -1 (back, from the left) on go-up. The new content is already in place; only the offset animates.
  //
  function slide(direction) {
    _slide.x = direction * panel.width
    _slideAnim.restart()
  }

  function enterFolder(node) {
    root.levelStack = root.levelStack.concat(
                        [{ text: node.text, nodes: node.children, folderId: node.id }])
    _search.text = ""
    root.recompute()
    root.slide(1)
  }

  function goUp() {
    if (root.levelStack.length <= 1) {
      root.close()
      return
    }

    root.levelStack = root.levelStack.slice(0, root.levelStack.length - 1)
    _search.text = ""
    root.recompute()
    root.slide(-1)
  }

  //
  // Jump straight to a breadcrumb level (a shallower folder in the trail). Ignores the current
  // level and any out-of-range index.
  //
  function goToLevel(level) {
    if (level < 0 || level >= root.levelStack.length - 1)
      return

    root.levelStack = root.levelStack.slice(0, level + 1)
    _search.text = ""
    root.recompute()
    root.slide(-1)
  }

  function activate(node) {
    if (!node)
      return

    if (node.isAdd) {
      Cpp_JSON_ProjectModel.promptAddWorkspaceInFolder(root.currentFolderId)
      root.currentIndex = -1
      return
    }

    if (node.isTool) {
      root.activateTool(node.tool)
      root.close()
      return
    }

    if (node.isWidget) {
      if (taskBar)
        taskBar.navigateToWidget(node.id, node.groupId)

      root.close()
      return
    }

    if (node.isFolder) {
      root.enterFolder(node)
      return
    }

    if (taskBar)
      taskBar.selectWorkspaceById(node.id)

    root.close()
  }

  function activateCurrent() {
    const idx = root.currentIndex >= 0 ? root.currentIndex : 0
    if (idx < 0 || idx >= root.displayNodes.length)
      return

    root.activate(root.displayNodes[idx])
  }

  function move(delta) {
    const count = root.displayNodes.length
    if (count === 0)
      return

    if (root.currentIndex < 0) {
      root.currentIndex = 0
      return
    }

    root.currentIndex = Math.max(0, Math.min(count - 1, root.currentIndex + delta))
  }

  //
  // Transparent catcher: a press anywhere outside the dialog dismisses it (no dimming).
  //
  MouseArea {
    anchors.fill: parent
    onClicked: root.close()
  }

  //
  // Folder context menu (right-click a cell).
  //
  Menu {
    id: _ctxMenu

    property int folderId: -1

    MenuItem {
      icon.width: 16
      icon.height: 16
      text: qsTr("New Folder")
      icon.source: "qrc:/icons/project-editor/actions/add-folder-small.svg"
      onTriggered: Cpp_JSON_ProjectModel.promptAddWorkspaceFolder(root.currentFolderId)
    }

    MenuItem {
      icon.width: 16
      icon.height: 16
      text: qsTr("Rename Folder")
      enabled: _ctxMenu.folderId >= 0
      icon.source: "qrc:/icons/project-editor/actions/rename.svg"
      onTriggered: Cpp_JSON_ProjectModel.promptRenameWorkspaceFolder(_ctxMenu.folderId)
    }
  }

  //
  // Drop shadow cast by the dialog.
  //
  RectangularShadow {
    blur: 28
    spread: 2
    color: "#59000000"
    anchors.fill: panel
    radius: panel.radius
  }

  //
  // One result cell (workspace, folder, group, widget, tool, or the add-workspace action).
  //
  Component {
    id: _cellComponent

    Item {
      id: _cell

      width: 152
      height: 104

      required property var modelData

      readonly property bool hovered: _cellMouse.containsMouse
      readonly property bool highlighted: _cell.modelData._idx === root.currentIndex

      Rectangle {
        radius: 6
        border.width: 1
        anchors.fill: parent
        anchors.margins: 6
        color: _cell.highlighted ? Cpp_ThemeManager.colors["highlight"]
                                 : Cpp_ThemeManager.colors["groupbox_background"]
        border.color: _cell.highlighted || _cell.hovered
                      ? Cpp_ThemeManager.colors["highlight"]
                      : Cpp_ThemeManager.colors["groupbox_border"]

        //
        // Very slight highlight wash on hover (skipped when already selected).
        //
        Rectangle {
          opacity: 0.10
          anchors.fill: parent
          radius: parent.radius
          color: Cpp_ThemeManager.colors["highlight"]
          visible: _cell.hovered && !_cell.highlighted
        }

        ColumnLayout {
          spacing: 8
          anchors.centerIn: parent
          width: parent.width - 16

          IconImage {
            width: 32
            height: 32
            opacity: 0.9
            sourceSize: Qt.size(32, 32)
            source: _cell.modelData.icon
            Layout.alignment: Qt.AlignHCenter
            color: _cell.modelData.isAdd === true
                   ? (_cell.highlighted ? Cpp_ThemeManager.colors["highlighted_text"]
                                        : Cpp_ThemeManager.colors["button_text"])
                   : "transparent"
          }

          Label {
            elide: Text.ElideRight
            Layout.fillWidth: true
            font: Cpp_Misc_CommonFonts.uiFont
            horizontalAlignment: Text.AlignHCenter
            text: _cell.modelData.fullPath !== undefined ? _cell.modelData.fullPath
                                                         : _cell.modelData.text
            color: _cell.highlighted ? Cpp_ThemeManager.colors["highlighted_text"]
                                     : Cpp_ThemeManager.colors["text"]
          }

          Label {
            opacity: 0.7
            text: qsTr("Folder")
            Layout.alignment: Qt.AlignHCenter
            visible: _cell.modelData.isFolder === true
            font: Cpp_Misc_CommonFonts.customUiFont(0.8, false)
            color: _cell.highlighted ? Cpp_ThemeManager.colors["highlighted_text"]
                                     : Cpp_ThemeManager.colors["text"]
          }
        }

        MouseArea {
          id: _cellMouse

          hoverEnabled: true
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          acceptedButtons: Qt.LeftButton | Qt.RightButton
          onClicked: (mouse) => {
                       // Right-click opens the folder menu without stealing the selection highlight.
                       if (mouse.button === Qt.RightButton) {
                         _ctxMenu.folderId = _cell.modelData.isFolder ? _cell.modelData.id : -1
                         _ctxMenu.popup()
                         return
                       }

                       root.currentIndex = _cell.modelData._idx
                       root.activate(_cell.modelData)
                     }
        }
      }
    }
  }

  //
  // Switcher dialog
  //
  Rectangle {
    id: panel

    clip: true
    radius: 12
    border.width: 1
    anchors.centerIn: parent
    width: Math.min(680, root.width - 80)
    height: Math.min(480, root.height - 80)
    color: Cpp_ThemeManager.colors["pane_background"]
    border.color: Cpp_ThemeManager.colors["groupbox_border"]

    //
    // Swallows clicks so they never reach the outside dismiss-catcher, and turns any right-click on
    // the dialog chrome (or empty space) into the folder context menu at the current level.
    //
    MouseArea {
      anchors.fill: parent
      acceptedButtons: Qt.LeftButton | Qt.RightButton
      onClicked: (mouse) => {
                   if (mouse.button === Qt.RightButton) {
                     _ctxMenu.folderId = -1
                     _ctxMenu.popup()
                   }
                 }
    }

    ColumnLayout {
      spacing: 0
      anchors.fill: parent

      //
      // Header: a constant Workspaces identity (icon + title), centered search, and close. Folder
      // navigation lives in the breadcrumb bar below, so this title never changes underfoot.
      //
      Rectangle {
        border.width: 1
        Layout.fillWidth: true
        Layout.preferredHeight: 56
        topLeftRadius: panel.radius
        topRightRadius: panel.radius
        border.color: Cpp_ThemeManager.colors["pane_caption_border"]

        gradient: Gradient {
          GradientStop {
            position: 0
            color: Cpp_ThemeManager.colors["pane_caption_bg_top"]
          }

          GradientStop {
            position: 1
            color: Cpp_ThemeManager.colors["pane_caption_bg_bottom"]
          }
        }

        IconImage {
          id: _headerIcon

          anchors.leftMargin: 12
          anchors.left: parent.left
          sourceSize: Qt.size(18, 18)
          source: "qrc:/icons/buttons/workspaces.svg"
          anchors.verticalCenter: parent.verticalCenter
          color: Cpp_ThemeManager.colors["pane_caption_foreground"]
        }

        Label {
          anchors.leftMargin: 8
          text: qsTr("Workspaces")
          anchors.left: _headerIcon.right
          verticalAlignment: Text.AlignVCenter
          font: Cpp_Misc_CommonFonts.boldUiFont
          anchors.verticalCenter: parent.verticalCenter
          color: Cpp_ThemeManager.colors["pane_caption_foreground"]
        }

        Widgets.SearchField {
          id: _search

          anchors.centerIn: parent
          onTextChanged: root.recompute()
          placeholderText: qsTr("Search")
          Keys.onLeftPressed: root.move(-1)
          Keys.onRightPressed: root.move(1)
          Keys.onEscapePressed: root.close()
          width: Math.min(300, parent.width - 360)
          Keys.onUpPressed: root.move(-root.columns)
          Keys.onDownPressed: root.move(root.columns)
          Keys.onEnterPressed: root.activateCurrent()
          Keys.onReturnPressed: root.activateCurrent()
        }

        Widgets.IconButton {
          flat: true
          iconSize: 18
          implicitWidth: 28
          background: Item {}
          onClicked: root.close()
          anchors.rightMargin: 12
          ToolTip.text: qsTr("Close")
          anchors.right: parent.right
          icon.source: "qrc:/icons/buttons/close.svg"
          anchors.verticalCenter: parent.verticalCenter
          icon.color: (hovered || down) ? Cpp_ThemeManager.colors["highlight"]
                                        : Cpp_ThemeManager.colors["pane_caption_foreground"]

          HoverHandler {
            cursorShape: Qt.PointingHandCursor
          }
        }
      }

      //
      // Breadcrumb bar: a Back button plus the folder trail. Each earlier crumb jumps to that level;
      // the trail is hidden at the root (where Back is disabled) and a hint explains how to drive it.
      //
      Rectangle {
        Layout.leftMargin: 1
        Layout.rightMargin: 1
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        color: Cpp_ThemeManager.colors["groupbox_background"]

        Rectangle {
          height: 1
          width: parent.width
          anchors.top: parent.top
          color: Cpp_ThemeManager.colors["groupbox_border"]
        }

        Rectangle {
          height: 1
          width: parent.width
          anchors.bottom: parent.bottom
          color: Cpp_ThemeManager.colors["groupbox_border"]
        }

        RowLayout {
          spacing: 4
          anchors.fill: parent
          anchors.leftMargin: 8
          anchors.rightMargin: 12

          Widgets.IconButton {
            flat: true
            iconSize: 16
            implicitWidth: 24
            background: Item {}
            onClicked: root.goUp()
            ToolTip.text: qsTr("Back")
            Layout.alignment: Qt.AlignVCenter
            enabled: root.levelStack.length > 1
            icon.color: Cpp_ThemeManager.colors["text"]
            icon.source: "qrc:/icons/buttons/backward.svg"

            HoverHandler {
              cursorShape: Qt.PointingHandCursor
              enabled: root.levelStack.length > 1
            }
          }

          Repeater {
            model: root.levelStack

            delegate: RowLayout {
              id: _crumb

              spacing: 4
              visible: root.levelStack.length > 1
              required property int index
              required property var modelData

              readonly property bool navigable: !_crumb.isLast
              readonly property bool isLast: _crumb.index === root.levelStack.length - 1

              Label {
                elide: Text.ElideRight
                Layout.maximumWidth: 160
                text: _crumb.modelData.text
                verticalAlignment: Text.AlignVCenter
                color: Cpp_ThemeManager.colors["text"]
                font: _crumb.isLast ? Cpp_Misc_CommonFonts.boldUiFont
                                    : Cpp_Misc_CommonFonts.uiFont

                MouseArea {
                  anchors.fill: parent
                  enabled: _crumb.navigable
                  cursorShape: Qt.PointingHandCursor
                  onClicked: root.goToLevel(_crumb.index)
                }
              }

              Label {
                text: "/"
                opacity: 0.4
                visible: !_crumb.isLast
                color: Cpp_ThemeManager.colors["text"]
                font: Cpp_Misc_CommonFonts.uiFont
              }
            }
          }

          Item { Layout.fillWidth: true }

          Label {
            opacity: 0.5
            elide: Text.ElideRight
            text: qsTr("Type to search, Enter to open, Esc to close")
            color: Cpp_ThemeManager.colors["text"]
            verticalAlignment: Text.AlignVCenter
            font: Cpp_Misc_CommonFonts.customUiFont(0.85, false)
          }
        }
      }

      Flickable {
        id: _scroll

        clip: true
        Layout.margins: 16
        contentWidth: width
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentHeight: _sectionColumn.height
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
          policy: _scroll.contentHeight > _scroll.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        //
        // Right-click on the empty space between/below result cells opens the current-level folder
        // menu. Spans the whole viewport (not just the content) so short result sets still respond.
        //
        MouseArea {
          z: -1
          width: _scroll.width
          acceptedButtons: Qt.RightButton
          height: Math.max(_scroll.height, _sectionColumn.height)
          onClicked: {
            _ctxMenu.folderId = -1
            _ctxMenu.popup()
          }
        }

        Column {
          id: _sectionColumn

          spacing: 12
          width: _scroll.width
          transform: Translate {
            id: _slide
          }

          NumberAnimation {
            id: _slideAnim

            to: 0
            property: "x"
            duration: 200
            target: _slide
            easing.type: Easing.OutCubic
          }

          Repeater {
            model: root.sections

            delegate: Column {
              id: _section

              spacing: 6
              width: _sectionColumn.width

              required property var modelData

              RowLayout {
                spacing: 8
                width: parent.width
                visible: _section.modelData.title.length > 0

                Label {
                  text: _section.modelData.title
                  color: Cpp_ThemeManager.colors["text"]
                  font: Cpp_Misc_CommonFonts.boldUiFont
                }

                Rectangle {
                  height: 1
                  Layout.fillWidth: true
                  Layout.alignment: Qt.AlignVCenter
                  color: Cpp_ThemeManager.colors["groupbox_border"]
                }
              }

              Flow {
                width: parent.width

                Repeater {
                  delegate: _cellComponent
                  model: _section.modelData.items
                }
              }
            }
          }
        }
      }

      Label {
        opacity: 0.6
        Layout.fillWidth: true
        Layout.bottomMargin: 16
        font: Cpp_Misc_CommonFonts.uiFont
        color: Cpp_ThemeManager.colors["text"]
        horizontalAlignment: Text.AlignHCenter
        visible: root.displayNodes.length === 0
        text: qsTr("No results found")
      }
    }
  }
}
