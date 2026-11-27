// SPDX-License-Identifier: GPL-3.0-or-later

#include "documentformats.h"
#include "settings.h"

#include <QFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using DocumentFormats::Kind;

class DocumentFormatsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Settings::self()->setDefaults();
    }

    void classifiesFormats_data()
    {
        QTest::addColumn<QString>("mimeType");
        QTest::addColumn<Kind>("kind");
        QTest::addColumn<bool>("viewer");
        QTest::newRow("pdf") << QStringLiteral("application/pdf") << Kind::Pdf << true;
        QTest::newRow("odt") << QStringLiteral("application/vnd.oasis.opendocument.text") << Kind::Office << true;
        QTest::newRow("odg") << QStringLiteral("application/vnd.oasis.opendocument.graphics") << Kind::Office << true;
        QTest::newRow("epub") << QStringLiteral("application/epub+zip") << Kind::Ebook << true;
        QTest::newRow("mobi") << QStringLiteral("application/x-mobipocket-ebook") << Kind::Ebook << true;
        QTest::newRow("fb2") << QStringLiteral("application/x-fictionbook+xml") << Kind::Ebook << true;
        QTest::newRow("cbz") << QStringLiteral("application/vnd.comicbook+zip") << Kind::Comic << true;
        QTest::newRow("cbr") << QStringLiteral("application/vnd.comicbook-rar") << Kind::Comic << true;
        QTest::newRow("djvu") << QStringLiteral("image/vnd.djvu") << Kind::DjVu << true;
        QTest::newRow("chm") << QStringLiteral("application/vnd.ms-htmlhelp") << Kind::HelpFile << true;
        QTest::newRow("chm alias") << QStringLiteral("application/x-chm") << Kind::HelpFile << true;
        QTest::newRow("ttf") << QStringLiteral("font/ttf") << Kind::Font << false;
        QTest::newRow("otf alias") << QStringLiteral("application/x-font-otf") << Kind::Font << false;
        QTest::newRow("woff2") << QStringLiteral("font/woff2") << Kind::Font << false;
        QTest::newRow("png") << QStringLiteral("image/png") << Kind::None << false;
        QTest::newRow("zip") << QStringLiteral("application/zip") << Kind::None << false;
        QTest::newRow("text") << QStringLiteral("text/plain") << Kind::None << false;
        QTest::newRow("empty") << QString() << Kind::None << false;
    }

    void classifiesFormats()
    {
        QFETCH(QString, mimeType);
        QFETCH(Kind, kind);
        QFETCH(bool, viewer);
        QCOMPARE(DocumentFormats::kind(mimeType), kind);
        QCOMPARE(DocumentFormats::usesDocumentViewer(mimeType), viewer);
    }

    void namesMissingViewers()
    {
        QVERIFY(DocumentFormats::missingViewerMessage(Kind::Ebook).contains(QStringLiteral("Install Okular")));
        QVERIFY(DocumentFormats::missingViewerMessage(Kind::HelpFile).contains(QStringLiteral("help files")));
        QVERIFY(DocumentFormats::missingViewerMessage(Kind::Office).contains(QStringLiteral("Calligra")));
        QVERIFY(DocumentFormats::missingViewerMessage(Kind::Pdf).contains(QStringLiteral("Okular")));
        QCOMPARE(DocumentFormats::missingViewerMessage(Kind::None), QStringLiteral("No document viewer is installed for this file type."));
    }

    void followsThePreviewSettings()
    {
        QVERIFY(DocumentFormats::previewEnabled(Kind::Office));
        QVERIFY(DocumentFormats::previewEnabled(Kind::Pdf));
        Settings::setOfficePreviews(false);
        QVERIFY(!DocumentFormats::previewEnabled(Kind::Office));
        QVERIFY(DocumentFormats::previewEnabled(Kind::Pdf));
        QVERIFY(DocumentFormats::previewDisabledMessage(Kind::Office).contains(QStringLiteral("Preferences › Applications › Office")));
        QVERIFY(DocumentFormats::previewDisabledMessage(Kind::Pdf).isEmpty());
    }

    void findsTheFontInstaller()
    {
        QVERIFY(DocumentFormats::isInstallableFont(QStringLiteral("font/ttf")));
        QVERIFY(DocumentFormats::isInstallableFont(QStringLiteral("application/x-font-otf")));
        QVERIFY(DocumentFormats::isInstallableFont(QStringLiteral("font/collection")));
        QVERIFY(!DocumentFormats::isInstallableFont(QStringLiteral("font/woff2")));
        QVERIFY(!DocumentFormats::isInstallableFont(QStringLiteral("image/png")));
        QVERIFY(!DocumentFormats::defaultSampleText().isEmpty());

        QTemporaryDir bin;
        QVERIFY(bin.isValid());
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", bin.path().toLocal8Bit());
        const auto restore = qScopeGuard([path] {
            qputenv("PATH", path);
        });
        QVERIFY(DocumentFormats::findFontInstaller().isEmpty());
        QVERIFY(DocumentFormats::fontInstallerUnavailableReason().contains(QStringLiteral("kfontinst")));
        QFile installer(bin.filePath(QStringLiteral("kfontinst")));
        QVERIFY(installer.open(QIODevice::WriteOnly));
        installer.close();
        QVERIFY(installer.setPermissions(installer.permissions() | QFileDevice::ExeOwner));
        QCOMPARE(DocumentFormats::findFontInstaller(), installer.fileName());
        QVERIFY(DocumentFormats::fontInstallerUnavailableReason().isEmpty());

        const QUrl local = QUrl::fromLocalFile(bin.filePath(QStringLiteral("--a/../--font.ttf")));
        const QUrl remote(QStringLiteral("sftp://server/fonts/font.otf"));
        QCOMPARE(DocumentFormats::fontInstallArguments({local, remote}), (QStringList{bin.filePath(QStringLiteral("--font.ttf")), remote.toString()}));
    }
};

QTEST_GUILESS_MAIN(DocumentFormatsTest)

#include "documentformatstest.moc"
