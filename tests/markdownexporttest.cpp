// SPDX-License-Identifier: GPL-3.0-or-later

#include "markdownexport.h"

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class MarkdownExportTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exportAndProtectSource()
    {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile source(directory.filePath(QStringLiteral("source.md")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("# source"), 8);
        source.close();
        const QUrl sourceUrl = QUrl::fromLocalFile(source.fileName());
        const QString pdf = directory.filePath(QStringLiteral("output.pdf"));
        QVERIFY(MarkdownExport::toPdf(QStringLiteral("# Heading"), sourceUrl, pdf).isEmpty());
        QFile output(pdf);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QVERIFY(output.read(5) == QByteArray("%PDF-"));
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("overwrite"), sourceUrl, source.fileName()).isEmpty());
        const QString link = directory.filePath(QStringLiteral("source-link"));
        QVERIFY(QFile::link(source.fileName(), link));
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("overwrite"), sourceUrl, link).isEmpty());
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), QByteArray("# source"));
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("text"), sourceUrl, directory.filePath(QStringLiteral("missing/out.pdf"))).isEmpty());
    }
};

QTEST_MAIN(MarkdownExportTest)
#include "markdownexporttest.moc"
