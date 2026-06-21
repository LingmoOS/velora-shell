// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>

namespace dock {

class LayoutModeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LayoutMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool isDockMode READ isDockMode NOTIFY modeChanged)
    QML_ELEMENT
    QML_SINGLETON
public:
    enum LayoutMode {
        Taskbar = 0,
        Dock = 1
    };
    Q_ENUM(LayoutMode)

    static LayoutModeManager &instance();
    static LayoutModeManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    LayoutMode mode() const;
    bool isDockMode() const;

public Q_SLOTS:
    void setMode(LayoutMode mode);
    void switchMode();

signals:
    void modeChanged(LayoutMode mode);

private:
    explicit LayoutModeManager(QObject *parent = nullptr);
    void saveMode();
    LayoutMode readMode();

    LayoutMode m_mode = Taskbar;
};

} // namespace dock
