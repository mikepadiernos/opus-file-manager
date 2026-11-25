// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"

#include <KConfigGroup>
#include <QUrl>

namespace SavedSearches
{
struct Search {
    QString name;
    QString text;
    QUrl folder;
    LiveSearch::Field field = LiveSearch::Field::FileName;
    bool indexed = false;
    PathFilter filter{};
    QString id;
    /// Searches the configured search folders instead of the folder.
    bool searchFolders = false;
    bool operator==(const Search &other) const = default;
};

QList<Search> load(const KConfigGroup &group, QString *error);
bool save(KConfigGroup group, const QList<Search> &searches, QString *error);
}
