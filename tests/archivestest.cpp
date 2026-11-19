// SPDX-License-Identifier: GPL-3.0-or-later

#include "archives.h"

#include <KZip>

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class ArchivesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void browseUrlUsesTheArchiveWorker()
    {
        const QUrl url = Archives::browseUrl(QUrl::fromLocalFile(QStringLiteral("/data/photos.zip")), QStringLiteral("application/zip"));
        // Whichever worker the system registers for zip, such as zip or krarc.
        QVERIFY(!url.scheme().isEmpty());
        QVERIFY(url.scheme() != QStringLiteral("file"));
        QCOMPARE(url.path(), QStringLiteral("/data/photos.zip/"));
    }

    void browseUrlIsEmptyForOtherFiles()
    {
        QVERIFY(Archives::browseUrl(QUrl::fromLocalFile(QStringLiteral("/data/notes.txt")), QStringLiteral("text/plain")).isEmpty());
        QVERIFY(Archives::browseUrl(QUrl(QStringLiteral("sftp://host/a.zip")), QStringLiteral("application/zip")).isEmpty());
    }

    void baseNameDropsArchiveExtensions()
    {
        QCOMPARE(Archives::baseName(QStringLiteral("photos.tar.gz")), QStringLiteral("photos"));
        QCOMPARE(Archives::baseName(QStringLiteral("backup.2026.zip")), QStringLiteral("backup.2026"));
        QCOMPARE(Archives::baseName(QStringLiteral("README")), QStringLiteral("README"));
    }

    void integrityTestReadsArchiveEntries()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("valid.zip"));
        KZip archive(path);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        QVERIFY(archive.writeFile(QStringLiteral("nested/file.txt"), QByteArray(100000, 'x')));
        QVERIFY(archive.close());

        QString error;
        QCOMPARE(Archives::testIntegrity(path, QString(), &error), Archives::TestResult::Passed);
        QVERIFY(error.isEmpty());
    }

    void integrityTestRejectsInvalidArchive()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("invalid.zip"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("not a zip"), 9);
        file.close();

        QString error;
        QCOMPARE(Archives::testIntegrity(path, QString(), &error), Archives::TestResult::Failed);
        QVERIFY(!error.isEmpty());
    }

    void integrityTestSupportsPasswordProtectedSevenZip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString sevenZip = QStandardPaths::findExecutable(QStringLiteral("7z"));
        if (sevenZip.isEmpty()) {
            QSKIP("7z is needed to create an encrypted test archive");
        }
        QFile input(dir.filePath(QStringLiteral("secret.txt")));
        QVERIFY(input.open(QIODevice::WriteOnly));
        QCOMPARE(input.write("protected data"), 14);
        input.close();
        const QString path = dir.filePath(QStringLiteral("protected.7z"));
        QProcess process;
        process.setWorkingDirectory(dir.path());
        process.start(sevenZip, {QStringLiteral("a"), QStringLiteral("-t7z"), QStringLiteral("-psecret"), QStringLiteral("-mhe=on"), path, input.fileName()});
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);

        QString error;
        const auto result = Archives::testIntegrity(path, QString(), &error);
        QVERIFY2(result == Archives::TestResult::PasswordRequired, qPrintable(error));
        QCOMPARE(Archives::testIntegrity(path, QStringLiteral("secret"), &error), Archives::TestResult::Passed);
        QCOMPARE(Archives::testIntegrity(path, QStringLiteral("wrong"), &error), Archives::TestResult::Failed);
    }
};

QTEST_GUILESS_MAIN(ArchivesTest)

#include "archivestest.moc"
