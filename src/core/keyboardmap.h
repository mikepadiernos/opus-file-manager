// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

/**
 * The keyboard map: every action with its description, shortcuts and where
 * they apply, searched by name or by keys and exported as a printable
 * reference. The window collects the entries; nothing here runs an action.
 */
namespace KeyboardMap
{
/// One action or key of the map.
struct Entry {
    /// The action's name, such as "search_folder", or empty for keys without an action.
    QString name;
    /// The action's text, without accelerator markers.
    QString text;
    /// What the action does, when it says more than its text.
    QString description;
    /// The primary shortcut first, then any alternates; empty sequences are ignored.
    QList<QKeySequence> shortcuts;
    /// How to show the shortcuts, when Qt's own text would be misleading, such as "G" for Shift+G; empty to use Qt's.
    QString keysText;
    /// Where the shortcuts apply, for the user.
    QString context;

    bool operator==(const Entry &other) const = default;
};

/**
 * The first of @p toolTip, @p statusTip and @p whatsThis that says more than
 * @p text, as plain text, or empty when none does. Accelerator markers,
 * trailing ellipses, case and rich text don't count as saying more.
 */
QString description(const QString &text, const QString &toolTip, const QString &statusTip, const QString &whatsThis);

/// How @p entry's shortcuts are shown: its keysText, or Qt's native text of each, separated by "; ", or empty.
QString shortcutsText(const Entry &entry);

/// Whether every word of @p query is found, ignoring case, in @p entry's text, name, description, shortcuts or context. An empty query matches.
bool matchesText(const Entry &entry, const QString &query);

/// Whether one of @p entry's shortcuts is @p keys or begins with them. Empty @p keys match.
bool matchesKeys(const Entry &entry, const QKeySequence &keys);

/// The entries of @p entries that match @p query and @p keys and, when @p withShortcutsOnly, have a shortcut, in order.
QList<Entry> filter(const QList<Entry> &entries, const QString &query, const QKeySequence &keys, bool withShortcutsOnly);

/// A printable HTML reference of @p entries headed @p title, grouped by context in order of first appearance.
QString toHtml(const QList<Entry> &entries, const QString &title);

/**
 * Writes the reference of @p entries to @p path: a PDF when the file name
 * ends with .pdf, otherwise HTML. Returns an error message, or empty on
 * success. The file is replaced only once it is complete.
 */
QString exportReference(const QList<Entry> &entries, const QString &title, const QString &path);
}
