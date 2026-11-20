// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedString>

#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TranslationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
