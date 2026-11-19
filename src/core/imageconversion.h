// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QString>
#include <QUrl>

namespace ImageConversion
{
struct Entry {
    QUrl source;
    QString destination;
};

struct Plan {
    QList<Entry> entries;
    QString error;
};

struct Failure {
    Entry entry;
    QString error;
};

Plan plan(const QList<QUrl> &sources, const QString &directory, const QByteArray &format);
QList<Failure> convert(const QList<Entry> &entries, const QByteArray &format, const QSize &bounds, int quality);
}
