// SPDX-FileCopyrightText: 2024-2026 Lingmo OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nativetrayitems.h"
#include "traysortordermodel.h"

#include <QDateTime>
#include <QDBusReply>
#include <QDebug>
#include <QFile>
#include <QDir>

// Singleton instance for QML
static NativeTrayItems *s_instance = nullptr;

NativeTrayItems *NativeTrayItems::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    if (!s_instance) {
        s_instance = new NativeTrayItems();
    }
    return s_instance;
}

// Plugin type constants (matching Dock QML enum)
namespace DockPluginType {
    constexpr int Tray = 0;
    constexpr int Quick = 1;
    constexpr int Fixed = 2;
}

// Section type constants (matching TraySortOrderModel)
namespace SectionType {
    const QString Fixed = QStringLiteral("fixed");
    const QString Pinned = QStringLiteral("pinned");
    const QString Collapsable = QStringLiteral("collapsable");
}

NativeTrayItems::NativeTrayItems(QObject *parent)
    : QObject(parent)
{
    setupTimers();
    setupDBusConnections();
}

NativeTrayItems::~NativeTrayItems()
{
    delete m_powerIface;
    delete m_soundIface;
    delete m_networkIface;
    delete m_bluetoothIface;
}

void NativeTrayItems::initialize()
{
    qDebug() << "[NativeTray] Initializing built-in tray items...";

    // === FIXED items (always visible, cannot drag) ===

    // 1. Date/Time - always visible in dock
    registerItem({
        QStringLiteral("datetime"),
        QStringLiteral("datetime-item-key"),
        tr("Date & Time"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/datetime.dci"),
        DockPluginType::Fixed,
        SectionType::Fixed,
        true
    });

    // 2. Power/Battery
    registerItem({
        QStringLiteral("power"),
        QStringLiteral("power-item-key"),
        tr("Power"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/power.dci"),
        DockPluginType::Fixed,
        SectionType::Fixed,
        true
    });

    // 3. Shutdown/Power menu
    registerItem({
        QStringLiteral("shutdown"),
        QStringLiteral("shutdown-item-key"),
        tr("Shutdown"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/shutdown.dci"),
        DockPluginType::Fixed,
        SectionType::Fixed,
        true
    });

    // === PINNED items (can drag to rearrange) ===

    // 4. Network status
    registerItem({
        QStringLiteral("network"),
        QStringLiteral("network-item-key"),
        tr("Network"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/network.dci"),
        DockPluginType::Tray,
        SectionType::Pinned,
        true
    });

    // 5. Sound volume
    registerItem({
        QStringLiteral("sound"),
        QStringLiteral("sound-item-key"),
        tr("Sound"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/sound.dci"),
        DockPluginType::Tray,
        SectionType::Pinned,
        true
    });

    // 6. Brightness control
    registerItem({
        QStringLiteral("brightness"),
        QStringLiteral("brightness-item-key"),
        tr("Brightness"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/brightness.dci"),
        DockPluginType::Tray,
        SectionType::Pinned,
        true
    });

    // 7. Bluetooth
    registerItem({
        QStringLiteral("bluetooth"),
        QStringLiteral("bluetooth-item-key"),
        tr("Bluetooth"),
        QStringLiteral("/usr/share/dde-dock/icons/dcc-setting/bluetooth.dci"),
        DockPluginType::Tray,
        SectionType::Pinned,
        true
    });

    // Initial data fetch
    updateDateTime();
    updatePowerStatus();
    updateSoundVolume();
    updateNetworkStatus();
    updateBrightness();
    updateBluetooth();

    qDebug() << "[NativeTray]" << m_items.size() << "built-in tray items initialized";
    emit initialized();
}

void NativeTrayItems::registerItem(const NativeTrayItem &item)
{
    m_items[item.pluginId] = item;
    qDebug() << "[NativeTray] Registered:" << item.pluginId << "-" << item.displayName;
}

void NativeTrayItems::setupTimers()
{
    // DateTime updates every second
    m_datetimeTimer = new QTimer(this);
    connect(m_datetimeTimer, &QTimer::timeout, this, &NativeTrayItems::updateDateTime);
    m_datetimeTimer->start(1000);

    // Slow updates for battery/network/etc (every 10 seconds)
    m_slowUpdateTimer = new QTimer(this);
    connect(m_slowUpdateTimer, &QTimer::timeout, this, [this]() {
        updatePowerStatus();
        updateNetworkStatus();
        updateBrightness();
    });
    m_slowUpdateTimer->start(10000);
}

void NativeTrayItems::setupDBusConnections()
{
    // UPower for battery info
    m_powerIface = new QDBusInterface(
        QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
        QStringLiteral("org.freedesktop.UPower.Device"),
        QDBusConnection::sessionBus(), this);

    if (!m_powerIface->isValid()) {
        qWarning() << "[NativeTray] UPower interface not available";
    }

    // PulseAudio for sound info (via dde-daemon)
    m_soundIface = new QDBusInterface(
        QStringLiteral("org.deepin.dde.Audio1"),
        QStringLiteral("/org/deepin/dde/Audio1"),
        QStringLiteral("org.deepin.dde.Audio1"),
        QDBusConnection::sessionBus(), this);

    if (!m_soundIface->isValid()) {
        qWarning() << "[NativeTray] Audio interface not available";
    }

    // NetworkManager for network state
    m_networkIface = new QDBusInterface(
        QStringLiteral("org.freedesktop.NetworkManager"),
        QStringLiteral("/org/freedesktop/NetworkManager"),
        QStringLiteral("org.freedesktop.NetworkManager"),
        QDBusConnection::systemBus(), this);

    if (!m_networkIface->isValid()) {
        qWarning() << "[NativeTray] NetworkManager not available";
    }

    // BlueZ for bluetooth
    m_bluetoothIface = new QDBusInterface(
        QStringLiteral("org.bluez"),
        QStringLiteral("/org/bluez/hci0"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QDBusConnection::systemBus(), this);

    if (!m_bluetoothIface->isValid()) {
        qWarning() << "[NativeTray] BlueZ not available (bluetooth may not exist)";
    }
}

// ========== Update functions ==========

void NativeTrayItems::updateDateTime()
{
    QString oldText = m_dateTimeText;
    QDateTime now = QDateTime::currentDateTime();

    // Format: HH:mm for dock display
    m_dateTimeText = now.toString(QStringLiteral("HH:mm"));

    if (oldText != m_dateTimeText) {
        emit itemUpdated(QStringLiteral("datetime"));
    }
}

void NativeTrayItems::updatePowerStatus()
{
    if (!m_powerIface || !m_powerIface->isValid()) return;

    QDBusVariant percentageVar = m_powerIface->call(QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.UPower.Device"), QStringLiteral("Percentage")).arguments().at(0).value<QDBusVariant>();
    m_batteryPercent = percentageVar.variant().toInt();

    QDBusVariant stateVar = m_powerIface->call(QStringLiteral("Get"),
        QStringLiteral("org.freedesktop.UPower.Device"), QStringLiteral("State")).arguments().at(0).value<QDBusVariant>();
    uint state = stateVar.variant().toUInt();
    m_batteryCharging = (state == 1 || state == 4);  // Charging or Fully Charged

    emit itemUpdated(QStringLiteral("power"));
}

void NativeTrayItems::updateSoundVolume()
{
    if (!m_soundIface || !m_soundIface->isValid()) return;

    QDBusReply<uint> volumeReply = m_soundIface->call(QStringLiteral("GetSinkVolume"));
    if (volumeReply.isValid()) {
        m_volumePercent = static_cast<int>(volumeReply.value());
    }

    QDBusReply<bool> muteReply = m_soundIface->call(QStringLiteral("GetSinkMute"));
    if (muteReply.isValid()) {
        m_muted = muteReply.value();
    }

    emit itemUpdated(QStringLiteral("sound"));
}

void NativeTrayItems::updateNetworkStatus()
{
    if (!m_networkIface || !m_networkIface->isValid()) return;

    QDBusReply<uint> stateReply = m_networkIface->call(QStringLiteral("state"));
    if (stateReply.isValid()) {
        uint state = stateReply.value();
        switch (state) {
            case 70: m_networkState = QStringLiteral("connecting"); break;
            case 50: m_networkState = QStringLiteral("connected"); break;
            case 30: m_networkState = QStringLiteral("disconnecting"); break;
            case 20:
            case 10: m_networkState = QStringLiteral("disconnected"); break;
            default: m_networkState = QStringLiteral("unknown"); break;
        }
    }

    emit itemUpdated(QStringLiteral("network"));
}

void NativeTrayItems::updateBrightness()
{
    // Read from sysfs (works on most Linux systems)
    QFile maxFile(QStringLiteral("/sys/class/backlight/intel_backlight/max_brightness"));
    QFile curFile(QStringLiteral("/sys/class/backlight/intel_backlight/brightness"));

    if (maxFile.open(QIODevice::ReadOnly) && curFile.open(QIODevice::ReadOnly)) {
        bool ok1 = false, ok2 = false;
        int maxValue = QString(maxFile.readAll()).trimmed().toInt(&ok1);
        int curValue = QString(curFile.readAll()).trimmed().toInt(&ok2);
        if (ok1 && ok2 && maxValue > 0) {
            m_brightnessPercent = curValue * 100 / maxValue;
        }
        maxFile.close();
        curFile.close();
    } else {
        // Try alternative backlight path
        QFile altCur(QStringLiteral("/sys/class/backlight/acpi_video0/brightness"));
        if (altCur.open(QIODevice::ReadOnly)) {
            m_brightnessPercent = QString(altCur.readAll()).trimmed().toInt();
            altCur.close();
        }
    }

    emit itemUpdated(QStringLiteral("brightness"));
}

void NativeTrayItems::updateBluetooth()
{
    if (!m_bluetoothIface || !m_bluetoothIface->isValid()) return;

    QDBusReply<QDBusVariant> poweredReply = m_bluetoothIface->call(QStringLiteral("Get"),
        QStringLiteral("org.bluez.Adapter1"), QStringLiteral("Powered"));
    if (poweredReply.isValid()) {
        m_bluetoothEnabled = poweredReply.value().variant().toBool();
    }

    emit itemUpdated(QStringLiteral("bluetooth"));
}

// ========== QML-accessible getters ==========

QString NativeTrayItems::getDisplayText(const QString &pluginId) const
{
    if (pluginId == QStringLiteral("datetime")) {
        return m_dateTimeText;
    } else if (pluginId == QStringLiteral("power")) {
        if (m_batteryCharging)
            return QString::number(m_batteryPercent) + QStringLiteral("%+");
        return QString::number(m_batteryPercent) + QStringLiteral("%");
    } else if (pluginId == QStringLiteral("sound")) {
        return m_muted ? tr("Mute") : QString::number(m_volumePercent) + QStringLiteral("%");
    } else if (pluginId == QStringLiteral("network")) {
        return m_networkState;
    } else if (pluginId == QStringLiteral("brightness")) {
        return QString::number(m_brightnessPercent) + QStringLiteral("%");
    } else if (pluginId == QStringLiteral("bluetooth")) {
        return m_bluetoothEnabled ? tr("On") : tr("Off");
    } else if (pluginId == QStringLiteral("shutdown")) {
        return QString();  // No text, just icon
    }

    return QString();
}

QString NativeTrayItems::getIconName(const QString &pluginId) const
{
    // Return themed icon names for rendering
    if (pluginId == QStringLiteral("power")) {
        return m_batteryCharging ? QStringLiteral("battery-charging")
                                 : QStringLiteral("battery-full");
    } else if (pluginId == QStringLiteral("sound")) {
        return m_muted ? QStringLiteral("audio-volume-muted")
                       : QStringLiteral("audio-volume-high");
    } else if (pluginId == QStringLiteral("network")) {
        if (m_networkState == QStringLiteral("connected"))
            return QStringLiteral("network-wireless-connected");
        else if (m_networkState == QStringLiteral("disconnected"))
            return QStringLiteral("network-offline");
        return QStringLiteral("network-wireless");
    } else if (pluginId == QStringLiteral("shutdown")) {
        return QStringLiteral("system-shutdown");
    } else if (pluginId == QStringLiteral("bluetooth")) {
        return m_bluetoothEnabled ? QStringLiteral("bluetooth-active")
                                 : QStringLiteral("bluetooth-disabled");
    } else if (pluginId == QStringLiteral("brightness")) {
        return QStringLiteral("display-brightness");
    } else if (pluginId == QStringLiteral("datetime")) {
        return QStringLiteral("x-office-calendar");  // Or clock icon
    }

    return QStringLiteral("application-x-executable");  // Fallback
}

QVariantMap NativeTrayItems::getItemData(const QString &pluginId) const
{
    QVariantMap data;
    if (!m_items.contains(pluginId)) return data;

    const auto &item = m_items[pluginId];
    data[QStringLiteral("pluginId")] = item.pluginId;
    data[QStringLiteral("itemKey")] = item.itemKey;
    data[QStringLiteral("displayName")] = item.displayName;
    data[QStringLiteral("dccIcon")] = item.dccIcon;
    data[QStringLiteral("displayText")] = getDisplayText(pluginId);
    data[QStringLiteral("iconName")] = getIconName(pluginId);
    data[QStringLiteral("visible")] = item.visible;
    data[QStringLiteral("sectionType")] = item.sectionType;

    return data;
}

}
