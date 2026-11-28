// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcuthints.h"

#include <KLocalizedString>

#include <QHash>

#include <algorithm>

namespace
{
bool isModifierKey(Qt::Key key)
{
    return (key >= Qt::Key_Shift && key <= Qt::Key_ScrollLock) || key == Qt::Key_Super_L || key == Qt::Key_Super_R || key == Qt::Key_Hyper_L
        || key == Qt::Key_Hyper_R || key == Qt::Key_AltGr || key == Qt::Key_unknown;
}

QString keysText(const QList<QKeyCombination> &keys, ShortcutHints::KeyStyle style)
{
    if (style == ShortcutHints::KeyStyle::Typed) {
        return YaziMode::sequenceText(keys);
    }
    const QKeyCombination none = QKeyCombination::fromCombined(0);
    return QKeySequence(keys.value(0, none), keys.value(1, none), keys.value(2, none), keys.value(3, none)).toString(QKeySequence::NativeText);
}

bool beginsAny(const QList<QKeySequence> &shortcuts, const QList<QKeyCombination> &keys)
{
    return std::any_of(shortcuts.cbegin(), shortcuts.cend(), [&keys](const QKeySequence &shortcut) {
        return ShortcutHints::continues(ShortcutHints::chords(shortcut), keys);
    });
}
}

QList<QKeyCombination> ShortcutHints::chords(const QKeySequence &sequence)
{
    QList<QKeyCombination> keys;
    for (int i = 0; i < sequence.count(); ++i) {
        keys.append(sequence[i]);
    }
    return keys;
}

bool ShortcutHints::continues(const QList<QKeyCombination> &keys, const QList<QKeyCombination> &prefix)
{
    if (keys.size() <= prefix.size()) {
        return false;
    }
    for (qsizetype i = 0; i < prefix.size(); ++i) {
        if (YaziMode::normalizedKey(keys.at(i)) != YaziMode::normalizedKey(prefix.at(i))) {
            return false;
        }
    }
    return true;
}

QList<ShortcutHints::Hint> ShortcutHints::hints(const QList<Entry> &entries, const QList<QKeyCombination> &pending, KeyStyle style, bool expand)
{
    QList<Hint> found;
    QHash<QString, qsizetype> prefixes;
    QList<int> prefixCounts;
    for (const Entry &entry : entries) {
        if (!continues(entry.keys, pending)) {
            continue;
        }
        const QList<QKeyCombination> remaining = entry.keys.mid(pending.size());
        if (expand || remaining.size() == 1) {
            found.append({keysText(remaining, style), entry.description, entry.group, false});
            prefixCounts.append(0);
            continue;
        }
        const QString next = keysText({remaining.first()}, style);
        const qsizetype index = prefixes.value(next, -1);
        if (index < 0) {
            prefixes.insert(next, found.size());
            found.append({next, QString(), entry.group, true});
            prefixCounts.append(1);
            continue;
        }
        ++prefixCounts[index];
        if (found.at(index).group != entry.group) {
            found[index].group = i18nc("@title shortcut hint group", "More keys");
        }
    }
    for (qsizetype i = 0; i < found.size(); ++i) {
        if (found.at(i).prefix) {
            found[i].description = i18ncp("@item shortcut hint for keys that begin longer sequences", "+%1 command", "+%1 commands", prefixCounts.at(i));
        }
    }
    QStringList groups;
    for (const Hint &hint : std::as_const(found)) {
        if (!groups.contains(hint.group)) {
            groups.append(hint.group);
        }
    }
    std::stable_sort(found.begin(), found.end(), [&groups](const Hint &a, const Hint &b) {
        return groups.indexOf(a.group) < groups.indexOf(b.group);
    });
    return found;
}

QString ShortcutHints::commandGroup(YaziMode::Command command)
{
    using YaziMode::Command;
    switch (command) {
    case Command::MoveUp:
    case Command::MoveDown:
    case Command::MoveFirst:
    case Command::MoveLast:
    case Command::PageUp:
    case Command::PageDown:
    case Command::HalfPageUp:
    case Command::HalfPageDown:
        return i18nc("@title shortcut hint group", "Move");
    case Command::Parent:
    case Command::Open:
    case Command::Back:
    case Command::Forward:
    case Command::Home:
        return i18nc("@title shortcut hint group", "Folders and history");
    case Command::FocusParent:
    case Command::FocusCurrent:
    case Command::FocusPreview:
    case Command::TogglePreview:
    case Command::TogglePreviewContent:
    case Command::ToggleHidden:
    case Command::Leave:
        return i18nc("@title shortcut hint group", "View");
    case Command::ToggleSelection:
    case Command::ToggleSelectionDown:
    case Command::VisualSelection:
    case Command::SelectAll:
        return i18nc("@title shortcut hint group", "Selection");
    case Command::Copy:
    case Command::Cut:
    case Command::Paste:
    case Command::Rename:
    case Command::Trash:
    case Command::Create:
    case Command::OpenWith:
    case Command::CopyToOtherPane:
    case Command::MoveToOtherPane:
    case Command::Undo:
        return i18nc("@title shortcut hint group", "Files");
    case Command::Filter:
    case Command::ClearFilter:
    case Command::SearchNames:
    case Command::SearchContents:
        return i18nc("@title shortcut hint group", "Filter and search");
    case Command::SortByName:
    case Command::SortBySize:
    case Command::SortByModified:
    case Command::SortByType:
    case Command::ReverseSort:
        return i18nc("@title shortcut hint group", "Sort");
    case Command::NewTab:
    case Command::PreviousTab:
    case Command::NextTab:
    case Command::CloseTab:
        return i18nc("@title shortcut hint group", "Tabs");
    }
    return QString();
}

