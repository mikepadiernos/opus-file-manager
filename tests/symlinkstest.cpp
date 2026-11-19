// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlinks.h"

#include <KIO/SimpleJob>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SymlinksTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void resolvesLinkedFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("real/sub")));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("real")), dir.filePath(QStringLiteral("link"))));

        const QUrl resolved = Symlinks::resolve(QUrl::fromLocalFile(dir.filePath(QStringLiteral("link/sub"))));
        QCOMPARE(resolved, QUrl::fromLocalFile(QDir(dir.filePath(QStringLiteral("real/sub"))).canonicalPath()));
    }

    void leavesOtherUrlsAlone()
    {
        const QUrl remote(QStringLiteral("sftp://host/link"));
        QCOMPARE(Symlinks::resolve(remote), remote);
        const QUrl missing = QUrl::fromLocalFile(QStringLiteral("/no/such/folder"));
        QCOMPARE(Symlinks::resolve(missing), missing);
    }

    void createsFileAndFolderLinks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("source")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
        file.close();
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("folder")));
        QString error;
        KIO::SimpleJob *fileJob = Symlinks::create(QUrl::fromLocalFile(file.fileName()), QStringLiteral("file link"), error);
        QVERIFY2(fileJob, qPrintable(error));
        QVERIFY2(fileJob->exec(), qPrintable(fileJob->errorString()));
        QCOMPARE(QFileInfo(dir.filePath(QStringLiteral("file link"))).symLinkTarget(), file.fileName());
        KIO::SimpleJob *folderJob = Symlinks::create(QUrl::fromLocalFile(dir.filePath(QStringLiteral("folder"))), QStringLiteral("folder link"), error);
        QVERIFY2(folderJob, qPrintable(error));
        QVERIFY2(folderJob->exec(), qPrintable(folderJob->errorString()));
        QCOMPARE(QFileInfo(dir.filePath(QStringLiteral("folder link"))).symLinkTarget(), dir.filePath(QStringLiteral("folder")));
    }

    void refusesInvalidRequestsAndExistingDestinations()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("source")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("original"), 8);
        file.close();
        const QUrl target = QUrl::fromLocalFile(file.fileName());
        QString error;
        for (const QString &name : {QString(), QStringLiteral("."), QStringLiteral(".."), QStringLiteral("other/link"), QString(QChar::Null)}) {
            QVERIFY(!Symlinks::create(target, name, error));
            QVERIFY(!error.isEmpty());
        }
        QVERIFY(!Symlinks::create(QUrl(QStringLiteral("sftp://host/file")), QStringLiteral("link"), error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!Symlinks::create(QUrl::fromLocalFile(QStringLiteral("relative")), QStringLiteral("link"), error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!Symlinks::create(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing"))), QStringLiteral("link"), error));
        QVERIFY(!error.isEmpty());

        const QString dangling = dir.filePath(QStringLiteral("dangling"));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("missing")), dangling));
        for (const QString &name : {QStringLiteral("source"), QStringLiteral("dangling")}) {
            KIO::SimpleJob *job = Symlinks::create(target, name, error);
            QVERIFY2(job, qPrintable(error));
            QVERIFY(!job->exec());
            QVERIFY(job->error());
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("original"));
        QVERIFY(QFileInfo(dangling).isSymLink());
    }
};

QTEST_GUILESS_MAIN(SymlinksTest)

#include "symlinkstest.moc"
