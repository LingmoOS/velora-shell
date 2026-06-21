// SPDX-FileCopyrightText: 2026 Lingmo OS Team.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "panel.h"
#include "dsglobal.h"

namespace toppanel {

class TopPanel : public DS_NAMESPACE::DPanel
{
    Q_OBJECT
public:
    explicit TopPanel(QObject *parent = nullptr);
    bool load() override;
    bool init() override;
};

} // namespace toppanel
