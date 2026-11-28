// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

class KXmlGuiWindow;
class QAction;

/**
 * Keeps a window's shortcuts in the Shortcuts group of opus-file-managerrc
 * (see ShortcutStorage), which KDE's Configure Keyboard Shortcuts dialog
 * doesn't know about: it saves into the window's ui.rc file, and KXmlGui
 * gives every action its defaults again whenever it rebuilds the menus and
 * toolbars.
 */
namespace WindowShortcuts
{
/**
 * The default shortcuts of @p action as the window applies them. Switch Pane
 * leaves out Tab while Tab doesn't switch panes, so turning that setting off
 * isn't a customized shortcut.
 */
QList<QKeySequence> defaults(QAction *action);

/// The names of the window actions whose shortcuts KDE's global shortcut service can reserve.
QStringList globalActionNames();

/// A window's actions whose shortcuts can be configured and their bindings, the gallery's fixed keys, then the global shortcuts KDE has registered.
struct Bindings {
    /// The action of each binding, or null for the gallery's keys.
    QList<QAction *> actions;
    QList<KeyBindings::Binding> bindings;
};

/// The bindings of @p window, or none without a window.
Bindings bindings(KXmlGuiWindow *window);

/// The path of the local ui.rc file @p xmlFile of component @p componentName, where KXmlGui saves a window's changes.
QString uiRcPath(const QString &componentName, const QString &xmlFile);

/**
 * Moves the shortcuts KDE's editor saved in the ui.rc file at @p uiRcPath
 * into the Shortcuts group, and removes them from the file. Call it before
 * the window loads the file: KXmlGui keeps the file's contents and Configure
 * Toolbars saves them back, which would bring old shortcuts back.
 */
void migrate(const QString &uiRcPath);

/// Gives each of @p window's actions its stored shortcuts, or its defaults, then applies the extra navigation keys (see applyNavigationKeys()).
void apply(KXmlGuiWindow *window);

/// Gives @p action, added to a window after its menus were built, its stored shortcuts, if it has any.
void apply(QAction *action);

/// Stores the shortcuts of @p window's actions, then removes those KDE's editor saved from its ui.rc file.
void save(KXmlGuiWindow *window);

/// Removes the stored shortcuts of the action named @p name, which no longer exists.
void forget(const QString &name);

/**
 * Gives @p window's actions with stored shortcuts their defaults while
 * KXmlGui rebuilds the menus and toolbars, so it doesn't take the stored
 * shortcuts of an action it hasn't seen before for its defaults; apply()
 * gives them back afterwards.
 */
void prepareRebuild(KXmlGuiWindow *window);

/**
 * Gives the navigation actions of @p window (see NavigationShortcuts) their
 * extra default shortcuts while Preferences › Keyboard › Navigation turns
 * them on, leaving out each key another action uses, or their own defaults
 * otherwise, then their stored shortcuts again. Call it whenever the window's
 * shortcuts or that setting change.
 */
void applyNavigationKeys(KXmlGuiWindow *window);

/// The default shortcuts @p action has without the extra navigation keys.
QList<QKeySequence> baseDefaults(QAction *action);
}
