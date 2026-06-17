// Copyright (C) 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
// Alpha Build Watermark - Only shown during Alpha stage (like Windows watermark)

import QtQuick
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "LingmoOS Alpha Watermark"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnBottomHint | Qt.Tool | Qt.WindowTransparentForInput
    color: "transparent"

    // System info properties
    property bool isAlphaBuild: false
    property string buildVersion: ""
    property string distroVersion: ""
    property string codename: ""
    property string buildDate: ""
    property string versionType: ""

    visible: isAlphaBuild

    // Position in bottom-right corner with margin
    x: Screen.width - width - 16
    y: Screen.height - height - 16

    width: watermarkContent.implicitWidth + 8
    height: watermarkContent.implicitHeight + 8

    // Prevent mouse interactions (pass-through)
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
    }

    // Watermark content - pure text, no background/border
    Column {
        id: watermarkContent
        anchors.centerIn: parent
        spacing: 2

        // Title: Development Version
        Text {
            text: qsTr("Lingmo OS Development Version")
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: "#E0E0E0"
            opacity: 0.85
        }

        // Separator
        Text {
            text: "────────────────────────────"
            font.pixelSize: 9
            color: "#606060"
            opacity: 0.5
        }

        // Version
        Text {
            text: qsTr("Version:") + " " + root.distroVersion
            font.pixelSize: 10
            color: "#C0C0C0"
            opacity: 0.8
        }

        // Codename
        Text {
            text: qsTr("Codename:") + " " + root.codename
            font.pixelSize: 10
            color: "#C0C0C0"
            opacity: 0.8
        }

        // Build Number
        Text {
            text: qsTr("Build Number:") + " " + root.buildVersion
            font.pixelSize: 10
            color: "#C0C0C0"
            opacity: 0.8
        }

        // Build Date
        Text {
            text: qsTr("Build Date:") + " " + root.buildDate
            font.pixelSize: 10
            color: "#C0C0C0"
            opacity: 0.8
        }

        // Description
        Text {
            text: qsTr("This is a pre-release version for testing and development purposes only.")
            font.pixelSize: 9
            font.italic: true
            color: "#A0A0A0"
            opacity: 0.7
            wrapMode: Text.WordWrap
            width: Math.min(Screen.width * 0.35, 380)
        }

        // Warning section
        Item {
            width: warningContent.implicitWidth
            height: warningContent.implicitHeight

            Row {
                id: warningContent
                spacing: 6

                Text {
                    text: "\u26A0"  // Warning sign
                    font.pixelSize: 12
                    color: "#FFD700"
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: 0.9
                }

                Column {
                    spacing: 1
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: qsTr("Warning: This is an unstable development build.")
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        color: "#FFD700"
                        opacity: 0.85
                    }

                    Text {
                        text: qsTr("Data loss may occur. Use at your own risk.")
                        font.pixelSize: 9
                        color: "#CCAA00"
                        opacity: 0.75
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        loadSystemRelease()
    }

    // Update position when screen changes
    Connections {
        target: Screen
        function onWidthChanged() { updatePosition() }
        function onHeightChanged() { updatePosition() }
    }

    function updatePosition() {
        x = Screen.width - width - 16
        y = Screen.height - height - 16
    }

    function loadSystemRelease() {
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    parseReleaseFile(xhr.responseText)
                }
                if (!root.isAlphaBuild) {
                    loadQuickVersion()
                }
            }
        }
        xhr.open("GET", "file:///system/release")
        xhr.send()
    }

    function parseReleaseFile(content) {
        var lines = content.split('\n')
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].trim()
            if (line.indexOf('TYPE=') === 0) {
                var type = line.substring(5).replace(/"/g, '')
                root.isAlphaBuild = (type.toLowerCase() === 'alpha')
                root.versionType = type
            } else if (line.indexOf('BUILD=') === 0) {
                root.buildVersion = line.substring(6).replace(/"/g, '')
            } else if (line.indexOf('VERSION=') === 0 && !line.indexOf('VERSION_TYPE') === 0 && !line.indexOf('VERSION_CODENAME') === 0) {
                root.distroVersion = line.substring(8).replace(/"/g, '')
            } else if (line.indexOf('CODENAME=') === 0 || line.indexOf('codename=') === 0) {
                var eqIdx = line.indexOf('=')
                root.codename = line.substring(eqIdx + 1).replace(/"/g, '')
            } else if (line.indexOf('BUILD_ID=') === 0) {
                var bidEqIdx = line.indexOf('=')
                root.buildDate = line.substring(bidEqIdx + 1).replace(/"/g, '')
            }
        }
        // Format build date if it's in YYYYMMDD format
        if (root.buildDate.length === 8 && root.buildDate.match(/^\d{8}$/)) {
            root.buildDate = root.buildDate.substring(0, 4) + "-" +
                             root.buildDate.substring(4, 6) + "-" +
                             root.buildDate.substring(6, 8)
        }
    }

    function loadQuickVersion() {
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                root.buildVersion = xhr.responseText.trim()
                if (root.buildVersion.match(/^\d+a\d+/)) {
                    root.isAlphaBuild = true
                    root.versionType = "Alpha"
                }
            }
        }
        xhr.open("GET", "file:///system/.version")
        xhr.send()
    }
}
