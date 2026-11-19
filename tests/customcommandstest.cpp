// SPDX-License-Identifier: GPL-3.0-or-later

#include "customcommands.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class CustomCommandsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void expandsArgumentsWithoutShellParsing()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString dangerous = root.filePath(QStringLiteral("name ; $(touch wrong) 'quoted'"));
        CustomCommands::Command command{QStringLiteral("test"),
                                        QStringLiteral("/usr/bin/printf"),
                                        {QStringLiteral("{path}"), QStringLiteral("{paths}"), QStringLiteral("{dir}"), QStringLiteral("{label}")},
                                        {{QStringLiteral("label"), QStringLiteral("two words; $HOME")}}};
        const auto result = CustomCommands::prepare(command, root.path(), {dangerous, root.filePath(QStringLiteral("second"))});
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.arguments,
                 (QStringList{dangerous, dangerous, root.filePath(QStringLiteral("second")), root.path(), QStringLiteral("two words; $HOME")}));
        QVERIFY(!CustomCommands::prepare(command, root.path(), {}).error.isEmpty());
        command.arguments = {QStringLiteral("prefix-{path}")};
        QVERIFY(!CustomCommands::prepare(command, root.path(), {dangerous}).error.isEmpty());
        command.arguments = {QStringLiteral("{missing}")};
        QVERIFY(!CustomCommands::prepare(command, root.path(), {dangerous}).error.isEmpty());
        command.arguments = {QString()};
        QVERIFY(!CustomCommands::validate(command).isEmpty());
        command.arguments = {QStringLiteral("line\nbreak")};
        QVERIFY(!CustomCommands::validate(command).isEmpty());
        command.arguments = {QStringLiteral("%F"), QStringLiteral("%d")};
        QCOMPARE(CustomCommands::prepare(command, root.path(), {dangerous}).arguments, (QStringList{dangerous, root.path()}));
        QVERIFY(!CustomCommands::prepare(command, root.path(), {QStringLiteral("relative")}).error.isEmpty());
    }

    void persistsPresetsAndRejectsBadData()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("commandsrc")), KConfig::SimpleConfig);
        auto group = config->group(QStringLiteral("CustomCommands"));
        const CustomCommands::Command command{QStringLiteral("Example"),
                                              QStringLiteral("echo"),
                                              {QStringLiteral("{path}")},
                                              {{QStringLiteral("x"), QStringLiteral("a")}}};
        QString error;
        QVERIFY(CustomCommands::save(group, {command}, &error));
        config->reparseConfiguration();
        const auto loaded = CustomCommands::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().arguments, command.arguments);
        QCOMPARE(loaded.first().defaults, command.defaults);
        QVERIFY(!CustomCommands::save(group, {command, command}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(CustomCommands::load(group, &error).size(), 1);
        group.writeEntry("Commands", QByteArray("{broken"));
        QVERIFY(CustomCommands::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        group.writeEntry("Commands", QByteArray("[{\"name\":\"a\",\"program\":\"echo\",\"arguments\":[4],\"defaults\":{}}]"));
        QVERIFY(CustomCommands::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(CustomCommandsTest)
#include "customcommandstest.moc"
