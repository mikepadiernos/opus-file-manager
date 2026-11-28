// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"
#include "shortcuthints.h"

#include <QObject>
#include <QPointer>

#include <functional>

class KXmlGuiWindow;
class Pane;
class QKeyEvent;
class ShortcutHintPopup;

/**
 * The on-demand shortcut reference, Help › Shortcut Hints: the shortcut hint
 * popup, over the active pane, listing the keys that work where the keyboard
 * focus is. That is Yazi mode's keys in Yazi mode, the Vim-style navigation
 * keys in a file view that takes them, then the window shortcuts that the
 * focus doesn't keep.
 *
 * The popup never takes the focus. While it shows in a file view or the
 * folder tree, Escape closes it and Page Up, Page Down and the arrow keys
 * turn its pages without reaching the view. Elsewhere, as in a text field or
 * the terminal, keys always reach the focus widget. Any other key closes it,
 * and still does what it does, as does moving the focus.
 */
class ShortcutReference : public QObject
{
    Q_OBJECT

public:
    /// Shows the reference for @p window over the pane @p pane returns.
    ShortcutReference(KXmlGuiWindow *window, std::function<Pane *()> pane);
    ~ShortcutReference() override;

    /// Shows the reference, or hides it if it shows.
    void toggle();
    void show();
    void hide();
    bool isShowing() const;

    /**
     * The hints the reference lists for @p pane with the keyboard focus in
     * @p focus, which may be outside the pane, and its title in @p title.
     */
    static QList<ShortcutHints::Hint> hints(KXmlGuiWindow *window, Pane *pane, QWidget *focus, QString *title = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Whether the reference's keys are kept from the focus widget, as in a file view.
    bool takesKeys() const;
    bool handleKey(QKeyEvent *key, bool press);

    KXmlGuiWindow *m_window;
    std::function<Pane *()> m_pane;
    QPointer<ShortcutHintPopup> m_popup;
    bool m_takesKeys = false;
};
