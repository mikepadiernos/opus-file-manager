// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationshortcuts.h"

#include <KLocalizedString>

namespace NavigationShortcuts
{
QString areaText(Area area)
{
    switch (area) {
    case Area::Panes:
        return i18nc("@title navigation shortcuts", "Panes");
    case Area::Tabs:
        return i18nc("@title navigation shortcuts", "Tabs");
    case Area::Selection:
        return i18nc("@title navigation shortcuts", "Selection");
    case Area::Filter:
        return i18nc("@title navigation shortcuts", "Filter");
    case Area::Preview:
        return i18nc("@title navigation shortcuts", "Preview Panel");
    case Area::Terminal:
        return i18nc("@title navigation shortcuts", "Embedded Terminal");
    }
    return {};
}

QList<Action> actions()
{
    // Keys no default action, KDE standard shortcut, Yazi mode preset, text
    // field or list uses; Ctrl+Page Down and Ctrl+Page Up are KDE's standard
    // Next Tab and Previous Tab, Ctrl+Shift+A its Deselect and Ctrl+Shift+F4
    // Dolphin's Focus Terminal Panel.
    return {
        {QStringLiteral("switch_pane"), Area::Panes, {}},
        {QStringLiteral("focus_first_pane"), Area::Panes, QKeySequence(Qt::ALT | Qt::Key_1)},
        {QStringLiteral("focus_second_pane"), Area::Panes, QKeySequence(Qt::ALT | Qt::Key_2)},
        {QStringLiteral("next_tab"), Area::Tabs, QKeySequence(Qt::CTRL | Qt::Key_PageDown)},
        {QStringLiteral("previous_tab"), Area::Tabs, QKeySequence(Qt::CTRL | Qt::Key_PageUp)},
        {QStringLiteral("activate_last_tab"), Area::Tabs, {}},
        {QStringLiteral("edit_select_all"), Area::Selection, {}},
        {QStringLiteral("deselect_all"), Area::Selection, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A)},
        {QStringLiteral("invert_selection"), Area::Selection, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I)},
        {QStringLiteral("selection_mode"), Area::Selection, {}},
        {QStringLiteral("show_filter_bar"), Area::Filter, {}},
        {QStringLiteral("clear_filter"), Area::Filter, {}},
        {QStringLiteral("show_preview_panel"), Area::Preview, {}},
        {QStringLiteral("focus_preview_panel"), Area::Preview, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F3)},
        {QStringLiteral("show_embedded_terminal"), Area::Terminal, {}},
        {QStringLiteral("focus_terminal"), Area::Terminal, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F4)},
    };
}

QKeySequence extraKey(const QString &name)
{
    for (const Action &action : actions()) {
        if (action.name == name) {
            return action.extra;
        }
    }
    return {};
}

int user(const QList<KeyBindings::Binding> &bindings, int self, const QKeySequence &key)
{
    if (key.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < bindings.size(); ++i) {
        const KeyBindings::Binding &binding = bindings.at(i);
        if (i == self || (!binding.global && !KeyBindings::overlaps(binding.context, KeyBindings::Context::Application))) {
            continue;
        }
        for (const QKeySequence &shortcut : KeyBindings::keys(binding.shortcuts)) {
            if (shortcut == key || KeyBindings::startsWith(shortcut, key) || KeyBindings::startsWith(key, shortcut)) {
                return i;
            }
        }
    }
    return -1;
}

QList<QKeySequence> defaults(const QList<QKeySequence> &base, const QKeySequence &extra, bool enabled, bool taken)
{
    QList<QKeySequence> found = base;
    if (enabled && !taken && !extra.isEmpty() && !base.contains(extra)) {
        found.append(extra);
    }
    return found;
}

State state(const QList<KeyBindings::Binding> &bindings, int index, const QKeySequence &extra, bool enabled)
{
    if (extra.isEmpty() || index < 0 || index >= bindings.size()) {
        return {};
    }
    if (!enabled) {
        return {Status::Off, user(bindings, index, extra)};
    }
    const KeyBindings::Binding &binding = bindings.at(index);
    if (binding.defaults.contains(extra)) {
        return {binding.shortcuts.contains(extra) ? Status::Added : Status::Replaced};
    }
    return {Status::Taken, user(bindings, index, extra)};
}

QString stateText(const State &state, const QList<KeyBindings::Binding> &bindings)
{
    switch (state.status) {
    case Status::NoExtraKey:
        return {};
    case Status::Off:
        if (state.user >= 0 && state.user < bindings.size()) {
            return i18nc("@info extra shortcut status, %1 action", "Off; “%1” uses it", bindings.at(state.user).text);
        }
        return i18nc("@info extra shortcut status", "Off");
    case Status::Added:
        return i18nc("@info extra shortcut status", "Added");
    case Status::Replaced:
        return i18nc("@info extra shortcut status", "Added as a default, but this action has shortcuts of your own");
    case Status::Taken:
        if (state.user >= 0 && state.user < bindings.size()) {
            return i18nc("@info extra shortcut status, %1 action", "Not added: “%1” uses it", bindings.at(state.user).text);
        }
        return i18nc("@info extra shortcut status", "Not added: another action uses it");
    }
    return {};
}
}
