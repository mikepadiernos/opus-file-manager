// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>
#include <QWidget>

class FavoritesModel;
class FolderTree;
class KFilePlacesModel;
class QDropEvent;
class QListView;

/**
 * One panel in place of the separate Places and Folders panels: a
 * Favorites section, the KDE Places sections, and a Tree section with
 * Home and Root, in a single scrolling column. Favorites and Tree can be
 * collapsed from their headers.
 */
class CombinedPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param places the window's places model, shared with the Places panel
     * @param favorites the user's favorite folders
     * @param parent the parent widget
     */
    CombinedPanel(KFilePlacesModel *places, FavoritesModel *favorites, QWidget *parent = nullptr);

    FolderTree *folderTree() const;

Q_SIGNALS:
    /// A favorite, place or folder was clicked.
    void urlActivated(const QUrl &url);
    /// A favorite, place or folder was middle-clicked.
    void tabRequested(const QUrl &url);
    /// Files were dropped on a place.
    void urlsDropped(const QUrl &destination, QDropEvent *event);

protected:
    /// Turns a middle-click on a favorite into tabRequested().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Offers Open in New Tab, Rename and Remove for the favorite at @p pos.
    void showFavoriteMenu(const QPoint &pos);

    FavoritesModel *m_favorites = nullptr;
    QListView *m_favoritesView = nullptr;
    FolderTree *m_folderTree = nullptr;
};
