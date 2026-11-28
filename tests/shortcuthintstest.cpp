// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcuthints.h"

#include "vimnavigation.h"

#include <QStandardPaths>
#include <QTest>

#include <algorithm>

using ShortcutHints::Hint;
using ShortcutHints::KeyStyle;
using YaziMode::Command;
using namespace Qt::StringLiterals;

namespace
{
const Hint *findHint(const QList<Hint> &hints, const QString &keys)
{
    const auto found = std::find_if(hints.cbegin(), hints.cend(), [&keys](const Hint &hint) {
        return hint.keys == keys;
    });
    return found == hints.cend() ? nullptr : &*found;
}

QKeyCombination key(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    return QKeyCombination(modifiers, key);
}
}

class ShortcutHintsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void listsTheKeysThatContinueASequence()
    {
        const QList<ShortcutHints::Entry> entries = ShortcutHints::keymapEntries(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        const QList<Hint> hints = ShortcutHints::hints(entries, {key(Qt::Key_G)}, KeyStyle::Typed);
        QStringList keys;
        for (const Hint &hint : hints) {
            keys.append(hint.keys);
            QVERIFY(!hint.prefix);
        }
        QCOMPARE(keys.size(), 8);
        for (const QString &expected : {u"g"_s, u"h"_s, u"p"_s, u"c"_s, u"v"_s, u"t"_s, u"T"_s, u"w"_s}) {
            QVERIFY2(keys.contains(expected), qPrintable(expected));
        }
        const Hint *first = findHint(hints, QStringLiteral("g"));
        QCOMPARE(first->description, YaziMode::commandText(Command::MoveFirst));
        QCOMPARE(first->group, ShortcutHints::commandGroup(Command::MoveFirst));
        // Grouped: the hints of a group are together, in the order the groups first appear.
        QStringList groups;
        for (const Hint &hint : hints) {
            if (groups.isEmpty() || groups.last() != hint.group) {
                QVERIFY2(!groups.contains(hint.group), qPrintable(hint.group));
                groups.append(hint.group);
            }
        }
        QCOMPARE(groups.first(), ShortcutHints::commandGroup(Command::MoveFirst));
        QVERIFY(groups.contains(ShortcutHints::commandGroup(Command::NextTab)));

        // Nothing continues a complete binding or a key no binding begins with.
        QVERIFY(ShortcutHints::hints(entries, {key(Qt::Key_J)}, KeyStyle::Typed).isEmpty());
        QVERIFY(ShortcutHints::hints(entries, {key(Qt::Key_Q), key(Qt::Key_Q)}, KeyStyle::Typed).isEmpty());
    }

    void mergesLongerSequencesIntoPrefixes()
    {
        const QList<ShortcutHints::Entry> entries = ShortcutHints::keymapEntries(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        const QList<Hint> hints = ShortcutHints::hints(entries, {}, KeyStyle::Typed);
        const Hint *g = findHint(hints, QStringLiteral("g"));
        QVERIFY(g);
        QVERIFY(g->prefix);
        QCOMPARE(g->description, QStringLiteral("+8 commands"));
        // Its bindings are in several groups.
        QCOMPARE(g->group, QStringLiteral("More keys"));
        const Hint *sort = findHint(hints, QStringLiteral(","));
        QVERIFY(sort && sort->prefix);
        QCOMPARE(sort->description, QStringLiteral("+5 commands"));
        QCOMPARE(sort->group, ShortcutHints::commandGroup(Command::SortByName));
        const Hint *trash = findHint(hints, QStringLiteral("d"));
        QVERIFY(trash && trash->prefix);
        QCOMPARE(trash->description, QStringLiteral("+1 command"));
        const Hint *down = findHint(hints, QStringLiteral("j"));
        QVERIFY(down && !down->prefix);
        QCOMPARE(down->description, YaziMode::commandText(Command::MoveDown));
        QCOMPARE(std::count_if(hints.cbegin(),
                               hints.cend(),
                               [](const Hint &hint) {
                                   return hint.keys == QLatin1String("g");
                               }),
                 1);

        // Expanded, as the reference lists them, every binding has its own hint.
        const QList<Hint> expanded = ShortcutHints::hints(entries, {}, KeyStyle::Typed, true);
        QCOMPARE(expanded.size(), entries.size());
        QVERIFY(findHint(expanded, QStringLiteral("gg")));
        QVERIFY(findHint(expanded, QStringLiteral(",n")));
        QVERIFY(std::none_of(expanded.cbegin(), expanded.cend(), [](const Hint &hint) {
            return hint.prefix;
        }));
    }

    void aCountListsOnlyTheCommandsItRepeats()
    {
        const QList<ShortcutHints::Entry> entries = ShortcutHints::keymapEntries(VimNavigation::keymap(), true);
        QVERIFY(!entries.isEmpty());
        const QList<Hint> hints = ShortcutHints::hints(entries, {}, KeyStyle::Typed);
        QVERIFY(findHint(hints, QStringLiteral("j")));
        QVERIFY(!findHint(hints, QStringLiteral("h")));
        QVERIFY(!findHint(hints, QStringLiteral("g")));
        QVERIFY(!findHint(hints, QStringLiteral("/")));
    }

    void windowShortcutsUseNativeText()
    {
        const QList<ShortcutHints::Entry> entries = {
            {ShortcutHints::chords(QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_T)), QStringLiteral("New Tab"), QStringLiteral("File")},
            {ShortcutHints::chords(QKeySequence(Qt::CTRL | Qt::Key_K, QKeyCombination(Qt::Key_D), QKeyCombination(Qt::Key_D))),
             QStringLiteral("Delete"),
             QStringLiteral("Edit")},
            {ShortcutHints::chords(QKeySequence(Qt::CTRL | Qt::Key_K, QKeyCombination(Qt::Key_D), QKeyCombination(Qt::Key_X))),
             QStringLiteral("Cut"),
             QStringLiteral("Edit")},
            {ShortcutHints::chords(QKeySequence(Qt::CTRL | Qt::Key_J)), QStringLiteral("Other"), QStringLiteral("File")},
        };
        const QList<Hint> hints = ShortcutHints::hints(entries, {key(Qt::Key_K, Qt::ControlModifier)}, KeyStyle::Native);
        QCOMPARE(hints.size(), 2);
        QCOMPARE(hints.at(0), (Hint{QKeySequence(Qt::CTRL | Qt::Key_T).toString(QKeySequence::NativeText), QStringLiteral("New Tab"), QStringLiteral("File")}));
        QCOMPARE(hints.at(1).keys, QStringLiteral("D"));
        QVERIFY(hints.at(1).prefix);
        QCOMPARE(hints.at(1).description, QStringLiteral("+2 commands"));
        QCOMPARE(hints.at(1).group, QStringLiteral("Edit"));

        const QList<Hint> deeper = ShortcutHints::hints(entries, {key(Qt::Key_K, Qt::ControlModifier), key(Qt::Key_D)}, KeyStyle::Native);
        QCOMPARE(deeper.size(), 2);
        QCOMPARE(deeper.at(0).keys, QStringLiteral("D"));
        QCOMPARE(deeper.at(1).keys, QStringLiteral("X"));

        const QList<Hint> expanded = ShortcutHints::hints(entries, {}, KeyStyle::Native, true);
        QCOMPARE(expanded.size(), 4);
        QCOMPARE(expanded.at(0).keys, QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_T).toString(QKeySequence::NativeText));
    }

    void continuesComparesNormalizedKeys()
    {
        using ShortcutHints::continues;
        QVERIFY(continues({key(Qt::Key_G), key(Qt::Key_G)}, {key(Qt::Key_G)}));
        QVERIFY(!continues({key(Qt::Key_G)}, {key(Qt::Key_G)}));
        QVERIFY(!continues({key(Qt::Key_G), key(Qt::Key_G)}, {key(Qt::Key_H)}));
        QVERIFY(continues({key(Qt::Key_G), key(Qt::Key_G)}, {}));
        // Keypad keys and Shift with punctuation are the same keys.
        QVERIFY(continues({key(Qt::Key_Slash), key(Qt::Key_G)}, {key(Qt::Key_Slash, Qt::KeypadModifier)}));
        QVERIFY(continues({key(Qt::Key_Comma), key(Qt::Key_N)}, {key(Qt::Key_Comma, Qt::ShiftModifier)}));
    }

    void pagesHintsWithHeadings()
    {
        const auto hint = [](const QString &keys, const QString &group) {
            return Hint{keys, keys, group};
        };
        const QList<Hint> hints = {hint(u"a"_s, u"A"_s),
                                   hint(u"b"_s, u"A"_s),
                                   hint(u"c"_s, u"A"_s),
                                   hint(u"d"_s, u"B"_s),
                                   hint(u"e"_s, u"B"_s),
                                   hint(u"f"_s, u"B"_s),
                                   hint(u"g"_s, u"B"_s)};
        const QList<ShortcutHints::Page> pages = ShortcutHints::pages(hints, 3, 2);
        QCOMPARE(pages.size(), 2);
        // A column continues its group without a heading.
        const ShortcutHints::Page &first = pages.at(0);
        QCOMPARE(first.size(), 2);
        QCOMPARE(first.at(0).size(), 3);
        QVERIFY(first.at(0).at(0).isHeading());
        QCOMPARE(first.at(0).at(0).heading, u"A"_s);
        QCOMPARE(first.at(0).at(2).hint.keys, u"b"_s);
        // c continues A in the next column, then B's heading and d fit.
        QCOMPARE(first.at(1).size(), 3);
        QCOMPARE(first.at(1).at(0).hint.keys, u"c"_s);
        QCOMPARE(first.at(1).at(1).heading, u"B"_s);
        QCOMPARE(first.at(1).at(2).hint.keys, u"d"_s);
        // A page that begins within a group repeats its heading.
        const ShortcutHints::Page &second = pages.at(1);
        QCOMPARE(second.at(0).at(0).heading, u"B"_s);
        QCOMPARE(second.at(0).at(1).hint.keys, u"e"_s);
        QCOMPARE(second.at(0).at(2).hint.keys, u"f"_s);
        QCOMPARE(second.at(1).size(), 1);
        QCOMPARE(second.at(1).at(0).hint.keys, u"g"_s);

        // A heading that would end a column moves to the next one.
        const QList<ShortcutHints::Page> moved = ShortcutHints::pages({hint(u"a"_s, u"A"_s), hint(u"b"_s, u"B"_s)}, 3, 2);
        QCOMPARE(moved.size(), 1);
        QCOMPARE(moved.at(0).at(0).size(), 2);
        QCOMPARE(moved.at(0).at(1).at(0).heading, u"B"_s);

        QVERIFY(ShortcutHints::pages({}, 3, 2).isEmpty());
        // Too few rows or columns still fit a heading and a hint.
        QCOMPARE(ShortcutHints::pages(hints, 0, 0).size(), 7);
    }

    void tracksAPendingWindowSequence()
    {
        const QList<QKeySequence> shortcuts = {QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_T),
                                               QKeySequence(Qt::CTRL | Qt::Key_K, QKeyCombination(Qt::Key_D), QKeyCombination(Qt::Key_D)),
                                               QKeySequence(Qt::CTRL | Qt::Key_J)};
        const QKeyCombination ctrlK = key(Qt::Key_K, Qt::ControlModifier);
        ShortcutHints::WindowSequenceTracker tracker;

        // A key whose press reaches a widget, or that runs a shortcut, begins nothing.
        tracker.shortcutOverride(ctrlK);
        QVERIFY(!tracker.keyPressed(ctrlK));
        QVERIFY(!tracker.settle(shortcuts));
        QVERIFY(!tracker.isPending());
        tracker.shortcutOverride(key(Qt::Key_J, Qt::ControlModifier));
        QVERIFY(!tracker.shortcutActivated());
        QVERIFY(!tracker.settle(shortcuts));
        // A key Qt took that begins no shortcut.
        tracker.shortcutOverride(key(Qt::Key_X));
        QVERIFY(!tracker.settle(shortcuts));

        // Modifier keys are ignored.
        QVERIFY(!tracker.shortcutOverride(key(Qt::Key_Control, Qt::ControlModifier)));
        tracker.shortcutOverride(ctrlK);
        QVERIFY(!tracker.keyPressed(key(Qt::Key_Control, Qt::ControlModifier)));
        QVERIFY(tracker.settle(shortcuts));
        QVERIFY(tracker.isPending());
        QCOMPARE(tracker.pending(), QList<QKeyCombination>{ctrlK});

        // Releasing the first key adds nothing, and neither do modifiers and auto-repeat.
        QVERIFY(!tracker.keyReleased(ctrlK, false, shortcuts));
        QVERIFY(!tracker.keyReleased(key(Qt::Key_Control), false, shortcuts));
        QVERIFY(!tracker.keyReleased(key(Qt::Key_D), true, shortcuts));
        QCOMPARE(tracker.pending().size(), 1);
        // The next key continues the sequence.
        QVERIFY(tracker.keyReleased(key(Qt::Key_D), false, shortcuts));
        QCOMPARE(tracker.pending(), (QList<QKeyCombination>{ctrlK, key(Qt::Key_D)}));
        // One that doesn't ends it, as it ends Qt's.
        QVERIFY(tracker.keyReleased(key(Qt::Key_Escape), false, shortcuts));
        QVERIFY(!tracker.isPending());

        // A shortcut that completes it ends it.
        tracker.shortcutOverride(ctrlK);
        QVERIFY(tracker.settle(shortcuts));
        QVERIFY(tracker.shortcutActivated());
        QVERIFY(!tracker.isPending());
        // So does a key press reaching a widget, or Qt offering a key, which it does only while nothing is pending.
        tracker.shortcutOverride(ctrlK);
        QVERIFY(tracker.settle(shortcuts));
        QVERIFY(tracker.keyPressed(key(Qt::Key_A)));
        tracker.shortcutOverride(ctrlK);
        QVERIFY(tracker.settle(shortcuts));
        QVERIFY(tracker.shortcutOverride(key(Qt::Key_A)));
        QVERIFY(!tracker.isPending());

        // The first key can be released before the tracker settles; the same key can then follow it.
        const QList<QKeySequence> twice = {QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_K, QKeyCombination(Qt::Key_A))};
        tracker.shortcutOverride(ctrlK);
        QVERIFY(!tracker.keyReleased(ctrlK, false, twice));
        QVERIFY(tracker.settle(twice));
        QVERIFY(tracker.keyReleased(ctrlK, false, twice));
        QCOMPARE(tracker.pending().size(), 2);
        tracker.reset();
        QVERIFY(!tracker.isPending());
    }

    void matcherTellsWhatContinuesASequence()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QVERIFY(!matcher.continues(key(Qt::Key_G)));
        QCOMPARE(matcher.feed(key(Qt::Key_G)), YaziMode::KeyMatcher::Result::Pending);
        QVERIFY(matcher.continues(key(Qt::Key_G)));
        QVERIFY(matcher.continues(key(Qt::Key_T, Qt::ShiftModifier)));
        QVERIFY(!matcher.continues(key(Qt::Key_J)));
        QVERIFY(!matcher.continues(key(Qt::Key_PageDown)));
        QVERIFY(!matcher.continues(key(Qt::Key_Escape)));
        QVERIFY(!matcher.continues(key(Qt::Key_Shift, Qt::ShiftModifier)));
        // Testing changes nothing.
        QCOMPARE(matcher.pendingText(), u"g"_s);
        matcher.reset();
        // Digits continue a count.
        QCOMPARE(matcher.feed(key(Qt::Key_5)), YaziMode::KeyMatcher::Result::Pending);
        QVERIFY(matcher.continues(key(Qt::Key_0)));
        QVERIFY(matcher.continues(key(Qt::Key_J)));
        QVERIFY(!matcher.continues(key(Qt::Key_Q, Qt::ShiftModifier)));
    }
};

QTEST_MAIN(ShortcutHintsTest)

#include "shortcuthintstest.moc"
