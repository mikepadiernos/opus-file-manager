// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QStringList>

namespace FolderMetadata
{
struct Format {
    QString view;
    QStringList columns;
    QString sortKey;
    bool sortAscending = true;
    QString group;
};

/// Reads validated metadata from a folder's .mfm.json.
QJsonObject read(const QString &folder, QString *error = nullptr);

/// Atomically writes metadata, deleting .mfm.json when the object is empty.
bool write(const QString &folder, const QJsonObject &metadata, QString *error = nullptr);

bool setLabel(const QString &folder, const QString &name, const QString &label, QString *error = nullptr);
bool setColor(const QString &folder, const QString &name, const QString &color, QString *error = nullptr);
bool setRating(const QString &folder, const QString &name, int rating, QString *error = nullptr);
bool setNote(const QString &folder, const QString &name, const QString &note, QString *error = nullptr);
bool togglePin(const QString &folder, const QString &name, QString *error = nullptr);
bool setFolderColor(const QString &folder, const QString &color, QString *error = nullptr);
bool setFormat(const QString &folder, const Format &format, QString *error = nullptr);

/// Validates the per-folder view format before it is applied to a view.
bool format(const QJsonObject &metadata, Format *result);
}
