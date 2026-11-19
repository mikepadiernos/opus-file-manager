// SPDX-License-Identifier: GPL-3.0-or-later

#include "symbolicactionicons.h"

#include "symbolicicon.h"

#include <QAction>
#include <QActionEvent>

namespace
{
/// Cache key of the icon this filter last gave the action.
constexpr auto WrappedIconKey = "opusfmSymbolicIconKey";
}

bool SymbolicActionIcons::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::ActionAdded && event->type() != QEvent::ActionChanged) {
        return QObject::eventFilter(watched, event);
    }

    QAction *action = static_cast<QActionEvent *>(event)->action();
    const QIcon icon = action->icon();
    if (action->property(WrappedIconKey).toLongLong() == icon.cacheKey()) {
        return false;
    }
    const QIcon symbolic = SymbolicIcon::wrap(icon);
    // Set before setIcon(): the ActionChanged it sends must find the key.
    action->setProperty(WrappedIconKey, symbolic.cacheKey());
    if (symbolic.cacheKey() != icon.cacheKey()) {
        action->setIcon(symbolic);
    }
    return false;
}
