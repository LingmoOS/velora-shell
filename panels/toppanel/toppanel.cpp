// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "toppanel.h"

#include "pluginfactory.h"

#include <QQuickWindow>
#include <DConfig>

namespace toppanel {

DCORE_USE_NAMESPACE

TopPanel::TopPanel(QObject *parent)
    : DPanel(parent)
{
    auto dconfig = DConfig::create("org.deepin.dde.shell", "org.deepin.ds.dock.layout", QString(), this);
    auto setFromMode = [this](int mode) { setProperty("visible", mode == 1); };
    setFromMode(dconfig ? dconfig->value("layoutMode", 0).toInt() : 0);
    if (dconfig) {
        connect(dconfig, &DConfig::valueChanged, this, [this, setFromMode, dconfig](const QString &key) {
            if (key == QLatin1String("layoutMode"))
                setFromMode(dconfig->value("layoutMode", 0).toInt());
        });
    }
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
