// SPDX-License-Identifier: GPL-3.0-or-later

#include "officeapps.h"

#include <QFile>
#include <QScopeGuard>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using OfficeApps::App;
using OfficeApps::Kind;

namespace
{
const QString Odt = QStringLiteral("application/vnd.oasis.opendocument.text");
const QString Xlsx = QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
const QString Odp = QStringLiteral("application/vnd.oasis.opendocument.presentation");
}

class OfficeAppsTest : public QObject
{
    Q_OBJECT

private:
    static bool writeExecutable(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write("#!/bin/sh\n") < 0) {
            return false;
        }
        file.close();
        return file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_bin.isValid());
        m_path = qgetenv("PATH");
        qputenv("PATH", m_bin.path().toLocal8Bit());
    }

    void cleanupTestCase()
    {
        qputenv("PATH", m_path);
    }

    void classifiesOfficeFormats_data()
    {
        QTest::addColumn<QString>("mimeType");
        QTest::addColumn<Kind>("kind");
        QTest::newRow("odt") << Odt << Kind::Document;
        QTest::newRow("ott") << QStringLiteral("application/vnd.oasis.opendocument.text-template") << Kind::Document;
        QTest::newRow("doc") << QStringLiteral("application/msword") << Kind::Document;
        QTest::newRow("docx") << QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document") << Kind::Document;
        QTest::newRow("docm") << QStringLiteral("application/vnd.ms-word.document.macroEnabled.12") << Kind::Document;
        QTest::newRow("ods") << QStringLiteral("application/vnd.oasis.opendocument.spreadsheet") << Kind::Spreadsheet;
        QTest::newRow("xls") << QStringLiteral("application/vnd.ms-excel") << Kind::Spreadsheet;
        QTest::newRow("xlsx") << Xlsx << Kind::Spreadsheet;
        QTest::newRow("odp") << Odp << Kind::Presentation;
        QTest::newRow("ppt") << QStringLiteral("application/vnd.ms-powerpoint") << Kind::Presentation;
        QTest::newRow("pptx") << QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation") << Kind::Presentation;
        QTest::newRow("pdf") << QStringLiteral("application/pdf") << Kind::None;
        QTest::newRow("csv") << QStringLiteral("text/csv") << Kind::None;
        QTest::newRow("rtf") << QStringLiteral("application/rtf") << Kind::None;
        QTest::newRow("odg") << QStringLiteral("application/vnd.oasis.opendocument.graphics") << Kind::None;
        QTest::newRow("empty") << QString() << Kind::None;
    }

    void classifiesOfficeFormats()
    {
        QFETCH(QString, mimeType);
        QFETCH(Kind, kind);
        QCOMPARE(OfficeApps::kind(mimeType), kind);
        QCOMPARE(OfficeApps::isOfficeDocument(mimeType), kind != Kind::None);
    }

    void describesApplications()
    {
        QCOMPARE(OfficeApps::apps(), (QList<App>{App::LibreOffice, App::Calligra, App::OnlyOffice}));
        QSet<QString> ids;
        for (const App app : OfficeApps::apps()) {
            ids.insert(OfficeApps::id(app));
            QVERIFY(!OfficeApps::name(app).isEmpty());
            QVERIFY(!OfficeApps::iconName(app).isEmpty());
        }
        QCOMPARE(ids.size(), 3);
        QCOMPARE(OfficeApps::executable(App::Calligra, Kind::Spreadsheet), QStringLiteral("calligrasheets"));
        QCOMPARE(OfficeApps::executable(App::Calligra, Kind::Presentation), QStringLiteral("calligrastage"));
        QVERIFY(OfficeApps::executable(App::Calligra, Kind::None).isEmpty());
        QCOMPARE(OfficeApps::desktopName(App::Calligra, Kind::Document), QStringLiteral("org.kde.calligrawords"));
        QCOMPARE(OfficeApps::desktopName(App::LibreOffice, Kind::Spreadsheet), QStringLiteral("libreoffice-calc"));
        QCOMPARE(OfficeApps::desktopName(App::LibreOffice, Kind::None), QStringLiteral("libreoffice-startcenter"));
        QVERIFY(OfficeApps::opensRemoteFiles(App::Calligra));
        QVERIFY(!OfficeApps::opensRemoteFiles(App::LibreOffice));
    }

    void reportsMissingApplications()
    {
        for (const App app : OfficeApps::apps()) {
            QVERIFY(OfficeApps::unavailableReason(app, {Kind::Document}).contains(OfficeApps::name(app)));
        }
        QVERIFY(OfficeApps::unavailableReason(App::LibreOffice, {}).isEmpty());
        // LibreOffice falls back to its soffice launcher.
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("soffice"))));
        QCOMPARE(OfficeApps::findExecutable(App::LibreOffice, Kind::Document), m_bin.filePath(QStringLiteral("soffice")));
        QVERIFY(OfficeApps::unavailableReason(App::LibreOffice, {Kind::Document, Kind::Presentation}).isEmpty());
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("calligrawords"))));
        QVERIFY(OfficeApps::unavailableReason(App::Calligra, {Kind::Document}).isEmpty());
        QVERIFY(OfficeApps::unavailableReason(App::Calligra, {Kind::Document, Kind::Spreadsheet}).contains(QStringLiteral("calligrasheets")));
    }

    void buildsLaunches()
    {
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("libreoffice"))));
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("calligrasheets"))));
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString letter = folder.filePath(QStringLiteral("--convert-to letter.odt"));
        const QString sheet = folder.filePath(QStringLiteral("sheet.xlsx"));
        const QList<QUrl> urls{QUrl::fromLocalFile(letter), QUrl::fromLocalFile(sheet)};
        QString error;

        QList<OfficeApps::Launch> launches = OfficeApps::launches(App::LibreOffice, urls, {Odt, Xlsx}, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(launches.size(), 1);
        QCOMPARE(launches.first().executable, m_bin.filePath(QStringLiteral("libreoffice")));
        QCOMPARE(launches.first().arguments, (QStringList{letter, sheet}));
        QCOMPARE(launches.first().kind, Kind::None);
        launches = OfficeApps::launches(App::LibreOffice, {urls.at(1)}, {Xlsx}, &error);
        QCOMPARE(launches.first().kind, Kind::Spreadsheet);

        launches = OfficeApps::launches(App::Calligra, urls, {Odt, Xlsx}, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(launches.size(), 2);
        QCOMPARE(launches.at(0).executable, m_bin.filePath(QStringLiteral("calligrawords")));
        QCOMPARE(launches.at(0).arguments, QStringList{letter});
        QCOMPARE(launches.at(1).executable, m_bin.filePath(QStringLiteral("calligrasheets")));
        QCOMPARE(launches.at(1).kind, Kind::Spreadsheet);

        const QUrl remote(QStringLiteral("smb://server/share/report.odp"));
        QVERIFY(OfficeApps::launches(App::Calligra, {remote}, {Odp}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("calligrastage")));
        QVERIFY(writeExecutable(m_bin.filePath(QStringLiteral("calligrastage"))));
        error.clear();
        launches = OfficeApps::launches(App::Calligra, {remote}, {Odp}, &error);
        QCOMPARE(launches.size(), 1);
        QCOMPARE(launches.first().arguments, QStringList{remote.toString()});
        QVERIFY(OfficeApps::launches(App::LibreOffice, {remote}, {Odp}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("local files")));
    }

    void rejectsUnsupportedSelections()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QString error;
        QVERIFY(OfficeApps::launches(App::LibreOffice, {}, {}, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(
            OfficeApps::launches(App::LibreOffice, {QUrl::fromLocalFile(folder.filePath(QStringLiteral("a.pdf")))}, {QStringLiteral("application/pdf")}, &error)
                .isEmpty());
        QVERIFY(error.contains(QStringLiteral("not an office document")));
        error.clear();
        QVERIFY(OfficeApps::launches(App::LibreOffice, {QUrl::fromLocalFile(folder.path())}, {Odt}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("not an office document")));
        error.clear();
        QVERIFY(OfficeApps::launches(App::OnlyOffice, {QUrl::fromLocalFile(folder.filePath(QStringLiteral("a.odt")))}, {Odt}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("ONLYOFFICE")));
    }

private:
    QTemporaryDir m_bin;
    QByteArray m_path;
};

QTEST_GUILESS_MAIN(OfficeAppsTest)

#include "officeappstest.moc"
