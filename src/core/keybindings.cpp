// SPDX-License-Identifier: GPL-3.0-or-later

#include "keybindings.h"

#include <KLocalizedString>

namespace KeyBindings
{
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
                const QList<QKeySequence> second = keys(bindings.at(j).shortcuts);
                for (const QKeySequence &other : second) {
                    addConflict(found, i, first.at(a), j, other);
                }
            }
        }
    }
    return found;
}

QString conflictText(const Conflict &conflict, const QList<Binding> &bindings)
{
    const Binding &first = bindings.at(conflict.first);
    const Binding &second = bindings.at(conflict.second);
    const QString firstKeys = conflict.firstKeys.toString(QKeySequence::NativeText);
    const QString secondKeys = conflict.secondKeys.toString(QKeySequence::NativeText);
    if (conflict.kind == ConflictKind::Prefix) {
        return i18nc("@item %1 and %3 shortcuts, %2 and %4 actions",
                     "%1 for “%2” begins %3 for “%4”, so %3 can't be typed",
                     firstKeys,
                     first.text,
                     secondKeys,
                     second.text);
    }
    if (first.global != second.global) {
        const Binding &global = first.global ? first : second;
        const Binding &local = first.global ? second : first;
        return i18nc("@item %1 shortcut, %2 global action, %3 window action",
                     "%1 is the global shortcut of “%2”, so it doesn't run “%3” while KDE's global shortcut service is running",
                     firstKeys,
                     global.text,
                     local.text);
    }
    return i18nc("@item %1 shortcut, %2 and %3 actions", "%1 is the shortcut of both “%2” and “%3”, so it runs neither", firstKeys, first.text, second.text);
}
}
