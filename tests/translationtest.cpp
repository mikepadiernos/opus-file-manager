// SPDX-License-Identifier: GPL-3.0-or-later

#include "interfacelanguage.h"

#include <KLocalizedString>

#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TranslationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void interfaceDirection_data()
    {
        QTest::addColumn<QStringList>("languages");
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::newRow("Persian") << QStringList{QStringLiteral("fa_IR")} << Qt::RightToLeft;
        QTest::newRow("Arabic") << QStringList{QStringLiteral("ar_EG")} << Qt::RightToLeft;
        QTest::newRow("Hebrew") << QStringList{QStringLiteral("he_IL")} << Qt::RightToLeft;
        QTest::newRow("English first") << (QStringList{QStringLiteral("en_US"), QStringLiteral("fa")}) << Qt::LeftToRight;
    }

    void interfaceDirection()
    {
        QFETCH(QStringList, languages);
        QFETCH(Qt::LayoutDirection, direction);
        QCOMPARE(InterfaceLanguage::layoutDirection(languages), direction);
    }

    void interfaceDirectionFromEnvironment()
    {
        for (const QString &language : {QStringLiteral("fa"), QStringLiteral("ar"), QStringLiteral("he")}) {
            QProcess process;
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
            environment.insert(QStringLiteral("LANGUAGE"), language);
            environment.insert(QStringLiteral("OPUSFM_DIRECTION_PROBE"), QStringLiteral("1"));
            process.setProcessEnvironment(environment);
            process.start(QCoreApplication::applicationFilePath(), {QStringLiteral("directionEnvironmentProbe")});
            QVERIFY(process.waitForFinished(30000));
            QCOMPARE(process.exitStatus(), QProcess::NormalExit);
            QVERIFY2(process.exitCode() == 0, process.readAll().constData());
        }
    }

    void directionEnvironmentProbe()
    {
        if (!qEnvironmentVariableIsSet("OPUSFM_DIRECTION_PROBE")) {
            QSKIP("Subprocess probe");
        }
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("opus-file-manager"), QStringLiteral(OPUSFM_TEST_LOCALE_DIR));
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
        QCOMPARE(InterfaceLanguage::layoutDirection(KLocalizedString::languages()), Qt::RightToLeft);
    }

    void productionCatalog()
    {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("OPUSFM_CATALOG_PROBE"), QStringLiteral("1"));
        process.setProcessEnvironment(environment);
        process.start(QCoreApplication::applicationFilePath(), {QStringLiteral("productionCatalogProbe")});
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAll().constData());
    }

    void productionCatalogProbe()
    {
        if (!qEnvironmentVariableIsSet("OPUSFM_CATALOG_PROBE")) {
            QSKIP("Subprocess probe");
        }
        QStandardPaths::setTestModeEnabled(true);
        QLocale::setDefault(QLocale::c());
        KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("opus-file-manager"), QStringLiteral(OPUSFM_PRODUCTION_LOCALE_DIR));
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
        KLocalizedString::setLanguages({QStringLiteral("fa")});
        QCOMPARE(i18nc("@action", "New Tab"), QString::fromUtf8("زبانهٔ جدید"));
        QCOMPARE(i18ncp("@info:status", "%1 item", "%1 items", 2), QString::fromUtf8("2 مورد"));
    }

    void catalogLoadsContextAndPlurals()
    {
        QStandardPaths::setTestModeEnabled(true);
        QLocale::setDefault(QLocale::c());
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
        KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("opus-file-manager"), QStringLiteral(OPUSFM_TEST_LOCALE_DIR));
        KLocalizedString::setLanguages({QStringLiteral("fa")});
        QCOMPARE(i18nc("@title:group", "Editor"), QString::fromUtf8("ویرایشگر"));
        QCOMPARE(i18np("Compared one item.", "Compared %1 items.", 1), QString::fromUtf8("یک مورد مقایسه شد."));
        QCOMPARE(i18np("Compared one item.", "Compared %1 items.", 2), QString::fromUtf8("2 مورد مقایسه شد."));
        KLocalizedString::clearLanguages();
    }

    void extractionIncludesCodeAndResources()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString output = directory.filePath(QStringLiteral("messages.pot"));
        QProcess process;
        process.setWorkingDirectory(QStringLiteral(OPUSFM_SOURCE_DIR));
        process.start(QStringLiteral("sh"), {QStringLiteral("scripts/extract-messages.sh"), output});
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        QFile pot(output);
        QVERIFY(pot.open(QIODevice::ReadOnly));
        const QByteArray messages = pot.readAll();
        QVERIFY(messages.contains("msgctxt \"@title:group\"\nmsgid \"Editor\""));
        QVERIFY(messages.contains("msgid \"Compared one item.\"\nmsgid_plural \"Compared %1 items.\""));
        QVERIFY(messages.contains("msgid \"&File\""));
        QVERIFY(messages.contains("src/app/opus-file-managerui.rc:6"));
        QVERIFY(messages.contains("msgid \"What the window shows when Opus starts\""));
    }
};

QTEST_GUILESS_MAIN(TranslationTest)

#include "translationtest.moc"
