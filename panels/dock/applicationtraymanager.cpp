// SPDX-FileCopyrightText: 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationtraymanager.h"
#include <QDBusReply>
#include <QDBusInterface>
#include <QDebug>
#include <QTimer>
#include <QIcon>
#include <QPixmap>

ApplicationTrayManager::ApplicationTrayManager(QObject *parent)
    : QObject(parent)
{
}

ApplicationTrayManager::~ApplicationTrayManager()
{
    qDeleteAll(m_appInterfaces);
    m_appInterfaces.clear();
}

void ApplicationTrayManager::initialize()
{
    qDebug() << "[AppTray] Initializing SNI (StatusNotifierItem) manager...";

    // Connect to org.kde.StatusNotifierWatcher
    m_sniWatcher = new QDBusInterface(
        QStringLiteral("org.kde.StatusNotifierWatcher"),
        QStringLiteral("/StatusNotifierWatcher"),
        QStringLiteral("org.kde.StatusNotifierWatcher"),
        QDBusConnection::sessionBus(), this);

    if (!m_sniWatcher->isValid()) {
        qWarning() << "[AppTray] StatusNotifierWatcher not available, trying to start...";
        // Some systems may not have it running, try alternative
        return;
    }

    // Listen for registration/unregistration signals
    connect(m_sniWatcher, SIGNAL(StatusNotifierItemRegistered(QString)),
            this, SLOT(onSniRegistered(QString)));
    connect(m_sniWatcher, SIGNAL(StatusNotifierItemUnregistered(QString)),
            this, SLOT(onSniUnregistered(QString)));

    // Initial check for already registered items
    QTimer::singleShot(500, this, &ApplicationTrayManager::checkRegisteredItems);

    qDebug() << "[AppTray] SNI manager initialized successfully";
}

void ApplicationTrayManager::checkRegisteredItems()
{
    if (!m_sniWatcher || !m_sniWatcher->isValid()) return;

    // Get list of currently registered SNI items
    QDBusReply<QStringList> reply = m_sniWatcher->call(QStringLiteral("registeredStatusNotifierItems"));
    if (reply.isValid()) {
        QStringList items = reply.value();
        qDebug() << "[AppTray] Found" << items.size() << "already registered items:";

        for (const QString &item : items) {
            registerApp(item);
        }
    } else {
        qWarning() << "[AppTray] Failed to get registered items:" << reply.error().message();
    }
}

void ApplicationTrayManager::onSniRegistered(const QString &servicePath)
{
    qDebug() << "[AppTray] New SNI item registered:" << servicePath;
    registerApp(servicePath);
}

void ApplicationTrayManager::onSniUnregistered(const QString &servicePath)
{
    qDebug() << "[AppTray] SNI item unregistered:" << servicePath;
    unregisterApp(servicePath);
}

QString ApplicationTrayManager::makeSurfaceId(const QString &servicePath) const
{
    // Convert "org.xxx/Path" to "app::org.xxx/Path"
    return QStringLiteral("app::") + servicePath;
}

