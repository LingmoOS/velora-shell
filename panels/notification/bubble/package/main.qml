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
    // 这里读 Applet.dock（DockProxy，查 daemon.Dock1 的 FrontendWindowRect /
    // Dock1 的 geometry）。
    //
    // 判据刻意做得“无状态”：不再依赖 dock.position 或屏幕对象的同一性，
    // 直接拿任务栏矩形与当前屏幕矩形求交集 —— 交界窄的那一维就是要预留的边距。
    // 这样任务栏在上下左右任意一边、压在两屏交界处、或几何晚到时都不会静默返回 0。
    function dockReserve() {
        let dock = Applet.dock
        if (!dock || !dock.available)
            return 0

        let d = dock.rect
        if (!d || d.width <= 0 || d.height <= 0)
            return 0

        let s = Qt.rect(root.screen.virtualX, root.screen.virtualY,
                        root.screen.width, root.screen.height)

        let overlapX = Math.max(0, Math.min(d.x + d.width, s.x + s.width) - Math.max(d.x, s.x))
        let overlapY = Math.max(0, Math.min(d.y + d.height, s.y + s.height) - Math.max(d.y, s.y))
        if (overlapX <= 0 || overlapY <= 0)
            return 0

        // 横向铺满、纵向很窄 => 任务栏在上下边，按纵向重叠量预留；反之同理
        let reserve = (overlapX >= overlapY) ? overlapY : overlapX

        // HideState == Hide(2)：任务栏已滑出屏幕，按完整尺寸预留，
        // 避免它弹出时压住通知
        if (dock.hideState === 2)
            reserve = Math.max(reserve, dock.dockSize)

        return reserve
    }

    visible: Applet.visible
    width: 390
    height: root.screen.height
    DLayerShellWindow.layer: DLayerShellWindow.LayerOverlay
    DLayerShellWindow.anchors: DLayerShellWindow.AnchorBottom | DLayerShellWindow.AnchorRight
    // 避让改由 bubbleView 的 bottomMargin 完成，见 dockReserve()
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
            // 直接偏移列表本身来避开任务栏：比塞给 layer-shell 的 margin 稳，
            // 整屏高度的表面上 margin 的语义容易被合成器改写。
            bottomMargin: 10 + dockReserve()
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
