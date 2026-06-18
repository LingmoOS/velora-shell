// SPDX-FileCopyrightText: 2023-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtWayland.Compositor
import Qt.labs.platform 1.1 as LP
import org.deepin.dtk 1.0 as D

import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0
import org.deepin.ds.dock.tray 1.0
import org.deepin.ds.dock.tray 1.0 as DDT
import org.deepin.ds.dock 1.0 as DockCore

AppletItem {
    id: tray

    readonly property int nextAppletSpacing: 6
    property bool useColumnLayout: Panel.rootObject.positionForAnimation % 2
    property int dockOrder: 25
    readonly property string quickpanelTrayItemPluginId: "sound"
    readonly property var filterTrayPlugins: [quickpanelTrayItemPluginId]

    implicitWidth: useColumnLayout ? Panel.rootObject.dockSize : trayContainter.implicitWidth + nextAppletSpacing
    implicitHeight: useColumnLayout ? trayContainter.implicitHeight + nextAppletSpacing: Panel.rootObject.dockSize
    Component.onCompleted: {
        Applet.trayPluginModel = Qt.binding(function () {
            return DockCompositor.trayPluginSurfaces
        })
        Applet.quickPluginModel = Qt.binding(function () {
            return DockCompositor.quickPluginSurfaces
        })
        Applet.fixedPluginModel = Qt.binding(function () {
            return DockCompositor.fixedPluginSurfaces
        })
    }

    PanelPopup {
        id: stashedPopup
        width: stashedContainer.width
        height: stashedContainer.height

        property alias dropHover: stashContainer.dropHover
        property alias stashItemDragging: stashContainer.stashItemDragging

        popupX: DockPanelPositioner.x
        popupY: DockPanelPositioner.y

        property point collapsedBtnCenterPoint: Qt.point(0, 0)

        Control {
            id: stashedContainer
            padding: 10
            contentItem: StashContainer {
                id: stashContainer
                color: "transparent"
                model: DDT.SortFilterProxyModel {
                    sourceModel: DDT.TraySortOrderModel
                    sortRoleName: "visualIndex"
                    sortOrder: Qt.AscendingOrder
                    filterRowCallback: (sourceRow, sourceParent) => {
                        let index = sourceModel.index(sourceRow, 0, sourceParent)
                        return sourceModel.data(index, DDT.TraySortOrderModel.SectionTypeRole) === "stashed" &&
                               sourceModel.data(index, DDT.TraySortOrderModel.VisibilityRole) === true
                    }
                    sortRole: DDT.TraySortOrderModel.VisualIndexRole
                }
                anchors.centerIn: parent
                onRowCountChanged: {
                    if (stashContainer.rowCount === 0 || stashContainer.columnCount === 0) {
                        stashedPopup.close()
                    }
                }
            }
        }

        Component.onCompleted: {
            DockPanelPositioner.bounding = Qt.binding(function () {
                return Qt.rect(collapsedBtnCenterPoint.x, collapsedBtnCenterPoint.y, stashedPopup.width, stashedPopup.height)
            })
        }
    }
    Connections {
        target: DDT.TraySortOrderModel
        function onActionsAlwaysVisibleChanged(val) {
            if (!val && !Panel.contextDragging && !stashedPopup.dropHover) {
                closeStashPopupTimer.start()
            }
        }
    }

    // Bug to prevent icons from returning to the application tray when the tray is already hidden, which can cause layout confusion
    Timer {
        id: closeStashPopupTimer
        interval: 10
        repeat: false
        onTriggered: {
            if (!Panel.contextDragging && !stashedPopup.dropHover) {
                stashedPopup.close()
            }
        }
    }

    TrayContainer {
        id: trayContainter
        isHorizontal: !tray.useColumnLayout
        model: DDT.TraySortOrderModel
        collapsed: DDT.TraySortOrderModel.collapsed
        trayHeight: Panel.rootObject.dockSize
        surfaceAcceptor: isTrayPluginPopup
        color: "transparent"
        Component.onCompleted: {
            DDT.TrayItemPositionManager.layoutHealthCheck(1500)
        }
    }

    function isTrayPluginPopup(surfaceId) {
        if (stashContainer.isStashPopup(surfaceId))
            return false
        if (DockCompositor.findSurfaceFromModel(DockCompositor.trayPluginSurfaces, surfaceId))
            return true
        if (DockCompositor.findSurfaceFromModel(DockCompositor.fixedPluginSurfaces, surfaceId))
            return true
        return false
    }

    Connections {
        target: DockCompositor
        function onPluginSurfacesUpdated() {
            // === Cache: Native items never change, build once ===
            if (!tray._cachedNativeItems) {
                let nativeItems = ["datetime", "power", "shutdown", "network", "sound", "brightness", "bluetooth"]
                tray._cachedNativeItems = []
                for (let j = 0; j < nativeItems.length; j++) {
                    let pluginId = nativeItems[j]
                    let surfaceId = `native::${pluginId}`
                    let sectionType, forbiddenSections

                    if (pluginId === "datetime" || pluginId === "power" || pluginId === "shutdown") {
                        sectionType = "fixed"
                        forbiddenSections = ["stashed", "collapsable", "pinned"]
                    } else {
                        sectionType = "pinned"
                        forbiddenSections = ["stashed", "fixed"]
                    }

                    tray._cachedNativeItems.push({
                        "surfaceId": surfaceId,
                        "delegateType": "native-tray-item",
                        "sectionType": sectionType,
                        "forbiddenSections": forbiddenSections,
                        "pluginFlags": 0
                    })
                }
            }

            // Build surfacesData from cached native + dynamic app + external
            let surfacesData = tray._cachedNativeItems.slice()  // Copy cached array

            // === Application tray items (WeChat, QQ, Fcitx, etc. via SNI) ===
            if (DDT.ApplicationTrayManager) {
                let appIds = DDT.ApplicationTrayManager.getAppIds()
                for (let k = 0; k < appIds.length; k++) {
                    let appSurfaceId = appIds[k]
                    let appData = DDT.ApplicationTrayManager.getAppData(appSurfaceId)
                    
                    surfacesData.push({
                        "surfaceId": appSurfaceId,
                        "delegateType": "app-tray-item",
                        "sectionType": "stashed",  // Default to stashed area
                        "forbiddenSections": ["fixed"],
                        "pluginFlags": 0,
                        "title": appData.title || "",
                        "iconName": appData.iconName || "",
                        "status": appData.status || ""
                    })
                }
            }

            // === External tray plugins (if any remain) ===
            for (let i = 0; i < DockCompositor.trayPluginSurfaces.count; i++) {
                let item = DockCompositor.trayPluginSurfaces.get(i).shellSurface
                if (filterTrayPlugins.indexOf(item.pluginId) >= 0)
                    continue;
                let surfaceId = `${item.pluginId}::${item.itemKey}`
                let forbiddenSections = ["fixed"]
                let preferredSection = item.pluginId === "application-tray" ? "stashed" : "collapsable"

                if (item.pluginSizePolicy === Dock.Custom) {
                    forbiddenSections = ["stashed", "fixed"]
                    preferredSection = "pinned"
                }

                if (item.pluginFlags & 0x1000) { // force dock.
                    forbiddenSections = ["stashed", "collapsable", "fixed"]
                    preferredSection = "pinned"
                }

                surfacesData.push({"surfaceId": surfaceId, "delegateType": "legacy-tray-plugin", "sectionType": preferredSection, "forbiddenSections": forbiddenSections, "pluginFlags": item.pluginFlags})
            }
            // actually only for datetime plugin currently
            for (let i = 0; i < DockCompositor.fixedPluginSurfaces.count; i++) {
                let item = DockCompositor.fixedPluginSurfaces.get(i).shellSurface
                let surfaceId = `${item.pluginId}::${item.itemKey}`
                let forbiddenSections = ["stashed", "collapsable", "pinned"]
                let preferredSection = "fixed"

                surfacesData.push({"surfaceId": surfaceId, "delegateType": "legacy-tray-plugin", "sectionType": preferredSection, "forbiddenSections": forbiddenSections, "pluginFlags": item.pluginFlags})
            }
            DDT.TraySortOrderModel.availableSurfaces = surfacesData
            console.log("onPluginSurfacesUpdated", surfacesData.length)
            Applet.emitPluginsChanged()
        }

        function onRequestShutdown(type) {
            var shutdown = DS.applet("org.deepin.ds.dde-shutdown")
            if (shutdown) {
                shutdown.requestShutdown(type)
            } else {
                console.warn("shutdown applet not found")
            }
        }
    }

    // Timer to refresh application tray items (WeChat, QQ may register/unregister dynamically)
    // NOTE: Disabled polling to prevent memory leak. Using signal-driven updates instead.
    // The 2-second timer was causing surfacesData to be rebuilt every 2 seconds,
    // creating ~500 objects/min that triggered model cascading updates.
    Timer {
        id: appTrayRefreshTimer
        interval: 10000  // 10 second fallback (only if signals fail)
        repeat: true
        running: false  // DISABLED by default - signal-driven is preferred
        
        onTriggered: {
            if (DDT.ApplicationTrayManager && DDT.ApplicationTrayManager.appCount() > 0) {
                DockCompositor.pluginSurfacesUpdated()
            }
        }
    }

    // Listen for application tray changes (signal-driven, NO polling)
    Connections {
        target: DDT.ApplicationTrayManager
        function onAppsChanged() {
            console.log("[AppTray] Apps changed, refreshing...")
            DockCompositor.pluginSurfacesUpdated()
        }
        
        function onAppRegistered(surfaceId) {
            console.log("[AppTray] App registered:", surfaceId)
            DockCompositor.pluginSurfacesUpdated()
        }
        
        function onAppUnregistered(surfaceId) {
            console.log("[AppTray] App unregistered:", surfaceId)
            DockCompositor.pluginSurfacesUpdated()
        }
    }

    WaylandOutput {
        compositor: DockCompositor.compositor
        window: Panel.rootObject
        sizeFollowsWindow: true
    }

    WaylandOutput {
        compositor: DockCompositor.compositor
        window: Panel.popupWindow
        sizeFollowsWindow: false
    }

    WaylandOutput {
        compositor: DockCompositor.compositor
        window: Panel.toolTipWindow
        sizeFollowsWindow: false
    }

    WaylandOutput {
        compositor: DockCompositor.compositor
        window: Panel.menuWindow
        sizeFollowsWindow: false
    }
}
