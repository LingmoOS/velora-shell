// SPDX-FileCopyrightText: 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// NativeTrayItems - Built-in tray items that replace external plugin processes.
//
// Instead of spawning separate processes for each dock tray plugin,
// this class provides native implementations that read system state directly.
// This eliminates IPC overhead, crash issues, and dependency on .so files.

#pragma once

#include <QObject>
#include <QTimer>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QQmlEngine>
#include <QtQmlIntegration/qqmlintegration.h>  // For QML_ELEMENT, QML_SINGLETON macros

namespace dock {

// Represents a single built-in tray item
struct NativeTrayItem {
    QString pluginId;        // e.g., "datetime", "power", "sound"
    QString itemKey;         // e.g., "datetime-item-key"
    QString displayName;     // Human-readable name
    QString dccIcon;         // Icon path for control center
    int pluginType = 0;      // Dock::Tray, Dock::Quick, or Dock::Fixed
    QString sectionType;     // "fixed", "pinned", "collapsable"
    bool visible = true;

    // Constructor for convenient initialization
    NativeTrayItem() = default;
    NativeTrayItem(const QString &id, const QString &key, const QString &name,
                   const QString &icon, int type, const QString &section, bool vis)
        : pluginId(id), itemKey(key), displayName(name), dccIcon(icon),
          pluginType(type), sectionType(section), visible(vis) {}
};

class NativeTrayItems : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit NativeTrayItems(QObject *parent = nullptr);
    ~NativeTrayItems() override;

    // QML singleton provider
    static NativeTrayItems *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Initialize all native tray items
    Q_INVOKABLE void initialize();

    // Get current value for display (called by QML delegates)
    Q_INVOKABLE QString getDisplayText(const QString &pluginId) const;
    Q_INVOKABLE QString getIconName(const QString &pluginId) const;
    Q_INVOKABLE QVariantMap getItemData(const QString &pluginId) const;

    // Returns a list of QVariantMap entries suitable for TraySortOrderModel.availableSurfaces.
    // Each entry contains: surfaceId, delegateType, sectionType, forbiddenSections, pluginFlags.
    // Surface IDs use the format "native::<pluginId>" to avoid conflicts with Wayland surfaces.
    Q_INVOKABLE QList<QVariantMap> getSurfacesData() const;

signals:
    void itemUpdated(const QString &pluginId);   // Emitted when item data changes
    void initialized();                           // Emitted when all items are registered

private slots:
    void updateDateTime();
    void updatePowerStatus();
    void updateSoundVolume();
    void updateNetworkStatus();
    void updateBrightness();
    void updateBluetooth();

private:
    void registerItem(NativeTrayItem item);  // Take by value for aggregate initialization
    void setupDBusConnections();
    void setupTimers();

    QMap<QString, NativeTrayItem> m_items;

    // Cached values for display
    QString m_dateTimeText;
    int m_batteryPercent = 0;
    bool m_batteryCharging = false;
    int m_volumePercent = 50;
    bool m_muted = false;
    QString m_networkState;      // disconnected, connected, wireless, wired
    int m_brightnessPercent = 100;
    bool m_bluetoothEnabled = false;

    // D-Bus interfaces
    QDBusInterface *m_powerIface = nullptr;
    QDBusInterface *m_soundIface = nullptr;
    QDBusInterface *m_networkIface = nullptr;
    QDBusInterface *m_bluetoothIface = nullptr;

    // Update timers
    QTimer *m_datetimeTimer = nullptr;
    QTimer *m_slowUpdateTimer = nullptr;  // For battery, network, etc.
};

} // namespace dock
