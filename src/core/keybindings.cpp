// SPDX-License-Identifier: GPL-3.0-or-later

#include "keybindings.h"

#include <KLocalizedString>

#include <QStringList>

#include <algorithm>
#include <initializer_list>

namespace KeyBindings
{
bool overlaps(Context a, Context b)
{
    if (a == b) {
        return true;
    }
    // The gallery is a window of its own, and the focus is in only one of the main window's widgets at a time.
    return (a == Context::Application && b != Context::Viewer) || (b == Context::Application && a != Context::Viewer);
}

QString contextText(Context context)
{
    switch (context) {
    case Context::Application:
        return i18nc("@item where a keyboard shortcut applies", "Window");
    case Context::FileView:
        return i18nc("@item where a keyboard shortcut applies", "File views and lists");
    case Context::FolderTree:
        return i18nc("@item where a keyboard shortcut applies", "Folder tree");
    case Context::Viewer:
        return i18nc("@item where a keyboard shortcut applies", "Image gallery");
    case Context::TextInput:
        return i18nc("@item where a keyboard shortcut applies", "Text fields");
    }
    return {};
}

QString contextText(const Binding &binding)
{
    return binding.global ? i18nc("@item where a keyboard shortcut applies", "Global") : contextText(binding.context);
}

namespace
{
/// The modifiers of @p key other than the keypad's, which doesn't change what a key does.
Qt::KeyboardModifiers modifiers(QKeyCombination key)
{
    return key.keyboardModifiers() & ~Qt::KeypadModifier;
}

bool isOneOf(Qt::Key key, std::initializer_list<Qt::Key> keys)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}
}

bool isTypingKey(QKeyCombination key)
{
    const Qt::KeyboardModifiers mods = modifiers(key);
    const Qt::Key k = key.key();
    if (mods == Qt::NoModifier || mods == Qt::ShiftModifier) {
        // Keys below Qt's function key range type text.
        return (k >= Qt::Key_Space && k < Qt::Key_Escape)
            || isOneOf(k,
                       {Qt::Key_Backspace,
                        Qt::Key_Delete,
                        Qt::Key_Insert,
                        Qt::Key_Left,
                        Qt::Key_Right,
                        Qt::Key_Up,
                        Qt::Key_Down,
                        Qt::Key_Home,
                        Qt::Key_End,
                        Qt::Key_PageUp,
                        Qt::Key_PageDown,
                        Qt::Key_Return,
                        Qt::Key_Enter,
                        Qt::Key_Tab,
                        Qt::Key_Backtab});
    }
    // The editing keys of KDE's text fields: clipboard, undo, select all, and moving and deleting by word.
    if (mods == Qt::ControlModifier) {
        return isOneOf(k,
                       {Qt::Key_A,
                        Qt::Key_C,
                        Qt::Key_V,
                        Qt::Key_X,
                        Qt::Key_Z,
                        Qt::Key_Y,
                        Qt::Key_Insert,
                        Qt::Key_Left,
                        Qt::Key_Right,
                        Qt::Key_Home,
                        Qt::Key_End,
                        Qt::Key_Backspace,
                        Qt::Key_Delete});
    }
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
        return isOneOf(k, {Qt::Key_Z, Qt::Key_Left, Qt::Key_Right, Qt::Key_Home, Qt::Key_End});
    }
    return false;
}

bool isNavigationKey(QKeyCombination key)
{
    const Qt::KeyboardModifiers mods = modifiers(key);
    return (mods == Qt::NoModifier || mods == Qt::ShiftModifier)
        && isOneOf(key.key(), {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Home, Qt::Key_End, Qt::Key_PageUp, Qt::Key_PageDown});
}

bool isAccessibilityKey(QKeyCombination key)
{
    const Qt::KeyboardModifiers mods = modifiers(key);
    const Qt::Key k = key.key();
    if (mods == Qt::NoModifier) {
        return isOneOf(k, {Qt::Key_Tab, Qt::Key_Backtab, Qt::Key_Escape, Qt::Key_F10, Qt::Key_Menu});
    }
    return mods == Qt::ShiftModifier && isOneOf(k, {Qt::Key_Tab, Qt::Key_Backtab, Qt::Key_F10});
}

