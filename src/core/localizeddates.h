// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QCalendar>
#include <QDateTime>
#include <QLocale>

namespace LocalizedDates
{
enum class Group {
    Today,
    Yesterday,
    LastSevenDays,
    ThisMonth,
    ThisYear,
    Older,
};

QCalendar calendar(const QLocale &locale, bool iso = false);
QString date(const QDate &value, const QLocale &locale = QLocale());
QString dateTime(const QDateTime &value, const QLocale &locale = QLocale());
QString listingDateTime(const QDateTime &value, bool iso, bool showSeconds, const QLocale &locale = QLocale());
QString fileTime(const KFileItem &item, KFileItem::FileTimes which = KFileItem::ModificationTime, const QLocale &locale = QLocale());
Group group(const QDate &value, const QDate &today, const QCalendar &calendar);
}
