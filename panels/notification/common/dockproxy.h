// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>

QT_BEGIN_NAMESPACE
class QDBusInterface;
class QDBusServiceWatcher;
class QScreen;
QT_END_NAMESPACE

namespace notification {

/**
 * @brief 任务栏几何信息。
 *
 * 本发行版的任务栏由 lingmo-dock 提供：dde-shell 自带的 dock 面板并不编译
 * （见 panels/CMakeLists.txt 里被注释掉的 add_subdirectory(dock)），打包时也没有
 * org.deepin.ds.dock，dde-shell 反而是通过 `Depends: lingmo-dock` 依赖它。
 * 因此通知面板不能再通过 DS.applet("org.deepin.ds.dock") 取任务栏几何——那个 applet
 * 不存在，DAppletBridge::applet() 会直接返回 null，导致避让逻辑静默失效、
 * 通知气泡压住任务栏。
 *
 * 这里改为直接查询 lingmo-dock 的 D-Bus 接口（与已下线的 dde-osd
 * BubbleManager::geometryChanged() 做法一致）：
 *   - org.deepin.dde.Dock1        -> geometry：任务栏实际窗口矩形（逻辑像素）
 *   - org.deepin.dde.daemon.Dock1 -> Position / WindowSize* / HideState / DisplayMode
 */
class DockProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed FINAL)
    Q_PROPERTY(int position READ position NOTIFY changed FINAL)
    Q_PROPERTY(QRect rect READ rect NOTIFY changed FINAL)
    Q_PROPERTY(uint dockSize READ dockSize NOTIFY changed FINAL)
    Q_PROPERTY(int hideState READ hideState NOTIFY changed FINAL)
    Q_PROPERTY(QScreen *screen READ screen NOTIFY changed FINAL)

public:
    // 与 panels/dock/constants.h、dock/frame/... 的 Dock::Position 保持一致
    enum DockPosition { DockTop = 0, DockRight = 1, DockBottom = 2, DockLeft = 3 };
    // 与 dock 侧 HideState 保持一致
    enum DockHideState { HideStateUnknown = 0, HideStateShow = 1, HideStateHide = 2 };

    explicit DockProxy(QObject *parent = nullptr);
    ~DockProxy() override;

    bool available() const;
    int position() const;
    QRect rect() const;
    uint dockSize() const;
    int hideState() const;
    QScreen *screen() const;

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void refresh();

private:
    QDBusInterface *m_dockInter = nullptr;
    QDBusInterface *m_daemonInter = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;

    bool m_available = false;
    int m_position = DockBottom;
    QRect m_rect;
    uint m_dockSize = 0;
    int m_hideState = HideStateUnknown;
    QPointer<QScreen> m_screen;
};

}
