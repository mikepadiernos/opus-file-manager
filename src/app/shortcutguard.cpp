// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcutguard.h"

#include "foldertree.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

ShortcutGuard::ShortcutGuard(QWidget *window)
    : QObject(window)
    , m_window(window)
{
    qApp->installEventFilter(this);
}

namespace
{
/// Whether @p widget takes typed text now; read-only text and combo boxes without a text field don't.
bool takesText(const QWidget *widget)
{
    if (const auto *line = qobject_cast<const QLineEdit *>(widget)) {
        return !line->isReadOnly();
    }
    if (const auto *text = qobject_cast<const QTextEdit *>(widget)) {
        return !text->isReadOnly();
    }
    if (const auto *text = qobject_cast<const QPlainTextEdit *>(widget)) {
        return !text->isReadOnly();
    }
    if (const auto *spin = qobject_cast<const QAbstractSpinBox *>(widget)) {
        return !spin->isReadOnly();
    }
    if (const auto *combo = qobject_cast<const QComboBox *>(widget)) {
        return combo->isEditable();
    }
    return qobject_cast<const QKeySequenceEdit *>(widget);
}
}

std::optional<KeyBindings::Context> ShortcutGuard::focusContext(const QWidget *focus)
{
    if (!focus) {
        return std::nullopt;
    }
    if (takesText(focus)) {
        return KeyBindings::Context::TextInput;
    }
    if (qobject_cast<const QAbstractItemView *>(focus)) {
        for (const QWidget *ancestor = focus->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (qobject_cast<const FolderTree *>(ancestor)) {
                return KeyBindings::Context::FolderTree;
            }
        }
        return KeyBindings::Context::FileView;
    }
    return std::nullopt;
}

bool ShortcutGuard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::ShortcutOverride || !m_window) {
        return QObject::eventFilter(watched, event);
    }
    const QWidget *focus = QApplication::focusWidget();
    if (!focus || focus != watched || focus->window() != m_window) {
        return QObject::eventFilter(watched, event);
    }
    const std::optional<KeyBindings::Context> context = focusContext(focus);
    auto *key = static_cast<QKeyEvent *>(event);
    if (context && KeyBindings::isReserved(key->keyCombination(), *context)) {
        // An accepted override sends the key to the focus widget instead of a shortcut.
        key->accept();
        return true;
    }
    return QObject::eventFilter(watched, event);
}
