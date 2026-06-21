// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "toppanel.h"

#include "pluginfactory.h"
#include "layoutmodemanager.h"

#include <QQuickWindow>

namespace toppanel {

TopPanel::TopPanel(QObject *parent)
    : DPanel(parent)
{
    setProperty("visible", dock::LayoutModeManager::instance().isDockMode());
    connect(&dock::LayoutModeManager::instance(), &dock::LayoutModeManager::modeChanged,
            this, [this](dock::LayoutMode mode) {
        setProperty("visible", mode == dock::LayoutMode::Dock);
    });
}

bool TopPanel::load()
{
    DPanel::load();
    return true;
}

bool TopPanel::init()
{
    DPanel::init();

    connect(this, &DApplet::rootObjectChanged, this, [this]() {
        if (rootObject()) {
            QMetaObject::invokeMethod(this, [this]() {
                auto win = window();
                if (win)
                    win->setFlags(win->flags() | Qt::WindowDoesNotAcceptFocus);
            }, Qt::QueuedConnection);
        }
    });

    return true;
}

D_APPLET_CLASS(TopPanel)

} // namespace toppanel

#include "toppanel.moc"
