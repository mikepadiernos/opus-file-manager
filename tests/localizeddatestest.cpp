// SPDX-License-Identifier: GPL-3.0-or-later

#include "localizeddates.h"
#include "localizednumbers.h"
#include "settings.h"

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class LocalizedDatesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void formatsNowruzAndPreservesGregorianDefaults()
    {
        const QDateTime nowruz(QDate(2024, 3, 20), QTime(13, 45, 37));
        QLocale persian(QStringLiteral("fa_IR"));
        persian.setNumberOptions(QLocale::OmitGroupSeparator);
        const QLocale english(QStringLiteral("en_US"));
        QCOMPARE(LocalizedDates::calendar(persian).partsFromDate(nowruz.date()).year, 1403);
        QCOMPARE(LocalizedDates::calendar(persian).partsFromDate(nowruz.date()).month, 1);
        QCOMPARE(LocalizedDates::calendar(persian).partsFromDate(nowruz.date()).day, 1);
        QVERIFY(LocalizedDates::date(nowruz.date(), persian).contains(persian.toString(1403)));
        QVERIFY(LocalizedDates::dateTime(nowruz, persian).contains(persian.toString(1403)));
        QCOMPARE(LocalizedDates::dateTime(nowruz, english), english.toString(nowruz, QLocale::ShortFormat));
        QCOMPARE(LocalizedDates::calendar(persian, true).partsFromDate(nowruz.date()).year, 2024);
        QVERIFY(LocalizedDates::dateTime(QDateTime(), persian).isEmpty());
    }

    void dateGroupsRespectCalendarBoundaries()
    {
        const QCalendar jalali = LocalizedDates::calendar(QLocale(QStringLiteral("fa_IR")));
        const QCalendar gregorian = LocalizedDates::calendar(QLocale::c());
        using Group = LocalizedDates::Group;
        QCOMPARE(LocalizedDates::group(QDate(2024, 3, 25), QDate(2024, 4, 4), jalali), Group::ThisMonth);
        QCOMPARE(LocalizedDates::group(QDate(2024, 3, 25), QDate(2024, 4, 4), gregorian), Group::ThisYear);
        QCOMPARE(LocalizedDates::group(QDate(2024, 3, 19), QDate(2024, 3, 28), jalali), Group::Older);
        QCOMPARE(LocalizedDates::group(QDate(2024, 3, 19), QDate(2024, 3, 28), gregorian), Group::ThisMonth);
        QCOMPARE(LocalizedDates::group(QDate(2024, 3, 27), QDate(2024, 3, 28), jalali), Group::Yesterday);
    }

    void calendarAndFormatsOverrideListingDefaults()
    {
        const QDateTime nowruz(QDate(2024, 3, 20), QTime(13, 45, 37));
        const QLocale english(QStringLiteral("en_US"));
        Settings::setCalendarOverride(Settings::EnumCalendarOverride::JalaliCalendar);
        Settings::setCustomDateFormat(QStringLiteral("yyyy/MM/dd"));
        Settings::setCustomTimeFormat(QStringLiteral("HH:mm:ss"));
        QCOMPARE(LocalizedDates::listingDateTime(nowruz, true, false, english), QStringLiteral("1403/01/01 13:45:37"));
        QCOMPARE(LocalizedDates::date(nowruz.date(), english), QStringLiteral("1403/01/01"));
        Settings::setCalendarOverride(Settings::EnumCalendarOverride::GregorianCalendar);
        QCOMPARE(LocalizedDates::listingDateTime(nowruz, true, false, english), QStringLiteral("2024/03/20 13:45:37"));
        Settings::setCalendarOverride(Settings::EnumCalendarOverride::FollowRegionalLocale);
        Settings::setCustomDateFormat(QString());
        Settings::setCustomTimeFormat(QString());
        QCOMPARE(LocalizedDates::listingDateTime(nowruz, true, false, QLocale(QStringLiteral("fa_IR"))), QStringLiteral("2024-03-20 13:45"));
    }

    void digitShapesAndCustomSymbols()
    {
        QVERIFY(!LocalizedNumbers::validCustomNumerals(QStringLiteral("123")));
        QVERIFY(!LocalizedNumbers::validCustomNumerals(QStringLiteral("0000000000")));
        QVERIFY(LocalizedNumbers::validCustomNumerals(QStringLiteral("abcdefghij")));
        Settings::setNumeralStyle(Settings::EnumNumeralStyle::WesternDigits);
        QCOMPARE(LocalizedNumbers::shape(QString::fromUtf8("۱۲۳")), QStringLiteral("123"));
        Settings::setNumeralStyle(Settings::EnumNumeralStyle::EasternArabicIndicDigits);
        QCOMPARE(LocalizedNumbers::shape(QStringLiteral("12.3")), QString::fromUtf8("۱۲.۳"));
        Settings::setCustomNumerals(QStringLiteral("abcdefghij"));
        Settings::setNumeralStyle(Settings::EnumNumeralStyle::CustomDigits);
        QCOMPARE(LocalizedNumbers::shape(QStringLiteral("12.3")), QStringLiteral("bc.d"));
        QCOMPARE(LocalizedDates::listingDateTime(QDateTime(QDate(2024, 3, 20), QTime(13, 45)), true, false, QLocale::c()), QStringLiteral("cace-ad-ca bd:ef"));
        Settings::setNumeralStyle(Settings::EnumNumeralStyle::LocaleDigits);
        Settings::setCustomNumerals(QStringLiteral("0123456789"));
    }

    void formatsFileTimes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("sample")));
        QVERIFY(file.open(QIODevice::ReadWrite));
        const QDateTime modified(QDate(2024, 3, 20), QTime(13, 45, 37));
        QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
        file.close();
        const KFileItem item(QUrl::fromLocalFile(file.fileName()));
        QLocale persian(QStringLiteral("fa_IR"));
        persian.setNumberOptions(QLocale::OmitGroupSeparator);
        QVERIFY(LocalizedDates::fileTime(item, KFileItem::ModificationTime, persian).contains(persian.toString(1403)));
        const QLocale german(QStringLiteral("de_DE"));
        QCOMPARE(LocalizedDates::fileTime(item, KFileItem::ModificationTime, german), LocalizedDates::dateTime(item.time(KFileItem::ModificationTime), german));
    }
};

QTEST_GUILESS_MAIN(LocalizedDatesTest)

#include "localizeddatestest.moc"
