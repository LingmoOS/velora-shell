// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loadtrayplugins.h"
#include "environments.h"

#include <signal.h>

#include <DConfig>

#include <QDir>
#include <QTimer>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

namespace dock {

LoadTrayPlugins::LoadTrayPlugins(QObject *parent)
    : QObject(parent)
{

}

LoadTrayPlugins::~LoadTrayPlugins()
{
    for (auto &pInfo : m_processes) {
        if (pInfo.process) {
            pInfo.process->kill();
            //pInfo.process->waitForFinished();
            pInfo.process->deleteLater();
        }
    }
}

void LoadTrayPlugins::loadDockPlugins()
{
    QString validExePath = loaderPath();
    if (validExePath.isEmpty()) {
        qWarning() << "No valid loader executable path found.";
        return;
    }

    auto pluginGroupMap = groupPlugins(allPluginPaths());
    for (auto it = pluginGroupMap.begin(); it != pluginGroupMap.end(); ++it) {
        if (it.value().isEmpty()) continue;
        qDebug() << "Load plugin:" << it.value() << " group:" << it.key();
        startProcess(validExePath, it.value(), it.key());
    }
}

void LoadTrayPlugins::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    auto *process = qobject_cast<QProcess*>(sender());
    if (!process) return;

    // 获取进程信息用于日志
    QString pluginPath = "unknown";
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        if (it->process == process) {
            pluginPath = it->pluginPath;
            break;
        }
    }

    qDebug() << "Plugin process finished:" << pluginPath
             << "exitCode:" << exitCode 
             << "exitStatus:" << (exitStatus == QProcess::CrashExit ? "CrashExit" : "NormalExit");

    // 正常退出或被终止（SIGKILL/SIGTERM），不重试
    if (exitCode == SIGKILL || exitCode == SIGTERM || exitStatus != QProcess::CrashExit) {
        // 清理已完成的进程
        for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
            if (it->process == process) {
                process->deleteLater();
                m_processes.erase(it);
                break;
            }
        }
        return;
    }

    // 崩溃重试逻辑
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        if (it->process == process) {
            if (it->retryCount < m_maxRetries) {
                it->retryCount++;
                qWarning() << "Plugin crashed, retrying (" << it->retryCount << "/" << m_maxRetries << "):" 
                          << it->pluginPath;
                
                // 延迟重启，使用指数退避策略避免频繁崩溃
                int delayMs = 1000 * it->retryCount;  // 1s, 2s, 3s, 4s, 5s
                QTimer::singleShot(delayMs, process, [this, process] {
                    if (!process) return;
                    
                    // 重启前检查进程状态
                    if (process->state() != QProcess::NotRunning) {
                        qWarning() << "Process still running, skipping restart";
                        return;
                    }
                    
                    setProcessEnv(process);
                    process->start();
                    
                    // 监控启动是否成功
                    QTimer::singleShot(3000, this, [this, process]() {
                        if (process && process->state() == QProcess::Starting) {
                            qWarning() << "Plugin startup timeout, may be stuck";
                        }
                    });
                });
            } else {
                qCritical() << "Maximum retries reached for plugin:" << it->pluginPath
                           << "Giving up after" << m_maxRetries << "attempts";
                
                // 记录失败插件到日志文件
                logFailedPlugin(it->pluginPath);
                
                process->kill();
                process->deleteLater();
                m_processes.erase(it);
            }
            break;
        }
    }
}

void LoadTrayPlugins::startProcess(const QString &loaderPath, const QString &pluginPath, const QString &groupName)
{
    // 全局异常捕获：防止任何异常导致 dock 崩溃
    try {
        startProcessInternal(loaderPath, pluginPath, groupName);
    } catch (const std::exception &e) {
        qCritical() << "Exception starting plugin:" << pluginPath << "-" << e.what();
        logFailedPlugin(pluginPath, "Exception: " + QString(e.what()));
    } catch (...) {
        qCritical() << "Unknown exception starting plugin:" << pluginPath;
        logFailedPlugin(pluginPath, "Unknown exception");
    }
}

