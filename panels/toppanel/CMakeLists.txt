# SPDX-FileCopyrightText: 2026 Lingmo OS Team.
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(ds-toppanel SHARED
    toppanel.h
    toppanel.cpp
)

target_link_libraries(ds-toppanel PRIVATE
    dde-shell-frame
)

install(TARGETS ds-toppanel DESTINATION "${LIB_INSTALL_DIR}")
ds_install_package(PACKAGE org.deepin.ds.toppanel TARGET ds-toppanel)
