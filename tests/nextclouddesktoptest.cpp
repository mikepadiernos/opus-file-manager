// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextclouddesktop.h"

#include "cloudsync.h"
#include "nextcloudaccount.h"

#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
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

NextcloudAccounts::Account account(const QString &server, const QString &user)
{
    NextcloudAccounts::Account result;
    result.id = user + QStringLiteral("-id");
    result.server = QUrl(server);
    result.user = user;
    return result;
}

/// A desktop client's socket that answers status requests from @p statuses
/// and records every line it receives.
class FakeClient : public QObject
{
public:
    explicit FakeClient(const QString &path)
    {
        QLocalServer::removeServer(path);
        m_server.listen(path);
        connect(&m_server, &QLocalServer::newConnection, this, [this] {
            while (QLocalSocket *socket = m_server.nextPendingConnection()) {
                // The client announces its folders first.
                socket->write("REGISTER_PATH:/elsewhere\n");
                connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                    while (socket->canReadLine()) {
                        const QByteArray line = socket->readLine();
                        received.append(line);
                        if (!answer || !line.startsWith("RETRIEVE_FILE_STATUS:")) {
                            continue;
                        }
                        const QByteArray path = line.mid(int(qstrlen("RETRIEVE_FILE_STATUS:"))).chopped(1);
                        // Unrelated pushes come in between, and the answer arrives in two parts.
                        socket->write("STATUS:OK:/elsewhere/a.txt\nUPDATE_VIEW:/elsewhere\n");
                        const QByteArray reply = "STATUS:" + statuses.value(QString::fromUtf8(path), QByteArray("NOP")) + ':' + path + '\n';
                        socket->write(reply.left(5));
                        socket->flush();
                        socket->write(reply.mid(5));
                    }
                });
            }
        });
    }
    bool isListening() const
    {
        return m_server.isListening();
    }

    QHash<QString, QByteArray> statuses;
    QByteArrayList received;
    bool answer = true;

private:
    QLocalServer m_server;
};
}

class NextcloudDesktopTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void readsTheConfigurationWithoutChangingIt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString home = dir.filePath(QStringLiteral("config"));
        const QString path = configurationPath(home);
        QCOMPARE(path, home + QStringLiteral("/Nextcloud/nextcloud.cfg"));
        const QString root = dir.path();
        // Recorded from a Nextcloud 3.14 client with two accounts, trimmed.
        const QByteArray config = QStringLiteral(
                                      "[General]\nclientVersion=3.14.1\nconfirmExternalStorage=true\n\n"
                                      "[Accounts]\n0\\Folders\\2\\ignoreHiddenFiles=false\n0\\Folders\\2\\journalPath=.sync_a1.db\n"
                                      "0\\Folders\\2\\localPath=\"%1/Work, Projects/\"\n0\\Folders\\2\\paused=false\n0\\Folders\\2\\targetPath=/Work\n"
                                      "0\\Folders\\2\\version=2\n0\\Folders\\2\\virtualFilesMode=off\n"
                                      "0\\Folders\\10\\localPath=%1/Nextcloud/\n0\\Folders\\10\\targetPath=/\n0\\Folders\\10\\virtualFilesMode=off\n"
                                      "0\\FoldersWithPlaceholders\\3\\localPath=%1/Photos\n0\\FoldersWithPlaceholders\\3\\targetPath=/Photos\n"
                                      "0\\FoldersWithPlaceholders\\3\\virtualFilesMode=suffix\n"
                                      "0\\authType=webflow\n0\\dav_user=alice\n0\\displayName=Alice\n0\\url=https://cloud.example.com/\n"
                                      "0\\version=1\n0\\webflow_user=alice@example.com\n"
                                      "1\\Folders\\1\\localPath=relative/path\n"
                                      "1\\Folders\\4\\localPath=%1/Other\n1\\Folders\\4\\paused=true\n1\\Folders\\4\\targetPath=/\n"
                                      "1\\Folders\\4\\usePlaceholders=true\n1\\url=https://example.org/nextcloud\n1\\webflow_user=bob\n"
                                      "version=2\n")
                                      .arg(root)
                                      .toUtf8();
        QVERIFY(writeFile(path, config));

        const Configuration configuration = readConfiguration(path);
        QVERIFY(configuration.exists);
        QVERIFY2(configuration.error.isEmpty(), qPrintable(configuration.error));
        QCOMPARE(configuration.folders.size(), 4);

        const Folder &work = configuration.folders.at(0);
        QCOMPARE(work.accountId, QStringLiteral("0"));
        QCOMPARE(work.id, QStringLiteral("2"));
        QCOMPARE(work.localPath, root + QStringLiteral("/Work, Projects"));
        QCOMPARE(work.targetPath, QStringLiteral("/Work"));
        QCOMPARE(work.server, QUrl(QStringLiteral("https://cloud.example.com")));
        QCOMPARE(work.user, QStringLiteral("alice"));
        QVERIFY(!usesVirtualFiles(work));
        QVERIFY(!work.paused);

        // Folders are numbered, not sorted as text.
        QCOMPARE(configuration.folders.at(1).id, QStringLiteral("10"));
        QCOMPARE(configuration.folders.at(1).localPath, root + QStringLiteral("/Nextcloud"));

        const Folder &photos = configuration.folders.at(2);
        QCOMPARE(photos.localPath, root + QStringLiteral("/Photos"));
        QCOMPARE(photos.virtualFilesMode, QStringLiteral("suffix"));
        QVERIFY(usesVirtualFiles(photos));

        // Relative paths are skipped; older clients marked virtual files with usePlaceholders.
        const Folder &other = configuration.folders.at(3);
        QCOMPARE(other.accountId, QStringLiteral("1"));
        QCOMPARE(other.localPath, root + QStringLiteral("/Other"));
        QCOMPARE(other.server, QUrl(QStringLiteral("https://example.org/nextcloud")));
        QCOMPARE(other.user, QStringLiteral("bob"));
        QVERIFY(other.paused);
        QVERIFY(usesVirtualFiles(other));

        // Reading neither changes nor locks the client's configuration.
        QCOMPARE(readFile(path), config);
        QCOMPARE(QDir(QFileInfo(path).path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), QStringList{QStringLiteral("nextcloud.cfg")});

        // CloudSync recognizes the same folders, including those with virtual files.
        const CloudSync::EngineLocations locations{home, dir.filePath(QStringLiteral("state"))};
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/Photos/2026"), locations), QStringLiteral("Nextcloud"));
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/Work, Projects"), locations), QStringLiteral("Nextcloud"));
        QCOMPARE(CloudSync::managingEngine(root + QStringLiteral("/Unrelated"), locations), QString());

        // A missing configuration means the client isn't set up.
        const Configuration missing = readConfiguration(dir.filePath(QStringLiteral("none.cfg")));
        QVERIFY(!missing.exists);
        QVERIFY(missing.folders.isEmpty());
        QVERIFY(missing.error.isEmpty());
        QVERIFY(!readConfiguration(dir.path()).exists);
        QVERIFY(!readConfiguration(QString()).exists);
    }

    void matchesAccounts()
    {
        Folder folder;
        folder.server = QUrl(QStringLiteral("https://Cloud.Example.com/"));
        folder.user = QStringLiteral("alice");
        folder.targetPath = QStringLiteral("/Work/Plans");
        const QList<NextcloudAccounts::Account> accounts{account(QStringLiteral("https://cloud.example.com/sub"), QStringLiteral("alice")),
                                                         account(QStringLiteral("https://cloud.example.com"), QStringLiteral("bob")),
                                                         account(QStringLiteral("https://cloud.example.com"), QStringLiteral("alice"))};
        const std::optional<NextcloudAccounts::Account> matched = accountFor(folder, accounts);
        QVERIFY(matched);
        QCOMPARE(*matched, accounts.at(2));
        QCOMPARE(serverFolder(*matched, folder).path(), QStringLiteral("/remote.php/dav/files/alice/Work/Plans/"));
        QCOMPARE(serverFolder(*matched, folder).scheme(), QStringLiteral("webdavs"));
        folder.targetPath = QStringLiteral("/");
        QCOMPARE(serverFolder(*matched, folder), NextcloudAccounts::filesUrl(*matched));
        folder.targetPath.clear();
        QCOMPARE(serverFolder(*matched, folder), NextcloudAccounts::filesUrl(*matched));

        folder.user = QStringLiteral("Alice");
        QVERIFY(!accountFor(folder, accounts));
        folder.user.clear();
        QVERIFY(!accountFor(folder, accounts));
        folder.user = QStringLiteral("alice");
        folder.server = QUrl(QStringLiteral("https://other.example.com"));
        QVERIFY(!accountFor(folder, accounts));
    }

    void parsesStatusMessages()
    {
        QCOMPARE(parseSync(QStringLiteral("OK")), Sync::UpToDate);
        QCOMPARE(parseSync(QStringLiteral("OK+SWM")), Sync::UpToDate);
        QCOMPARE(parseSync(QStringLiteral("SYNC")), Sync::Syncing);
        QCOMPARE(parseSync(QStringLiteral("NEW+SWM")), Sync::Syncing);
        QCOMPARE(parseSync(QStringLiteral("IGNORE")), Sync::Warning);
        QCOMPARE(parseSync(QStringLiteral("ERROR")), Sync::Error);
        QCOMPARE(parseSync(QStringLiteral("NOP")), Sync::NotSynchronized);
        QCOMPARE(parseSync(QStringLiteral("SOMETHING")), Sync::Unknown);
        QCOMPARE(parseSync(QString()), Sync::Unknown);

        const auto message = parseStatusMessage("STATUS:SYNC+SWM:/home/u/Nextcloud/a: b.txt\r\n");
        QVERIFY(message);
        QCOMPARE(message->status, QStringLiteral("SYNC+SWM"));
        QCOMPARE(message->path, QStringLiteral("/home/u/Nextcloud/a: b.txt"));
        QVERIFY(!parseStatusMessage("REGISTER_PATH:/home/u/Nextcloud\n"));
        QVERIFY(!parseStatusMessage("STATUS:OK\n"));
        QVERIFY(!parseStatusMessage("STATUS::/x\n"));
        QVERIFY(!parseStatusMessage("STATUS:OK:\n"));

        QCOMPARE(statusRequest(QStringLiteral("/home/u/Nextcloud/ü")), QByteArray("RETRIEVE_FILE_STATUS:/home/u/Nextcloud/\xc3\xbc\n"));
    }

    void distinguishesFolderStates()
    {
        Folder full;
        full.localPath = QStringLiteral("/home/u/Nextcloud");
        full.virtualFilesMode = QStringLiteral("off");
        Folder virtualFiles = full;
        virtualFiles.virtualFilesMode = QStringLiteral("xattr");
        const std::optional<QString> ok = QStringLiteral("OK");

        FolderState state = folderState(full, true, true, ok);
        QCOMPARE(state.availability, Availability::Local);
        QCOMPARE(state.sync, Sync::UpToDate);
        QVERIFY(!state.description.isEmpty());

        state = folderState(virtualFiles, true, true, QStringLiteral("SYNC"));
        QCOMPARE(state.availability, Availability::Remote);
        QCOMPARE(state.sync, Sync::Syncing);

        // Without the client, nothing is synchronized, whatever the folder holds.
        state = folderState(full, true, false, std::nullopt);
        QCOMPARE(state.availability, Availability::Offline);
        QCOMPARE(state.sync, Sync::Unknown);
        QVERIFY(folderState(virtualFiles, true, false, std::nullopt).description != state.description);

        Folder paused = full;
        paused.paused = true;
        QCOMPARE(folderState(paused, true, true, ok).availability, Availability::Offline);

        // Status the client doesn't give, or a missing folder, is unknown.
        QCOMPARE(folderState(full, true, true, std::nullopt).availability, Availability::Unknown);
        QCOMPARE(folderState(full, true, true, QStringLiteral("NOP")).availability, Availability::Unknown);
        QCOMPARE(folderState(full, false, true, ok).availability, Availability::Unknown);

        for (Availability availability : {Availability::Local, Availability::Remote, Availability::Offline, Availability::Unknown}) {
            QVERIFY(!availabilityText(availability).isEmpty());
        }
        QVERIFY(syncText(Sync::UpToDate) != syncText(Sync::Unknown));
    }

    void asksTheClientForStatus()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString socketPath = dir.filePath(QStringLiteral("socket"));
        FakeClient client(socketPath);
        QVERIFY(client.isListening());
        client.statuses.insert(QStringLiteral("/home/u/Nextcloud"), "OK+SWM");
        client.statuses.insert(QStringLiteral("/home/u/Photos"), "SYNC");

        StatusQuery query(socketPath);
        QSignalSpy finished(&query, &StatusQuery::finished);
        query.start({QStringLiteral("/home/u/Nextcloud"), QStringLiteral("/home/u/Photos"), QStringLiteral("/home/u/Gone")}, 5000);
        QVERIFY(query.isRunning());
        QVERIFY(finished.wait(5000));
        QVERIFY(!query.isRunning());
        QCOMPARE(finished.size(), 1);
        QVERIFY(finished.first().at(0).toBool());
        const auto statuses = finished.first().at(1).value<QHash<QString, QString>>();
        QCOMPARE(statuses.size(), 3);
        QCOMPARE(statuses.value(QStringLiteral("/home/u/Nextcloud")), QStringLiteral("OK+SWM"));
        QCOMPARE(statuses.value(QStringLiteral("/home/u/Photos")), QStringLiteral("SYNC"));
        QCOMPARE(statuses.value(QStringLiteral("/home/u/Gone")), QStringLiteral("NOP"));
        // The pushed status of another path is ignored.
        QVERIFY(!statuses.contains(QStringLiteral("/elsewhere/a.txt")));

        // Only status was asked for.
        QCOMPARE(client.received.size(), 3);
        for (const QByteArray &line : std::as_const(client.received)) {
            QVERIFY2(line.startsWith("RETRIEVE_FILE_STATUS:/home/u/") && line.endsWith('\n'), line.constData());
        }

        // A client that doesn't answer in time leaves the status unknown.
        client.answer = false;
        client.received.clear();
        query.start({QStringLiteral("/home/u/Nextcloud")}, 300);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.size(), 2);
        QVERIFY(finished.last().at(0).toBool());
        QVERIFY((finished.last().at(1).value<QHash<QString, QString>>().isEmpty()));
        QTRY_COMPARE(client.received.size(), 1);
    }

    void reportsAClientThatIsntRunning()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        StatusQuery missing(dir.filePath(QStringLiteral("socket")));
        QSignalSpy missingFinished(&missing, &StatusQuery::finished);
        missing.start({QStringLiteral("/home/u/Nextcloud")});
        QVERIFY(missingFinished.wait(5000));
        QVERIFY(!missingFinished.first().at(0).toBool());

        // A socket left behind by a client that quit refuses the connection.
        const QString stale = dir.filePath(QStringLiteral("stale"));
        QVERIFY(writeFile(stale, QByteArray()));
        StatusQuery refused(stale);
        QSignalSpy refusedFinished(&refused, &StatusQuery::finished);
        refused.start({QStringLiteral("/home/u/Nextcloud")});
        QVERIFY(refusedFinished.wait(5000));
        QVERIFY(!refusedFinished.first().at(0).toBool());

        StatusQuery none{QString()};
        QSignalSpy noneFinished(&none, &StatusQuery::finished);
        none.start({});
        QVERIFY(noneFinished.wait(5000));
        QVERIFY(!noneFinished.first().at(0).toBool());
    }
};

QTEST_GUILESS_MAIN(NextcloudDesktopTest)

#include "nextclouddesktoptest.moc"
