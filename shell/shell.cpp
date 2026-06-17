// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shell.h"
#include "treelandoutputwatcher.h"

#include <DConfig>
#include <QDBusConnection>
#include <QDBusError>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlAbstractUrlInterceptor>
#include <QWheelEvent>
#include <QProcessEnvironment>
#include <memory>
#include <qmlengine.h>

DS_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(dsLoaderLog)

class DtkInterceptor : public QObject, public QQmlAbstractUrlInterceptor
{
public:
    DtkInterceptor(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    QUrl intercept(const QUrl &path, DataType type)
    {
        if (type != DataType::QmlFile)
            return path;
        if (path.path().endsWith("overridable/InWindowBlur.qml")) {
            qDebug() << "Override dtk's InWindowBlur";
            return QStringLiteral("qrc:/shell/override/dtk/InWindowBlur.qml");
        }

        return path;
    }
};

Shell::Shell(QObject *parent)
    : QObject(parent)
{
    // Since Wayland has no concept of primaryScreen, it is necessary to rely on wayland private protocols
    // to update the client's primaryScreen information

    auto platformName = QGuiApplication::platformName();
    if (QStringLiteral("wayland") == platformName) {
        new TreelandOutputWatcher(this);
    }

    // fix: 全局滚轮方向修正，解决 KWin 自然滚动下所有组件滚动反向的问题
    qApp->installEventFilter(this);
}

void Shell::installDtkInterceptor()
{
    auto engine = DQmlEngine().engine();
    engine->addUrlInterceptor(new DtkInterceptor(this));
}

void Shell::disableQmlCache()
{
    if (qEnvironmentVariableIsEmpty("QML_DISABLE_DISK_CACHE"))
        qputenv("QML_DISABLE_DISK_CACHE", "1");
}

void Shell::setFlickableWheelDeceleration(const int &value)
{
    if (qEnvironmentVariableIsEmpty("QT_QUICK_FLICKABLE_WHEEL_DECELERATION"))
        qputenv("QT_QUICK_FLICKABLE_WHEEL_DECELERATION", QString::number(value).toLocal8Bit());
}

bool Shell::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);

        // 检测是否需要反转滚轮方向：
        // 1. Wayland 下：Qt 设置了 inverted=true 标志（KWin 自然滚动已反转 delta）
        // 2. X11+KWin 下：Qt 不设置 inverted 标志，但 xinput/kwinrc 已反转 delta
        //    通过环境变量 DDE_SHELL_INVERT_WHEEL=1 或检测 WM_NAME 为 kwin 来判断
        bool needInvert = false;

        // Wayland 路径：Qt 已标记 inverted
        if (wheelEvent->inverted()) {
            needInvert = true;
        }
        // X11 路径：通过环境变量或自动检测判断
        else {
            static const bool forceInvert = qEnvironmentVariableIsSet("DDE_SHELL_INVERT_WHEEL")
                && qEnvironmentVariableIntValue("DDE_SHELL_INVERT_WHEEL") > 0;
            if (forceInvert) {
                needInvert = true;
            } else {
                // 自动检测：X11 下检查当前窗口管理器是否为 kwin
                static const bool isKwinWM = []() -> bool {
                    if (QGuiApplication::platformName() != QStringLiteral("xcb"))
                        return false;
                    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                    QString wmName = env.value("XDG_CURRENT_DESKTOP", "");
                    return wmName.contains("KDE", Qt::CaseInsensitive)
                        || wmName.contains("KWIN", Qt::CaseInsensitive);
                }();
                if (isKwinWM) {
                    needInvert = true;
                }
            }
        }

        if (needInvert) {
            // 反转 delta 值并清除 inverted 标志，避免 Flickable 双重反转
            auto *correctedEvent = new QWheelEvent(
                wheelEvent->position(),
                wheelEvent->globalPosition(),
                -wheelEvent->pixelDelta(),
                -wheelEvent->angleDelta(),
                wheelEvent->buttons(),
                wheelEvent->modifiers(),
                wheelEvent->phase(),
                Qt::NoScrollPhase
            );
            correctedEvent->setAccepted(false);
            qApp->postEvent(obj, correctedEvent);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void Shell::dconfigsMigrate()
{
    std::unique_ptr<Dtk::Core::DConfig> dconfig(Dtk::Core::DConfig::create("org.deepin.dde.shell", "org.deepin.dde.shell"));
    auto migratedDconfigs = dconfig->value(QStringLiteral("migratedDConfigs"), QVariantHash()).toHash();

    // format appid/subpath/name
    QHash<QString, QString> dconfigMigrationLists = {
        // dock
        {"org.deepin.dde.shell//org.deepin.ds.dock", "org.deepin.ds.dock//org.deepin.ds.dock"},
        {"org.deepin.dde.shell//org.deepin.ds.dock.taskmanager", "org.deepin.ds.dock//org.deepin.ds.dock.taskmanager"},
        {"org.deepin.dde.shell//org.deepin.ds.dock.tray", "org.deepin.ds.dock//org.deepin.ds.dock.tray"},

        // tray-loader
        {"org.deepin.dde.tray-loader//org.deepin.dde.network", "org.deepin.dde.tray.network//org.deepin.dde.network"},
        {"org.deepin.dde.tray-loader//org.deepin.dde.dock", "org.deepin.dde.dock//org.deepin.dde.dock"},
        {"org.deepin.dde.tray-loader//org.deepin.dde.dock.plugin.quick-panel", "org.deepin.dde.dock//org.deepin.dde.dock.plugin.quick-panel"},
        {"org.deepin.dde.tray-loader//org.deepin.dde.dock.plugin.common", "org.deepin.dde.dock//org.deepin.dde.dock.plugin.common"},
        {"org.deepin.dde.tray-loader//org.deepin.dde.dock.plugin.power", "org.deepin.dde.dock//org.deepin.dde.dock.plugin.power"},
        {"org.deepin.dde.tray-loader//org.deepin.dde.dock.plugin.sound","org.deepin.dde.dock//org.deepin.dde.dock.plugin.sound"},

        // launchpad
        {"org.deepin.dde.shell//org.deepin.ds.launchpad","dde-launchpad//org.deepin.dde.launchpad.appsmodel"},
    };

    for (auto it = dconfigMigrationLists.constBegin(); it != dconfigMigrationLists.constEnd(); ++it) {
        auto newConf = it.key();
        auto oldConf = it.value();
        if (migratedDconfigs.contains(newConf) && (migratedDconfigs.value(newConf).toString() == oldConf)) {
            continue;
        }
        if (dconfigMigrate(newConf, oldConf)) {
            migratedDconfigs.insert(newConf, oldConf);
        }
    }

    dconfig->setValue(QStringLiteral("migratedDConfigs"), migratedDconfigs);
}

bool Shell::registerDBusService(const QString &serviceName)
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(serviceName)) {
        qCWarning(dsLoaderLog).noquote() << QStringLiteral("Failed to register the dbus service: \"%1\".").arg(serviceName) << bus.lastError().message();
        return false;
    }
    return true;
}

