// SPDX-License-Identifier: GPL-3.0-or-later

#include "keybindings.h"

#include <QStandardPaths>
#include <QTest>

using KeyBindings::Binding;
using KeyBindings::Conflict;
using KeyBindings::ConflictKind;

class KeyBindingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void emptySequencesAreIgnored()
    {
        QCOMPARE(KeyBindings::keys({QKeySequence(), QKeySequence(Qt::Key_F5), QKeySequence()}), QList<QKeySequence>{QKeySequence(Qt::Key_F5)});
        QCOMPARE(KeyBindings::keys({}), QList<QKeySequence>());
    }

    void customizedBindingsDifferFromTheirDefaults()
    {
        const QKeySequence f8(Qt::Key_F8);
        const QKeySequence del(Qt::Key_Delete);
        const QList<Binding> bindings{
            {QStringLiteral("unchanged"), QStringLiteral("Unchanged"), {f8, del}, {f8, del}},
            // An empty alternate is the same as none.
            {QStringLiteral("empty_alternate"), QStringLiteral("Empty Alternate"), {f8, QKeySequence()}, {f8}},
            {QStringLiteral("removed"), QStringLiteral("Removed"), {}, {f8}},
            {QStringLiteral("added"), QStringLiteral("Added"), {f8}, {}},
            // Swapping the primary and alternate shortcuts is a change.
            {QStringLiteral("swapped"), QStringLiteral("Swapped"), {del, f8}, {f8, del}},
            {QStringLiteral("unbound"), QStringLiteral("Unbound"), {}, {}},
        };
        QVERIFY(!KeyBindings::isCustomized(bindings.at(0)));
        QVERIFY(!KeyBindings::isCustomized(bindings.at(1)));
        QCOMPARE(KeyBindings::customized(bindings), (QList<int>{2, 3, 4}));
    }

    void sequencesStartWithShorterPrefixes()
    {
        const QKeySequence chord(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_C);
        QVERIFY(KeyBindings::startsWith(chord, QKeySequence(Qt::CTRL | Qt::Key_K)));
        QVERIFY(!KeyBindings::startsWith(chord, QKeySequence(Qt::CTRL | Qt::Key_C)));
        QVERIFY(!KeyBindings::startsWith(chord, chord));
        QVERIFY(!KeyBindings::startsWith(QKeySequence(Qt::CTRL | Qt::Key_K), chord));
        QVERIFY(!KeyBindings::startsWith(chord, QKeySequence()));
        QVERIFY(KeyBindings::startsWith(QKeySequence(Qt::Key_G, Qt::Key_G, Qt::Key_H), QKeySequence(Qt::Key_G, Qt::Key_G)));
    }

    void conflictsFindSameShortcutsAndChordPrefixes()
    {
        const QKeySequence ctrlK(Qt::CTRL | Qt::Key_K);
        const QKeySequence chord(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_C);
        const QKeySequence f5(Qt::Key_F5);
        const QList<Binding> bindings{
            {QStringLiteral("copy_to"), QStringLiteral("Copy To"), {f5}, {f5}},
            {QStringLiteral("comment"), QStringLiteral("Comment"), {chord}, {}},
            {QStringLiteral("reload"), QStringLiteral("Reload"), {f5}, {}},
            {QStringLiteral("kill"), QStringLiteral("Kill"), {ctrlK}, {}},
            {QStringLiteral("unrelated"), QStringLiteral("Unrelated"), {QKeySequence(Qt::CTRL | Qt::Key_C)}, {}},
        };
        const QList<Conflict> expected{
            {0, f5, 2, f5, ConflictKind::Same},
            // The shorter shortcut comes first, whichever binding is listed first.
            {3, ctrlK, 1, chord, ConflictKind::Prefix},
        };
        QCOMPARE(KeyBindings::conflicts(bindings), expected);
    }

    void aBindingCanConflictWithItsOwnAlternate()
    {
        const QKeySequence ctrlK(Qt::CTRL | Qt::Key_K);
        const QKeySequence chord(Qt::CTRL | Qt::Key_K, QKeyCombination(Qt::Key_D));
        const QList<Binding> bindings{
            {QStringLiteral("delete"), QStringLiteral("Delete"), {chord, ctrlK}, {}},
            // A shortcut listed twice for one action is harmless.
            {QStringLiteral("twice"), QStringLiteral("Twice"), {QKeySequence(Qt::Key_F2), QKeySequence(Qt::Key_F2)}, {}},
        };
        QCOMPARE(KeyBindings::conflicts(bindings), (QList<Conflict>{{0, ctrlK, 0, chord, ConflictKind::Prefix}}));
    }

    void emptyShortcutsNeverConflict()
    {
        const QList<Binding> bindings{
            {QStringLiteral("a"), QStringLiteral("A"), {QKeySequence()}, {}},
            {QStringLiteral("b"), QStringLiteral("B"), {QKeySequence()}, {}},
        };
        QVERIFY(KeyBindings::conflicts(bindings).isEmpty());
    }

    void conflictTextNamesBothActions()
    {
        const QKeySequence key(Qt::META | Qt::ALT | Qt::Key_O);
        QList<Binding> bindings{
            {QStringLiteral("toggle_window_visibility"), QStringLiteral("Show or Hide"), {key}, {key}, true},
            {QStringLiteral("open"), QStringLiteral("Open"), {key}, {}},
        };
        const QList<Conflict> found = KeyBindings::conflicts(bindings);
        QCOMPARE(found.size(), 1);
        const QString keyText = key.toString(QKeySequence::NativeText);
        QString text = KeyBindings::conflictText(found.first(), bindings);
        QVERIFY2(text.contains(keyText) && text.contains(QStringLiteral("“Show or Hide”")) && text.contains(QStringLiteral("“Open”")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("global")), qPrintable(text));

        bindings[0].global = false;
        text = KeyBindings::conflictText(found.first(), bindings);
        QVERIFY2(!text.contains(QStringLiteral("global")) && text.contains(QStringLiteral("neither")), qPrintable(text));

        const QKeySequence chord(Qt::META | Qt::ALT | Qt::Key_O, QKeyCombination(Qt::Key_P));
        bindings[1].shortcuts = {chord};
        const QList<Conflict> prefix = KeyBindings::conflicts(bindings);
        QCOMPARE(prefix.size(), 1);
        text = KeyBindings::conflictText(prefix.first(), bindings);
        QVERIFY2(text.contains(chord.toString(QKeySequence::NativeText)) && text.contains(QStringLiteral("can't be typed")), qPrintable(text));
    }
};

QTEST_GUILESS_MAIN(KeyBindingsTest)

#include "keybindingstest.moc"
