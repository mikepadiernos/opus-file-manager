// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationshortcuts.h"

#include <QSet>
#include <QStandardPaths>
#include <QTest>

using KeyBindings::Binding;
using KeyBindings::Context;
using NavigationShortcuts::State;
using NavigationShortcuts::Status;

namespace
{
QKeySequence key(const QString &text)
{
    return QKeySequence(text, QKeySequence::PortableText);
}

Binding binding(const QString &name, const QList<QKeySequence> &shortcuts, const QList<QKeySequence> &defaults = {})
{
    return {name, name, shortcuts, defaults};
}
}

class NavigationShortcutsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void listsEveryAreaWithDistinctExtraKeys()
    {
        const QList<NavigationShortcuts::Action> actions = NavigationShortcuts::actions();
        QSet<QString> names;
        QSet<NavigationShortcuts::Area> areas;
        QList<QKeySequence> extras;
        for (const NavigationShortcuts::Action &action : actions) {
            QVERIFY2(!names.contains(action.name), qPrintable(action.name));
            names.insert(action.name);
            areas.insert(action.area);
            QVERIFY(!NavigationShortcuts::areaText(action.area).isEmpty());
            if (!action.extra.isEmpty()) {
                QCOMPARE(action.extra.count(), 1);
                QVERIFY2(!extras.contains(action.extra), qPrintable(action.extra.toString()));
                extras.append(action.extra);
                QCOMPARE(NavigationShortcuts::extraKey(action.name), action.extra);
            }
        }
        QCOMPARE(areas.size(), 6);
        for (const QString &name : {QStringLiteral("focus_first_pane"),
                                    QStringLiteral("focus_second_pane"),
                                    QStringLiteral("next_tab"),
                                    QStringLiteral("previous_tab"),
                                    QStringLiteral("activate_last_tab"),
                                    QStringLiteral("deselect_all"),
                                    QStringLiteral("invert_selection"),
                                    QStringLiteral("clear_filter"),
                                    QStringLiteral("focus_preview_panel"),
                                    QStringLiteral("focus_terminal")}) {
            QVERIFY2(names.contains(name), qPrintable(name));
        }
        QCOMPARE(NavigationShortcuts::extraKey(QStringLiteral("next_tab")), key(QStringLiteral("Ctrl+PgDown")));
        QCOMPARE(NavigationShortcuts::extraKey(QStringLiteral("focus_terminal")), key(QStringLiteral("Ctrl+Shift+F4")));
        QVERIFY(NavigationShortcuts::extraKey(QStringLiteral("clear_filter")).isEmpty());
        QVERIFY(NavigationShortcuts::extraKey(QStringLiteral("no_such_action")).isEmpty());
    }

    void extraKeysAreNeverKeptByWidgets()
    {
        for (const NavigationShortcuts::Action &action : NavigationShortcuts::actions()) {
            if (action.extra.isEmpty()) {
                continue;
            }
            for (Context context : {Context::FileView, Context::FolderTree, Context::TextInput}) {
                QVERIFY2(!KeyBindings::isReserved(action.extra[0], context), qPrintable(action.extra.toString()));
            }
            QVERIFY2(!KeyBindings::isAccessibilityKey(action.extra[0]), qPrintable(action.extra.toString()));
        }
    }

    void findsTheActionThatUsesAKey()
    {
        const QList<Binding> bindings{
            binding(QStringLiteral("next_tab"), {key(QStringLiteral("Ctrl+Tab")), key(QStringLiteral("Ctrl+PgDown"))}),
            binding(QStringLiteral("close_tab"), {key(QStringLiteral("Ctrl+W"))}),
            binding(QStringLiteral("sequence"), {key(QStringLiteral("Ctrl+Shift+I, X"))}),
            binding(QStringLiteral("prefix"), {key(QStringLiteral("Alt+1"))}),
        };
        // The binding itself doesn't count.
        QCOMPARE(NavigationShortcuts::user(bindings, 0, key(QStringLiteral("Ctrl+PgDown"))), -1);
        QCOMPARE(NavigationShortcuts::user(bindings, 1, key(QStringLiteral("Ctrl+PgDown"))), 0);
        // A longer sequence that begins with the key, and a shorter one that begins it.
        QCOMPARE(NavigationShortcuts::user(bindings, 0, key(QStringLiteral("Ctrl+Shift+I"))), 2);
        QCOMPARE(NavigationShortcuts::user(bindings, 0, key(QStringLiteral("Alt+1, Y"))), 3);
        QCOMPARE(NavigationShortcuts::user(bindings, 0, key(QStringLiteral("Ctrl+Shift+F3"))), -1);
        QCOMPARE(NavigationShortcuts::user(bindings, 0, QKeySequence()), -1);
    }

    void ignoresTheGalleryButNotGlobalShortcuts()
    {
        Binding gallery = binding(QStringLiteral("gallery_next"), {key(QStringLiteral("Alt+2"))});
        gallery.context = Context::Viewer;
        Binding global = binding(QStringLiteral("toggle_window_visibility"), {key(QStringLiteral("Alt+1"))});
        global.global = true;
        global.context = Context::Viewer;
        const QList<Binding> bindings{binding(QStringLiteral("focus_first_pane"), {}), gallery, global};
        QCOMPARE(NavigationShortcuts::user(bindings, 0, key(QStringLiteral("Alt+2"))), -1);
        QCOMPARE(NavigationShortcuts::user(bindings, 0, key(QStringLiteral("Alt+1"))), 2);
    }

    void addsTheExtraKeyOnlyWhenOnAndFree()
    {
        const QList<QKeySequence> base{key(QStringLiteral("Ctrl+Tab"))};
        const QKeySequence extra = key(QStringLiteral("Ctrl+PgDown"));
        QCOMPARE(NavigationShortcuts::defaults(base, extra, false, false), base);
        QCOMPARE(NavigationShortcuts::defaults(base, extra, true, true), base);
        QCOMPARE(NavigationShortcuts::defaults(base, extra, true, false), (QList<QKeySequence>{key(QStringLiteral("Ctrl+Tab")), extra}));
        QCOMPARE(NavigationShortcuts::defaults(base, QKeySequence(), true, false), base);
        QCOMPARE(NavigationShortcuts::defaults({extra}, extra, true, false), QList<QKeySequence>{extra});
        QCOMPARE(NavigationShortcuts::defaults({}, extra, true, false), QList<QKeySequence>{extra});
    }

    void reportsWhatBecameOfTheExtraKey()
    {
        const QKeySequence extra = key(QStringLiteral("Ctrl+Shift+I"));
        QList<Binding> bindings{
            binding(QStringLiteral("invert_selection"), {extra}, {extra}),
            binding(QStringLiteral("other"), {key(QStringLiteral("Ctrl+W"))}),
        };
        QCOMPARE(NavigationShortcuts::state(bindings, 0, {}, true), State{});
        QCOMPARE(NavigationShortcuts::state(bindings, 0, extra, true), (State{Status::Added}));
        QCOMPARE(NavigationShortcuts::state(bindings, 0, extra, false), (State{Status::Off}));

        bindings[0].shortcuts = {key(QStringLiteral("Ctrl+I"))};
        QCOMPARE(NavigationShortcuts::state(bindings, 0, extra, true), (State{Status::Replaced}));

        bindings[0].defaults = {};
        bindings[1].shortcuts = {extra};
        QCOMPARE(NavigationShortcuts::state(bindings, 0, extra, true), (State{Status::Taken, 1}));
        QCOMPARE(NavigationShortcuts::state(bindings, 0, extra, false), (State{Status::Off, 1}));
        QVERIFY(NavigationShortcuts::stateText({Status::Taken, 1}, bindings).contains(QStringLiteral("other")));
        QVERIFY(NavigationShortcuts::stateText({Status::Off, 1}, bindings).contains(QStringLiteral("other")));
        QVERIFY(!NavigationShortcuts::stateText({Status::Taken}, bindings).isEmpty());
        QVERIFY(NavigationShortcuts::stateText({}, bindings).isEmpty());
    }
};

QTEST_GUILESS_MAIN(NavigationShortcutsTest)
#include "navigationshortcutstest.moc"