bool Shell::dconfigMigrate(const QString &newConf, const QString &oldConf)
{
    auto newLastIndex = newConf.lastIndexOf('/');
    auto newFirstIndex = newConf.indexOf('/');

    auto oldLastIndex = oldConf.lastIndexOf('/');
    auto oldFirstIndex = oldConf.indexOf('/');

    std::unique_ptr<Dtk::Core::DConfig> newDconfig(Dtk::Core::DConfig::create(newConf.left(newFirstIndex), newConf.mid(newLastIndex + 1), newConf.mid(newFirstIndex + 1, newLastIndex - newFirstIndex - 1)));
    std::unique_ptr<Dtk::Core::DConfig> oldDconfig(Dtk::Core::DConfig::create(oldConf.left(oldFirstIndex), oldConf.mid(oldLastIndex + 1), oldConf.mid(oldFirstIndex + 1, oldLastIndex - newFirstIndex - 1)));

    if (!newDconfig->isValid() || !oldDconfig->isValid()) {
        return false;
    }

    auto keys = newDconfig->keyList();

    for (auto key : keys) {
         // 无效/空/新配置已发生修改 的 key 无需迁移
        if (oldDconfig->isDefaultValue(key) || !newDconfig->isDefaultValue(key)) {
            qCWarning(dsLoaderLog()) << QStringLiteral("Value is default on oldConf or not default for newConf. Do not migrate dconfig from %1 to %2 for key %3").arg(oldConf).arg(newConf).arg(key);
            continue;
        }

        auto value = oldDconfig->value(key);
        if (!value.isValid()) {
            qCWarning(dsLoaderLog()) << QStringLiteral("Value is invaild. Do not migrate dconfig from %1 to %2 for key %3").arg(oldConf).arg(newConf).arg(key);
            continue;
        }

        qCInfo(dsLoaderLog()) << QStringLiteral("migrate dconfig from %1 to %2 for key %3").arg(oldConf).arg(newConf).arg(key);
        newDconfig->setValue(key, value);
    }

    return true;
}

DS_END_NAMESPACE
