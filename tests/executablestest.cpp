// SPDX-License-Identifier: GPL-3.0-or-later

#include "executables.h"

#include <QTest>

class ExecutablesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void programsNeedTheExecutableBit()
    {
        QVERIFY(Executables::isProgram(QStringLiteral("application/x-executable"), true));
        QVERIFY(Executables::isProgram(QStringLiteral("application/x-shellscript"), true));
        QVERIFY(Executables::isProgram(QStringLiteral("text/x-python3"), true));
        QVERIFY(!Executables::isProgram(QStringLiteral("application/x-shellscript"), false));
        QVERIFY(!Executables::isProgram(QStringLiteral("image/png"), true));
    }

    void terminalCommandQuotesThePath()
    {
        QCOMPARE(Executables::terminalCommand(QStringLiteral("/home/me/my script.sh"), false), QStringLiteral("'/home/me/my script.sh'"));
        QCOMPARE(Executables::terminalCommand(QStringLiteral("/tmp/it's.sh"), false), QStringLiteral("'/tmp/it'\\''s.sh'"));
    }

    void keepOpenLeavesAShell()
    {
        const QString command = Executables::terminalCommand(QStringLiteral("/tmp/run.sh"), true);
        QVERIFY(command.startsWith(QStringLiteral("sh -c ")));
        QVERIFY(command.contains(QStringLiteral("/tmp/run.sh")));
        QVERIFY(command.contains(QStringLiteral("exec")));
    }
};

QTEST_GUILESS_MAIN(ExecutablesTest)

#include "executablestest.moc"
