// SPDX-License-Identifier: GPL-3.0-or-later

#include "checksums.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

class ChecksumsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void createsAndVerifiesStandardChecksums()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("alpha"));
        const QString second = dir.filePath(QStringLiteral("back\\slash\nname"));
        for (const QString &path : {first, second}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("abc"), 3);
        }
        const QString manifest = dir.filePath(QStringLiteral("sums.sha256"));
        const auto created = Checksums::create({first, second}, manifest);
        QVERIFY2(created.error.isEmpty(), qPrintable(created.error));
        QCOMPARE(created.entries.size(), 2);
        const auto verified = Checksums::verify(manifest);
        QVERIFY2(verified.error.isEmpty(), qPrintable(verified.error));
        QCOMPARE(verified.entries.size(), 2);
        QVERIFY(verified.entries.at(0).error.isEmpty());
        QVERIFY(verified.entries.at(1).error.isEmpty());
        const QString tool = QStandardPaths::findExecutable(QStringLiteral("sha256sum"));
        if (!tool.isEmpty()) {
            QProcess process;
            process.setWorkingDirectory(dir.path());
            process.start(tool, {QStringLiteral("-c"), manifest});
            QVERIFY(process.waitForFinished());
            QCOMPARE(process.exitCode(), 0);
        }
        QFile changed(first);
        QVERIFY(changed.open(QIODevice::WriteOnly | QIODevice::Truncate));
        changed.write("changed");
        changed.close();
        QVERIFY(QFile::remove(second));
        const auto mismatch = Checksums::verify(manifest);
        QVERIFY(mismatch.error.isEmpty());
        QVERIFY(!mismatch.entries.at(0).error.isEmpty());
        QVERIFY(!mismatch.entries.at(1).error.isEmpty());
    }

    void rejectsUnsafeEntriesAndPreservesFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString fileName = dir.filePath(QStringLiteral("input"));
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("source");
        file.close();
        QVERIFY(!Checksums::create({fileName}, fileName).error.isEmpty());
        const QString outside = dir.filePath(QStringLiteral("../outside"));
        QVERIFY(!Checksums::create({fileName}, outside).error.isEmpty());
        const QString manifest = dir.filePath(QStringLiteral("sums"));
        const QString outputLink = dir.filePath(QStringLiteral("output-link"));
        QVERIFY(QFile::link(fileName, outputLink));
        QVERIFY(!Checksums::create({fileName}, outputLink).error.isEmpty());
        QFile output(manifest);
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write("untouched");
        output.close();
        const auto canceled = Checksums::create({fileName}, manifest, [] {
            return true;
        });
        QVERIFY(canceled.canceled);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), QByteArray("untouched"));
        output.close();
        QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
        output.write(QByteArray(64, '0') + "  ../outside\n");
        output.close();
        QVERIFY(!Checksums::verify(manifest).error.isEmpty());
        QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
        output.write(QByteArray("\\") + QByteArray(64, '0') + "  bad\\x\n");
        output.close();
        QVERIFY(!Checksums::verify(manifest).error.isEmpty());
        QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
        output.write(QByteArray(64, '0') + "  alias\n");
        output.close();
        QVERIFY(QFile::link(fileName, dir.filePath(QStringLiteral("alias"))));
        const auto result = Checksums::verify(manifest);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.entries.size(), 1);
        QVERIFY(!result.entries.first().error.isEmpty());
        const QByteArray fifo = QFile::encodeName(dir.filePath(QStringLiteral("fifo")));
        QCOMPARE(::mkfifo(fifo.constData(), 0600), 0);
        QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
        output.write(QByteArray(64, '0') + "  fifo\n");
        output.close();
        const auto special = Checksums::verify(manifest);
        QVERIFY(special.error.isEmpty());
        QVERIFY(!special.entries.first().error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("source"));
    }
};

QTEST_GUILESS_MAIN(ChecksumsTest)
#include "checksumstest.moc"
