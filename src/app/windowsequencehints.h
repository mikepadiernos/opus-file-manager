// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"
#include "shortcuthints.h"

#include <QObject>
#include <QPointer>

#include <functional>

class KXmlGuiWindow;
class Pane;
class QTimer;
class ShortcutHintPopup;

/**
 * Shows the shortcut hint popup while the first keys of a window's multi-key
 * shortcut, such as Ctrl+K of Ctrl+K, Ctrl+T, wait for the rest: Qt matches
 * such shortcuts itself, so ShortcutHints::WindowSequenceTracker infers
 * from the application's key events when one is pending.
 *
 * Qt takes every key while a sequence is pending, so the popup's pages turn
 * only with the mouse wheel; Escape, or any key that doesn't fit, ends the
 * sequence as Qt always does. Moving the focus cancels it too, and so does
 * the key sequence timeout while the popup doesn't show.
 */
class WindowSequenceHints : public QObject
{
    Q_OBJECT

public:
    /// Watches the keys typed in @p window, showing hints over the pane @p pane returns.
    WindowSequenceHints(KXmlGuiWindow *window, std::function<Pane *()> pane);
    ~WindowSequenceHints() override;

    bool isPending() const;
    /// The keys typed so far of the pending sequence.
    QList<QKeyCombination> pending() const;
    /// Ends the pending sequence, in Qt's shortcut map too, so the next key starts afresh.
    void cancel();
    /**
     * Applies the shortcut hint settings: whether pending sequences show
     * hints, after which delay, and how long a sequence waits for its next
     * key before it is cancelled.
     */
    void reloadSettings();

    /**
     * The shortcuts of @p window's enabled actions that work with the
     * keyboard focus in @p context, as hint entries grouped by the menu
     * that holds their action.
     */
    static QList<ShortcutHints::Entry> entries(KXmlGuiWindow *window, KeyBindings::Context context);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// The shortcuts of the window's enabled actions with more than one key.
    QList<QKeySequence> sequenceShortcuts() const;
    void settle();
    void update();
    void showHints();
    bool inWindow(const QObject *object) const;

    KXmlGuiWindow *m_window;
    std::function<Pane *()> m_pane;
    ShortcutHints::WindowSequenceTracker m_tracker;
    QTimer *m_settle;
    QTimer *m_delay;
    QTimer *m_timeout;
    bool m_hintsEnabled = true;
    QPointer<ShortcutHintPopup> m_popup;
    bool m_cancelling = false;
};
