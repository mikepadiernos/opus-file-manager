// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"

#include <QKeySequence>
#include <QList>
#include <QString>

/**
 * The actions that move between the panes, tabs, selection, filter, preview
 * panel and embedded terminal, as Preferences › Keyboard › Navigation lists
 * them, and the extra default shortcuts its setting gives them. An extra key
 * is added only while no other action uses it, so it never takes a key from
 * one of the window's defaults or from the user's own shortcuts.
 */
namespace NavigationShortcuts
{
/// What a navigation action moves through.
enum class Area {
    Panes,
    Tabs,
    Selection,
    Filter,
    Preview,
    Terminal,
};

/// The name of @p area for the user.
QString areaText(Area area);

struct Action {
    /// The action's name, such as "focus_terminal".
    QString name;
    Area area = Area::Panes;
    /// The default shortcut the setting adds, or none.
    QKeySequence extra;

    bool operator==(const Action &other) const = default;
};

/// The navigation actions, by area, in the order Preferences lists them.
QList<Action> actions();

/// The extra key of the action named @p name, or none.
QKeySequence extraKey(const QString &name);

/**
 * The index of a binding of @p bindings other than the one at @p self whose
 * shortcuts are @p key, begin with it or begin its longer key sequence, in a
 * context the window's actions overlap, or -1 when none does.
 */
int user(const QList<KeyBindings::Binding> &bindings, int self, const QKeySequence &key);

/**
 * The default shortcuts of an action whose own defaults are @p base: @p base
 * and then @p extra, while the setting is @p enabled, @p extra isn't
 * already one of them and no other action uses it (@p taken).
 */
QList<QKeySequence> defaults(const QList<QKeySequence> &base, const QKeySequence &extra, bool enabled, bool taken);

/// What became of an action's extra key.
enum class Status {
    /// The action has no extra key.
    NoExtraKey,
    /// The setting is off.
    Off,
    /// The extra key is one of the action's defaults and its shortcuts.
    Added,
    /// The extra key is one of the action's defaults, but the user's shortcuts for it leave the key out.
    Replaced,
    /// Another action uses the key, so it isn't added.
    Taken,
};

struct State {
    Status status = Status::NoExtraKey;
    /// For a taken key, or one that would be while the setting is off, the index of the binding that uses it, if any.
    int user = -1;

    bool operator==(const State &other) const = default;
};

/// What became of @p extra for the binding at @p index of @p bindings, whose defaults include the extra keys the window added.
State state(const QList<KeyBindings::Binding> &bindings, int index, const QKeySequence &extra, bool enabled);

/// Describes @p state for the user, naming the action that took a key from @p bindings.
QString stateText(const State &state, const QList<KeyBindings::Binding> &bindings);
}
