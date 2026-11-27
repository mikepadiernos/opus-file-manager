// SPDX-License-Identifier: GPL-3.0-or-later

#include "mailhandoff.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

using namespace MailHandoff;

class MailHandoffTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void classifiesLocalRemoteAndUnsupportedItems()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("report.pdf"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("x");
        out.close();
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("folder")));
        const QString link = dir.filePath(QStringLiteral("link.pdf"));
        QVERIFY(QFile::link(file, link));
        const QString fifo = dir.filePath(QStringLiteral("pipe"));
        QCOMPARE(mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);

        const QUrl remote(QStringLiteral("sftp://user:secret@host/notes.txt"));
        const QUrl remoteFolder(QStringLiteral("sftp://host/folder"));
        const KFileItemList items{
            KFileItem(QUrl::fromLocalFile(file)),
            KFileItem(QUrl::fromLocalFile(dir.filePath(QStringLiteral("folder")))),
            KFileItem(QUrl::fromLocalFile(link)),
            KFileItem(QUrl::fromLocalFile(fifo)),
            KFileItem(remote, QStringLiteral("text/plain"), S_IFREG),
            KFileItem(remote, QStringLiteral("text/plain"), S_IFREG),
            KFileItem(remoteFolder, QStringLiteral("inode/directory"), S_IFDIR),
            KFileItem(QUrl::fromLocalFile(dir.filePath(QStringLiteral("gone.txt"))), QStringLiteral("text/plain"), S_IFREG),
        };
        const Selection selection = classify(items);
        QCOMPARE(selection.files, QList<QUrl>{QUrl::fromLocalFile(file)});
        QCOMPARE(selection.remote, QList<QUrl>{remote});
        QCOMPARE(selection.rejected.size(), 4);
        QCOMPARE(selection.rejected.at(0).reason, Reason::Folder);
        QCOMPARE(selection.rejected.at(1).reason, Reason::NotRegularFile);
        QCOMPARE(selection.rejected.at(2).url, remoteFolder);
        QCOMPARE(selection.rejected.at(2).reason, Reason::Folder);
        QCOMPARE(selection.rejected.at(3).reason, Reason::Missing);
        for (const Rejected &rejected : selection.rejected) {
            QVERIFY(!reasonText(rejected.reason).isEmpty());
        }
    }

    void rejectsUnreadableFiles()
    {
        if (geteuid() == 0) {
            QSKIP("root can read files without read permission");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("private.txt"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.close();
        QVERIFY(QFile::setPermissions(file, QFileDevice::WriteOwner));
        const Selection selection = classify({KFileItem(QUrl::fromLocalFile(file))});
        QVERIFY(selection.files.isEmpty());
        QCOMPARE(selection.rejected.size(), 1);
        QCOMPARE(selection.rejected.first().reason, Reason::Unreadable);
    }

    void hidesPasswordsInDisplayNames()
    {
        const QString shown = displayName(QUrl(QStringLiteral("sftp://user:secret@host/notes.txt")));
        QVERIFY(!shown.contains(QStringLiteral("secret")));
        QVERIFY(shown.contains(QStringLiteral("host/notes.txt")));
        QCOMPARE(displayName(QUrl::fromLocalFile(QStringLiteral("/tmp/a b.txt"))), QStringLiteral("/tmp/a b.txt"));
    }

    void flagsClientsThatIgnoreAttachments()
    {
        QVERIFY(ignoresAttachments(QStringLiteral("io.github.mikepadiernos.plasma_impresario.desktop")));
        QVERIFY(!ignoresAttachments(QStringLiteral("org.kde.kmail2.desktop")));
        QVERIFY(!ignoresAttachments(QString()));
    }

    void buildsPortalOptions()
    {
        QVERIFY(portalOptions({}).isEmpty());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("a.txt")));
        QVERIFY(file.open(QIODevice::ReadWrite));
        const QVariantMap options = portalOptions({QDBusUnixFileDescriptor(file.handle())});
        QCOMPARE(options.keys(), QStringList{QStringLiteral("attachment_fds")});
        const auto descriptors = options.value(QStringLiteral("attachment_fds")).value<QList<QDBusUnixFileDescriptor>>();
        QCOMPARE(descriptors.size(), 1);
        QVERIFY(descriptors.first().isValid());
    }
};

QTEST_GUILESS_MAIN(MailHandoffTest)
#include "mailhandofftest.moc"
