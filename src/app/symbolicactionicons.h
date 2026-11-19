// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

/**
 * Application-wide event filter that gives every action added to or changed
 * in a widget (menus, toolbars, context menus) a SymbolicIcon::wrap() icon.
 * Install it once on the QApplication.
 */
class SymbolicActionIcons : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

protected:
    /**
     * Rewraps the icon of the action carried by ActionAdded and
     * ActionChanged events. Never consumes an event.
     * @param watched the object receiving the event
     * @param event the event being delivered
     * @returns always false
     */
    bool eventFilter(QObject *watched, QEvent *event) override;
};
