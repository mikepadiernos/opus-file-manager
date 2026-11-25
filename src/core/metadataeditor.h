// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

namespace MetadataEditor
{
struct Snapshot {
    QString path;
    QString mimeType;
    QMap<QString, QString> values;
    quint64 device = 0;
    quint64 inode = 0;
    qint64 size = 0;
    qint64 modifiedSeconds = 0;
    qint64 modifiedNanoseconds = 0;
    qint64 changedSeconds = 0;
    qint64 changedNanoseconds = 0;
    QString error;
};

Snapshot read(const QString &path);
QString write(const Snapshot &snapshot, const QMap<QString, QString> &values);
/// Whether files of MIME type @p mimeType can be edited.
bool supports(const QString &mimeType);
}
