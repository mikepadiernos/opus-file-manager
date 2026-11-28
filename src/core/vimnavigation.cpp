// SPDX-License-Identifier: GPL-3.0-or-later

#include "vimnavigation.h"

#include <KLocalizedString>

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
        bind({Qt::Key_I}, Command::Filter),
        bind({Qt::Key_Backslash}, Command::ClearFilter),
    };
    return keymap;
}

QString VimNavigation::modeText(Mode mode, const QString &pending)
{
    QString text;
    switch (mode) {
    case Mode::Normal:
        text = i18nc("@info:status Vim-style mode in which keys move through the items; keep it short and upper case where usual", "NORMAL");
        break;
    case Mode::Filter:
        text = i18nc("@info:status Vim-style mode in which typing filters the items; keep it short and upper case where usual", "FILTER");
        break;
    case Mode::Insert:
        text = i18nc("@info:status Vim-style mode in which typing goes to a text field; keep it short and upper case where usual", "INSERT");
        break;
    }
    if (mode == Mode::Normal && !pending.isEmpty()) {
        text = i18nc("@info:status %1 is the mode, %2 the count and keys typed so far of a key sequence", "%1 · %2…", text, pending);
    }
    return text;
}

QString VimNavigation::modeAccessibleName(Mode mode, const QString &pending)
{
    switch (mode) {
    case Mode::Normal:
        if (!pending.isEmpty()) {
            return i18nc("@info:accessibility %1 the count and keys typed so far of a key sequence",
                         "Vim-style navigation, normal mode, waiting for the keys after %1",
                         pending);
        }
        return i18nc("@info:accessibility", "Vim-style navigation, normal mode: keys move through the items; press slash or i to filter");
    case Mode::Filter:
        return i18nc("@info:accessibility", "Vim-style navigation, filter mode: typing filters the items; press Escape to return to the items");
    case Mode::Insert:
        return i18nc("@info:accessibility", "Vim-style navigation, insert mode: typing goes to the text field");
    }
    return QString();
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
