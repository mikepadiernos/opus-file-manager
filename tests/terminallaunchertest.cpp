// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminallauncher.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KActionMenu>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using TerminalProviders::Provider;

class FakeYakuake : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.yakuake")

public:
    QString terminalIds = QStringLiteral("7");
    int activeTerminal = 3;
    QList<QPair<int, QString>> commands;

public Q_SLOTS:
    Q_SCRIPTABLE int addSession()
    {
        return 2;
    }
    Q_SCRIPTABLE QString terminalIdsForSessionId(int sessionId)
    {
        return sessionId == 2 ? terminalIds : QString();
    }
    Q_SCRIPTABLE int activeTerminalId()
    {
        return activeTerminal;
    }
    Q_SCRIPTABLE void runCommandInTerminal(int terminalId, const QString &command)
    {
        commands.append({terminalId, command});
    }
};

class TerminalLauncherTest : public QObject
{
    Q_OBJECT

private:
    static bool writeExecutable(const QString &path, const QByteArray &script)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(script) != script.size()) {
            return false;
        }
        file.close();
        return file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
    }

private Q_SLOTS:
    // Each test sees a PATH holding only the fake executables it writes.
    void init()
    {
        m_path = qgetenv("PATH");
        QVERIFY(m_bin.isValid());
        qputenv("PATH", m_bin.path().toLocal8Bit());
    }

    void cleanup()
    {
        qputenv("PATH", m_path);
        const QDir bin(m_bin.path());
        for (const QString &entry : bin.entryList(QDir::Files)) {
            QFile::remove(bin.filePath(entry));
        }
    }

    void reportsUnsupportedFoldersAndMissingProviders()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QStringList messages;
        const auto report = [&messages](const QString &message) {
            messages.append(message);
        };
        TerminalLauncher::open(Provider::Konsole, QUrl(QStringLiteral("sftp://example.invalid/tmp")), nullptr, report);
        TerminalLauncher::open(Provider::Konsole, QUrl::fromLocalFile(folder.filePath(QStringLiteral("missing"))), nullptr, report);
        TerminalLauncher::open(Provider::Kwi, QUrl::fromLocalFile(folder.path()), nullptr, report);
        TerminalLauncher::open(Provider::Yakuake, QUrl::fromLocalFile(folder.path()), nullptr, report);
        QCOMPARE(messages.size(), 4);
        QVERIFY(messages.at(0).contains(QStringLiteral("existing local folder")));
        QVERIFY(messages.at(1).contains(QStringLiteral("existing local folder")));
        QCOMPARE(messages.at(2), QStringLiteral("Kwi is not installed or is not in PATH."));
        QCOMPARE(messages.at(3), QStringLiteral("Yakuake is not installed or is not in PATH."));
    }

    void startsSeparateTerminalsInTheFolder()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString output = folder.filePath(QStringLiteral("arguments"));
        QVERIFY(QDir(folder.path()).mkdir(QStringLiteral("it's here")));
        const QString target = folder.filePath(QStringLiteral("it's here"));
        for (const QString &name : {QStringLiteral("konsole"), QStringLiteral("kwi")}) {
            QVERIFY(writeExecutable(m_bin.filePath(name), "#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" \"$PWD\" > '" + output.toLocal8Bit() + "'\n"));
        }
        QStringList messages;
        const auto report = [&messages](const QString &message) {
            messages.append(message);
        };
        for (const Provider provider : {Provider::Konsole, Provider::Kwi}) {
            QFile::remove(output);
            TerminalLauncher::open(provider, QUrl::fromLocalFile(target), nullptr, report);
            QFile file(output);
            QTRY_VERIFY(file.exists() && file.size() > 0);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            QCOMPARE(lines.size(), 4);
            QVERIFY(lines.at(0).endsWith(TerminalProviders::executable(provider)));
            QCOMPARE(lines.mid(1, 2), (QStringList{QStringLiteral("--workdir"), target}));
            QCOMPARE(QFileInfo(lines.at(3)).canonicalFilePath(), QFileInfo(target).canonicalFilePath());
        }
        QVERIFY2(messages.isEmpty(), qPrintable(messages.join(QLatin1Char('\n'))));
    }

    void opensYakuakeSessionsOnTheBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("No session bus");
        }
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("yakuake")), "#!/bin/sh\nexit 1\n"));
        FakeYakuake yakuake;
        QVERIFY(bus.registerObject(QStringLiteral("/yakuake/sessions"), &yakuake, QDBusConnection::ExportScriptableSlots));
        QVERIFY(bus.registerService(TerminalProviders::yakuakeService()));
        const auto unregister = qScopeGuard([&bus] {
            bus.unregisterService(TerminalProviders::yakuakeService());
            bus.unregisterObject(QStringLiteral("/yakuake/sessions"));
        });
        QStringList messages;
        const auto report = [&messages](const QString &message) {
            messages.append(message);
        };
        const QString directory = QDir::cleanPath(folder.path());
        TerminalLauncher::open(Provider::Yakuake, QUrl::fromLocalFile(folder.path()), nullptr, report);
        QCOMPARE(yakuake.commands.size(), 1);
        QCOMPARE(yakuake.commands.first().first, 7);
        QCOMPARE(yakuake.commands.first().second, TerminalProviders::yakuakeChangeDirectoryCommand(directory));
        QCOMPARE(messages.size(), 1);
        QVERIFY(messages.first().startsWith(QStringLiteral("Opened a Yakuake session in ")));

        yakuake.terminalIds.clear();
        TerminalLauncher::open(Provider::Yakuake, QUrl::fromLocalFile(folder.path()), nullptr, report);
        QCOMPARE(yakuake.commands.size(), 2);
        QCOMPARE(yakuake.commands.last().first, 3);

        yakuake.activeTerminal = -1;
        TerminalLauncher::open(Provider::Yakuake, QUrl::fromLocalFile(folder.path()), nullptr, report);
        QCOMPARE(yakuake.commands.size(), 2);
        QVERIFY(messages.last().contains(QStringLiteral("did not report its terminal")));
    }

    void menuReportsUnavailableProviders()
    {
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("konsole")), "#!/bin/sh\n"));
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        auto *openWith = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("open_terminal_with")));
        QVERIFY(openWith);
        QVERIFY(openWith->isEnabled());
        QMenu *menu = openWith->menu();
        Q_EMIT menu->aboutToShow();
        QStringList names;
        for (const QAction *entry : menu->actions()) {
            names.append(entry->objectName());
        }
        QCOMPARE(names,
                 (QStringList{QStringLiteral("openTerminalWith_default"),
                              QStringLiteral("openTerminalWith_konsole"),
                              QStringLiteral("openTerminalWith_yakuake"),
                              QStringLiteral("openTerminalWith_kwi")}));
        const QList<QAction *> entries = menu->actions();
        QVERIFY(entries.at(0)->isEnabled());
        QVERIFY(entries.at(1)->isEnabled());
        QVERIFY(entries.at(1)->toolTip().contains(QStringLiteral("Konsole")));
        QVERIFY(!entries.at(2)->isEnabled());
        QCOMPARE(entries.at(2)->toolTip(), QStringLiteral("Yakuake is not installed or is not in PATH."));
        QVERIFY(!entries.at(3)->isEnabled());
        QCOMPARE(entries.at(3)->toolTip(), QStringLiteral("Kwi is not installed or is not in PATH."));
    }

private:
    QTemporaryDir m_bin;
    QByteArray m_path;
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid()) {
        return 1;
    }
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toLocal8Bit());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toLocal8Bit());
    qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toLocal8Bit());
    qputenv("XDG_STATE_HOME", home.filePath(QStringLiteral("state")).toLocal8Bit());
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    TerminalLauncherTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "terminallaunchertest.moc"
