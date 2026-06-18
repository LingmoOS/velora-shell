// SPDX-FileCopyrightText: 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// NativeTrayItemDelegate - Renders built-in native tray items.
//
// This delegate displays system information (datetime, battery, volume, etc.)
// directly without requiring external plugin processes or Wayland surfaces.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.dtk 1.0 as D
import org.deepin.ds.dock.tray 1.0 as DDT

Item {
    id: root

    // Properties from model (via DelegateChooser/Repeater)
    required property string surfaceId  // Format: "native::<pluginId>"
    property string pluginId: surfaceId.replace("native::", "")

    // Access native tray data from C++ singleton
    property var nativeData: DDT.NativeTrayItems ? DDT.NativeTrayItems.getItemData(pluginId) : null

    readonly property string displayText: nativeData ? nativeData.displayText : ""
    readonly property string iconName: nativeData ? nativeData.iconName : "application-x-executable"

    implicitWidth: contentArea.width + itemPadding * 2
    implicitHeight: contentArea.height + itemPadding * 2

    // Item padding from parent
    property int itemPadding: 4

    // Visual size for positioning
    property size visualSize: Qt.size(implicitWidth, implicitHeight)
    property bool itemVisible: true

    // Drag support (disabled for fixed items like datetime/power/shutdown)
    property bool dragable: model ? model.sectionType !== "fixed" : false

    // Click handler
    signal clicked(string pluginId)

    Rectangle {
        id: contentArea
        anchors.centerIn: parent
        width: Math.max(iconLabel.width + 12, 28)
        height: Math.max(iconLabel.height + 4, 28)
        radius: 6
        color: mouseArea.containsMouse ? D.Colors.highlightAlpha08 : "transparent"
        border.color: mouseArea.containsMouse ? D.Colors.highlightAlpha20 : "transparent"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Row {
            anchors.centerIn: parent
            spacing: 4

            // Icon
            D.DciIcon {
                id: iconDisplay
                source: iconName
                theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
                width: 16
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                visible: iconName !== "" && !showTextOnly()
            }

            // Text label (for items that show text like datetime, power %)
            Label {
                id: iconLabel
                text: displayText
                font.pixelSize: 11
                color: D.Colors.textTitle
                anchors.verticalCenter: parent.verticalCenter
                visible: displayText !== ""
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: dragable ? Qt.OpenHandCursor : Qt.ArrowCursor

            onClicked: function(mouse) {
                root.clicked(root.pluginId)
            }

            onPressed: function(mouse) {
                if (dragable) {
                    // Start drag if this is a draggable item
                    let drag = Drag.active
                    Drag.mimeData = {}
                    Drag.mimeData["text/x-dde-shell-tray-dnd-surfaceId"] = root.surfaceId
                    Drag.mimeData["text/x-dde-shell-tray-dnd-sectionType"] = model.sectionType || ""
                    Drag.mimeData["text/x-dde-shell-tray-dnd-source"] = ""
                    Drag.dragType = Drag.Automatic
                }
            }
        }
    }

    function showTextOnly() {
        return ["datetime"].includes(pluginId)
    }

    Component.onCompleted: {
        console.log("[NativeTray] Created delegate for:", pluginId, "text:", displayText)
    }
}
