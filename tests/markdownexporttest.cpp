// SPDX-License-Identifier: GPL-3.0-or-later

#include "markdownexport.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
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
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("# Heading"), sourceUrl, pdf, QPageLayout()).isEmpty());
        QVERIFY(!QFile::exists(pdf));
        QVERIFY(MarkdownExport::toPdf(QStringLiteral("# Heading"), sourceUrl, pdf).isEmpty());
        QFile output(pdf);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QVERIFY(output.read(5) == QByteArray("%PDF-"));
        output.close();
        const QPageLayout letter(QPageSize(QPageSize::Letter), QPageLayout::Portrait, QMarginsF(20, 15, 10, 5), QPageLayout::Millimeter);
        QVERIFY(MarkdownExport::toPdf(QStringLiteral("# Heading"), sourceUrl, pdf, letter).isEmpty());
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QString content = QString::fromLatin1(output.readAll());
        const QRegularExpression mediaBox(QStringLiteral("/MediaBox\\s*\\[\\s*0(?:\\.0+)?\\s+0(?:\\.0+)?\\s+612(?:\\.0+)?\\s+792(?:\\.0+)?\\s*\\]"));
        QVERIFY2(mediaBox.match(content).hasMatch(), qPrintable(content.left(2000)));
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("overwrite"), sourceUrl, source.fileName()).isEmpty());
        const QString link = directory.filePath(QStringLiteral("source-link"));
        QVERIFY(QFile::link(source.fileName(), link));
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("overwrite"), sourceUrl, link).isEmpty());
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), QByteArray("# source"));
        QVERIFY(!MarkdownExport::toPdf(QStringLiteral("text"), sourceUrl, directory.filePath(QStringLiteral("missing/out.pdf"))).isEmpty());
    }

    void pageMarginsAffectTextPosition()
    {
        const QString tool = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
        if (tool.isEmpty()) {
            QSKIP("pdftotext is unavailable");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto textX = [&](int left) {
            const QString pdf = directory.filePath(QStringLiteral("%1.pdf").arg(left));
            const QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(left, 10, 10, 10), QPageLayout::Millimeter);
            if (!MarkdownExport::toPdf(QStringLiteral("word"), QUrl(), pdf, layout).isEmpty()) {
                return -1.0;
            }
            QProcess process;
            process.start(tool, {QStringLiteral("-bbox"), pdf, QStringLiteral("-")});
            if (!process.waitForFinished() || process.exitCode() != 0) {
                return -1.0;
            }
            const QString output = QString::fromUtf8(process.readAllStandardOutput());
            const auto match = QRegularExpression(QStringLiteral("<word xMin=\"([0-9.]+)\"")).match(output);
            return match.hasMatch() ? match.captured(1).toDouble() : -1.0;
        };
        const double left10 = textX(10);
        const double left40 = textX(40);
        QVERIFY(left10 >= 0);
        QVERIFY(left40 - left10 > 80 && left40 - left10 < 90);
    }
};

QTEST_MAIN(MarkdownExportTest)
#include "markdownexporttest.moc"
