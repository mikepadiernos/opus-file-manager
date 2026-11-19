// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QStringList>

namespace ListingExport
{
struct Table {
    QStringList headers;
    QList<QStringList> rows;
};

/// Returns an error message on failure, or an empty string on success.
QString toCsv(const Table &table, const QString &destination);
QString toPdf(const Table &table, const QString &destination);
}
