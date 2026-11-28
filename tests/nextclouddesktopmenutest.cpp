// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "nextcloudaccount.h"
#include "nextclouddesktop.h"
#include "nextcloudmenu.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KService>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace NextcloudDesktop;

namespace
{
bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

NextcloudAccounts::Account alice()
{
    NextcloudAccounts::Account account;
    account.id = QStringLiteral("alice-id");
    account.server = QUrl(QStringLiteral("https://cloud.example.com"));
    account.user = QStringLiteral("alice");
    return account;
}

/// A configuration with a folder of full copies and one of virtual files for
/// alice, a folder of another account, and a folder that doesn't exist.
QByteArray configuration(const QString &root)
{
    return QStringLiteral(
               "[Accounts]\n0\\Folders\\1\\localPath=%1/Nextcloud/\n0\\Folders\\1\\targetPath=/\n0\\Folders\\1\\virtualFilesMode=off\n"
               "0\\FoldersWithPlaceholders\\2\\localPath=%1/Photos\n0\\FoldersWithPlaceholders\\2\\targetPath=/Photos\n"
               "0\\FoldersWithPlaceholders\\2\\virtualFilesMode=suffix\n0\\dav_user=alice\n0\\url=https://cloud.example.com\n"
               "1\\Folders\\1\\localPath=%1/Work\n1\\Folders\\1\\targetPath=/\n1\\dav_user=bob\n1\\url=https://work.example.org\n"
               "1\\Folders\\2\\localPath=%1/Gone\n1\\Folders\\2\\targetPath=/Old\n")
        .arg(root)
        .toUtf8();
}

QMenu *folderMenu(QMenu *menu, const QString &account, const QString &folder)
{
    return menu->findChild<QMenu *>(QStringLiteral("nextcloudDesktopFolder-%1-%2").arg(account, folder));
}

QAction *action(QMenu *menu, const QString &name)
{
    const QList<QAction *> actions = menu->actions();
    for (QAction *candidate : actions) {
        if (candidate->objectName() == name) {
            return candidate;
        }
    }
    return nullptr;
}

/// A desktop client's socket that reports every path as up to date and records what it is asked.
class FakeClient : public QObject
{
public:
    explicit FakeClient(const QString &path)
    {
        m_server.listen(path);
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            while (QLocalSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                    while (socket->canReadLine()) {
                        const QByteArray line = socket->readLine();
                        received.append(line);
                        if (line.startsWith("RETRIEVE_FILE_STATUS:")) {
                            socket->write("STATUS:OK:" + line.mid(int(qstrlen("RETRIEVE_FILE_STATUS:"))));
                        }
                    }
                });
            }
        });
    }
    bool isListening() const
    {
        return m_server.isListening();
    }
    QByteArrayList received;

private:
    QLocalServer m_server;
};
}

class NextcloudDesktopMenuTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void foldersShowTheirStateAndOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        for (const QString &folder : {QStringLiteral("Nextcloud"), QStringLiteral("Photos"), QStringLiteral("Work")}) {
            QVERIFY(QDir().mkpath(root + QLatin1Char('/') + folder));
        }
        const QString path = dir.filePath(QStringLiteral("nextcloud.cfg"));
        QVERIFY(writeFile(path, configuration(root)));

        NextcloudMenu::DesktopClient client;
        client.configuration = readConfiguration(path);
        QCOMPARE(client.configuration.folders.size(), 4);

        QStringList opened;
        QList<QUrl> openedOnServer;
        int clientOpened = 0;
        const NextcloudMenu::DesktopHandlers handlers{[&opened](const QString &folder, bool otherPane) {
                                                          opened.append(folder + (otherPane ? QStringLiteral(" other") : QString()));
                                                      },
                                                      [&openedOnServer](const QUrl &url) {
                                                          openedOnServer.append(url);
                                                      },
                                                      [&clientOpened] {
                                                          ++clientOpened;
                                                      }};
        QMenu menu;

        // While the status is checked, nothing is claimed about the folders.
        NextcloudMenu::populateDesktopClient(&menu, client, {alice()}, true, this, handlers);
        QAction *openClient = action(&menu, QStringLiteral("nextcloudOpenDesktopClient"));
        QVERIFY(openClient);
        QVERIFY(!openClient->isEnabled());
        QVERIFY(!openClient->toolTip().isEmpty());
        QAction *status = action(&menu, QStringLiteral("nextcloudDesktopStatus"));
        QVERIFY(status);
        QVERIFY(!status->isEnabled());
        QMenu *full = folderMenu(&menu, QStringLiteral("0"), QStringLiteral("1"));
        QVERIFY(full);
        QCOMPARE(full->title(), QStringLiteral("Nextcloud"));

        // The client reports each folder: full copies are local, virtual files remote.
        client.installed = true;
        client.statusChecked = true;
        client.running = true;
        client.statuses = {{root + QStringLiteral("/Nextcloud"), QStringLiteral("OK")},
                           {root + QStringLiteral("/Photos"), QStringLiteral("SYNC+SWM")},
                           {root + QStringLiteral("/Work"), QStringLiteral("NOP")}};
        NextcloudMenu::populateDesktopClient(&menu, client, {alice()}, true, this, handlers);
        QVERIFY(!action(&menu, QStringLiteral("nextcloudDesktopStatus")));
        openClient = action(&menu, QStringLiteral("nextcloudOpenDesktopClient"));
        QVERIFY(openClient->isEnabled());
        openClient->trigger();
        QCOMPARE(clientOpened, 1);

        full = folderMenu(&menu, QStringLiteral("0"), QStringLiteral("1"));
        QVERIFY(full);
        QCOMPARE(full->title(), QStringLiteral("Nextcloud (Local, Up to date)"));
        QVERIFY(full->menuAction()->toolTip().contains(root + QStringLiteral("/Nextcloud")));
        QMenu *photos = folderMenu(&menu, QStringLiteral("0"), QStringLiteral("2"));
        QVERIFY(photos);
        QCOMPARE(photos->title(), QStringLiteral("Photos (Remote, Synchronizing)"));
        QVERIFY(photos->menuAction()->toolTip().contains(QStringLiteral("/Photos")));
        QMenu *work = folderMenu(&menu, QStringLiteral("1"), QStringLiteral("1"));
        QVERIFY(work);
        QCOMPARE(work->title(), QStringLiteral("Work (Unknown)"));
        QMenu *gone = folderMenu(&menu, QStringLiteral("1"), QStringLiteral("2"));
        QVERIFY(gone);
        QCOMPARE(gone->title(), QStringLiteral("Gone (Unknown)"));

        action(full, QStringLiteral("nextcloudDesktopOpen"))->trigger();
        action(photos, QStringLiteral("nextcloudDesktopOpenOther"))->trigger();
        QCOMPARE(opened, (QStringList{root + QStringLiteral("/Nextcloud"), root + QStringLiteral("/Photos other")}));
        QVERIFY(!action(gone, QStringLiteral("nextcloudDesktopOpen"))->isEnabled());
        QVERIFY(!action(gone, QStringLiteral("nextcloudDesktopOpenOther"))->isEnabled());

        // Folders of a Plasma Opus account also open on the server; others say how to add the account.
        action(photos, QStringLiteral("nextcloudDesktopOpenServer"))->trigger();
        QCOMPARE(openedOnServer.size(), 1);
        QCOMPARE(openedOnServer.first().scheme(), QStringLiteral("webdavs"));
        QCOMPARE(openedOnServer.first().host(), QStringLiteral("cloud.example.com"));
        QCOMPARE(openedOnServer.first().path(), QStringLiteral("/remote.php/dav/files/alice/Photos/"));
        QAction *workServer = action(work, QStringLiteral("nextcloudDesktopOpenServer"));
        QVERIFY(!workServer->isEnabled());
        QVERIFY(workServer->toolTip().contains(QStringLiteral("bob")));
        QVERIFY(workServer->toolTip().contains(QStringLiteral("work.example.org")));

        // Without a second pane, only the active pane is offered.
        NextcloudMenu::populateDesktopClient(&menu, client, {alice()}, false, this, handlers);
        QVERIFY(!action(folderMenu(&menu, QStringLiteral("0"), QStringLiteral("1")), QStringLiteral("nextcloudDesktopOpenOther"))->isEnabled());

        // When the client isn't running, every existing folder is offline and the menu says why.
        client.running = false;
        client.statuses.clear();
        NextcloudMenu::populateDesktopClient(&menu, client, {alice()}, true, this, handlers);
        status = action(&menu, QStringLiteral("nextcloudDesktopStatus"));
        QVERIFY(status);
        QVERIFY(!status->toolTip().isEmpty());
        QCOMPARE(folderMenu(&menu, QStringLiteral("0"), QStringLiteral("1"))->title(), QStringLiteral("Nextcloud (Offline)"));
        QCOMPARE(folderMenu(&menu, QStringLiteral("0"), QStringLiteral("2"))->title(), QStringLiteral("Photos (Offline)"));
        QCOMPARE(folderMenu(&menu, QStringLiteral("1"), QStringLiteral("2"))->title(), QStringLiteral("Gone (Unknown)"));

        // Opening the menu never changed the client's configuration.
        QCOMPARE(readFile(path), configuration(root));
    }

    void emptyAndDamagedConfigurations()
    {
        QMenu menu;
        NextcloudMenu::DesktopClient client;
        client.installed = true;
        client.statusChecked = true;
        client.running = true;
        client.configuration.exists = true;
        NextcloudMenu::populateDesktopClient(&menu, client, {}, true, this, {});
        bool none = false;
        for (QAction *entry : menu.actions()) {
            none = none || (entry->text() == QStringLiteral("No synchronized folders") && !entry->isEnabled());
        }
        QVERIFY(none);
        // Without a handler, the client can't be opened.
        QVERIFY(!action(&menu, QStringLiteral("nextcloudOpenDesktopClient"))->isEnabled());

        client.configuration.error = QStringLiteral("Damaged & unreadable");
        NextcloudMenu::populateDesktopClient(&menu, client, {}, true, this, {});
        bool problem = false;
        for (QAction *entry : menu.actions()) {
            problem = problem || (entry->text() == QStringLiteral("Damaged && unreadable") && !entry->isEnabled());
        }
        QVERIFY(problem);
    }

    void statusIsNotAskedWhenTurnedOff()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        QVERIFY(QDir().mkpath(root + QStringLiteral("/Nextcloud")));
        const QString path = dir.filePath(QStringLiteral("nextcloud.cfg"));
        QVERIFY(writeFile(path, configuration(root)));

        NextcloudMenu::DesktopClient client;
        client.configuration = readConfiguration(path);
        client.installed = true;
        client.statusEnabled = false;
        QMenu menu;
        NextcloudMenu::populateDesktopClient(&menu, client, {alice()}, true, this, {});
        // Neither a pending check nor a missing client is claimed, and the folders have no state.
        QVERIFY(!action(&menu, QStringLiteral("nextcloudDesktopStatus")));
        QMenu *full = folderMenu(&menu, QStringLiteral("0"), QStringLiteral("1"));
        QVERIFY(full);
        QCOMPARE(full->title(), QStringLiteral("Nextcloud"));
        QVERIFY2(full->menuAction()->toolTip().contains(QStringLiteral("as set in Preferences")), qPrintable(full->menuAction()->toolTip()));

        // Even a stale answer of the client isn't shown.
        client.statusChecked = true;
        client.running = true;
        client.statuses = {{root + QStringLiteral("/Nextcloud"), QStringLiteral("OK")}};
        NextcloudMenu::populateDesktopClient(&menu, client, {alice()}, true, this, {});
        QCOMPARE(folderMenu(&menu, QStringLiteral("0"), QStringLiteral("1"))->title(), QStringLiteral("Nextcloud"));
    }

    void mainWindowFollowsTheDesktopClientSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        QVERIFY(QDir().mkpath(root + QStringLiteral("/Nextcloud")));
        const QString path = dir.filePath(QStringLiteral("config/Nextcloud/nextcloud.cfg"));
        const QString socket = dir.filePath(QStringLiteral("socket"));
        QVERIFY(writeFile(path, configuration(root)));
        FakeClient client(socket);
        QVERIFY(client.isListening());

        MainWindow window;
        window.setNextcloudDesktopLocations(path, socket);
        QMenu *menu = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("nextcloud")))->menu();

        // Without status checks, the client is never asked.
        Settings::setNextcloudDesktopStatus(false);
        Q_EMIT menu->aboutToShow();
        QMenu *desktop = menu->findChild<QMenu *>(QStringLiteral("nextcloudDesktopClient"));
        QVERIFY(desktop && menu->actions().contains(desktop->menuAction()));
        QCOMPARE(folderMenu(desktop, QStringLiteral("0"), QStringLiteral("1"))->title(), QStringLiteral("Nextcloud"));
        QTest::qWait(100);
        QVERIFY(client.received.isEmpty());

        // Turning the submenu off removes it even though the client is configured.
        Settings::setNextcloudDesktopClientMenu(false);
        Q_EMIT menu->aboutToShow();
        QVERIFY(!menu->actions().contains(desktop->menuAction()));
        QVERIFY(client.received.isEmpty());

        // The defaults offer the submenu and ask the client again.
        Settings::self()->setDefaults();
        Q_EMIT menu->aboutToShow();
        QVERIFY(menu->actions().contains(desktop->menuAction()));
        QTRY_COMPARE(folderMenu(desktop, QStringLiteral("0"), QStringLiteral("1"))->title(), QStringLiteral("Nextcloud (Local, Up to date)"));
    }

    void mainWindowOffersTheClientOnlyWhenConfigured()
    {
        if (KService::serviceByDesktopName(QStringLiteral("com.nextcloud.desktopclient.nextcloud"))
            || KService::serviceByDesktopName(QStringLiteral("nextcloud")) || KService::serviceByDesktopName(QStringLiteral("Nextcloud"))) {
            QSKIP("The Nextcloud desktop client is installed, so the submenu is always offered.");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        QVERIFY(QDir().mkpath(root + QStringLiteral("/Nextcloud")));
        const QString path = dir.filePath(QStringLiteral("config/Nextcloud/nextcloud.cfg"));
        const QString socket = dir.filePath(QStringLiteral("socket"));

        MainWindow window;
        window.setNextcloudDesktopLocations(path, socket);
        auto *nextcloud = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("nextcloud")));
        QVERIFY(nextcloud);
        QMenu *menu = nextcloud->menu();
        Q_EMIT menu->aboutToShow();
        QMenu *desktop = menu->findChild<QMenu *>(QStringLiteral("nextcloudDesktopClient"));
        QVERIFY(!desktop || !menu->actions().contains(desktop->menuAction()));

        // Once configured, the submenu comes before Manage Nextcloud Accounts and asks the running client.
        QVERIFY(writeFile(path, configuration(root)));
        FakeClient client(socket);
        QVERIFY(client.isListening());
        Q_EMIT menu->aboutToShow();
        desktop = menu->findChild<QMenu *>(QStringLiteral("nextcloudDesktopClient"));
        QVERIFY(desktop);
        const QList<QAction *> entries = menu->actions();
        const qsizetype position = entries.indexOf(desktop->menuAction());
        QVERIFY(position >= 0);
        QCOMPARE(entries.value(position + 1), window.actionCollection()->action(QStringLiteral("manage_nextcloud_accounts")));
        QTRY_VERIFY(!action(desktop, QStringLiteral("nextcloudDesktopStatus")));
        QMenu *folder = folderMenu(desktop, QStringLiteral("0"), QStringLiteral("1"));
        QVERIFY(folder);
        QCOMPARE(folder->title(), QStringLiteral("Nextcloud (Local, Up to date)"));
        for (const QByteArray &line : std::as_const(client.received)) {
            QVERIFY2(line.startsWith("RETRIEVE_FILE_STATUS:"), line.constData());
        }

        action(folder, QStringLiteral("nextcloudDesktopOpen"))->trigger();
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(root + QStringLiteral("/Nextcloud")));

        // Opening the menu again keeps one submenu, and the configuration is unchanged.
        Q_EMIT menu->aboutToShow();
        QCOMPARE(menu->actions().count(desktop->menuAction()), 1);
        QCOMPARE(readFile(path), configuration(root));

        // When the configuration goes away, so does the submenu.
        QVERIFY(QFile::remove(path));
        Q_EMIT menu->aboutToShow();
        QVERIFY(!menu->actions().contains(desktop->menuAction()));
    }
};

QTEST_MAIN(NextcloudDesktopMenuTest)

#include "nextclouddesktopmenutest.moc"
