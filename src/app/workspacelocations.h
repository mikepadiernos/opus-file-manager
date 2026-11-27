// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QObject>
#include <QUrl>

class FavoritesModel;
class KFilePlacesModel;
class QModelIndex;

/**
 * Presents assignable Favorites and user-created KDE Places as saved locations.
 */
class WorkspaceLocations : public QObject
{
    Q_OBJECT

public:
    struct Location {
        QString id;
        QString title;
        QUrl url;
        bool ambiguous = false;
    };

    /**
     * @param favorites the window's Favorites model
     * @param places the window's KDE Places model
     * @param parent the QObject parent
     */
    WorkspaceLocations(FavoritesModel *favorites, KFilePlacesModel *places, QObject *parent = nullptr);

    /// Returns saved locations, marking identities shared by different URLs as ambiguous.
    QList<Location> locations() const;
    /// Whether a KDE Places row is a user-created saved location.
    bool isSavedPlace(const QModelIndex &index) const;
    /// Whether a location is visible in the selected workspace.
    bool isVisible(const QUrl &url, const WorkspaceCatalog::Catalog &catalog, const QString &workspaceId) const;
    /**
     * The local folder of the saved location @p locationId, for use as an
     * editor vault, or empty with @p unavailable set to why it can't be used:
     * the location was removed, several saved locations share its identity,
     * or it isn't a local folder.
     */
    QString vaultFolder(const QString &locationId, QString *unavailable = nullptr) const;

Q_SIGNALS:
    void changed();

private:
    FavoritesModel *m_favorites;
    KFilePlacesModel *m_places;
    mutable QList<Location> m_locations;
    mutable QMap<QString, bool> m_ambiguous;
    mutable bool m_dirty = true;
};
