// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminallauncher.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KSharedConfig>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QStatusBar>
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

        auto *history = window.actionCollection()->action(QStringLiteral("search_terminal_history"));
        QVERIFY(history);
        history->trigger();
        QVERIFY(window.actionCollection()->action(QStringLiteral("show_embedded_terminal"))->isChecked());
        auto *status = window.findChild<QLabel *>(QStringLiteral("terminalPanelStatus"));
        QVERIFY(status);
        QVERIFY(status->text().contains(QStringLiteral("Atuin is not installed")));
    }

    void editWithStartsGraphicalEditors()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString output = m_bin.filePath(QStringLiteral("arguments"));
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("kwrite")), "#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" > '" + output.toLocal8Bit() + "'\n"));
        const QString first = folder.filePath(QStringLiteral("--help"));
        const QString second = folder.filePath(QStringLiteral("it's here.txt"));
        for (const QString &name : {first, second}) {
            QFile file(name);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(QDir(folder.path()).mkdir(QStringLiteral("sub")));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        auto *editWith = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("edit_with")));
        QVERIFY(editWith);
        QVERIFY(!editWith->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(folder.filePath(QStringLiteral("sub")))});
        QVERIFY(!editWith->isEnabled());

        QMenu *menu = editWith->menu();
        Q_EMIT menu->aboutToShow();
        QStringList names;
        for (const QAction *entry : menu->actions()) {
            names.append(entry->objectName());
        }
        QCOMPARE(names,
                 (QStringList{QStringLiteral("editWith_kwrite"),
                              QStringLiteral("editWith_kate"),
                              QStringLiteral("editWith_nvim"),
                              QStringLiteral("editWith_rune"),
                              QStringLiteral("editWith_micro"),
                              QStringLiteral("editWith_nano")}));
        const QList<QAction *> entries = menu->actions();
        QVERIFY(entries.at(0)->isEnabled());
        QCOMPARE(entries.at(0)->toolTip(), QStringLiteral("Open the selected files in KWrite"));
        for (int i = 1; i < entries.size(); ++i) {
            QVERIFY(!entries.at(i)->isEnabled());
            QVERIFY(entries.at(i)->toolTip().contains(QStringLiteral("is not installed")));
        }

        view->selectUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)});
        QTRY_VERIFY(editWith->isEnabled());
        entries.at(0)->trigger();
        QFile file(output);
        QTRY_VERIFY(file.exists() && file.size() > 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.at(0).endsWith(QStringLiteral("kwrite")));
        lines.removeFirst();
        lines.sort();
        QStringList expected{first, second};
        expected.sort();
        QCOMPARE(lines, expected);
    }

    void openInOfficeStartsTheChosenApplication()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString output = m_bin.filePath(QStringLiteral("arguments"));
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("libreoffice")), "#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" > '" + output.toLocal8Bit() + "'\n"));
        const QString letter = folder.filePath(QStringLiteral("--convert-to letter.odt"));
        const QString sheet = folder.filePath(QStringLiteral("sheet.xlsx"));
        const QString notes = folder.filePath(QStringLiteral("notes.txt"));
        for (const QString &name : {letter, sheet, notes}) {
            QFile file(name);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        auto *openInOffice = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("open_in_office")));
        QVERIFY(openInOffice);
        QVERIFY(!openInOffice->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(letter), QUrl::fromLocalFile(notes)});
        QVERIFY(!openInOffice->isEnabled());

        view->selectUrls({QUrl::fromLocalFile(letter), QUrl::fromLocalFile(sheet)});
        QTRY_VERIFY(openInOffice->isEnabled());
        QMenu *menu = openInOffice->menu();
        Q_EMIT menu->aboutToShow();
        QStringList names;
        for (const QAction *entry : menu->actions()) {
            names.append(entry->objectName());
        }
        QCOMPARE(names,
                 (QStringList{QStringLiteral("openInOffice_libreoffice"), QStringLiteral("openInOffice_calligra"), QStringLiteral("openInOffice_onlyoffice")}));
        const QList<QAction *> entries = menu->actions();
        QVERIFY(entries.at(0)->isEnabled());
        QCOMPARE(entries.at(0)->toolTip(), QStringLiteral("Open the selected documents in LibreOffice"));
        QVERIFY(!entries.at(1)->isEnabled());
        QVERIFY(entries.at(1)->toolTip().contains(QStringLiteral("calligrawords")));
        QVERIFY(!entries.at(2)->isEnabled());

        entries.at(0)->trigger();
        QFile file(output);
        QTRY_VERIFY(file.exists() && file.size() > 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.at(0).endsWith(QStringLiteral("libreoffice")));
        lines.removeFirst();
        lines.sort();
        QStringList expected{letter, sheet};
        expected.sort();
        QCOMPARE(lines, expected);
    }

    void editStartsASuitableTool()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString output = m_bin.filePath(QStringLiteral("arguments"));
        const QString photo = folder.filePath(QStringLiteral("--photo.png"));
        const QString notes = folder.filePath(QStringLiteral("notes.txt"));
        const QString clip = folder.filePath(QStringLiteral("clip.mp4"));
        for (const QString &name : {photo, notes, clip}) {
            QFile file(name);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        QAction *edit = window.actionCollection()->action(QStringLiteral("edit_file"));
        QVERIFY(edit);
        QCOMPARE(edit->shortcut(), QKeySequence(Qt::Key_F4));
        QVERIFY(!edit->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(clip)});
        QVERIFY(!edit->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(photo), QUrl::fromLocalFile(notes)});
        QVERIFY(!edit->isEnabled());

        view->selectUrls({QUrl::fromLocalFile(notes)});
        QTRY_VERIFY(edit->isEnabled());
        QCOMPARE(edit->toolTip(), QStringLiteral("Edit the selected file with the built-in editor"));
        view->selectUrls({QUrl::fromLocalFile(photo)});
        QTRY_COMPARE(edit->toolTip(), QStringLiteral("No image editor is installed. Install KolourPaint, Krita or GIMP."));

        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("kolourpaint")), "#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" > '" + output.toLocal8Bit() + "'\n"));
        view->selectUrls({QUrl::fromLocalFile(notes)});
        view->selectUrls({QUrl::fromLocalFile(photo)});
        QTRY_COMPARE(edit->toolTip(), QStringLiteral("Edit the selected file with KolourPaint"));
        edit->trigger();
        QFile file(output);
        QTRY_VERIFY(file.exists() && file.size() > 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines.at(0).endsWith(QStringLiteral("kolourpaint")));
        QCOMPARE(lines.at(1), photo);
    }

    void remoteConnectionsListSavedProfilesAndOpenSsh()
    {
        RemoteProfiles::Profile server;
        server.name = QStringLiteral("Build & deploy");
        server.host = QStringLiteral("build.example.invalid");
        server.port = 2222;
        server.user = QStringLiteral("deploy");
        RemoteProfiles::Profile cloud;
        cloud.name = QStringLiteral("Cloud");
        cloud.protocol = RemoteProfiles::Protocol::WebDavs;
        cloud.host = QStringLiteral("cloud.example.invalid");
        QList<RemoteProfiles::Profile> profiles{server, cloud};
        KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("RemoteProfiles"));
        const auto removeProfiles = qScopeGuard([&group] {
            group.deleteGroup();
            group.sync();
        });
        QVERIFY(RemoteProfiles::save(group, profiles));

        MainWindow window;
        auto *remote = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("remote_connections")));
        QVERIFY(remote);
        Q_EMIT remote->menu()->aboutToShow();
        const QList<QAction *> entries = remote->menu()->actions();
        QCOMPARE(entries.size(), 5);
        QCOMPARE(entries.at(0)->text(), QStringLiteral("Build && deploy"));
        QCOMPARE(entries.at(0)->toolTip(), QStringLiteral("sftp://deploy@build.example.invalid:2222"));
        QVERIFY(entries.at(0)->menu());
        QCOMPARE(entries.at(0)->menu()->actions().size(), 3);
        QVERIFY(entries.at(1)->menu());
        QCOMPARE(entries.at(1)->menu()->actions().size(), 2);
        QVERIFY(entries.at(2)->isSeparator());
        QCOMPARE(entries.at(3), window.actionCollection()->action(QStringLiteral("network_locations")));
        QVERIFY(entries.at(3)->isEnabled());
        QCOMPARE(entries.at(4), window.actionCollection()->action(QStringLiteral("manage_remote_connections")));

        window.openSshTerminal(profiles.first());
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("ssh command was not found")));

        const QString output = m_bin.filePath(QStringLiteral("arguments"));
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("ssh")), "#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + output.toLocal8Bit() + "'\n"));
        entries.at(0)->menu()->actions().at(2)->trigger();
        QVERIFY(window.actionCollection()->action(QStringLiteral("show_embedded_terminal"))->isChecked());
        QFile file(output);
        QTRY_VERIFY_WITH_TIMEOUT(file.exists() && file.size() > 0, 10000);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
                 (QStringList{QStringLiteral("-p"),
                              QStringLiteral("2222"),
                              QStringLiteral("-l"),
                              QStringLiteral("deploy"),
                              QStringLiteral("--"),
                              QStringLiteral("build.example.invalid")}));
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
