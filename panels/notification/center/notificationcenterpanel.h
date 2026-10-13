// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dockproxy.h"
#include "panel.h"

namespace notification {

class NotificationCenterProxy;
class NotificationCenterPanel : public DS_NAMESPACE::DPanel
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(DockProxy *dock READ dock CONSTANT FINAL)
public:
    explicit NotificationCenterPanel(QObject *parent = nullptr);
    ~NotificationCenterPanel();

    virtual bool load() override;
    virtual bool init() override;

    bool visible() const;
    void setVisible(bool newVisible);
    Q_INVOKABLE void close();
    DockProxy *dock() const;

Q_SIGNALS:
    void visibleChanged();

private slots:
    void setBubblePanelEnabled(bool enabled);

private:
    bool m_visible = false;
    NotificationCenterProxy *m_proxy = nullptr;
    DockProxy *m_dock = nullptr;
};
}
