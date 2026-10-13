// SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts

import org.deepin.ds 1.0
import org.deepin.dtk 1.0

Window {
    id: root

    // 本发行版任务栏由 lingmo-dock 提供，dde-shell 自带的 dock 面板并不编译，
    // 所以 DS.applet("org.deepin.ds.dock") 取不到任务栏，避让会静默失效。
    // 改为读 Applet.dock（DockProxy，查 org.deepin.dde.Dock1 / daemon.Dock1）。
    function dockReserve(position) {
        let dock = Applet.dock
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

    visible: Applet.visible
    width: 390
    height: root.screen.height
    DLayerShellWindow.layer: DLayerShellWindow.LayerOverlay
    DLayerShellWindow.anchors: DLayerShellWindow.AnchorBottom | DLayerShellWindow.AnchorRight
    DLayerShellWindow.topMargin: dockReserve(0)
    DLayerShellWindow.rightMargin: dockReserve(1)
    DLayerShellWindow.bottomMargin: dockReserve(2)
    DLayerShellWindow.exclusionZone: -1
    palette: DTK.palette
    ColorSelector.family: Palette.CrystalColor
    // DWindow.windowEffect: PlatformHandle.EffectNoBorder | PlatformHandle.EffectNoShadow
    color: "transparent"
    // DWindow.windowRadius: 0
    // DWindow.enabled: false
    // DWindow.enableBlurWindow: true
    // 跟随任务栏所在屏幕；取不到时退回首个屏幕
    screen: (Applet.dock && Applet.dock.screen) ? Applet.dock.screen : Qt.application.screens[0]

    ListView  {
        id: bubbleView
        width: 360
        height: contentHeight
        anchors {
            right: parent.right
            bottom: parent.bottom
            rightMargin: 10
            bottomMargin: 10
        }

        function updateInputRegion() {
            root.DLayerShellWindow.setInputRegionRect(
                Math.ceil(bubbleView.x),
                Math.ceil(bubbleView.y),
                Math.ceil(bubbleView.width), 
                Math.ceil(Math.max(10, bubbleView.contentHeight))
            )
        }
        onContentHeightChanged: updateInputRegion()
        onHeightChanged: updateInputRegion()
        onYChanged: updateInputRegion()

        spacing: 10
        model: Applet.bubbles
        interactive: false
        verticalLayoutDirection: ListView.BottomToTop
        add: Transition {
            id: addTrans
            PropertyAnimation {
                target: addTrans.ViewTransition.item
                properties: "x"
                from: addTrans.ViewTransition.item.width
                to: 0
                duration: 600
                easing.type: Easing.OutExpo
            }
        }

        addDisplaced: Transition {
            id: addDisplacedTrans
            PropertyAnimation {
                target: addDisplacedTrans.ViewTransition.item
                properties: "x"
                to: 0
                duration: 600
                easing.type: Easing.OutExpo
            }
            PropertyAnimation {
                target: addDisplacedTrans.ViewTransition.item
                properties: "y"
                duration: 600
                easing.type: Easing.OutExpo
            }
        }

        remove: Transition {
            id: removeTrans
            ParallelAnimation {
                PropertyAnimation {
                    target: removeTrans.ViewTransition.item
                    property: "opacity"
                    to: 0
                    duration: 400
                    easing.type: Easing.OutCubic
                }
            }
        }

        removeDisplaced: Transition {
            PropertyAnimation {
                properties: "opacity,y"
                duration: 400
                easing.type: Easing.OutExpo
            }
        }

        delegate: BubbleDelegate {
            maxCount: model.bubbleCount
        }

        HoverHandler {
            onPointChanged: {
                const local = point.position
                let hoveredItem = bubbleView.itemAt(local.x, local.y)
                if (hoveredItem && hoveredItem.bubble) {
                    Applet.setHoveredId(hoveredItem.bubble.id)
                } else {
                    Applet.setHoveredId(0)
                }
            }

            onHoveredChanged: {
                if (!hovered) {
                    Applet.setHoveredId(0)
                }
            }
        }
    }
}
