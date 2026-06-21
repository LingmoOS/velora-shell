// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Layouts 2.15
import QtQuick.Window 2.15

import org.deepin.ds 1.0
import org.deepin.dtk 1.0 as D
import Qt.labs.platform as LP

Window {
    id: topPanel

    visible: true
    width: Screen.width
    height: 34
    color: "transparent"
    flags: Qt.WindowDoesNotAcceptFocus

    DLayerShellWindow.anchors: DLayerShellWindow.AnchorLeft | DLayerShellWindow.AnchorRight | DLayerShellWindow.AnchorTop
    DLayerShellWindow.layer: DLayerShellWindow.LayerTop
    DLayerShellWindow.exclusionZone: active ? height : 0
    DLayerShellWindow.scope: "dde-shell/toppanel"
    DLayerShellWindow.keyboardInteractivity: DLayerShellWindow.KeyboardInteractivityOnDemand

    D.DWindow.enabled: true
    D.DWindow.windowRadius: 0
    D.DWindow.enableBlurWindow: Qt.platform.pluginName !== "xcb"
    D.DWindow.shadowColor: Qt.rgba(0, 0, 0, 0)
    D.DWindow.borderWidth: 0
    D.DWindow.themeType: Panel.colorTheme
    D.ColorSelector.family: D.Palette.CrystalColor

    property bool active: Panel.visible
    property int notificationCount: 0

    opacity: active ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    Timer {
        id: timeTimer
        interval: 1000
        repeat: true
        running: active
        triggeredOnStart: true
        onTriggered: clockText.text = Qt.formatTime(new Date(), "HH:mm")
    }

    LP.Menu {
        id: topPanelMenu
        LP.MenuItem {
            text: qsTr("Launcher")
            icon.source: "deepin-launcher"
            onTriggered: DDBusSender.service("org.deepin.dde.Launcher1")
                .path("/org/deepin/dde/Launcher1")
                .interface("org.deepin.dde.Launcher1")
                .method("Toggle").call()
        }
        LP.MenuItem {
            text: qsTr("System Settings")
            icon.source: "quick_settings"
            onTriggered: DDBusSender.service("org.deepin.dde.ControlCenter1")
                .path("/org/deepin/dde/ControlCenter1")
                .interface("org.deepin.dde.ControlCenter1")
                .method("Show").call()
        }
        LP.MenuSeparator {}
        LP.MenuItem {
            text: qsTr("Lock Screen")
            onTriggered: DDBusSender.service("org.deepin.dde.ShutdownFront1")
                .path("/org/deepin/dde/ShutdownFront1")
                .interface("org.deepin.dde.ShutdownFront1")
                .method("Lock").call()
        }
        LP.MenuItem {
            text: qsTr("Log Out")
            onTriggered: DDBusSender.service("org.deepin.dde.ShutdownFront1")
                .path("/org/deepin/dde/ShutdownFront1")
                .interface("org.deepin.dde.ShutdownFront1")
                .method("Logout").call()
        }
        LP.MenuSeparator {}
        LP.MenuItem {
            text: qsTr("Suspend")
            onTriggered: DDBusSender.service("org.deepin.dde.ShutdownFront1")
                .path("/org/deepin/dde/ShutdownFront1")
                .interface("org.deepin.dde.ShutdownFront1")
                .method("Suspend").call()
        }
        LP.MenuItem {
            text: qsTr("Restart")
            onTriggered: DDBusSender.service("org.deepin.dde.ShutdownFront1")
                .path("/org/deepin/dde/ShutdownFront1")
                .interface("org.deepin.dde.ShutdownFront1")
                .method("Restart").call()
        }
        LP.MenuItem {
            text: qsTr("Shut Down")
            onTriggered: DDBusSender.service("org.deepin.dde.ShutdownFront1")
                .path("/org/deepin/dde/ShutdownFront1")
                .interface("org.deepin.dde.ShutdownFront1")
                .method("Shutdown").call()
        }
    }

    RowLayout {
        id: topBarLayout
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 4

        // Left section: Launcher icon
        Item {
            id: leftSection
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: launcherIcon.width + 8
            implicitHeight: parent.height

            D.DciIcon {
                id: launcherIcon
                anchors.centerIn: parent
                source: "deepin-launcher"
                theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
                width: 22
                height: 22
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        topPanelMenu.popup()
                    } else {
                        DDBusSender.service("org.deepin.dde.Launcher1")
                            .path("/org/deepin/dde/Launcher1")
                            .interface("org.deepin.dde.Launcher1")
                            .method("Toggle").call()
                    }
                }
            }
        }

        // Spacer
        Item {
            Layout.fillWidth: true
        }

        // Right section: Clock + Tray items + Notification + Control Centre
        Item {
            id: rightSection
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: clockText.width + trayRow.width + notificationIcon.width + controlCenterIcon.width + spacing * 3
            implicitHeight: parent.height

            RowLayout {
                anchors.fill: parent
                spacing: 4

                D.DciIcon {
                    id: clockIcon
                    Layout.alignment: Qt.AlignVCenter
                    source: "clock"
                    theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
                    width: 18
                    height: 18
                }
                Text {
                    id: clockText
                    Layout.alignment: Qt.AlignVCenter
                    text: Qt.formatTime(new Date(), "HH:mm")
                    color: D.DTK.themeType === D.ApplicationHelper.DarkType ? "white" : "black"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                TrayItemsRow {
                    id: trayRow
                    Layout.alignment: Qt.AlignVCenter
                }

                D.DciIcon {
                    id: notificationIcon
                    Layout.alignment: Qt.AlignVCenter
                    source: "notification"
                    theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
                    width: 20
                    height: 20
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            DDBusSender.service("org.deepin.dde.Notification1")
                                .path("/org/deepin/dde/Notification1")
                                .interface("org.deepin.dde.Notification1")
                                .method("Toggle").call()
                        }
                    }
                }

                D.DciIcon {
                    id: controlCenterIcon
                    Layout.alignment: Qt.AlignVCenter
                    source: "quick_settings"
                    theme: D.Colors.isDarkTheme ? D.DIcon.DarkMode : D.DIcon.LightMode
                    width: 20
                    height: 20
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            DDBusSender.service("org.deepin.dde.ControlCenter1")
                                .path("/org/deepin/dde/ControlCenter1")
                                .interface("org.deepin.dde.ControlCenter1")
                                .method("Show").call()
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        Panel.toolTipWindow.D.DWindow.themeType = Qt.binding(function(){
            return D.DTK.themeType
        })
        Panel.popupWindow.D.DWindow.themeType = Qt.binding(function(){
            return D.DTK.themeType
        })
    }
}