void LoadTrayPlugins::startProcessInternal(const QString &loaderPath, const QString &pluginPath, const QString &groupName)
{
    // 验证 loader 路径
    if (!QFile::exists(loaderPath)) {
        qCritical() << "Loader executable not found:" << loaderPath;
        logFailedPlugin(pluginPath, "Loader not found: " + loaderPath);
        return;
    }

    // 验证插件路径
    if (!QFile::exists(pluginPath)) {
        qCritical() << "Plugin file not found:" << pluginPath;
        logFailedPlugin(pluginPath, "Plugin not found");
        return;
    }

    // 安全创建进程对象
    QProcess *process = nullptr;
    try {
        process = new QProcess(this);
        if (!process) {
            qCritical() << "Failed to allocate QProcess for:" << pluginPath;
            return;
        }
    } catch (const std::bad_alloc &e) {
        qCritical() << "Memory allocation failed for process:" << pluginPath << "-" << e.what();
        return;
    }
    
    // 设置进程错误处理（增强版）
    connect(process, &QProcess::errorOccurred, this, [this, pluginPath](QProcess::ProcessError error) {
        qCritical() << "Plugin process error:" << pluginPath << "-" << error;
        
        // 记录到失败日志
        logFailedPlugin(pluginPath, QString("Process error: %1").arg(error));
        
        // 特定错误处理（非致命，仅记录）
        switch (error) {
            case QProcess::FailedToStart:
                qWarning() << "  Reason: Failed to start (check permissions/dependencies)";
                break;
            case QProcess::Crashed:
                qWarning() << "  Reason: Process crashed (will auto-restart if retries available)";
                break;
            case QProcess::Timedout:
                qWarning() << "  Reason: Process timeout";
                break;
            case QProcess::WriteError:
                qWarning() << "  Reason: Write error (non-critical)";
                break;
            case QProcess::ReadError:
                qWarning() << "  Reason: Read error (non-critical)";
                break;
            default:
                qWarning() << "  Reason: Unknown error";
                break;
        }
    });

    // 安全设置环境变量
    try {
        setProcessEnv(process);
    } catch (const std::exception &e) {
        qWarning() << "Non-critical: Failed to set process env for" << pluginPath << "-" << e.what();
        // 继续执行，使用默认环境
    }

    // 安全连接信号
    try {
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &LoadTrayPlugins::handleProcessFinished);
    } catch (const std::exception &e) {
        qCritical() << "Failed to connect finished signal for:" << pluginPath << "-" << e.what();
        delete process;  // 清理资源
        return;
    }

    // 安全添加到进程列表
    ProcessInfo pInfo = { process, pluginPath, 0 };
    try {
        m_processes.append(pInfo);
    } catch (const std::exception &e) {
        qCritical() << "Failed to add process to list:" << pluginPath << "-" << e.what();
        delete process;  // 清理资源
        return;
    }
    
    // 安全设置程序参数
    try {
        process->setProgram(loaderPath);
        process->setArguments({"-p", pluginPath, "-g", groupName, "-platform", "wayland"});
    } catch (const std::exception &e) {
        qCritical() << "Failed to set program arguments for:" << pluginPath << "-" << e.what();
        m_processes.removeLast();  // 移除刚添加的项
        delete process;  // 清理资源
        return;
    }
    
    qDebug() << "Starting plugin:" << pluginPath << "group:" << groupName;
    
    // 安全启动进程
    try {
        process->start();
    } catch (const std::exception &e) {
        qCritical() << "Exception starting process for:" << pluginPath << "-" << e.what();
        m_processes.removeLast();  // 移除刚添加的项
        delete process;  // 清理资源
        logFailedPlugin(pluginPath, "Start exception: " + QString(e.what()));
        return;
    }
    
    // 检查启动是否成功（带超时）
    if (!process->waitForStarted(3000)) {
        qCritical() << "Failed to start plugin within timeout:" << pluginPath
                   << "Error:" << process->errorString();
        logFailedPlugin(pluginPath, "Start timeout: " + process->errorString());
        // 注意：不删除进程，让 handleProcessFinished 处理清理
    }
}

void LoadTrayPlugins::setProcessEnv(QProcess *process)
{
    if (!process) return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // TODO: use protocols to determine the environment instead of environment variables
    env.remove("DDE_CURRENT_COMPOSITOR");

    process->setProcessEnvironment(env);
}

