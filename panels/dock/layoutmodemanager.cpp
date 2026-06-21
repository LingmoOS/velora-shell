// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "layoutmodemanager.h"

#include <DConfig>

#include <QDebug>

DCORE_USE_NAMESPACE

namespace dock {

static LayoutModeManager *s_instance = nullptr;

LayoutModeManager *LayoutModeManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return &instance();
}

LayoutModeManager &LayoutModeManager::instance()
{
    if (!s_instance) {
        s_instance = new LayoutModeManager();
    }
    return *s_instance;
}

LayoutModeManager::LayoutModeManager(QObject *parent)
    : QObject(parent)
{
    m_mode = readMode();
    qDebug() << "[LayoutMode] Initialized, mode:" << m_mode;
}

LayoutModeManager::LayoutMode LayoutModeManager::mode() const
{
    return m_mode;
}

bool LayoutModeManager::isDockMode() const
{
    return m_mode == Dock;
}

void LayoutModeManager::setMode(LayoutMode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    saveMode();
    emit modeChanged(m_mode);
    qDebug() << "[LayoutMode] Switched to:" << m_mode;
}

void LayoutModeManager::switchMode()
{
    setMode(m_mode == Taskbar ? Dock : Taskbar);
}

void LayoutModeManager::saveMode()
{
    auto dConfig = DConfig::create("org.deepin.dde.shell", "org.deepin.ds.dock.layout");
    if (dConfig) {
        dConfig->setValue("layoutMode", static_cast<int>(m_mode));
        dConfig->deleteLater();
    }
}

LayoutModeManager::LayoutMode LayoutModeManager::readMode()
{
    auto dConfig = DConfig::create("org.deepin.dde.shell", "org.deepin.ds.dock.layout");
    if (dConfig) {
        int val = dConfig->value("layoutMode", static_cast<int>(Taskbar)).toInt();
        dConfig->deleteLater();
        if (val == Taskbar || val == Dock) {
            return static_cast<LayoutMode>(val);
        }
    }
    return Taskbar;
}

} // namespace dock
