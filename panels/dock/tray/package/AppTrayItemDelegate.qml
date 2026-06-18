// SPDX-FileCopyrightText: 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// AppTrayItemDelegate - Renders third-party application tray icons (WeChat, QQ, etc.)
//
// This delegate displays tray icons from applications that register via the
// StatusNotifierItem (SNI) D-Bus protocol.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.dtk 1.0 as D
import org.deepin.ds.dock.tray 1.0 as DDT

Item {
    id: root

    // Properties from model
    required property string surfaceId  // Format: "app::<service-path>"
    
    // Get app data from ApplicationTrayManager
    property var appData: DDT.ApplicationTrayManager ? 
                          DDT.ApplicationTrayManager.getAppData(surfaceId) : null
    
    readonly property string title: appData ? (appData.title || "") : ""
    readonly property string iconName: appData ? (appData.iconName || "application-x-executable") : "application-x-executable"
    readonly property string status: appData ? (appData.status || "Active") : "Active"
    
    implicitWidth: contentArea.width + itemPadding * 2
    implicitHeight: contentArea.height + itemPadding * 2
    
    property int itemPadding: 4
    property size visualSize: Qt.size(implicitWidth, implicitHeight)
    property bool itemVisible: true
    property bool dragable: true  // App tray items can be dragged to rearrange

    signal clicked(string surfaceId)
    signal rightClicked(string surfaceId)

    Rectangle {
        id: contentArea
        anchors.centerIn: parent
        width: Math.max(iconDisplay.width + 12, 28)
        height: Math.max(iconDisplay.height + 4, 28)
        radius: 6
        color: mouseArea.containsMouse ? D.Colors.highlightAlpha08 : "transparent"
        border.color: mouseArea.containsMouse ? D.Colors.highlightAlpha20 : "transparent"
        border.width: 1
        
        // Show attention indicator for apps with "Attention" status
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: 6
            border.color: root.status === "Attention" ? D.Colors.highWarningColor : "transparent"
            border.width: 2
            visible: root.status === "Attention"
            
            // Pulsing animation for attention state
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.3; duration: 500 }
                NumberAnimation { from: 0.3; to: 1.0; duration: 500 }
            }
        }

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        D.DciIcon {
            id: iconDisplay
            source: iconName
            theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
            width: 20
            height: 20
            anchors.centerIn: parent
            
            // Fallback if icon name doesn't resolve
            Component.onCompleted: {
                if (!source || source === "") {
                    console.warn("[AppTray] Icon not found for:", root.title)
                }
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    root.rightClicked(root.surfaceId)
                } else {
                    root.clicked(root.surfaceId)
                }
            }

            onPressed: function(mouse) {
                if (dragable && mouse.button === Qt.LeftButton) {
                    Drag.mimeData = {}
                    Drag.mimeData["text/x-dde-shell-tray-dnd-surfaceId"] = root.surfaceId
                    Drag.mimeData["text/x-dde-shell-tray-dnd-sectionType"] = model.sectionType || "stashed"
                    Drag.mimeData["text/x-dde-shell-tray-dnd-source"] = ""
                    Drag.dragType = Drag.Automatic
                }
            }
        }
        
        // Tooltip showing app name
        ToolTip.visible: mouseArea.containsMouse && title !== ""
        ToolTip.text: title
        ToolTip.delay: 500
    }

    Component.onCompleted: {
        console.log("[AppTray] Created delegate for:", title, "(" + surfaceId + ")")
    }
}
