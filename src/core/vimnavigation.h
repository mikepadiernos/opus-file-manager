// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "yazimode.h"

#include <QKeyCombination>
#include <QList>
#include <QString>

/**
 * Optional Vim-style navigation in the file views of panes that aren't in
 * Yazi mode, turned on by the VimNavigation setting. It reuses Yazi mode's
 * commands, key sequences and KeyMatcher; a pane in Yazi mode uses its own
 * key preset instead.
 *
 * Plain keys, such as j or /, take over a window shortcut that uses them
 * while a file view has the keyboard focus, as Yazi mode's keys do. Keys
 * with Ctrl, Alt or Meta, such as Ctrl+D, never do: a window shortcut that
 * uses one keeps it, and the binding works only while no shortcut does.
 */
namespace VimNavigation
{
/**
 * The keys: j and k move down and up, gg and G to the first and last item,
 * Ctrl+D and Ctrl+U, or J and K as in ranger, half a page; h opens the
 * parent folder, l the focused item, H and L go back and forward, gh home,
 * gt and gT to the next and previous tab, / or i filters and \ clears the
 * filter. Digits typed first count how often to repeat a command, as in 5j.
 */
YaziMode::Keymap keymap();

/// Where a pane's keys go while Vim-style navigation is on.
enum class Mode {
    /// The file view: the Vim-style keys move and open.
    Normal,
    /// The filter bar: keys type the filter; Escape returns to Normal.
    Filter,
    /// Another text field of the pane, such as the location bar or an inline rename editor.
    Insert,
};

/**
 * The short text of the pane's mode indicator, such as "NORMAL", or, with
 * the count and keys typed so far of an incomplete sequence, "NORMAL · 5g…".
 */
QString modeText(Mode mode, const QString &pending = QString());

/**
 * The indicator's accessible name: the mode in words, what the keys do in
 * it and how to leave it, and the keys waiting for the rest of a sequence.
 */
QString modeAccessibleName(Mode mode, const QString &pending = QString());

/// Whether @p key has Ctrl, Alt or Meta, so that a window shortcut using it keeps it.
bool yieldsToShortcuts(QKeyCombination key);

/**
 * The shortcuts of @p actions that @p keymap takes over while a file view
 * has the keyboard focus: those whose first key is a plain key that begins
 * a binding or a count. See YaziMode::shadowedShortcuts().
 */
QList<YaziMode::ShadowedShortcut> shadowedShortcuts(const YaziMode::Keymap &keymap, const QList<YaziMode::ActionShortcuts> &actions);

/**
 * The shortcuts of @p actions whose first key, with Ctrl, Alt or Meta, also
 * begins a binding of @p keymap. The shortcut keeps its key, so those
 * bindings don't run while the shortcut is set.
 */
QList<YaziMode::ShadowedShortcut> yieldedShortcuts(const YaziMode::Keymap &keymap, const QList<YaziMode::ActionShortcuts> &actions);
}
