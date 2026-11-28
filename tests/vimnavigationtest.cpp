// SPDX-License-Identifier: GPL-3.0-or-later

#include "vimnavigation.h"

#include <QStandardPaths>
#include <QTest>

using YaziMode::Command;
using Result = YaziMode::KeyMatcher::Result;

Q_DECLARE_METATYPE(YaziMode::Command)

class VimNavigationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void keymapHasNoConflictsAndTakesCounts()
    {
        const YaziMode::Keymap keymap = VimNavigation::keymap();
        QVERIFY(keymap.counts);
        QVERIFY(YaziMode::conflicts(keymap).isEmpty());
        // Every binding is a plain key or a key with Ctrl, never Tab, Escape or an arrow key that the views keep.
        for (const YaziMode::Binding &binding : keymap.bindings) {
            for (QKeyCombination key : binding.keys) {
                QVERIFY(key.key() != Qt::Key_Tab && key.key() != Qt::Key_Backtab && key.key() != Qt::Key_Escape);
                QVERIFY(key.key() < Qt::Key_Left || key.key() > Qt::Key_Down);
                QVERIFY(key.key() != Qt::Key_Return && key.key() != Qt::Key_Enter);
            }
        }
    }

    void matchesKeysSequencesAndCounts_data()
    {
        QTest::addColumn<QList<QKeyCombination>>("keys");
        QTest::addColumn<Command>("command");
        QTest::addColumn<int>("count");

        QTest::newRow("j") << QList<QKeyCombination>{Qt::Key_J} << Command::MoveDown << 1;
        QTest::newRow("k") << QList<QKeyCombination>{Qt::Key_K} << Command::MoveUp << 1;
        QTest::newRow("5j") << QList<QKeyCombination>{Qt::Key_5, Qt::Key_J} << Command::MoveDown << 5;
        QTest::newRow("12k") << QList<QKeyCombination>{Qt::Key_1, Qt::Key_2, Qt::Key_K} << Command::MoveUp << 12;
        QTest::newRow("gg") << QList<QKeyCombination>{Qt::Key_G, Qt::Key_G} << Command::MoveFirst << 1;
        QTest::newRow("G") << QList<QKeyCombination>{Qt::ShiftModifier | Qt::Key_G} << Command::MoveLast << 1;
        QTest::newRow("Ctrl+D") << QList<QKeyCombination>{Qt::ControlModifier | Qt::Key_D} << Command::HalfPageDown << 1;
        QTest::newRow("Ctrl+U") << QList<QKeyCombination>{Qt::ControlModifier | Qt::Key_U} << Command::HalfPageUp << 1;
        QTest::newRow("J") << QList<QKeyCombination>{Qt::ShiftModifier | Qt::Key_J} << Command::HalfPageDown << 1;
        QTest::newRow("3K") << QList<QKeyCombination>{Qt::Key_3, Qt::ShiftModifier | Qt::Key_K} << Command::HalfPageUp << 3;
        QTest::newRow("h") << QList<QKeyCombination>{Qt::Key_H} << Command::Parent << 1;
        QTest::newRow("l") << QList<QKeyCombination>{Qt::Key_L} << Command::Open << 1;
        QTest::newRow("H") << QList<QKeyCombination>{Qt::ShiftModifier | Qt::Key_H} << Command::Back << 1;
        QTest::newRow("L") << QList<QKeyCombination>{Qt::ShiftModifier | Qt::Key_L} << Command::Forward << 1;
        QTest::newRow("gh") << QList<QKeyCombination>{Qt::Key_G, Qt::Key_H} << Command::Home << 1;
        QTest::newRow("gt") << QList<QKeyCombination>{Qt::Key_G, Qt::Key_T} << Command::NextTab << 1;
        QTest::newRow("gT") << QList<QKeyCombination>{Qt::Key_G, Qt::ShiftModifier | Qt::Key_T} << Command::PreviousTab << 1;
        QTest::newRow("/") << QList<QKeyCombination>{Qt::Key_Slash} << Command::Filter << 1;
        // Layouts that need Shift for / still filter.
        QTest::newRow("Shift+/") << QList<QKeyCombination>{Qt::ShiftModifier | Qt::Key_Slash} << Command::Filter << 1;
        QTest::newRow("\\") << QList<QKeyCombination>{Qt::Key_Backslash} << Command::ClearFilter << 1;
        // A broken sequence drops the g and matches the next key on its own.
        QTest::newRow("g then j") << QList<QKeyCombination>{Qt::Key_G, Qt::Key_J} << Command::MoveDown << 1;
    }

    void matchesKeysSequencesAndCounts()
    {
        QFETCH(QList<QKeyCombination>, keys);
        QFETCH(Command, command);
        QFETCH(int, count);
        YaziMode::KeyMatcher matcher(VimNavigation::keymap());
        for (qsizetype i = 0; i + 1 < keys.size(); ++i) {
            QCOMPARE(matcher.feed(keys.at(i)), Result::Pending);
        }
        QCOMPARE(matcher.feed(keys.last()), Result::Matched);
        QCOMPARE(matcher.command(), command);
        QCOMPARE(YaziMode::commandTakesCount(command) ? matcher.count() : 1, count);
        QVERIFY(!matcher.isPending());
    }

    void otherKeysPassThrough()
    {
        YaziMode::KeyMatcher matcher(VimNavigation::keymap());
        // Unbound letters still filter as you type; the views keep their own keys.
        for (QKeyCombination key : {QKeyCombination(Qt::Key_A),
                                    QKeyCombination(Qt::Key_X),
                                    QKeyCombination(Qt::Key_0),
                                    QKeyCombination(Qt::Key_Down),
                                    QKeyCombination(Qt::Key_Return),
                                    QKeyCombination(Qt::Key_Tab),
                                    QKeyCombination(Qt::ControlModifier | Qt::Key_C),
                                    QKeyCombination(Qt::AltModifier | Qt::Key_J)}) {
            QCOMPARE(matcher.test(key), Result::NoMatch);
        }
        // A pending count is cancelled by Escape.
        QCOMPARE(matcher.feed(Qt::Key_4), Result::Pending);
        QCOMPARE(matcher.pendingText(), QStringLiteral("4"));
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::Cancelled);
        QVERIFY(!matcher.isPending());
    }

    void modifiedKeysYieldToShortcuts()
    {
        QVERIFY(VimNavigation::yieldsToShortcuts(Qt::ControlModifier | Qt::Key_D));
        QVERIFY(VimNavigation::yieldsToShortcuts(Qt::AltModifier | Qt::Key_J));
        QVERIFY(VimNavigation::yieldsToShortcuts(Qt::MetaModifier | Qt::Key_K));
        QVERIFY(!VimNavigation::yieldsToShortcuts(Qt::Key_J));
        QVERIFY(!VimNavigation::yieldsToShortcuts(Qt::ShiftModifier | Qt::Key_G));
        QVERIFY(!VimNavigation::yieldsToShortcuts(Qt::Key_Slash));
    }

    void splitsShadowedAndYieldedShortcuts()
    {
        const YaziMode::Keymap keymap = VimNavigation::keymap();
        const QList<YaziMode::ActionShortcuts> actions{
            {QStringLiteral("add_to_favorites"), QStringLiteral("Add to Favorites"), {QKeySequence(Qt::CTRL | Qt::Key_D)}},
            {QStringLiteral("open_gallery"), QStringLiteral("Open Gallery"), {QKeySequence(Qt::Key_G), QKeySequence(Qt::Key_F7)}},
            {QStringLiteral("jump"), QStringLiteral("Jump"), {QKeySequence(Qt::Key_J, Qt::Key_X)}},
            {QStringLiteral("third"), QStringLiteral("Third Tab"), {QKeySequence(Qt::Key_3)}},
            {QStringLiteral("copy"), QStringLiteral("Copy"), {QKeySequence(Qt::CTRL | Qt::Key_C)}},
            {QStringLiteral("unbound"), QStringLiteral("Unbound"), {QKeySequence(Qt::Key_X)}},
        };

        // Plain keys take over the shortcuts that begin with them, including digits that begin a count.
        const QList<YaziMode::ShadowedShortcut> shadowed = VimNavigation::shadowedShortcuts(keymap, actions);
        QCOMPARE(shadowed.size(), 3);
        QCOMPARE(shadowed.at(0).name, QStringLiteral("open_gallery"));
        QCOMPARE(shadowed.at(0).shortcut, QKeySequence(Qt::Key_G));
        QCOMPARE(shadowed.at(1).name, QStringLiteral("jump"));
        QCOMPARE(shadowed.at(2).name, QStringLiteral("third"));
        QVERIFY(shadowed.at(2).bindings.isEmpty());

        // Keys with Ctrl leave the window's shortcut alone.
        const QList<YaziMode::ShadowedShortcut> yielded = VimNavigation::yieldedShortcuts(keymap, actions);
        QCOMPARE(yielded.size(), 1);
        QCOMPARE(yielded.first().name, QStringLiteral("add_to_favorites"));
        QCOMPARE(yielded.first().bindings.size(), 1);
        QCOMPARE(keymap.bindings.at(yielded.first().bindings.first()).command, Command::HalfPageDown);
    }
};

QTEST_GUILESS_MAIN(VimNavigationTest)

#include "vimnavigationtest.moc"
