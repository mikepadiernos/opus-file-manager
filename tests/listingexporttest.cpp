// SPDX-License-Identifier: GPL-3.0-or-later

#include "listingexport.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class ListingExportTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void csvQuotesAndProtectsSpreadsheetValues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const ListingExport::Table table{{QStringLiteral("Name"), QStringLiteral("Other")},
                                         {{QStringLiteral("a,\"b\nc"), QStringLiteral(";")},
                                          {QStringLiteral(" =1+1"), QStringLiteral("-safe")},
                                          {QStringLiteral("unicode-é"), QStringLiteral("ordinary")}}};
        const QString path = dir.filePath(QStringLiteral("listing.csv"));
        QVERIFY(ListingExport::toCsv(table, path).isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("\xef\xbb\xbfName,Other\r\n\"a,\"\"b\nc\",\";\"\r\n' =1+1,'-safe\r\nunicode-\xc3\xa9,ordinary\r\n"));
        file.close();
        const ListingExport::Table invalid{{QStringLiteral("one")}, {{QStringLiteral("one"), QStringLiteral("two")}}};
        QVERIFY(!ListingExport::toCsv(invalid, path).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().startsWith(QByteArray("\xef\xbb\xbfName")));
        QVERIFY(!ListingExport::toCsv(table, dir.filePath(QStringLiteral("missing/out.csv"))).isEmpty());
    }

    void pdfEscapesCellsAndIncludesAllPages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ListingExport::Table table{{QStringLiteral("Name"), QStringLiteral("Other")}, {}};
        table.rows.append({QStringLiteral("<tag> & value"), QStringLiteral("first")});
        for (int i = 0; i < 250; ++i) {
            table.rows.append({QStringLiteral("item-%1").arg(i), QStringLiteral("value")});
        }
        const QString path = dir.filePath(QStringLiteral("listing.pdf"));
        QVERIFY(ListingExport::toPdf(table, path).isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.read(5), QByteArray("%PDF-"));
        const QString tool = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
        if (!tool.isEmpty()) {
            QProcess process;
            process.start(tool, {path, QStringLiteral("-")});
            QVERIFY(process.waitForFinished());
            QCOMPARE(process.exitCode(), 0);
            const QString text = QString::fromUtf8(process.readAllStandardOutput());
            QVERIFY(text.contains(QStringLiteral("<tag> & value")));
            QVERIFY(text.contains(QStringLiteral("item-249")));
        }
        QVERIFY(!ListingExport::toPdf(table, dir.filePath(QStringLiteral("missing/out.pdf"))).isEmpty());
    }
};

QTEST_MAIN(ListingExportTest)
#include "listingexporttest.moc"
