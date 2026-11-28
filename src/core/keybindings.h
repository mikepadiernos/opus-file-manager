// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

/**
 * The window's keyboard shortcuts as Preferences › Keyboard summarizes them:
 * which actions have shortcuts other than their defaults, and which
 * shortcuts conflict. KDE's Configure Keyboard Shortcuts dialog edits them.
 */
namespace KeyBindings
{
/// An action's shortcuts and the ones it has by default.
struct Binding {
    /// The action's name, such as "search_folder".
    QString name;
    /// The action's text, without accelerator markers.
    QString text;
    /// The primary shortcut first, then any alternates; empty sequences are ignored.
    QList<QKeySequence> shortcuts;
    QList<QKeySequence> defaults;
    /// Whether the shortcuts are global ones, which KDE's global shortcut service reserves in every application.
    bool global = false;

    bool operator==(const Binding &other) const = default;
};

/// @p shortcuts without empty sequences, in order.
QList<QKeySequence> keys(const QList<QKeySequence> &shortcuts);

/// Whether @p binding's shortcuts differ from its defaults, including their order.
bool isCustomized(const Binding &binding);

/// The indexes of the customized @p bindings, in order.
QList<int> customized(const QList<Binding> &bindings);

/// Whether @p sequence begins with @p prefix and is longer than it.
bool startsWith(const QKeySequence &sequence, const QKeySequence &prefix);

enum class ConflictKind {
    /// Both bindings have the same shortcut.
    Same,
    /// The first binding's shortcut begins the second's longer key sequence, so the longer one can't be typed.
    Prefix,
};

/// Two shortcuts that get in each other's way.
struct Conflict {
    /// Index of the binding with the shorter, or equal, shortcut.
    int first = -1;
    QKeySequence firstKeys;
    int second = -1;
    QKeySequence secondKeys;
    ConflictKind kind = ConflictKind::Same;

    bool operator==(const Conflict &other) const = default;
};

/**
 * The conflicts between @p bindings, in binding order: two bindings with the
 * same shortcut, and a shortcut that begins another binding's, or the same
 * binding's, longer key sequence. A shortcut listed twice for one binding is
 * not a conflict.
 */
QList<Conflict> conflicts(const QList<Binding> &bindings);

/// Describes @p conflict between @p bindings as a sentence for the user.
QString conflictText(const Conflict &conflict, const QList<Binding> &bindings);
}
