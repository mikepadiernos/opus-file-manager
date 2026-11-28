// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "yazimode.h"

#include <QKeyCombination>
#include <QKeySequence>
#include <QList>
#include <QString>

#include <optional>

/**
 * What the shortcut hint popup lists, inspired by which-key.nvim: while a
 * key sequence is partly typed, the keys that can follow it, grouped, with
 * what they do. The popup itself is ShortcutHintPopup.
 */
namespace ShortcutHints
{
/// How long a pending sequence waits before its hints show, in milliseconds, by default.
constexpr int DefaultDelay = 500;

/// One key, or keys, a hint popup lists.
struct Hint {
    /// The keys to press next as the user sees them, such as "g" or "Ctrl+T".
    QString keys;
    /// What they do, or, for a prefix, how many commands continue there.
    QString description;
    /// The heading the hint is listed under.
    QString group;
    /// Whether the keys begin longer sequences rather than complete one.
    bool prefix = false;

    bool operator==(const Hint &other) const = default;
};

/// A binding hints can describe.
struct Entry {
    QList<QKeyCombination> keys;
    QString description;
    QString group;
};

/// How hints show keys.
enum class KeyStyle {
    /// As Yazi mode and Vim-style navigation show them: "g" and "G" for letters, as typed.
    Typed,
    /// As window shortcuts show them, such as "Ctrl+T" or "Shift+G".
    Native,
};

/// @p sequence's keys, in order.
QList<QKeyCombination> chords(const QKeySequence &sequence);

/// Whether @p keys begins with, and is longer than, @p prefix, comparing keys as YaziMode::normalizedKey() does.
bool continues(const QList<QKeyCombination> &keys, const QList<QKeyCombination> &prefix);

/**
 * The keys that can follow @p pending among @p entries, in the order their
 * groups first appear, then in entry order.
 *
 * Entries whose next key begins a longer sequence are merged into one
 * prefix hint, "+N commands", as which-key.nvim shows groups; an entry that
 * ends there keeps its own hint. With @p expand, every entry is listed with
 * all its remaining keys instead, as the on-demand reference does.
 */
QList<Hint> hints(const QList<Entry> &entries, const QList<QKeyCombination> &pending, KeyStyle style, bool expand = false);

/// The heading @p command is listed under, such as "Move" or "Tabs".
QString commandGroup(YaziMode::Command command);

/**
 * The bindings of @p keymap as entries, described by their commands. With
 * @p counted, only the commands a count repeats, which are what can follow
 * a typed count such as 5.
 */
QList<Entry> keymapEntries(const YaziMode::Keymap &keymap, bool counted = false);

/// A line of a page of hints: a group heading or a hint.
struct Row {
    /// The heading, or empty for a hint.
    QString heading;
    Hint hint;

    bool isHeading() const
    {
        return !heading.isEmpty();
    }
    bool operator==(const Row &other) const = default;
};

/// A page of hints: columns of rows, left to right.
using Page = QList<QList<Row>>;

/**
 * Lays @p hints out on pages of up to @p columns columns of up to @p rows
 * rows, under a heading wherever their group changes. A heading never ends
 * a column, and a page that begins within a group repeats its heading.
 * There are always at least two rows and one column.
 */
QList<Page> pages(const QList<Hint> &hints, int rows, int columns);

/**
 * Follows the keys of a window's multi-key shortcuts, such as Ctrl+K, Ctrl+T,
 * which Qt's shortcut map matches without telling anyone a sequence is
 * pending. The tracker infers it from the events the application sees:
 *
 * - Qt asks the focus widget whether it overrides a key only while no
 *   sequence is pending; shortcutOverride() makes that key a candidate.
 * - When the key press then reaches a widget (keyPressed()) or a shortcut
 *   runs (shortcutActivated()), no sequence began. Otherwise Qt took the
 *   key as the start of a sequence: settle() confirms it if the key begins
 *   one of the shortcuts.
 * - While a sequence is pending, Qt takes every key press, so only key
 *   releases tell which keys followed: keyReleased() extends the sequence
 *   when it still begins a shortcut and ends it otherwise, as Qt does.
 *
 * It is an inference: it can't tell which shortcut contexts apply, or keys
 * that auto-repeat.
 */
class WindowSequenceTracker
{
public:
    /// A key was offered to the focus widget before Qt's shortcut map. @returns whether a pending sequence ended
    bool shortcutOverride(QKeyCombination key);
    /// A key press reached a widget. @returns whether a pending sequence ended
    bool keyPressed(QKeyCombination key);
    /// A shortcut ran. @returns whether a pending sequence ended
    bool shortcutActivated();
    /**
     * Confirms the candidate key, once the events it caused have been
     * delivered, as the start of a sequence if it begins one of @p shortcuts.
     * @returns whether a sequence began
     */
    bool settle(const QList<QKeySequence> &shortcuts);
    /// A key was released. @returns whether the pending sequence changed
    bool keyReleased(QKeyCombination key, bool autoRepeat, const QList<QKeySequence> &shortcuts);
    void reset();

    bool isPending() const;
    /// Whether a key offered to the focus widget waits for settle().
    bool hasCandidate() const;
    /// The keys typed so far of the pending sequence, or none.
    QList<QKeyCombination> pending() const;

private:
    std::optional<QKeyCombination> m_candidate;
    bool m_candidateReleased = false;
    QList<QKeyCombination> m_pending;
    /// The key typed last, whose release doesn't add a key.
    Qt::Key m_held = Qt::Key_unknown;
};
}
