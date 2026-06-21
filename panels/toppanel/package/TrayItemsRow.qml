// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.4
import QtQuick.Layouts 2.15

import org.deepin.ds.dock.tray 1.0 as DDT
import org.deepin.dtk 1.0 as D

RowLayout {
    id: root
    spacing: 8

    property var nativeItems: DDT.NativeTrayItems

    Component.onCompleted: {
        DDT.NativeTrayItems.initialize()
    }

    Repeater {
        model: [
            { pluginId: "datetime" },
            { pluginId: "network" },
            { pluginId: "sound" },
            { pluginId: "power" },
            { pluginId: "bluetooth" }
        ]

        delegate: Item {
            id: itemDelegate
            required property var modelData
            property string pluginId: modelData.pluginId

            implicitWidth: contentRow.implicitWidth + 8
            implicitHeight: 28

            Row {
                id: contentRow
                anchors.centerIn: parent
                spacing: 4

                D.DciIcon {
                    id: statusIcon
                    source: root.nativeItems ? root.nativeItems.getIconName(pluginId) : ""
                    theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    id: statusLabel
                    text: root.nativeItems ? root.nativeItems.getDisplayText(pluginId) : ""
                    font.pixelSize: 11
                    color: D.Colors.textTitle
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text !== ""
                }
            }

            Connections {
                target: root.nativeItems
                function onItemUpdated(id) {
                    if (id === pluginId) {
                        statusIcon.source = root.nativeItems.getIconName(pluginId)
                        statusLabel.text = root.nativeItems.getDisplayText(pluginId)
                    }
                }
            }
        }
    }
}
