// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dsglobal.h"

#include <QObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickWindow>

DS_BEGIN_NAMESPACE

class Shell : public QObject
{
    Q_OBJECT
public:
    explicit Shell(QObject *parent = nullptr);
    void installDtkInterceptor();
    void disableQmlCache();
    void setFlickableWheelDeceleration(const int &value);
    void dconfigsMigrate();
    bool registerDBusService(const QString &serviceName);
    void showAlphaWatermark();

private:
    bool dconfigMigrate(const QString &newConf, const QString &oldConf);
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool isAlphaBuild();
    void loadTranslations();
    QPointer<QQuickWindow> m_watermarkWindow;
};

DS_END_NAMESPACE
