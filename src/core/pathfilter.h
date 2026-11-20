// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

class QJsonObject;
class QJsonValue;

struct PathFilter {
    QString pathText;
    QString parentText;
    QStringList excludedFolders;
    int maxDepth = -1;

    bool isActive() const;
    QString error() const;
    bool excludes(const QString &path) const;
    bool matches(const QString &root, const QString &path) const;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonValue &value, PathFilter *filter);
    bool operator==(const PathFilter &other) const = default;
};