QList<ShortcutHints::Entry> ShortcutHints::keymapEntries(const YaziMode::Keymap &keymap, bool counted)
{
    QList<Entry> entries;
    for (const YaziMode::Binding &binding : keymap.bindings) {
        if (counted && !YaziMode::commandTakesCount(binding.command)) {
            continue;
        }
        entries.append({binding.keys, YaziMode::commandText(binding.command), commandGroup(binding.command)});
    }
    return entries;
}

QList<ShortcutHints::Page> ShortcutHints::pages(const QList<Hint> &hints, int rows, int columns)
{
    rows = qMax(2, rows);
    columns = qMax(1, columns);
    QList<Page> result;
    Page page;
    QList<Row> column;
    QString group;
    for (const Hint &hint : hints) {
        const auto needsHeading = [&] {
            if (hint.group.isEmpty()) {
                return false;
            }
            return hint.group != group || (column.isEmpty() && page.isEmpty());
        };
        bool heading = needsHeading();
        if (column.size() + (heading ? 2 : 1) > rows) {
            page.append(column);
            column.clear();
            if (page.size() == columns) {
                result.append(page);
                page.clear();
            }
            heading = needsHeading();
        }
        if (heading) {
            column.append({hint.group, {}});
        }
        column.append({QString(), hint});
        group = hint.group;
    }
    if (!column.isEmpty()) {
        page.append(column);
    }
    if (!page.isEmpty()) {
        result.append(page);
    }
    return result;
}

bool ShortcutHints::WindowSequenceTracker::shortcutOverride(QKeyCombination key)
{
    if (isModifierKey(key.key())) {
        return false;
    }
    const bool ended = isPending();
    reset();
    m_candidate = key;
    return ended;
}

bool ShortcutHints::WindowSequenceTracker::keyPressed(QKeyCombination key)
{
    if (isModifierKey(key.key())) {
        return false;
    }
    const bool ended = isPending();
    reset();
    return ended;
}

bool ShortcutHints::WindowSequenceTracker::shortcutActivated()
{
    const bool ended = isPending();
    reset();
    return ended;
}

bool ShortcutHints::WindowSequenceTracker::settle(const QList<QKeySequence> &shortcuts)
{
    if (!m_candidate) {
        return false;
    }
    const QKeyCombination key = *m_candidate;
    m_candidate.reset();
    if (!beginsAny(shortcuts, {key})) {
        m_candidateReleased = false;
        return false;
    }
    m_pending = {key};
    m_held = m_candidateReleased ? Qt::Key_unknown : key.key();
    m_candidateReleased = false;
    return true;
}

bool ShortcutHints::WindowSequenceTracker::keyReleased(QKeyCombination key, bool autoRepeat, const QList<QKeySequence> &shortcuts)
{
    if (autoRepeat || isModifierKey(key.key())) {
        return false;
    }
    if (!isPending()) {
        // The candidate's key can be released before settle(), as when keys are sent at once.
        m_candidateReleased = m_candidateReleased || (m_candidate && m_candidate->key() == key.key());
        return false;
    }
    if (key.key() == m_held) {
        m_held = Qt::Key_unknown;
        return false;
    }
    QList<QKeyCombination> typed = m_pending;
    typed.append(key);
    if (beginsAny(shortcuts, typed)) {
        m_pending = typed;
    } else {
        reset();
    }
    return true;
}

void ShortcutHints::WindowSequenceTracker::reset()
{
    m_candidate.reset();
    m_candidateReleased = false;
    m_pending.clear();
    m_held = Qt::Key_unknown;
}

bool ShortcutHints::WindowSequenceTracker::isPending() const
{
    return !m_pending.isEmpty();
}

bool ShortcutHints::WindowSequenceTracker::hasCandidate() const
{
    return m_candidate.has_value();
}

QList<QKeyCombination> ShortcutHints::WindowSequenceTracker::pending() const
{
    return m_pending;
}