bool isReserved(QKeyCombination key, Context context)
{
    switch (context) {
    case Context::TextInput:
        return isTypingKey(key);
    case Context::FileView:
    case Context::FolderTree:
        return isNavigationKey(key);
    case Context::Application:
    case Context::Viewer:
        break;
    }
    return false;
}

QList<QKeySequence> keys(const QList<QKeySequence> &shortcuts)
{
    QList<QKeySequence> found;
    for (const QKeySequence &shortcut : shortcuts) {
        if (!shortcut.isEmpty()) {
            found.append(shortcut);
        }
    }
    return found;
}

bool isCustomized(const Binding &binding)
{
    return keys(binding.shortcuts) != keys(binding.defaults);
}

QList<int> customized(const QList<Binding> &bindings)
{
    QList<int> found;
    for (int i = 0; i < bindings.size(); ++i) {
        if (isCustomized(bindings.at(i))) {
            found.append(i);
        }
    }
    return found;
}

bool startsWith(const QKeySequence &sequence, const QKeySequence &prefix)
{
    if (prefix.isEmpty() || prefix.count() >= sequence.count()) {
        return false;
    }
    for (int i = 0; i < prefix.count(); ++i) {
        if (prefix[i] != sequence[i]) {
            return false;
        }
    }
    return true;
}

namespace
{
/// Adds the conflict between @p a of binding @p ai and @p b of binding @p bi, if any, shorter sequence first.
void addConflict(QList<Conflict> &found, int ai, const QKeySequence &a, int bi, const QKeySequence &b)
{
    if (a == b) {
        if (ai != bi) {
            found.append({ai, a, bi, b, ConflictKind::Same});
        }
    } else if (startsWith(b, a)) {
        found.append({ai, a, bi, b, ConflictKind::Prefix});
    } else if (startsWith(a, b)) {
        found.append({bi, b, ai, a, ConflictKind::Prefix});
    }
}
}

QList<Conflict> conflicts(const QList<Binding> &bindings)
{
    QList<Conflict> found;
    for (int i = 0; i < bindings.size(); ++i) {
        const QList<QKeySequence> first = keys(bindings.at(i).shortcuts);
        for (int a = 0; a < first.size(); ++a) {
            for (int b = a + 1; b < first.size(); ++b) {
                addConflict(found, i, first.at(a), i, first.at(b));
            }
            for (int j = i + 1; j < bindings.size(); ++j) {
                const Binding &other = bindings.at(j);
                if (!bindings.at(i).global && !other.global && !overlaps(bindings.at(i).context, other.context)) {
                    continue;
                }
                for (const QKeySequence &otherKeys : keys(other.shortcuts)) {
                    addConflict(found, i, first.at(a), j, otherKeys);
                }
            }
        }
    }
    // Default shortcuts are chosen to fit; only the user's own choices are checked against the keys widgets keep.
    for (int i = 0; i < bindings.size(); ++i) {
        const Binding &binding = bindings.at(i);
        if (binding.global || binding.context != Context::Application) {
            continue;
        }
        for (const QKeySequence &shortcut : keys(binding.shortcuts)) {
            if (binding.defaults.contains(shortcut)) {
                continue;
            }
            for (Context context : {Context::TextInput, Context::FileView, Context::FolderTree}) {
                if (isReserved(shortcut[0], context)) {
                    found.append({i, shortcut, -1, {}, ConflictKind::Reserved, context});
                }
            }
            if (isAccessibilityKey(shortcut[0])) {
                found.append({i, shortcut, -1, {}, ConflictKind::Reserved, Context::Application});
            }
        }
    }
    return found;
}

