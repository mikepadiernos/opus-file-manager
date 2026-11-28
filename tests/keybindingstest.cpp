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
            // Ctrl+C is this binding's default, so keeping it from text fields isn't a conflict.
            {QStringLiteral("unrelated"), QStringLiteral("Unrelated"), {QKeySequence(Qt::CTRL | Qt::Key_C)}, {QKeySequence(Qt::CTRL | Qt::Key_C)}},
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

    void contextsOverlapWithinTheMainWindowOnly()
    {
        using KeyBindings::Context;
        const QList<Context> contexts{Context::Application, Context::FileView, Context::FolderTree, Context::Viewer, Context::TextInput};
        for (Context context : contexts) {
            QVERIFY(KeyBindings::overlaps(context, context));
            QVERIFY(!KeyBindings::contextText(context).isEmpty());
        }
        for (Context context : {Context::FileView, Context::FolderTree, Context::TextInput}) {
            QVERIFY(KeyBindings::overlaps(Context::Application, context));
            QVERIFY(KeyBindings::overlaps(context, Context::Application));
            QVERIFY(!KeyBindings::overlaps(Context::Viewer, context));
        }
        QVERIFY(!KeyBindings::overlaps(Context::Application, Context::Viewer));
        QVERIFY(!KeyBindings::overlaps(Context::FileView, Context::FolderTree));
        QVERIFY(!KeyBindings::overlaps(Context::FileView, Context::TextInput));
        QVERIFY(!KeyBindings::overlaps(Context::FolderTree, Context::TextInput));
    }

    void typingNavigationAndAccessibilityKeys()
    {
        // Typing and editing keys, with or without Shift.
        for (QKeyCombination key : {QKeyCombination(Qt::Key_F),
                                    QKeyCombination(Qt::SHIFT, Qt::Key_F),
                                    QKeyCombination(Qt::Key_Space),
                                    QKeyCombination(Qt::Key_5),
                                    QKeyCombination(Qt::KeypadModifier, Qt::Key_5),
                                    QKeyCombination(Qt::Key_Backspace),
                                    QKeyCombination(Qt::Key_Delete),
                                    QKeyCombination(Qt::SHIFT, Qt::Key_Left),
                                    QKeyCombination(Qt::Key_Tab),
                                    QKeyCombination(Qt::SHIFT, Qt::Key_Backtab),
                                    QKeyCombination(Qt::CTRL, Qt::Key_C),
                                    QKeyCombination(Qt::CTRL, Qt::Key_Backspace),
                                    QKeyCombination(Qt::CTRL | Qt::SHIFT, Qt::Key_Z)}) {
            QVERIFY2(KeyBindings::isTypingKey(key), qPrintable(QKeySequence(key).toString()));
        }
        for (QKeyCombination key : {QKeyCombination(Qt::Key_F5),
                                    QKeyCombination(Qt::Key_Escape),
                                    QKeyCombination(Qt::CTRL, Qt::Key_W),
                                    QKeyCombination(Qt::ALT, Qt::Key_Left),
                                    QKeyCombination(Qt::CTRL, Qt::Key_Tab),
                                    QKeyCombination(Qt::CTRL | Qt::ALT, Qt::Key_F),
                                    QKeyCombination(Qt::META, Qt::Key_E)}) {
            QVERIFY2(!KeyBindings::isTypingKey(key), qPrintable(QKeySequence(key).toString()));
        }

        QVERIFY(KeyBindings::isNavigationKey(Qt::Key_Down));
        QVERIFY(KeyBindings::isNavigationKey(QKeyCombination(Qt::SHIFT, Qt::Key_PageDown)));
        QVERIFY(KeyBindings::isNavigationKey(Qt::Key_Home));
        QVERIFY(!KeyBindings::isNavigationKey(QKeyCombination(Qt::ALT, Qt::Key_Up)));
        QVERIFY(!KeyBindings::isNavigationKey(Qt::Key_F));
        QVERIFY(!KeyBindings::isNavigationKey(Qt::Key_Tab));

        QVERIFY(KeyBindings::isAccessibilityKey(Qt::Key_Tab));
        QVERIFY(KeyBindings::isAccessibilityKey(QKeyCombination(Qt::SHIFT, Qt::Key_Backtab)));
        QVERIFY(KeyBindings::isAccessibilityKey(Qt::Key_Escape));
        QVERIFY(KeyBindings::isAccessibilityKey(Qt::Key_F10));
        QVERIFY(KeyBindings::isAccessibilityKey(QKeyCombination(Qt::SHIFT, Qt::Key_F10)));
        QVERIFY(KeyBindings::isAccessibilityKey(Qt::Key_Menu));
        QVERIFY(!KeyBindings::isAccessibilityKey(QKeyCombination(Qt::CTRL, Qt::Key_Tab)));
        QVERIFY(!KeyBindings::isAccessibilityKey(Qt::Key_F9));

        using KeyBindings::Context;
        QVERIFY(KeyBindings::isReserved(Qt::Key_F, Context::TextInput));
        QVERIFY(!KeyBindings::isReserved(Qt::Key_F, Context::FileView));
        QVERIFY(KeyBindings::isReserved(Qt::Key_Down, Context::FileView));
        QVERIFY(KeyBindings::isReserved(Qt::Key_Down, Context::FolderTree));
        QVERIFY(KeyBindings::isReserved(Qt::Key_Down, Context::TextInput));
        QVERIFY(!KeyBindings::isReserved(Qt::Key_Down, Context::Application));
        QVERIFY(!KeyBindings::isReserved(Qt::Key_Down, Context::Viewer));
    }

    void conflictsNeedOverlappingContexts()
    {
        using KeyBindings::Context;
        const QKeySequence space(Qt::Key_Space);
        const QKeySequence f(Qt::Key_F);
        QList<Binding> bindings{
            {QStringLiteral("open_gallery"), QStringLiteral("Open Gallery"), {space}, {space}},
            {QStringLiteral("gallery_slideshow"), QStringLiteral("Slideshow"), {space}, {space}, false, Context::Viewer},
            {QStringLiteral("tree_action"), QStringLiteral("Tree Action"), {f}, {f}, false, Context::FolderTree},
            {QStringLiteral("view_action"), QStringLiteral("View Action"), {f}, {f}, false, Context::FileView},
        };
        // The gallery is another window, and the focus can't be in the folder tree and a file view at once.
        QVERIFY(KeyBindings::conflicts(bindings).isEmpty());

        // A window shortcut reaches the folder tree.
        bindings.append({QStringLiteral("window_action"), QStringLiteral("Window Action"), {f}, {f}});
        QCOMPARE(KeyBindings::conflicts(bindings), (QList<Conflict>{{2, f, 4, f, ConflictKind::Same}, {3, f, 4, f, ConflictKind::Same}}));
        const QString text = KeyBindings::conflictText(KeyBindings::conflicts(bindings).first(), bindings);
        QVERIFY2(text.contains(KeyBindings::contextText(Context::FolderTree)), qPrintable(text));
        bindings.removeLast();

        // A global shortcut reaches every window.
        bindings.append({QStringLiteral("toggle_window_visibility"), QStringLiteral("Show or Hide"), {space}, {space}, true});
        QCOMPARE(KeyBindings::conflicts(bindings), (QList<Conflict>{{0, space, 4, space, ConflictKind::Same}, {1, space, 4, space, ConflictKind::Same}}));
        QCOMPARE(KeyBindings::contextText(bindings.last()), QStringLiteral("Global"));
    }

    void customizedKeysThatWidgetsKeepAreReported()
    {
        using KeyBindings::Context;
        const QKeySequence down(Qt::Key_Down);
        const QKeySequence tab(Qt::Key_Tab);
        const QKeySequence del(Qt::Key_Delete);
        const QList<Binding> bindings{
            // Default shortcuts are chosen to fit, so they aren't reported.
            {QStringLiteral("move_to_trash"), QStringLiteral("Move to Trash"), {del}, {del}},
            {QStringLiteral("copy_to_other_pane"), QStringLiteral("Copy to Other Pane"), {QKeySequence(Qt::Key_F5), down}, {QKeySequence(Qt::Key_F5)}},
            {QStringLiteral("switch_pane"), QStringLiteral("Switch Pane"), {tab}, {}},
            // Only the first key of a sequence matters.
            {QStringLiteral("chord"), QStringLiteral("Chord"), {QKeySequence(Qt::CTRL | Qt::Key_K, QKeyCombination(Qt::Key_Down))}, {}},
            // The gallery's keys are fixed and global shortcuts are KDE's.
            {QStringLiteral("gallery_next"), QStringLiteral("Next Image"), {QKeySequence(Qt::Key_Right)}, {}, false, Context::Viewer},
            {QStringLiteral("toggle_drop_down"), QStringLiteral("Drop-Down"), {QKeySequence(Qt::Key_F12)}, {}, true},
        };
        const QList<Conflict> expected{
            {1, down, -1, {}, ConflictKind::Reserved, Context::TextInput},
            {1, down, -1, {}, ConflictKind::Reserved, Context::FileView},
            {1, down, -1, {}, ConflictKind::Reserved, Context::FolderTree},
            {2, tab, -1, {}, ConflictKind::Reserved, Context::TextInput},
            {2, tab, -1, {}, ConflictKind::Reserved, Context::Application},
        };
        const QList<Conflict> found = KeyBindings::conflicts(bindings);
        QCOMPARE(found, expected);
        const QStringList texts{KeyBindings::conflictText(found.at(0), bindings),
                                KeyBindings::conflictText(found.at(1), bindings),
                                KeyBindings::conflictText(found.at(2), bindings),
                                KeyBindings::conflictText(found.at(4), bindings)};
        for (const QString &text : texts) {
            QVERIFY2(text.contains(QStringLiteral("“Copy to Other Pane”")) || text.contains(QStringLiteral("“Switch Pane”")), qPrintable(text));
        }
        QVERIFY2(texts.at(0).contains(QStringLiteral("text field")), qPrintable(texts.at(0)));
        QVERIFY2(texts.at(1).contains(QStringLiteral("file view")), qPrintable(texts.at(1)));
        QVERIFY2(texts.at(2).contains(QStringLiteral("folder tree")), qPrintable(texts.at(2)));
        QVERIFY2(texts.at(3).contains(QStringLiteral("without a mouse")), qPrintable(texts.at(3)));
    }
};

QTEST_GUILESS_MAIN(KeyBindingsTest)

#include "keybindingstest.moc"