QString LoadTrayPlugins::loaderPath() const
{
    QStringList execPaths;
    execPaths << qEnvironmentVariable("TRAY_LOADER_EXECUTE_PATH")
              << QString("%1/trayplugin-loader").arg(CMAKE_INSTALL_FULL_LIBEXECDIR);

    QString validExePath;
    for (const QString &execPath : execPaths) {
        if (QFile::exists(execPath)) {
            validExePath = execPath;
            break;
        }
    }

    return validExePath;
}

QStringList LoadTrayPlugins::allPluginPaths() const
{
    QStringList dirs;
    const auto pluginsPath = qEnvironmentVariable("TRAY_DEBUG_PLUGIN_PATH");
    if (!pluginsPath.isEmpty())
        dirs << pluginsPath.split(QDir::listSeparator());

    if (dirs.isEmpty())
        dirs << pluginDirs;

    QStringList pluginPaths;
    for (auto &pluginDir : dirs) {
        QDir dir(pluginDir);
        if (!dir.exists()) {
            qWarning() << "The plugin directory does not exist:" << pluginDir;
            continue;
        }

        auto pluginFileInfos = dir.entryInfoList({"*.so"}, QDir::Files);
        for (auto &pluginInfo : pluginFileInfos) {
            pluginPaths.append(pluginInfo.absoluteFilePath());
        }
    }

    return pluginPaths;
}

QMap<QString, QString> LoadTrayPlugins::groupPlugins(const QStringList &pluginPaths) const
{
    const QString selfMaintenancePluginsKey = "selfMaintenanceTrayPlugins";
    const QString subprojectPluginsKey = "subprojectTrayPlugins";
    const QString crashPronePluginsKey = "crashProneTrayPlugins";
    const QString otherPluginsKey = "otherTrayPlugins";

    auto dConfig = Dtk::Core::DConfig::create("org.deepin.dde.shell", "org.deepin.ds.dock.tray", QString());
    QStringList selfMaintenanceTrayPlugins = dConfig->value(selfMaintenancePluginsKey).toStringList();
    QStringList subprojectTrayPlugins = dConfig->value(subprojectPluginsKey).toStringList();
    QStringList crashProneTrayPlugins = dConfig->value(crashPronePluginsKey).toStringList();
    dConfig->deleteLater();

    QStringList selfMaintenancePluginPaths;
    QStringList subprojectPluginPaths;
    QStringList crashPronePluginPaths;
    QStringList otherPluginPaths;

    for (auto &filePath : pluginPaths) {
        QString pluginName = filePath.section("/", -1);
        if (crashProneTrayPlugins.contains(pluginName)) {
            crashPronePluginPaths.append(filePath);
        } else if (selfMaintenanceTrayPlugins.contains(pluginName)) {
            selfMaintenancePluginPaths.append(filePath);
        } else if (subprojectTrayPlugins.contains(pluginName)) {
            subprojectPluginPaths.append(filePath);
        } else {
            otherPluginPaths.append(filePath);
        }
    }

    QMap<QString, QString> pluginGroup;

    if (!selfMaintenancePluginPaths.isEmpty()) {
        pluginGroup.insert(selfMaintenancePluginsKey, selfMaintenancePluginPaths.join(";"));
    }

    if (!subprojectPluginPaths.isEmpty()) {
        pluginGroup.insert(subprojectPluginsKey, subprojectPluginPaths.join(";"));
    }

    if (!crashPronePluginPaths.isEmpty()) {
        pluginGroup.insert(crashPronePluginsKey, crashPronePluginPaths.join(";"));
    }

    if (!otherPluginPaths.isEmpty()) {
        pluginGroup.insert(otherPluginsKey, otherPluginPaths.join(";"));
    }

    return pluginGroup;
}

void LoadTrayPlugins::logFailedPlugin(const QString &pluginPath)
{
    // 记录失败的插件到日志文件，便于后续分析
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/failed_plugins.log";
    QFile logFile(logPath);
    
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString(Qt::ISODate) 
               << " - Failed plugin: " << pluginPath << "\n";
        logFile.close();
        qDebug() << "Logged failed plugin to:" << logPath;
    } else {
        qWarning() << "Could not open log file for writing:" << logPath;
    }
}

}
