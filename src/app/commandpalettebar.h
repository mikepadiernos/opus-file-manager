// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "commandpalette.h"

#include <KCommandBar>

#include <QList>
#include <QPointer>

class KXmlGuiWindow;
class QAction;

/**
 * The command palette: KDE's command bar listing a window's menu, toolbar and
 * saved-command actions. When it closes with the keyboard focus, the focus
 * goes back to the widget that had it before the palette opened, so that the
 * chosen action runs where the user was. It deletes itself once closed, and
 * KDE's command bar then saves the recently used actions.
 */
class CommandPaletteBar : public KCommandBar
{
    Q_OBJECT

public:
    /// Opens over @p window, remembering the widget that has the keyboard focus.
    explicit CommandPaletteBar(KXmlGuiWindow *window);

    /**
     * The actions of @p window's menus, then of its toolbars, then its saved
     * commands, each once, with what the palette needs to know about them in
     * @p candidates.
     */
    static QList<QAction *> collect(KXmlGuiWindow *window, QList<CommandPalette::Candidate> *candidates);

    /// The groups of actions the palette lists for @p window.
    static QList<KCommandBar::ActionGroup> actionGroups(KXmlGuiWindow *window, const CommandPalette::Options &options = {});

    /// Lists @p window's actions and shows the palette.
    void open(const CommandPalette::Options &options = {});

    /// Where the focus goes on closing when the widget that had it is gone.
    void setFallbackFocus(QWidget *widget);

Q_SIGNALS:
    /// The palette closed and, when it had the focus, gave it back; a chosen action runs after this.
    void focusReturned();

protected:
    void hideEvent(QHideEvent *event) override;

private:
    bool canTakeFocus(const QWidget *widget) const;

    KXmlGuiWindow *m_window;
    QPointer<QWidget> m_focus;
    QPointer<QWidget> m_fallback;
    bool m_returned = false;
};
