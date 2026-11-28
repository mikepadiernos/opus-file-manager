// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandpalettebar.h"

#include <KActionCollection>
#include <KHamburgerMenu>
#include <KLocalizedString>
#include <KToolBar>
#include <KXmlGuiWindow>

#include <QAction>
#include <QHideEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QTreeView>
#include <QWidgetAction>

namespace
{
struct Collector {
    QList<QAction *> actions;
    QList<CommandPalette::Candidate> candidates;
    QSet<QAction *> seen;
    QSet<QMenu *> menus;

    void addMenu(QMenu *menu, const QStringList &path, const QString &toolBar, bool enabled, bool visible)
    {
        if (!menu || menus.contains(menu)) {
            return;
        }
        menus.insert(menu);
        // Some menus are filled only when they are about to show.
        if (menu->actions().isEmpty()) {
            Q_EMIT menu->aboutToShow();
        }
        const QList<QAction *> menuActions = menu->actions();
        for (QAction *action : menuActions) {
            add(action, path, toolBar, enabled, visible);
        }
    }

    void add(QAction *action, const QStringList &path, const QString &toolBar, bool enabled, bool visible)
    {
        if (!action || action->isSeparator() || qobject_cast<KHamburgerMenu *>(action)) {
            return;
        }
        enabled = enabled && action->isEnabled();
        visible = visible && action->isVisible();
        if (QMenu *menu = action->menu<QMenu *>()) {
            const QString title = menu->title().isEmpty() ? action->text() : menu->title();
            addMenu(menu, path + QStringList{title}, toolBar, enabled, visible);
            return;
        }
        // A widget such as a search field does nothing when triggered.
        if (qobject_cast<QWidgetAction *>(action) || seen.contains(action)) {
            return;
        }
        seen.insert(action);
        actions.append(action);
        candidates.append({action->objectName(), action->text(), path, toolBar, enabled, visible});
    }
};
}

CommandPaletteBar::CommandPaletteBar(KXmlGuiWindow *window)
    : KCommandBar(window)
    , m_window(window)
    , m_focus(window->focusWidget())
{
    setObjectName(QStringLiteral("commandPalette"));
    setAccessibleName(i18nc("@label accessible name", "Command Palette"));
    if (auto *search = findChild<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly)) {
        search->setObjectName(QStringLiteral("commandPaletteSearch"));
        search->setAccessibleName(i18nc("@label accessible name", "Search actions"));
        search->setPlaceholderText(i18nc("@info:placeholder", "Search actions by name…"));
    }
    if (auto *list = findChild<QTreeView *>(QString(), Qt::FindDirectChildrenOnly)) {
        list->setObjectName(QStringLiteral("commandPaletteActions"));
        list->setAccessibleName(i18nc("@label accessible name", "Actions and their shortcuts"));
    }
}

QList<QAction *> CommandPaletteBar::collect(KXmlGuiWindow *window, QList<CommandPalette::Candidate> *candidates)
{
    Collector collector;
    if (window->menuBar()) {
        const QList<QAction *> menus = window->menuBar()->actions();
        for (QAction *menu : menus) {
            collector.add(menu, {}, {}, true, true);
        }
    }
    const QList<KToolBar *> toolBars = window->toolBars();
    for (KToolBar *toolBar : toolBars) {
        const QList<QAction *> toolBarActions = toolBar->actions();
        for (QAction *action : toolBarActions) {
            collector.add(action, {}, toolBar->windowTitle(), true, true);
        }
    }
    const QList<QAction *> all = window->actionCollection()->actions();
    for (QAction *action : all) {
        if (CommandPalette::isSavedCommand(action->objectName())) {
            collector.add(action, {}, {}, true, true);
        }
    }
    if (candidates) {
        *candidates = collector.candidates;
    }
    return collector.actions;
}

QList<KCommandBar::ActionGroup> CommandPaletteBar::actionGroups(KXmlGuiWindow *window, const CommandPalette::Options &options)
{
    QList<CommandPalette::Candidate> candidates;
    const QList<QAction *> actions = collect(window, &candidates);
    QList<KCommandBar::ActionGroup> result;
    for (const CommandPalette::Group &group : CommandPalette::groups(candidates, options)) {
        KCommandBar::ActionGroup actionGroup{group.name, {}};
        for (int index : group.indexes) {
            actionGroup.actions.append(actions.at(index));
        }
        result.append(actionGroup);
    }
    return result;
}

void CommandPaletteBar::open(const CommandPalette::Options &options)
{
    setActions(actionGroups(m_window, options));
}

void CommandPaletteBar::setFallbackFocus(QWidget *widget)
{
    m_fallback = widget;
}

bool CommandPaletteBar::canTakeFocus(const QWidget *widget) const
{
    return widget && widget != this && !isAncestorOf(widget) && widget->window() == window() && widget->isVisible() && widget->isEnabled()
        && widget->focusPolicy() != Qt::NoFocus;
}

void CommandPaletteBar::hideEvent(QHideEvent *event)
{
    KCommandBar::hideEvent(event);
    if (m_returned) {
        return;
    }
    m_returned = true;
    // The palette closes before the chosen action runs. Qt would move the
    // focus to the next widget, which may be in the other pane; give it back
    // to the one that had it instead. When the user clicked elsewhere, the
    // focus is already there.
    const QWidget *focus = window()->focusWidget();
    if (focus && (focus == this || isAncestorOf(focus))) {
        if (canTakeFocus(m_focus)) {
            m_focus->setFocus(Qt::OtherFocusReason);
        } else if (canTakeFocus(m_fallback)) {
            m_fallback->setFocus(Qt::OtherFocusReason);
        }
    }
    Q_EMIT focusReturned();
}
