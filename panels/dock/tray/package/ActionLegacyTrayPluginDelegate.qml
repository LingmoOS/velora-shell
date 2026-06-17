// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWayland.Compositor
import Qt.labs.platform 1.1 as LP
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.private 1.0 as DP

import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0
import org.deepin.ds.dock.tray 1.0 as DDT

AppletItemButton {
    id: root
    property alias inputEventsEnabled: surfaceItem.inputEventsEnabled

    property size visualSize: isHorizontal ? Qt.size(pluginItem.implicitWidth, Math.min(itemHeight, pluginItem.implicitHeight))
                                           : Qt.size(Math.min(itemWidth, pluginItem.implicitWidth), pluginItem.implicitHeight)

    readonly property int itemWidth: isHorizontal ? 0 : DDT.TrayItemPositionManager.dockHeight
    readonly property int itemHeight: isHorizontal ? DDT.TrayItemPositionManager.dockHeight : 0

    required property bool itemVisible
    property bool dragable: true

    padding: 0

    visible: !Drag.active && itemVisible
    hoverEnabled: inputEventsEnabled

    function updatePluginMargins()
    {
        pluginItem.plugin.margins = itemPadding
    }

    contentItem: Item {
        id: pluginItem
        property var plugin: DockCompositor.findSurface(model.surfaceId)
        // 安全的空值检查，确保不会返回 null 导致崩溃
        property bool isValidPlugin: plugin !== null && plugin !== undefined
        implicitHeight: isValidPlugin ? plugin.height : (isHorizontal ? DDT.TrayItemPositionManager.dockHeight : itemHeight)
        implicitWidth: isValidPlugin ? plugin.width : (!isHorizontal ? DDT.TrayItemPositionManager.dockHeight : itemWidth)

        property var itemGlobalPoint: {
            if (!isValidPlugin) return Qt.point(0, 0)
            var a = pluginItem
            var x = 0, y = 0
            while(a.parent) {
                x += a.x
                y += a.y
                a = a.parent
            }

            return Qt.point(x, y)
        }
        
        property var itemGlobalPos: {
            if (!isValidPlugin) return Qt.point(0, 0)
            var a = pluginItem
            var x = 0, y = 0

            if (a.Window.window && surfaceItem.visible) {
                while (a.parent) {
                    x += a.x
                    y += a.y
                    a = a.parent
                }
                x += pluginItem.Window.window.x
                y += pluginItem.Window.window.y

            }

            return Qt.point(x, y)
        }

        HoverHandler {
            id: hoverHandler
            parent: surfaceItem
            enabled: pluginItem.isValidPlugin
        }
        TapHandler {
            id: tapHandler
            parent: surfaceItem
            enabled: pluginItem.isValidPlugin
        }

        ShellSurfaceItem {
            id: surfaceItem
            anchors.fill: parent
            shellSurface: pluginItem.isValidPlugin ? pluginItem.plugin : null
            smooth: false
            visible: pluginItem.isValidPlugin
        }

        Component.onCompleted: {
            try {
                if (!pluginItem.isValidPlugin || !itemVisible)
                    return
                updatePluginMargins()
                
                // 安全调用，防止 plugin 对象方法不存在或抛出异常
                if (pluginItem.plugin && typeof pluginItem.plugin.updatePluginGeometry === 'function') {
                    pluginItem.plugin.updatePluginGeometry(Qt.rect(pluginItem.itemGlobalPoint.x, pluginItem.itemGlobalPoint.y, 0, 0))
                }
                if (pluginItem.plugin && typeof pluginItem.plugin.setGlobalPos === 'function') {
                    pluginItem.plugin.setGlobalPos(pluginItem.itemGlobalPos)
                }
            } catch (e) {
                console.warn("Error in ActionLegacyTrayPluginDelegate.onCompleted:", e.message)
            }
        }

        Timer {
            id: updatePluginItemGeometryTimer
            interval: 200
            running: false
            repeat: false
            onTriggered: {
                try {
                    if (!pluginItem.isValidPlugin || !itemVisible)
                        return
                    updatePluginMargins()
                    
                    // 安全调用，带边界检查
                    if (pluginItem.itemGlobalPoint && 
                        pluginItem.itemGlobalPoint.x >= 0 && 
                        pluginItem.itemGlobalPoint.y >= 0 &&
                        pluginItem.plugin &&
                        typeof pluginItem.plugin.updatePluginGeometry === 'function') {
                        pluginItem.plugin.updatePluginGeometry(Qt.rect(pluginItem.itemGlobalPoint.x, pluginItem.itemGlobalPoint.y, 0, 0))
                    }
                } catch (e) {
                    console.warn("Error in updatePluginItemGeometryTimer:", e.message)
                }
            }
        }

        Timer {
            id: updatePluginItemPosTimer
            interval: 200
            running: false
            repeat: false
            onTriggered: {
                try {
                    if (!pluginItem.isValidPlugin || !itemVisible)
                        return
                    
                    // 安全调用
                    if (pluginItem.plugin && typeof pluginItem.plugin.setGlobalPos === 'function') {
                        pluginItem.plugin.setGlobalPos(pluginItem.itemGlobalPos)
                    }
                } catch (e) {
                    console.warn("Error in updatePluginItemPosTimer:", e.message)
                }
            }
        }

        onItemGlobalPointChanged: {
            updatePluginItemGeometryTimer.start()
        }

        onItemGlobalPosChanged: {
            updatePluginItemPosTimer.start()
        }

        onVisibleChanged: {
            try {
                if (!pluginItem.isValidPlugin || !itemVisible)
                    return
                updatePluginMargins()
                
                // 安全调用
                if (pluginItem.plugin && typeof pluginItem.plugin.setGlobalPos === 'function') {
                    pluginItem.plugin.setGlobalPos(pluginItem.itemGlobalPos)
                }
            } catch (e) {
                console.warn("Error in onVisibleChanged:", e.message)
            }
        }
    }

    D.ColorSelector.hovered: root.inputEventsEnabled && (pluginItem.isValidPlugin && pluginItem.plugin.isItemActive || hoverHandler.hovered)
    D.ColorSelector.pressed: tapHandler.pressed

    property Component overlayWindow: QuickDragWindow {
        height: root.visualSize.height
        width: root.visualSize.width

        Loader {
            height: parent.height
            width: parent.width
            active: root.DQuickDrag.isDragging && pluginItem.isValidPlugin
            sourceComponent: ShellSurfaceItem {
                anchors.centerIn: parent
                shellSurface: pluginItem.plugin
            }
        }
    }

    Drag.dragType: Drag.Automatic
    DQuickDrag.overlay: overlayWindow
    DQuickDrag.active: Drag.active && Qt.platform.pluginName === "xcb"
    DQuickDrag.hotSpotScale: Qt.size(0.5, 1)
    Drag.mimeData: {
        "text/x-dde-shell-tray-dnd-surfaceId": model.surfaceId,
        "text/x-dde-shell-tray-dnd-sectionType": model.sectionType
    }
    Drag.supportedActions: Qt.MoveAction
    Drag.onActiveChanged: {
        // only drag application-tray plugin can activate application-tray action
        if (model.surfaceId.startsWith("application-tray")) {
            DDT.TraySortOrderModel.actionsAlwaysVisible = Drag.active
        }

        if (Qt.platform.pluginName !== "xcb") {
            if (pluginItem.isValidPlugin) {
                root.grabToImage(function(result) {
                    root.Drag.imageSource = result.url;
                })
            }
        }

        if (!Drag.active) {
            Panel.contextDragging = false
            // reset position on drop
            Qt.callLater(() => { x = 0; y = 0; });
            return
        }
        Panel.contextDragging = true
    }

    onWidthChanged: {
        if (Qt.platform.pluginName !== "xcb" && pluginItem.isValidPlugin) {
            root.grabToImage(function(result) {
                root.Drag.imageSource = result.url;
            })
        }
    }

    DragHandler {
        id: dragHandler
        enabled: dragable
        // To avoid being continuously active in a short period of time
        onActiveChanged: {
            Qt.callLater(function() { root.Drag.active = dragHandler.active })
        }
    }
}
