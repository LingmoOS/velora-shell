// SPDX-FileCopyrightText: 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ApplicationTrayManager - Manages third-party system tray icons via SNI protocol.
//
// This class replaces the external libapplication-tray.so plugin by directly
// listening to org.kde.StatusNotifierWatcher D-Bus interface for tray icon
// registrations from applications like WeChat, QQ, Fcitx, etc.

#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QMap>
#include <QIcon>

namespace dock {

struct ApplicationTrayItem {
    QString surfaceId;       // Format: "app::<sni-service-path>"
    QString service;         // D-Bus service name
    QString path;            // D-Bus object path
    QString title;           // Application title/name
    QString iconName;        // Icon name or path
    QString status;          // Active/Passive/Attention
    QString category;        // ApplicationStatus/Communications/Hardware/SystemTrayAgent
    QPixmap iconPixmap;      // Cached icon pixmap
    bool visible;
};

class ApplicationTrayManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ApplicationTrayManager(QObject *parent = nullptr);
    ~ApplicationTrayManager() override;

    void initialize();

    // QML-accessible API
    Q_INVOKABLE int appCount() const;
    Q_INVOKABLE QStringList getAppIds() const;
    Q_INVOKABLE QVariantMap getAppData(const QString &surfaceId) const;
    Q_INVOKABLE void activateApp(const QString &surfaceId);
    Q_INVOKABLE void contextMenuApp(const QString &surfaceId);

signals:
    void appRegistered(const QString &surfaceId);
    void appUnregistered(const QString &surfaceId);
    void appUpdated(const QString &surfaceId);
    void appsChanged();

private slots:
    void onSniRegistered(const QString &servicePath);
    void onSniUnregistered(const QString &servicePath);
    void checkRegisteredItems();
    void updateAppIcon(const QString &servicePath);
    void updateAppTitle(const QString &servicePath);
    void updateAppStatus(const QString &servicePath);

private:
    void registerApp(const QString &servicePath);
    void unregisterApp(const QString &servicePath);
    QString makeSurfaceId(const QString &servicePath) const;
    QIcon fetchIcon(const QString &service, const QString &path) const;

    // D-Bus interfaces
    QDBusInterface *m_sniWatcher = nullptr;   // org.kde.StatusNotifierWatcher
    QMap<QString, QDBusInterface*> m_appInterfaces;  // Per-app StatusNotifierItem interfaces

    // Registered applications
    QMap<QString, ApplicationTrayItem> m_apps;  // surfaceId -> item data
    QStringList m_knownServicePaths;
};

} // namespace dock
