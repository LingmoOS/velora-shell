# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt${QT_VERSION_MAJOR} ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS DBus)
find_package(DDEApplicationManager REQUIRED)
find_package(yaml-cpp REQUIRED)

# Keep the installed API location independent from legacy package-config variables.
# The Lingmo API package installs its D-Bus XML definitions in this directory.
if(NOT DEFINED LINGMO_APPLICATION_MANAGER_DBUS_API_DIR)
    set(LINGMO_APPLICATION_MANAGER_DBUS_API_DIR
        "${CMAKE_INSTALL_FULL_DATAROOTDIR}/lingmo-application-manager")
endif()
set_source_files_properties(
    ${LINGMO_APPLICATION_MANAGER_DBUS_API_DIR}/org.desktopspec.ApplicationManager1.Application.xml
    PROPERTIES  INCLUDE api/types/am.h
                CLASSNAME Application
)

set_source_files_properties(
    ${LINGMO_APPLICATION_MANAGER_DBUS_API_DIR}/org.desktopspec.ApplicationManager1.xml
    PROPERTIES  INCLUDE api/types/am.h
                CLASSNAME ApplicationManager
)

set_source_files_properties(
    ${LINGMO_APPLICATION_MANAGER_DBUS_API_DIR}/org.desktopspec.ObjectManager1.xml
    PROPERTIES  INCLUDE api/types/am.h
                CLASSNAME ObjectManager
)

qt_add_dbus_interfaces(
    DBUS_INTERFACES
    ${LINGMO_APPLICATION_MANAGER_DBUS_API_DIR}/org.desktopspec.ApplicationManager1.Application.xml
    ${LINGMO_APPLICATION_MANAGER_DBUS_API_DIR}/org.desktopspec.ApplicationManager1.xml
    ${LINGMO_APPLICATION_MANAGER_DBUS_API_DIR}/org.desktopspec.ObjectManager1.xml
)


add_library(dde-apps SHARED ${DBUS_INTERFACES}
    amappitem.cpp
    amappitem.h
    amappitemmodel.cpp
    amappitemmodel.h
    appgroup.cpp
    appgroup.h
    appgroupmanager.cpp
    appgroupmanager.h
    appitem.cpp
    appitem.h
    appitemmodel.cpp
    appitemmodel.h
    appsapplet.cpp
    appsapplet.h
    appsdockedhelper.cpp
    appsdockedhelper.h
    appslaunchtimes.cpp
    appslaunchtimes.h
    categoryutils.cpp
    categoryutils.h
    itemspage.cpp
    itemspage.h
)

target_link_libraries(dde-apps PRIVATE
    dde-shell-frame
    Qt${QT_VERSION_MAJOR}::Concurrent
    yaml-cpp
)

ds_install_package(PACKAGE org.deepin.ds.dde-apps TARGET dde-apps)
dtk_add_config_meta_files(APPID org.deepin.dde.shell FILES configs/org.deepin.ds.dde-apps.json)
