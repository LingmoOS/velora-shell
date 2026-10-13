// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.dtk 1.0
import org.deepin.dtk.style 1.0 as DStyle
import org.deepin.ds 1.0
import org.deepin.ds.notification
import org.deepin.ds.notificationcenter

Window {
    id: root

    // 本发行版任务栏由 lingmo-dock 提供，dde-shell 自带的 dock 面板并不编译，
    // 所以 DS.applet("org.deepin.ds.dock") 取不到任务栏，避让会静默失效。
    // 改为读 Panel.dock（DockProxy，查 org.deepin.dde.Dock1 / daemon.Dock1）。
    function dockReserve(position) {
        let dock = Panel.dock
        if (!dock || !dock.available)
            return 0

        // 只避让与自身在同一块屏幕上的任务栏
        if (dock.screen !== root.screen)
            return 0
        if (dock.position !== position)
            return 0

        let dockRect = dock.rect
        let vertical = (position === 0 || position === 2)   // DOCK_TOP / DOCK_BOTTOM
        let cap = dock.dockSize > 0 ? dock.dockSize : (vertical ? dockRect.height : dockRect.width)

        // HideState == Hide(2)：任务栏已滑出屏幕，按完整尺寸预留，
        // 避免它弹出时压住通知
        if (dock.hideState === 2)
            return cap

        let screenGeometry = Qt.rect(
            root.screen.virtualX,
            root.screen.virtualY,
            root.screen.width,
            root.screen.height
        )

        // cap 用于挡住位置切换瞬间几何尚未更新的错误值
        switch (position) {
            case 0: { // DOCK_TOP
                let visibleHeight = Math.max(0, dockRect.y + dockRect.height - screenGeometry.y)
                return Math.min(visibleHeight, cap)
            }
            case 1: { // DOCK_RIGHT
                let visibleWidth = Math.max(0, screenGeometry.x + screenGeometry.width - dockRect.x)
                return Math.min(visibleWidth, cap)
            }
            case 2: { // DOCK_BOTTOM
                let visibleHeight = Math.max(0, screenGeometry.y + screenGeometry.height - dockRect.y)
                return Math.min(visibleHeight, cap)
            }
            case 3: { // DOCK_LEFT
                let visibleWidth = Math.max(0, dockRect.x + dockRect.width - screenGeometry.x)
                return Math.min(visibleWidth, cap)
            }
            return 0
        }
    }

    
    // visible: true
    visible: Panel.visible
    flags: Qt.Tool

    property int contentPadding: 10
    width: NotifyStyle.contentItem.width + contentPadding * 2
    // height: 800
    DLayerShellWindow.layer: DLayerShellWindow.LayerOverlay
    DLayerShellWindow.anchors: DLayerShellWindow.AnchorRight | DLayerShellWindow.AnchorTop | DLayerShellWindow.AnchorBottom
    DLayerShellWindow.topMargin: dockReserve(0) + contentPadding
    DLayerShellWindow.rightMargin: dockReserve(1) + contentPadding
    DLayerShellWindow.bottomMargin: dockReserve(2) + contentPadding
    DLayerShellWindow.exclusionZone: -1
    DLayerShellWindow.keyboardInteractivity: DLayerShellWindow.KeyboardInteractivityOnDemand
    palette: DTK.palette
    ColorSelector.family: Palette.CrystalColor
    // DWindow.windowEffect: PlatformHandle.EffectNoBorder | PlatformHandle.EffectNoShadow
    DWindow.windowRadius: DTK.platformTheme.windowRadius
    DWindow.enableSystemResize: false
    DWindow.enableSystemMove: false
    DWindow.enabled: true
    color: "transparent"
    DWindow.enableBlurWindow: true
    DWindow.borderColor: DTK.themeType === ApplicationHelper.DarkType ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(0, 0, 0, 0.06)

    // 跟随任务栏所在屏幕；取不到时退回首个屏幕
    screen: (Panel.dock && Panel.dock.screen) ? Panel.dock.screen : Qt.application.screens[0]

    onVisibleChanged: function (v) {
        if (v) {
            requestActivate()
        }
    }

    onActiveChanged: function () {
        if (!root.active) {
            Panel.close()
        }
    }

    // close Panel when click dock.
    Connections {
        target: DS.applet("org.deepin.ds.dock")
        function onRequestClosePopup() {
            Panel.close()
        }
    }


    function blendColorAlpha(fallback) {
        var appearance = DS.applet("org.deepin.ds.dde-appearance")
        if (!appearance || appearance.opacity < 0)
            return fallback
        return Math.max(appearance.opacity, 0.4)
    }

    // only add blendColor effect when DWindow.enableBlurWindow is true,
    // avoid to updating blur area frequently.--
    StyledBehindWindowBlur {
        InsideBoxBorder {
            anchors.fill: parent
            radius: DTK.platformTheme.windowRadius
            color: DTK.themeType === ApplicationHelper.DarkType ?
                Qt.rgba(1, 1, 1, 0.1) :
                Qt.rgba(0, 0, 0, 0.1)
        }
        control: parent
        anchors.fill: parent
        cornerRadius: 0
        blendColor: {
            if (valid) {
                return DStyle.Style.control.selectColor(undefined,
                                                    Qt.rgba(238 / 255.0, 238 / 255.0, 238 / 255.0, blendColorAlpha(0.8)),
                                                    Qt.rgba(20 / 255, 20 / 255, 20 /255, blendColorAlpha(0.8)))
            }
            return DStyle.Style.control.selectColor(undefined,
                                                DStyle.Style.behindWindowBlur.lightNoBlurColor,
                                                DStyle.Style.behindWindowBlur.darkNoBlurColor)
        }
    }

    Item {
        id: view
        // clear focus when NotificationCenter is closed.
        focus: root.visible
        anchors {
            top: parent.top
            topMargin: contentPadding
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        NotifyStaging {
            id: notifyStaging
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            // Tab navigation: staging -> header
            onGotoHeaderFirst: notifyCenter.focusHeaderFirst()
            onGotoHeaderLast: notifyCenter.focusHeaderLast()
            Connections {
                target: Panel
                function onVisibleChanged() {
                    if (Panel.visible) {
                        notifyStaging.model.open()
                        DS.singleShot(100, function() {
                            notifyCenter.viewPanelShown = true
                        })
                    } else {
                        notifyStaging.model.close()
                        notifyCenter.viewPanelShown = false
                    }
                }
            }
        }

        NotifyCenter {
            id: notifyCenter
            anchors {
                top: notifyStaging.bottom
                topMargin: notifyStaging.viewCount > 0 ? contentPadding : 0
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            // Shift+Tab navigation: header -> staging (or view last item if no staging)
            onGotoStagingLast: {
                if (notifyStaging.viewCount > 0) {
                    notifyStaging.focusLastButton()
                } else if (notifyCenter.viewCount > 0) {
                    notifyCenter.focusViewLastItem()
                } else {
                    notifyCenter.focusHeaderLast()
                }
            }
            // Tab cycle: view last item -> staging first item
            onGotoStagingFirst: {
                if (notifyStaging.viewCount > 0) {
                    notifyStaging.focusFirstItem()
                } else {
                    notifyCenter.focusHeaderFirst()
                }
            }

            Connections {
                target: Panel
                function onVisibleChanged() {
                    if (Panel.visible) {
                        notifyCenter.model.open()
                        DS.singleShot(100, function() {
                            notifyCenter.viewPanelShown = true
                        })
                    } else {
                        notifyCenter.model.close()
                        notifyCenter.viewPanelShown = false
                    }
                }
            }

            maxViewHeight: root.height
            stagingViewCount: notifyStaging.viewCount
        }
    }
}