void ApplicationTrayManager::registerApp(const QString &servicePath)
{
    QString surfaceId = makeSurfaceId(servicePath);

    if (m_apps.contains(surfaceId)) {
        qDebug() << "[AppTray] App already registered:" << surfaceId;
        return;
    }

    // Parse service and path from "service/path"
    QStringList parts = servicePath.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString service = parts.takeFirst();
    QString path = QStringLiteral("/") + parts.join(QStringLiteral("/"));
    if (path == QStringLiteral("/")) path = QStringLiteral("/StatusNotifierItem");

    // Create interface to this specific SNI item
    auto *iface = new QDBusInterface(
        service,
        path,
        QStringLiteral("org.kde.StatusNotifierItem"),
        QDBusConnection::sessionBus(), this);

    if (!iface->isValid()) {
        qWarning() << "[AppTray] Failed to create interface for:" << servicePath;
        delete iface;
        return;
    }
    iface->setTimeout(3000);  // 3 second timeout

    m_appInterfaces[surfaceId] = iface;

    // Fetch initial data using property interface (local, auto-deleted when function returns)
    QDBusInterface propsIface(service, path,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::sessionBus());

    ApplicationTrayItem item;
    item.surfaceId = surfaceId;
    item.service = service;
    item.path = path;
    item.visible = true;

    // Get properties via D-Bus - use async-style with immediate cleanup
    if (propsIface.isValid()) {
        QDBusReply<QVariant> titleReply = propsIface.call(QStringLiteral("Get"),
            QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Title"));
        if (titleReply.isValid()) item.title = titleReply.value().toString();

        QDBusReply<QVariant> iconReply = propsIface.call(QStringLiteral("Get"),
            QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("IconName"));
        if (iconReply.isValid()) item.iconName = iconReply.value().toString();

        QDBusReply<QVariant> statusReply = propsIface.call(QStringLiteral("Get"),
            QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Status"));
        if (statusReply.isValid()) item.status = statusReply.value().toString();

        QDBusReply<QVariant> catReply = propsIface.call(QStringLiteral("Get"),
            QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Category"));
        if (catReply.isValid()) item.category = catReply.value().toString();
    }

    m_apps[surfaceId] = item;

    // Connect to SNI property change signals using string-based syntax
    // (QDBusInterface doesn't support modern signal syntax for dynamic D-Bus signals)
    connect(iface, SIGNAL(NewIcon()), this, SLOT(handleAppIconChanged()));
    connect(iface, SIGNAL(NewAttentionIcon()), this, SLOT(handleAppAttentionChanged()));

    qDebug() << "[AppTray] Registered app:" << item.title << "(" << surfaceId << ")";

    emit appRegistered(surfaceId);
    emit appsChanged();
}

void ApplicationTrayManager::unregisterApp(const QString &servicePath)
{
    QString surfaceId = makeSurfaceId(servicePath);
    
    if (!m_apps.contains(surfaceId)) return;

    // Remove interface
    if (m_appInterfaces.contains(surfaceId)) {
        delete m_appInterfaces.take(surfaceId);
    }

    m_apps.remove(surfaceId);
    
    qDebug() << "[AppTray] Unregistered app:" << surfaceId;
    
    emit appUnregistered(surfaceId);
    emit appsChanged();
}

void ApplicationTrayManager::updateAppIcon(const QString &surfaceId)
{
    if (!m_apps.contains(surfaceId) || !m_appInterfaces.contains(surfaceId)) return;

    QDBusInterface propsIface(m_apps[surfaceId].service, m_apps[surfaceId].path,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::sessionBus());

    QDBusVariant iconVar = propsIface.call(QStringLiteral("Get"),
        QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("IconName")).arguments().at(0).value<QDBusVariant>();
    m_apps[surfaceId].iconName = iconVar.variant().toString();

    emit appUpdated(surfaceId);
}

void ApplicationTrayManager::updateAppTitle(const QString &surfaceId)
{
    if (!m_apps.contains(surfaceId) || !m_appInterfaces.contains(surfaceId)) return;

    QDBusInterface propsIface(m_apps[surfaceId].service, m_apps[surfaceId].path,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::sessionBus());

    QDBusVariant titleVar = propsIface.call(QStringLiteral("Get"),
        QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Title")).arguments().at(0).value<QDBusVariant>();
    m_apps[surfaceId].title = titleVar.variant().toString();

    emit appUpdated(surfaceId);
}

void ApplicationTrayManager::updateAppStatus(const QString &surfaceId)
{
    if (!m_apps.contains(surfaceId) || !m_appInterfaces.contains(surfaceId)) return;

    QDBusInterface propsIface(m_apps[surfaceId].service, m_apps[surfaceId].path,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::sessionBus());

    QDBusVariant statusVar = propsIface.call(QStringLiteral("Get"),
        QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Status")).arguments().at(0).value<QDBusVariant>();
    m_apps[surfaceId].status = statusVar.variant().toString();

    emit appUpdated(surfaceId);
}

// ========== QML-accessible API ==========

int ApplicationTrayManager::appCount() const
{
    return m_apps.size();
}

QStringList ApplicationTrayManager::getAppIds() const
{
    return m_apps.keys();
}

QVariantMap ApplicationTrayManager::getAppData(const QString &surfaceId) const
{
    QVariantMap data;
    if (!m_apps.contains(surfaceId)) return data;

    const auto &app = m_apps[surfaceId];
    data[QStringLiteral("surfaceId")] = app.surfaceId;
    data[QStringLiteral("title")] = app.title;
    data[QStringLiteral("iconName")] = app.iconName;
    data[QStringLiteral("status")] = app.status;
    data[QStringLiteral("category")] = app.category;
    data[QStringLiteral("visible")] = app.visible;

    return data;
}

void ApplicationTrayManager::activateApp(const QString &surfaceId)
{
    if (!m_apps.contains(surfaceId) || !m_appInterfaces.contains(surfaceId)) return;

    // Call Activate method on the SNI item (left-click behavior)
    m_appInterfaces[surfaceId]->call(QStringLiteral("Activate"), 0, 0);
    qDebug() << "[AppTray] Activated app:" << surfaceId;
}

void ApplicationTrayManager::contextMenuApp(const QString &surfaceId)
{
    if (!m_apps.contains(surfaceId) || !m_appInterfaces.contains(surfaceId)) return;

    // Call ContextMenu method on the SNI item (right-click behavior)
    m_appInterfaces[surfaceId]->call(QStringLiteral("ContextMenu"), 0, 0);
    qDebug() << "[AppTray] Context menu requested for:" << surfaceId;
}

void ApplicationTrayManager::handleAppIconChanged()
{
    // Find which app sent this signal and update it
    auto *iface = qobject_cast<QDBusInterface *>(sender());
    if (!iface) return;

    QString servicePath = iface->path();
    QString surfaceId = makeSurfaceId(servicePath);
    updateAppIcon(surfaceId);
    emit appUpdated(surfaceId);
}

void ApplicationTrayManager::handleAppAttentionChanged()
{
    auto *iface = qobject_cast<QDBusInterface *>(sender());
    if (!iface) return;

    QString servicePath = iface->path();
    QString surfaceId = makeSurfaceId(servicePath);
    updateAppStatus(surfaceId);  // Update attention state
    emit appUpdated(surfaceId);
}
