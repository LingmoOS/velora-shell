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
#include <QFile>
#include <QTextStream>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QRegularExpression>
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
    // Load translations for AlphaWatermark
    loadTranslations();

    // Since Wayland has no concept of primaryScreen, it is necessary to rely on wayland private protocols
    // to update the client's primaryScreen information

    auto platformName = QGuiApplication::platformName();
    if (QStringLiteral("wayland") == platformName) {
        new TreelandOutputWatcher(this);
    }

    // fix: 全局滚轮方向修正，解决 KWin 自然滚动下所有组件滚动反向的问题
    qApp->installEventFilter(this);
}

void Shell::loadTranslations()
{
    // Search for translation files in standard locations
    const QString translationDir = QStringLiteral(DATADIR "/dde-shell/translations");
    const QString locale = QLocale::system().name();

    // Try to load translation for current locale (e.g., alphawatermark_zh_CN.qm)
    QTranslator *translator = new QTranslator(this);
    if (translator->load(QStringLiteral("alphawatermark_") + locale, translationDir)) {
        qApp->installTranslator(translator);
        qDebug(dsLoaderLog()) << "Loaded AlphaWatermark translation for" << locale;
    } else {
        // Try without country code (e.g., alphawatermark_zh.qm)
        QString langCode = locale.split('_').first();
        if (translator->load(QStringLiteral("alphawatermark_") + langCode, translationDir)) {
            qApp->installTranslator(translator);
            qDebug(dsLoaderLog()) << "Loaded AlphaWatermark translation for" << langCode;
        } else {
            delete translator;
        }
    }
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
        //
        // 问题根因：X11 + KWin 环境下，libinput 自然滚动开启时，
        //   X Server 发送反转后的 delta 给应用，但 Qt X11 平台插件不设置 inverted 标志，
        //   导致所有 Flickable/ListView/ScrollView 滚动方向相反。
        //
        // 检测优先级（由高到低）：
        //   1. DDE_SHELL_INVERT_WHEEL=1 环境变量（手动强制）
        //   2. wheelEvent->inverted()（Qt Wayland 平台已标记）
        //   3. 自动检测：X11 + KWin（通过 XDG_CURRENT_DESKTOP 或 KDE_SESSION_VERSION）

        static const bool forceInvert = qEnvironmentVariableIsSet("DDE_SHELL_INVERT_WHEEL")
            && qEnvironmentVariableIntValue("DDE_SHELL_INVERT_WHEEL") > 0;

        bool needInvert = forceInvert;

        if (!needInvert && wheelEvent->inverted()) {
            needInvert = true;
        }

        if (!needInvert) {
            // 自动检测 X11 + KWin 环境
            static const bool autoDetected = []() -> bool {
                if (QGuiApplication::platformName() != QStringLiteral("xcb"))
                    return false;
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                QString desktop = env.value("XDG_CURRENT_DESKTOP", "");
                if (desktop.contains("KDE", Qt::CaseInsensitive)
                    || desktop.contains("KWIN", Qt::CaseInsensitive))
                    return true;
                // 也检查 KDE_SESSION_VERSION（KDE Plasma 会设置此变量）
                return env.contains("KDE_SESSION_VERSION");
            }();
            needInvert = autoDetected;
        }

        if (needInvert) {
            // 反转 delta 值，补偿 libinput 自然滚动导致的反转
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

bool Shell::isAlphaBuild()
{
    // Check /system/release for TYPE=Alpha
    QFile releaseFile(QStringLiteral("/system/release"));
    if (releaseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&releaseFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(QLatin1String("TYPE="))) {
                QString type = line.mid(5).remove('"');
                releaseFile.close();
                return type.compare(QLatin1String("Alpha"), Qt::CaseInsensitive) == 0;
            }
        }
        releaseFile.close();
    }

    // Fallback: check /system/.version for alpha pattern (e.g., 26a01)
    QFile versionFile(QStringLiteral("/system/.version"));
    if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString version = QTextStream(&versionFile).readAll().trimmed();
        versionFile.close();
        // Match pattern like 26a01, 27a03, etc. (contains 'a' but not 'b' or 'R')
        QRegularExpression alphaPattern(QRegularExpression::anchoredPattern("\\d+a\\d+"));
        return alphaPattern.match(version).hasMatch();
    }

    return false;
}

void Shell::showAlphaWatermark()
{
    if (!isAlphaBuild()) {
        qDebug() << "Not an Alpha build, skipping watermark";
        return;
    }

    qDebug() << "Alpha build detected, showing watermark";

    QQmlComponent component(DQmlEngine().engine());
    component.loadUrl(QUrl(QStringLiteral("qrc:/shell/AlphaWatermark.qml")));

    if (component.isError()) {
        qCWarning(dsLoaderLog()) << "Failed to load AlphaWatermark.qml:" << component.errors();
        return;
    }

    QObject *object = component.create();
    if (auto *window = qobject_cast<QQuickWindow *>(object)) {
        m_watermarkWindow = window;
        window->show();
    } else {
        qCWarning(dsLoaderLog()) << "AlphaWatermark root object is not a window";
        delete object;
    }
}

DS_END_NAMESPACE
