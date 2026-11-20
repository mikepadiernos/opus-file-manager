// SPDX-License-Identifier: GPL-3.0-or-later

#include "localizeddates.h"

#include "localizednumbers.h"
#include "settings.h"

#if !QT_CONFIG(jalalicalendar)
#error "Opus requires Qt built with Jalali calendar support"
#endif

QCalendar LocalizedDates::calendar(const QLocale &locale, bool iso)
{
    if (Settings::calendarOverride() == Settings::EnumCalendarOverride::GregorianCalendar) {
        return QCalendar(QCalendar::System::Gregorian);
    }
    if (Settings::calendarOverride() == Settings::EnumCalendarOverride::JalaliCalendar) {
        return QCalendar(QCalendar::System::Jalali);
    }
    return QCalendar(!iso && locale.language() == QLocale::Persian ? QCalendar::System::Jalali : QCalendar::System::Gregorian);
}

QString LocalizedDates::date(const QDate &value, const QLocale &locale)
{
    const QString format = Settings::customDateFormat();
    return LocalizedNumbers::shape(format.isEmpty() ? locale.toString(value, QLocale::ShortFormat, calendar(locale))
                                                    : locale.toString(value, format, calendar(locale)));
}

QString LocalizedDates::dateTime(const QDateTime &value, const QLocale &locale)
{
    const QString dateFormat = Settings::customDateFormat();
    const QString timeFormat = Settings::customTimeFormat();
    if (dateFormat.isEmpty() && timeFormat.isEmpty()) {
        return LocalizedNumbers::shape(locale.toString(value, QLocale::ShortFormat, calendar(locale)));
    }
    const QString format = (dateFormat.isEmpty() ? locale.dateFormat(QLocale::ShortFormat) : dateFormat) + u' '
        + (timeFormat.isEmpty() ? locale.timeFormat(QLocale::ShortFormat) : timeFormat);
    return LocalizedNumbers::shape(locale.toString(value, format, calendar(locale)));
}

QString LocalizedDates::listingDateTime(const QDateTime &value, bool iso, bool showSeconds, const QLocale &locale)
{
    if (!value.isValid()) {
        return {};
    }
    const QString customDate = Settings::customDateFormat();
    const QString customTime = Settings::customTimeFormat();
    if (customDate.isEmpty() && customTime.isEmpty() && iso && Settings::calendarOverride() == Settings::EnumCalendarOverride::FollowRegionalLocale) {
        return LocalizedNumbers::shape(value.toString(showSeconds ? QStringLiteral("yyyy-MM-dd HH:mm:ss") : QStringLiteral("yyyy-MM-dd HH:mm")));
    }
    if (customDate.isEmpty() && customTime.isEmpty() && !iso && !showSeconds) {
        return dateTime(value, locale);
    }
    const QCalendar selectedCalendar = calendar(locale, iso);
    const QString dateFormat = !customDate.isEmpty() ? customDate : (iso ? QStringLiteral("yyyy-MM-dd") : locale.dateFormat(QLocale::ShortFormat));
    const QString timeFormat = !customTime.isEmpty() ? customTime
                                                     : (iso ? (showSeconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm"))
                                                            : locale.timeFormat(showSeconds ? QLocale::LongFormat : QLocale::ShortFormat));
    const QString format = dateFormat + u' ' + timeFormat;
    return LocalizedNumbers::shape(locale.toString(value, format, selectedCalendar));
}

QString LocalizedDates::fileTime(const KFileItem &item, KFileItem::FileTimes which, const QLocale &locale)
{
    return locale.language() == QLocale::Persian || locale != QLocale::system()
            || Settings::calendarOverride() != Settings::EnumCalendarOverride::FollowRegionalLocale || !Settings::customDateFormat().isEmpty()
            || !Settings::customTimeFormat().isEmpty() || Settings::numeralStyle() != Settings::EnumNumeralStyle::LocaleDigits
        ? dateTime(item.time(which), locale)
        : item.timeString(which);
}

LocalizedDates::Group LocalizedDates::group(const QDate &value, const QDate &today, const QCalendar &calendar)
{
    const qint64 daysAgo = value.daysTo(today);
    if (daysAgo <= 0) {
        return Group::Today;
    }
    if (daysAgo == 1) {
        return Group::Yesterday;
    }
    if (daysAgo < 7) {
        return Group::LastSevenDays;
    }
    const auto item = calendar.partsFromDate(value);
    const auto current = calendar.partsFromDate(today);
    if (item.year == current.year && item.month == current.month) {
        return Group::ThisMonth;
    }
    return item.year == current.year ? Group::ThisYear : Group::Older;
}
