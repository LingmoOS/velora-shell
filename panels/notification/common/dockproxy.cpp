// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dockproxy.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

namespace notification {

static const QString DockService = QStringLiteral("org.deepin.dde.Dock1");
static const QString DockPath = QStringLiteral("/org/deepin/dde/Dock1");
static const QString DockInterface = QStringLiteral("org.deepin.dde.Dock1");

static const QString DockDaemonService = QStringLiteral("org.deepin.dde.daemon.Dock1");
static const QString DockDaemonPath = QStringLiteral("/org/deepin/dde/daemon/Dock1");
static const QString DockDaemonInterface = QStringLiteral("org.deepin.dde.daemon.Dock1");

DockProxy::DockProxy(QObject *parent)
    : QObject(parent)
    , m_watcher(new QDBusServiceWatcher(DockDaemonService, QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForOwnerChange, this))
{
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &DockProxy::refresh);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &DockProxy::refresh);

    // dock 侧没有几何变更信号，轮询兜底（只是 4 次属性读取，代价很低）
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &DockProxy::refresh);
    m_pollTimer->start();

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(DockService, DockPath, DockInterface,
                QStringLiteral("geometryChanged"), this, SLOT(refresh()));
    for (const auto &signal : { QStringLiteral("PositionChanged"),
                                QStringLiteral("DisplayModeChanged"),
                                QStringLiteral("WindowSizeEfficientChanged"),
                                QStringLiteral("WindowSizeFashionChanged"),
                                QStringLiteral("HideStateChanged") }) {
        bus.connect(DockDaemonService, DockDaemonPath, DockDaemonInterface,
                    signal, this, SLOT(refresh()));
    }

    refresh();
}

DockProxy::~DockProxy() = default;

/**
 * 任务栏没运行时绝不能去读它的属性：该服务带 D-Bus 自动激活，读属性会把它提前
 * 拉起来（旧 dde-osd 的 BubbleManager 里对同一问题有明确注释）。
 * isServiceRegistered 只是向总线查 NameHasOwner，不会激活服务。
 */
void DockProxy::refresh()
{
    bool running = false;
    if (QDBusConnectionInterface *busIface = QDBusConnection::sessionBus().interface())
        running = busIface->isServiceRegistered(DockDaemonService).value();

    // QDBusInterface 的构造会向远端做一次 Introspect。任务栏没起来时不能建，
    // 否则可能把它提前拉起来（旧 dde-osd 的 BubbleManager 对此有明确警告）。
    if (!running) {
        delete m_dockInter;
        m_dockInter = nullptr;
        delete m_daemonInter;
        m_daemonInter = nullptr;
    } else {
        if (!m_daemonInter) {
            m_daemonInter = new QDBusInterface(DockDaemonService, DockDaemonPath,
                                               DockDaemonInterface,
                                               QDBusConnection::sessionBus(), this);
        }
        if (!m_dockInter) {
            m_dockInter = new QDBusInterface(DockService, DockPath, DockInterface,
                                             QDBusConnection::sessionBus(), this);
        }
    }

    bool available = false;
    int position = DockBottom;
    QRect rect;
    uint dockSize = 0;
    int hideState = HideStateUnknown;

    if (running && m_dockInter && m_daemonInter) {
        position = qvariant_cast<int>(m_daemonInter->property("Position"));
        hideState = qvariant_cast<int>(m_daemonInter->property("HideState"));

        const int displayMode = qvariant_cast<int>(m_daemonInter->property("DisplayMode"));
        dockSize = qvariant_cast<uint>(m_daemonInter->property(
            displayMode == 0 ? "WindowSizeFashion" : "WindowSizeEfficient"));

        // 优先用 daemon 的 FrontendWindowRect：它由桌面插件 SetFrontendWindowRect 推上来，
        // 就是任务栏当前可见的矩形（隐藏/弹出动画期间也会跟随）。取不到再退回
        // Dock1.geometry（任务栏窗口自身几何）。
        const QRect frontendRect = qvariant_cast<QRect>(m_daemonInter->property("FrontendWindowRect"));
        const QRect windowRect = qvariant_cast<QRect>(m_dockInter->property("geometry"));
        if (frontendRect.isValid() && !frontendRect.isEmpty())
            rect = frontendRect;
        else
            rect = windowRect;
        available = rect.isValid() && !rect.isEmpty();
    }

    // 按“与任务栏矩形相交面积最大”的屏幕来判定，避免任务栏压在两屏交界处时
    // 用中心点包含法找不到屏幕（那样避让会静默失效）。
    QScreen *screen = nullptr;
    if (available) {
        qint64 bestArea = 0;
        for (QScreen *s : QGuiApplication::screens()) {
            const QRect inter = s->geometry().intersected(rect);
            const qint64 area = static_cast<qint64>(inter.width()) * inter.height();
            if (area > bestArea) {
                bestArea = area;
                screen = s;
            }
        }
    }

    if (available == m_available && position == m_position && rect == m_rect
        && dockSize == m_dockSize && hideState == m_hideState && screen == m_screen) {
        return;
    }

    m_available = available;
    m_position = position;
    m_rect = rect;
    m_dockSize = dockSize;
    m_hideState = hideState;
    m_screen = screen;

    Q_EMIT changed();
}

bool DockProxy::available() const { return m_available; }
int DockProxy::position() const { return m_position; }
QRect DockProxy::rect() const { return m_rect; }
uint DockProxy::dockSize() const { return m_dockSize; }
int DockProxy::hideState() const { return m_hideState; }
QScreen *DockProxy::screen() const { return m_screen; }

}
