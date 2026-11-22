// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacelocations.h"

#include "favoritesmodel.h"

#include <KBookmark>
#include <KFilePlacesModel>

WorkspaceLocations::WorkspaceLocations(FavoritesModel *favorites, KFilePlacesModel *places, QObject *parent)
    : QObject(parent)
    , m_favorites(favorites)
    , m_places(places)
{
    const auto invalidate = [this] {
        m_dirty = true;
        Q_EMIT changed();
    };
    for (QAbstractItemModel *model : {static_cast<QAbstractItemModel *>(favorites), static_cast<QAbstractItemModel *>(places)}) {
        connect(model, &QAbstractItemModel::rowsInserted, this, invalidate);
        connect(model, &QAbstractItemModel::rowsRemoved, this, invalidate);
        connect(model, &QAbstractItemModel::rowsMoved, this, invalidate);
        connect(model, &QAbstractItemModel::modelReset, this, invalidate);
        connect(model, &QAbstractItemModel::layoutChanged, this, invalidate);
        connect(model, &QAbstractItemModel::dataChanged, this, invalidate);
    }
}

QList<WorkspaceLocations::Location> WorkspaceLocations::locations() const
{
    if (!m_dirty) {
        return m_locations;
    }
    m_locations.clear();
    m_ambiguous.clear();
    QMap<QString, int> indexes;
    const auto append = [this, &indexes](const QUrl &url, const QString &title) {
        const QString id = WorkspaceCatalog::locationId(url);
        if (id.isEmpty()) {
            return;
        }
        const auto existing = indexes.constFind(id);
        if (existing != indexes.cend()) {
            auto &location = m_locations[*existing];
            location.ambiguous |= location.url.adjusted(QUrl::StripTrailingSlash) != url.adjusted(QUrl::StripTrailingSlash);
            m_ambiguous[id] = location.ambiguous;
        } else {
            indexes.insert(id, m_locations.size());
            m_locations.append({id, title, url, false});
            m_ambiguous.insert(id, false);
        }
    };
    for (int row = 0; row < m_favorites->rowCount(); ++row) {
        const QModelIndex index = m_favorites->index(row, 0);
        append(index.data(FavoritesModel::UrlRole).toUrl(), index.data(Qt::DisplayRole).toString());
    }
    for (int row = 0; row < m_places->rowCount(); ++row) {
        const QModelIndex index = m_places->index(row, 0);
        if (isSavedPlace(index)) {
            append(m_places->url(index), index.data(Qt::DisplayRole).toString());
        }
    }
    m_dirty = false;
    return m_locations;
}

bool WorkspaceLocations::isSavedPlace(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != m_places || m_places->isDevice(index)) {
        return false;
    }
    const auto group = m_places->groupType(index);
    if (group != KFilePlacesModel::PlacesType && group != KFilePlacesModel::RemoteType) {
        return false;
    }
    const KBookmark bookmark = m_places->bookmarkForIndex(index);
    return !bookmark.isNull() && bookmark.metaDataItem(QStringLiteral("isSystemItem")) != QLatin1String("true")
        && !WorkspaceCatalog::locationId(m_places->url(index)).isEmpty();
}

bool WorkspaceLocations::isVisible(const QUrl &url, const WorkspaceCatalog::Catalog &catalog, const QString &workspaceId) const
{
    if (workspaceId.isEmpty()) {
        return true;
    }
    const QString id = WorkspaceCatalog::locationId(url);
    locations();
    if (m_ambiguous.value(id)) {
        return true;
    }
    const QString owner = catalog.locationAssignments.value(id);
    return owner.isEmpty() || owner == workspaceId;
}
