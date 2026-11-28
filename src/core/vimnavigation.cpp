// SPDX-License-Identifier: GPL-3.0-or-later

#include "vimnavigation.h"

namespace
{
YaziMode::Binding bind(const YaziMode::KeySequence &keys, YaziMode::Command command)
{
    return {keys, command};
}

/// The shortcuts of @p actions whose first key does, or doesn't, yield to window shortcuts.
QList<YaziMode::ActionShortcuts> splitShortcuts(const QList<YaziMode::ActionShortcuts> &actions, bool yielding)
{
    QList<YaziMode::ActionShortcuts> found;
    for (const YaziMode::ActionShortcuts &action : actions) {
        YaziMode::ActionShortcuts kept{action.name, action.text, {}};
        for (const QKeySequence &shortcut : action.shortcuts) {
            if (!shortcut.isEmpty() && VimNavigation::yieldsToShortcuts(YaziMode::normalizedKey(shortcut[0])) == yielding) {
                kept.shortcuts.append(shortcut);
            }
        }
        if (!kept.shortcuts.isEmpty()) {
            found.append(kept);
        }
    }
    return found;
}
}

YaziMode::Keymap VimNavigation::keymap()
{
    using YaziMode::Command;
    YaziMode::Keymap keymap;
    keymap.counts = true;
    keymap.bindings = {
        bind({Qt::Key_K}, Command::MoveUp),
        bind({Qt::Key_J}, Command::MoveDown),
        bind({Qt::Key_G, Qt::Key_G}, Command::MoveFirst),
        bind({Qt::ShiftModifier | Qt::Key_G}, Command::MoveLast),
        bind({Qt::ControlModifier | Qt::Key_U}, Command::HalfPageUp),
        bind({Qt::ShiftModifier | Qt::Key_K}, Command::HalfPageUp),
        bind({Qt::ControlModifier | Qt::Key_D}, Command::HalfPageDown),
        bind({Qt::ShiftModifier | Qt::Key_J}, Command::HalfPageDown),
        bind({Qt::Key_H}, Command::Parent),
        bind({Qt::Key_L}, Command::Open),
        bind({Qt::ShiftModifier | Qt::Key_H}, Command::Back),
        bind({Qt::ShiftModifier | Qt::Key_L}, Command::Forward),
        bind({Qt::Key_G, Qt::Key_H}, Command::Home),
        bind({Qt::Key_G, Qt::ShiftModifier | Qt::Key_T}, Command::PreviousTab),
        bind({Qt::Key_G, Qt::Key_T}, Command::NextTab),
        bind({Qt::Key_Slash}, Command::Filter),
        bind({Qt::Key_Backslash}, Command::ClearFilter),
    };
    return keymap;
}

bool VimNavigation::yieldsToShortcuts(QKeyCombination key)
{
    return key.keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

QList<YaziMode::ShadowedShortcut> VimNavigation::shadowedShortcuts(const YaziMode::Keymap &keymap, const QList<YaziMode::ActionShortcuts> &actions)
{
    return YaziMode::shadowedShortcuts(keymap, splitShortcuts(actions, false));
}

QList<YaziMode::ShadowedShortcut> VimNavigation::yieldedShortcuts(const YaziMode::Keymap &keymap, const QList<YaziMode::ActionShortcuts> &actions)
{
    return YaziMode::shadowedShortcuts(keymap, splitShortcuts(actions, true));
}
