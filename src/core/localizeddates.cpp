// SPDX-License-Identifier: GPL-3.0-or-later

#include "localizeddates.h"

#if !QT_CONFIG(jalalicalendar)
#error "Opus requires Qt built with Jalali calendar support"
#endif

QCalendar LocalizedDates::calendar(const QLocale &locale, bool iso)
{
    return QCalendar(!iso && locale.language() == QLocale::Persian ? QCalendar::System::Jalali : QCalendar::System::Gregorian);
}

QString LocalizedDates::date(const QDate &value, const QLocale &locale)
{
    return locale.toString(value, QLocale::ShortFormat, calendar(locale));
}

QString LocalizedDates::dateTime(const QDateTime &value, const QLocale &locale)
{
    return locale.toString(value, QLocale::ShortFormat, calendar(locale));
}

QString LocalizedDates::fileTime(const KFileItem &item, KFileItem::FileTimes which, const QLocale &locale)
{
    return locale.language() == QLocale::Persian ? dateTime(item.time(which), locale) : item.timeString(which);
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
