// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"

#include <QObject>
#include <QPointer>

#include <optional>

class QWidget;

/**
 * Keeps the keys that the main window's text fields and lists use for
 * themselves away from its window shortcuts. While a text field, spin box
 * or combo box has the keyboard focus, its typing and editing keys, Tab and
 * Shift+Tab reach it rather than a shortcut, and while a list or tree has
 * the focus, so do its navigation keys. Other keys, the embedded terminal
 * and other windows are left alone.
 */
class ShortcutGuard : public QObject
{
    Q_OBJECT

public:
    /// Guards the keys of @p window's widgets, watching the application's events.
    explicit ShortcutGuard(QWidget *window);

    /// Where @p focus is, as far as the guard tells contexts apart, or nothing for widgets it leaves alone.
    static std::optional<KeyBindings::Context> focusContext(const QWidget *focus);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QWidget> m_window;
};