QString conflictText(const Conflict &conflict, const QList<Binding> &bindings)
{
    const Binding &first = bindings.at(conflict.first);
    const QString firstKeys = conflict.firstKeys.toString(QKeySequence::NativeText);
    if (conflict.kind == ConflictKind::Reserved) {
        switch (conflict.context) {
        case Context::TextInput:
            return i18nc("@item %1 shortcut, %2 action",
                         "%1 doesn't run “%2” while a text field has the keyboard focus, because the field keeps the key for typing",
                         firstKeys,
                         first.text);
        case Context::FileView:
            return i18nc("@item %1 shortcut, %2 action",
                         "%1 doesn't run “%2” while a file view or another list has the keyboard focus, because the list keeps the key for "
                         "moving through its items",
                         firstKeys,
                         first.text);
        case Context::FolderTree:
            return i18nc("@item %1 shortcut, %2 action",
                         "%1 doesn't run “%2” while the folder tree has the keyboard focus, because the tree keeps the key for moving "
                         "through its folders",
                         firstKeys,
                         first.text);
        case Context::Application:
        case Context::Viewer:
            break;
        }
        return i18nc("@item %1 shortcut, %2 action",
                     "%1 for “%2” also moves around without a mouse, such as moving the focus or opening menus, which it no longer "
                     "does in the window",
                     firstKeys,
                     first.text);
    }
    const Binding &second = bindings.at(conflict.second);
    const QString secondKeys = conflict.secondKeys.toString(QKeySequence::NativeText);
    const QString firstName =
        first.global || first.context == second.context ? first.text : i18nc("@item %1 action, %2 where it applies", "%1 (%2)", first.text, contextText(first));
    const QString secondName = second.global || first.context == second.context
        ? second.text
        : i18nc("@item %1 action, %2 where it applies", "%1 (%2)", second.text, contextText(second));
    if (conflict.kind == ConflictKind::Prefix) {
        return i18nc("@item %1 and %3 shortcuts, %2 and %4 actions",
                     "%1 for “%2” begins %3 for “%4”, so %3 can't be typed",
                     firstKeys,
                     firstName,
                     secondKeys,
                     secondName);
    }
    if (first.global != second.global) {
        const QString &global = first.global ? firstName : secondName;
        const QString &local = first.global ? secondName : firstName;
        return i18nc("@item %1 shortcut, %2 global action, %3 window action",
                     "%1 is the global shortcut of “%2”, so it doesn't run “%3” while KDE's global shortcut service is running",
                     firstKeys,
                     global,
                     local);
    }
    return i18nc("@item %1 shortcut, %2 and %3 actions", "%1 is the shortcut of both “%2” and “%3”, so it runs neither", firstKeys, firstName, secondName);
}

GlobalAvailability globalAvailability(bool serviceAvailable, const QString &sessionType, const QString &currentDesktops)
{
    if (!serviceAvailable) {
        return GlobalAvailability::NoService;
    }
    const QStringList desktops = currentDesktops.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    const bool plasma = std::any_of(desktops.cbegin(), desktops.cend(), [](const QString &desktop) {
        return desktop.compare(QLatin1String("KDE"), Qt::CaseInsensitive) == 0;
    });
    if (sessionType.compare(QLatin1String("wayland"), Qt::CaseInsensitive) == 0 && !plasma) {
        return GlobalAvailability::OtherDesktop;
    }
    return GlobalAvailability::Available;
}

QString globalAvailabilityText(GlobalAvailability availability)
{
    switch (availability) {
    case GlobalAvailability::Available:
        return {};
    case GlobalAvailability::NoService:
        return i18nc("@info", "KDE's global shortcut service is not running, so global shortcuts don't work in this session.");
    case GlobalAvailability::OtherDesktop:
        return i18nc("@info",
                     "This Wayland session isn't Plasma, so global shortcuts work only if its desktop passes keys to KDE's global "
                     "shortcut service.");
    }
    return {};
}

QList<QKeySequence> refusedKeys(const QList<QKeySequence> &requested, const QList<QKeySequence> &accepted)
{
    QList<QKeySequence> refused;
    for (const QKeySequence &key : keys(requested)) {
        if (!accepted.contains(key) && !refused.contains(key)) {
            refused.append(key);
        }
    }
    return refused;
}

QString refusedText(const QList<QKeySequence> &refused)
{
    const QList<QKeySequence> found = keys(refused);
    if (found.isEmpty()) {
        return {};
    }
    QStringList texts;
    for (const QKeySequence &key : found) {
        texts.append(key.toString(QKeySequence::NativeText));
    }
    return i18nc("@info %1 key combinations",
                 "KDE didn't accept %1 because another application already uses it; choose another key combination.",
                 texts.join(i18nc("@item separator of keyboard shortcuts", "; ")));
}
}
