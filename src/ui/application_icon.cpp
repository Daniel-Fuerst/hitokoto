// SPDX-License-Identifier: GPL-3.0-or-later
#include "application_icon.hpp"

#include <QResource>

#include <mutex>

static void initializeApplicationIconResources() {
    static std::once_flag once;
    std::call_once(once, [] { Q_INIT_RESOURCE(hitokoto_icon); });
}

namespace hitokoto::ui {

QIcon applicationIcon() {
    initializeApplicationIconResources();
    return QIcon(QStringLiteral(":/icons/hitokoto.svg"));
}

}  // namespace hitokoto::ui
