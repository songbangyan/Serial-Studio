/*
 * Serial Studio - https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru <https://aspatru.com>
 *
 * SPDX-License-Identifier: LicenseRef-SerialStudio-Commercial
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import "../Widgets" as Widgets

Widgets.SmartDialog {
  id: root

  //
  // Window options
  //
  minimumWidth: 680
  title: qsTr("API Keys")
  preferredWidth: column.implicitWidth
  preferredHeight: column.implicitHeight

  //
  // Per-provider tagline (visible under the provider name)
  //
  function providerTagline(idx) {
    if (idx === 0) {
      return qsTr("Anthropic Claude. The default is Claude Haiku 4.5 "
              + "($1 input / $5 output per million tokens). Sonnet 4.6 "
              + "and Opus 4.7 are also available. Supports streaming, "
              + "tool use, extended thinking, and prompt caching.")
    } else if (idx === 1) {
      return qsTr("OpenAI Chat Completions. The default is GPT-5 mini "
              + "for fast, cost-conscious agentic work. GPT-5.2 is the "
              + "stronger general-purpose option, and GPT-5.2 Chat "
              + "tracks the model currently used in ChatGPT.")
    } else if (idx === 2) {
      return qsTr("Google Gemini. The default is Gemini 2.0 Flash, which "
              + "has a generous free tier (subject to rate limits). "
              + "Gemini 1.5 Pro and Gemini 1.5 Flash are also available.")
    } else if (idx === 3) {
      return qsTr("DeepSeek. OpenAI-compatible API. The default is "
              + "deepseek-chat (V3). deepseek-reasoner (R1) is also "
              + "available. Often the cheapest cloud option for tool use.")
    } else if (idx === 4) {
      return qsTr("Local model server. Works with any OpenAI-compatible "
              + "endpoint -- Ollama, llama.cpp's llama-server, LM Studio, "
              + "or vLLM. Nothing leaves your machine. The model list is "
              + "queried live from the server.")
    }
    return ""
  }

  //
  // Layout
  //
  dialogContent: ColumnLayout {
    id: column

    spacing: 14
    anchors.centerIn: parent

    Label {
      Layout.fillWidth: true
      Layout.preferredWidth: 640
      wrapMode: Text.WordWrap
      font: Cpp_Misc_CommonFonts.uiFont
      color: Cpp_ThemeManager.colors["text"]
      text: qsTr("Bring your own API keys. They are encrypted at rest with "
              + "a per-machine key and never leave your computer except to "
              + "communicate with the provider you select.")
    }

    Repeater {
      model: Cpp_AI_Assistant.providerNames

      delegate: Rectangle {
        id: providerCard

        radius: 4
        border.width: 1
        Layout.fillWidth: true
        implicitHeight: providerColumn.implicitHeight + 24
        color: Cpp_ThemeManager.colors["groupbox_background"]
        border.color: Cpp_ThemeManager.colors["groupbox_border"]

        property bool revealed: false
        property int providerIdx: index

        //
        // Mirrored Q_INVOKABLE hasKey(); refreshed via keysChanged
        //
        property bool hasKey: Cpp_AI_Assistant.hasKey(providerIdx)

        Connections {
          target: Cpp_AI_Assistant
          function onKeysChanged() {
            providerCard.hasKey = Cpp_AI_Assistant.hasKey(providerCard.providerIdx)
          }
        }

        ColumnLayout {
          id: providerColumn

          spacing: 8
          anchors.margins: 12
          anchors.fill: parent

          //
          // Header row: provider name + status pill
          //
          RowLayout {
            spacing: 10
            Layout.fillWidth: true

            Label {
              text: modelData
              font: Cpp_Misc_CommonFonts.customUiFont(1.1, true)
              color: Cpp_ThemeManager.colors["text"]
            }

            Rectangle {
              radius: 9
              implicitWidth: statusPill.implicitWidth + 14
              implicitHeight: statusPill.implicitHeight + 4
              color: providerCard.hasKey
                     ? Qt.darker(Cpp_ThemeManager.colors["highlight"], 1.1)
                     : Qt.darker(Cpp_ThemeManager.colors["mid"], 1.05)
              border.width: 1
              border.color: providerCard.hasKey
                           ? Cpp_ThemeManager.colors["highlight"]
                           : Cpp_ThemeManager.colors["mid"]

              Label {
                id: statusPill

                anchors.centerIn: parent
                text: providerCard.hasKey
                      ? qsTr("Key set")
                      : qsTr("No key")
                font: Cpp_Misc_CommonFonts.customUiFont(0.75, true)
                color: Cpp_ThemeManager.colors["highlighted_text"]
                Component.onCompleted: font.capitalization = Font.AllUppercase
              }
            }

            Item { Layout.fillWidth: true }
          }

          //
          // Provider tagline / model line
          //
          Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.75
            font: Cpp_Misc_CommonFonts.customUiFont(0.9, false)
            color: Cpp_ThemeManager.colors["text"]
            text: root.providerTagline(providerIdx)
          }

          //
          // Key field row -- only shown for cloud providers that need an API key
          //
          RowLayout {
            spacing: 6
            Layout.fillWidth: true
            visible: Cpp_AI_Assistant.requiresApiKey(providerIdx)

            TextField {
              id: keyField

              Layout.fillWidth: true
              font: Cpp_Misc_CommonFonts.monoFont
              echoMode: revealed ? TextInput.Normal : TextInput.Password
              placeholderText: providerCard.hasKey
                               ? qsTr("A key is on file -- paste a new one to replace it")
                               : qsTr("Paste your API key here")
            }

            ToolButton {
              text: ""
              display: AbstractButton.IconOnly
              ToolTip.text: revealed ? qsTr("Hide key") : qsTr("Show key while typing")
              ToolTip.visible: hovered
              ToolTip.delay: 400
              icon.color: Cpp_ThemeManager.colors["text"]
              icon.source: revealed
                           ? "qrc:/icons/buttons/invisible.svg"
                           : "qrc:/icons/buttons/visible.svg"
              onClicked: revealed = !revealed
            }

            ToolButton {
              text: qsTr("Get key")
              ToolTip.text: qsTr("Open the provider's console to create a new key")
              ToolTip.visible: hovered
              ToolTip.delay: 400
              display: AbstractButton.TextBesideIcon
              font: Cpp_Misc_CommonFonts.uiFont
              icon.color: Cpp_ThemeManager.colors["text"]
              icon.source: "qrc:/icons/buttons/website.svg"
              onClicked: Qt.openUrlExternally(Cpp_AI_Assistant.keyVendorUrl(providerIdx))
            }

            ToolButton {
              text: qsTr("Save")
              display: AbstractButton.TextBesideIcon
              enabled: keyField.text.length > 0
              font: Cpp_Misc_CommonFonts.uiFont
              icon.color: Cpp_ThemeManager.colors["text"]
              icon.source: "qrc:/icons/buttons/save.svg"
              onClicked: {
                Cpp_AI_Assistant.setKey(providerIdx, keyField.text)
                keyField.clear()
                revealed = false

                // Activate the just-saved provider and dismiss the dialog
                if (Cpp_AI_Assistant.currentProvider !== providerIdx)
                  Cpp_AI_Assistant.selectProvider(providerIdx)

                root.close()
              }
            }

            ToolButton {
              text: ""
              display: AbstractButton.IconOnly
              enabled: providerCard.hasKey
              ToolTip.text: qsTr("Remove the stored key for %1").arg(modelData)
              ToolTip.visible: hovered
              ToolTip.delay: 400
              icon.color: Cpp_ThemeManager.colors["alarm"]
              icon.source: "qrc:/icons/buttons/trash.svg"
              onClicked: Cpp_AI_Assistant.clearKey(providerIdx)
            }
          }

          //
          // Local-server URL row -- shown only for the Local provider
          //
          RowLayout {
            spacing: 6
            Layout.fillWidth: true
            visible: !Cpp_AI_Assistant.requiresApiKey(providerIdx)

            TextField {
              id: urlField

              Layout.fillWidth: true
              font: Cpp_Misc_CommonFonts.monoFont
              text: Cpp_AI_Assistant.localBaseUrl()
              placeholderText: qsTr("http://localhost:11434/v1")
            }

            ToolButton {
              text: qsTr("Install Ollama")
              ToolTip.text: qsTr("Open the Ollama download page")
              ToolTip.visible: hovered
              ToolTip.delay: 400
              display: AbstractButton.TextBesideIcon
              font: Cpp_Misc_CommonFonts.uiFont
              icon.color: Cpp_ThemeManager.colors["text"]
              icon.source: "qrc:/icons/buttons/website.svg"
              onClicked: Qt.openUrlExternally(Cpp_AI_Assistant.keyVendorUrl(providerIdx))
            }

            ToolButton {
              text: qsTr("Apply")
              display: AbstractButton.TextBesideIcon
              enabled: urlField.text.length > 0
              font: Cpp_Misc_CommonFonts.uiFont
              icon.color: Cpp_ThemeManager.colors["text"]
              icon.source: "qrc:/icons/buttons/save.svg"
              onClicked: {
                Cpp_AI_Assistant.setLocalBaseUrl(urlField.text)
                if (Cpp_AI_Assistant.currentProvider !== providerIdx)
                  Cpp_AI_Assistant.selectProvider(providerIdx)

                root.close()
              }
            }

            ToolButton {
              text: ""
              display: AbstractButton.IconOnly
              ToolTip.text: qsTr("Re-query the model list")
              ToolTip.visible: hovered
              ToolTip.delay: 400
              icon.color: Cpp_ThemeManager.colors["text"]
              icon.source: "qrc:/icons/buttons/refresh.svg"
              onClicked: Cpp_AI_Assistant.refreshLocalModels()
            }
          }
        }
      }
    }

    //
    // Footer with summary status
    //
    RowLayout {
      spacing: 8
      Layout.topMargin: 4
      Layout.fillWidth: true

      Label {
        id: footerSummary

        Layout.fillWidth: true
        opacity: 0.6
        font: Cpp_Misc_CommonFonts.customUiFont(0.85, false)
        color: Cpp_ThemeManager.colors["text"]

        property int keysReady: 0

        function refresh() {
          let count = 0
          for (let i = 0; i < Cpp_AI_Assistant.providerNames.length; ++i)
            if (Cpp_AI_Assistant.hasKey(i)) ++count

          keysReady = count
        }

        Component.onCompleted: refresh()

        Connections {
          target: Cpp_AI_Assistant
          function onKeysChanged() { footerSummary.refresh() }
        }

        text: {
          if (keysReady === 0)
            return qsTr("No API keys configured yet. Add at least one above to get started.")

          if (keysReady === 1)
            return qsTr("One provider is ready.")

          return qsTr("%1 providers are ready.").arg(keysReady)
        }
      }

      DialogButtonBox {
        standardButtons: DialogButtonBox.Close
        onRejected: root.close()
      }
    }
  }
}
