// SPDX-License-Identifier: GPL-3.0-or-later

#include "interfacelanguage.h"
#include "settings.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <clocale>

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

    void directionOverride()
    {
        const QStringList persian{QStringLiteral("fa")};
        const QStringList english{QStringLiteral("en_US")};
        QCOMPARE(InterfaceLanguage::layoutDirection(persian, Settings::EnumTranslationDirection::LeftToRight), Qt::LeftToRight);
        QCOMPARE(InterfaceLanguage::layoutDirection(english, Settings::EnumTranslationDirection::RightToLeft), Qt::RightToLeft);
        QCOMPARE(InterfaceLanguage::layoutDirection(persian, Settings::EnumTranslationDirection::FollowLanguage), Qt::RightToLeft);
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

    void selectedLanguageOverridesAndRestoresSystem()
    {
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
        KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("opus-file-manager"), QStringLiteral(OPUSFM_TEST_LOCALE_DIR));
        InterfaceLanguage::apply(QString());
        const QStringList systemLanguages = KLocalizedString::languages();
        InterfaceLanguage::apply(QStringLiteral("fa"));
        QCOMPARE(i18nc("@title:group", "Editor"), QString::fromUtf8("ویرایشگر"));
        QCOMPARE(InterfaceLanguage::layoutDirection(KLocalizedString::languages()), Qt::RightToLeft);
        InterfaceLanguage::apply(QStringLiteral("en_US"));
        QCOMPARE(i18nc("@title:group", "Editor"), QStringLiteral("Editor"));
        QCOMPARE(InterfaceLanguage::layoutDirection(KLocalizedString::languages()), Qt::LeftToRight);
        InterfaceLanguage::apply(QString());
        QCOMPARE(KLocalizedString::languages(), systemLanguages);
    }

    void startupUsesSavedInterfaceLanguage()
    {
        const QByteArray original(std::setlocale(LC_MESSAGES, nullptr));
        const bool messageLocaleAvailable =
            std::setlocale(LC_MESSAGES, "fa_IR.UTF-8") || std::setlocale(LC_MESSAGES, "en_US.UTF-8") || std::setlocale(LC_MESSAGES, "en_GB.UTF-8");
        std::setlocale(LC_MESSAGES, original.constData());
        if (!messageLocaleAvailable) {
            QSKIP("No non-C message locale available for gettext");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QDir root(directory.path());
        QVERIFY(root.mkpath(QStringLiteral("config")));
        QVERIFY(root.mkpath(QStringLiteral("data/locale/fa/LC_MESSAGES")));
        QFile config(root.filePath(QStringLiteral("config/opus-file-managerrc")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray settings("[Translation]\nInterfaceLanguage=fa\nRegionalLocale=en_US\n");
        QCOMPARE(config.write(settings), settings.size());
        config.close();
        QVERIFY(QFile::copy(QStringLiteral(OPUSFM_PRODUCTION_LOCALE_DIR "/fa/LC_MESSAGES/opus-file-manager.mo"),
                            root.filePath(QStringLiteral("data/locale/fa/LC_MESSAGES/opus-file-manager.mo"))));
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), directory.path());
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), root.filePath(QStringLiteral("config")));
        environment.insert(QStringLiteral("XDG_DATA_HOME"), root.filePath(QStringLiteral("data")));
        environment.insert(QStringLiteral("XDG_DATA_DIRS"), root.filePath(QStringLiteral("data")));
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), root.filePath(QStringLiteral("cache")));
        environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
        environment.insert(QStringLiteral("LANGUAGE"), QStringLiteral("en"));
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral(OPUSFM_EXECUTABLE), {QStringLiteral("--help")});
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        const QByteArray output = process.readAllStandardOutput();
        const QByteArray diagnostics = output + process.readAllStandardError();
        QVERIFY2(output.contains(QString::fromUtf8("مدیر پروندهٔ دو قسمتی").toUtf8()), diagnostics.constData());
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
