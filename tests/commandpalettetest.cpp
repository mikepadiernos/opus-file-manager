// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandpalette.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>

using CommandPalette::Candidate;
using CommandPalette::Group;
using CommandPalette::Options;

class CommandPaletteTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void recognizesSavedCommands()
    {
        QVERIFY(CommandPalette::isSavedCommand(QStringLiteral("custom_command_preset_0a1b")));
        QVERIFY(!CommandPalette::isSavedCommand(QStringLiteral("custom_command")));
        QVERIFY(!CommandPalette::isSavedCommand(QString()));
    }

    void listsOnlyEnabledVisibleNamedActions()
    {
        const Candidate copy{QStringLiteral("edit_copy"), QStringLiteral("&Copy"), {QStringLiteral("&Edit")}, {}, true, true};
        QVERIFY(CommandPalette::isListed(copy));

        Candidate disabled = copy;
        disabled.enabled = false;
        QVERIFY(!CommandPalette::isListed(disabled));

        Candidate hidden = copy;
        hidden.visible = false;
        QVERIFY(!CommandPalette::isListed(hidden));

        Candidate textless = copy;
        textless.text = QStringLiteral("  ");
        QVERIFY(!CommandPalette::isListed(textless));

        // Menu items without a name, such as recent folders, are listed.
        Candidate unnamed = copy;
        unnamed.name.clear();
        QVERIFY(CommandPalette::isListed(unnamed));
    }

    void leavesOutItselfAndOptionallySavedCommands()
    {
        const Candidate palette{QString::fromLatin1(CommandPalette::ActionName), QStringLiteral("Command Palette…"), {QStringLiteral("&Help")}, {}, true, true};
        QVERIFY(!CommandPalette::isListed(palette));
        Options other;
        other.paletteAction = QStringLiteral("other_palette");
        QVERIFY(CommandPalette::isListed(palette, other));

        const Candidate saved{QStringLiteral("custom_command_preset_ab"), QStringLiteral("Saved Command: Build"), {}, {}, true, true};
        QVERIFY(CommandPalette::isListed(saved));
        Options withoutSaved;
        withoutSaved.savedCommands = false;
        QVERIFY(!CommandPalette::isListed(saved, withoutSaved));
        QVERIFY(CommandPalette::isListed({QStringLiteral("custom_command"), QStringLiteral("Scripts…"), {}, {}, true, true}, withoutSaved));
    }

    void namesGroupsByMenuPathOrToolBar()
    {
        QCOMPARE(CommandPalette::groupName({QStringLiteral("protect_folder"),
                                            QStringLiteral("Protect…"),
                                            {QStringLiteral("&File"), QStringLiteral("Password &Protection")},
                                            QStringLiteral("Main Toolbar"),
                                            true,
                                            true}),
                 QStringLiteral("File › Password Protection"));
        QCOMPARE(CommandPalette::groupName({QStringLiteral("go_up"), QStringLiteral("Up"), {}, QStringLiteral("&Main Toolbar"), true, true}),
                 QStringLiteral("Main Toolbar"));
        // Empty titles are skipped.
        QCOMPARE(CommandPalette::groupName({QStringLiteral("a"), QStringLiteral("A"), {QString(), QStringLiteral("&View")}, {}, true, true}),
                 QStringLiteral("View"));
        QCOMPARE(CommandPalette::groupName({QStringLiteral("custom_command_preset_ab"), QStringLiteral("Saved Command: Build"), {}, {}, true, true}),
                 QString());
        QCOMPARE(CommandPalette::groupName({QStringLiteral("a"), QStringLiteral("A"), {}, {}, true, true}), QString());
    }

    void groupsListedCandidatesInOrderOfFirstAppearance()
    {
        const QList<Candidate> candidates{
            {QStringLiteral("new_folder"), QStringLiteral("New Folder…"), {QStringLiteral("&File")}, {}, true, true},
            {QStringLiteral("edit_copy"), QStringLiteral("&Copy"), {QStringLiteral("&Edit")}, {}, false, true},
            {QStringLiteral("edit_paste"), QStringLiteral("&Paste"), {QStringLiteral("&Edit")}, {}, true, true},
            {QStringLiteral("file_quit"), QStringLiteral("&Quit"), {QStringLiteral("&File")}, {}, true, true},
            {QStringLiteral("go_up"), QStringLiteral("Up"), {}, QStringLiteral("Main Toolbar"), true, true},
            {QString::fromLatin1(CommandPalette::ActionName), QStringLiteral("Command Palette…"), {QStringLiteral("&Help")}, {}, true, true},
            {QStringLiteral("custom_command_preset_ab"), QStringLiteral("Saved Command: Build"), {}, {}, true, true},
        };
        QCOMPARE(CommandPalette::groups(candidates),
                 (QList<Group>{{QStringLiteral("File"), {0, 3}}, {QStringLiteral("Edit"), {2}}, {QStringLiteral("Main Toolbar"), {4}}, {QString(), {6}}}));
        Options withoutSaved;
        withoutSaved.savedCommands = false;
        QCOMPARE(CommandPalette::groups(candidates, withoutSaved).size(), 3);
        QVERIFY(CommandPalette::groups({}).isEmpty());
    }

    void readsAndClearsTheCommandBarHistory()
    {
        KConfigGroup state = KSharedConfig::openStateConfig()->group(QString::fromLatin1(CommandPalette::HistoryGroup));
        CommandPalette::clearHistory(state);
        QVERIFY(CommandPalette::history(state).isEmpty());

        const QStringList stored{QStringLiteral("&Copy"), QStringLiteral("New &Folder…"), QStringLiteral("Copy"), QStringLiteral(" ")};
        state.writeEntry(CommandPalette::HistoryKey, stored);
        QCOMPARE(CommandPalette::history(state), stored);
        QCOMPARE(CommandPalette::historyText(stored), (QStringList{QStringLiteral("Copy"), QStringLiteral("New Folder…")}));

        CommandPalette::clearHistory(state);
        QVERIFY(!state.hasKey(CommandPalette::HistoryKey));
        QVERIFY(CommandPalette::history(KSharedConfig::openStateConfig()->group(QString::fromLatin1(CommandPalette::HistoryGroup))).isEmpty());
    }
};

QTEST_GUILESS_MAIN(CommandPaletteTest)
#include "commandpalettetest.moc"
