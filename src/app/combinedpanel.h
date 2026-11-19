// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>
#include <QWidget>

class FavoritesModel;
class KFilePlacesView;
class FolderTree;
class KFilePlacesModel;
class QDropEvent;
class QListView;
class QScrollArea;
class QToolButton;
class PlacesSection;

/**
 * One panel in place of the separate Places and Folders panels: a
 * Favorites section, the KDE Places sections, and a Tree section with
 * Home and Root, in a single scrolling column. Sections can be collapsed
 * independently from their headers.
 */
class CombinedPanel : public QWidget
{
    Q_OBJECT

public:
    void setSectionVisibility(bool favorites, bool places, bool folders);
    /**
     * @param places the window's places model, shared with the Places panel
     * @param favorites the user's favorite folders
     * @param parent the parent widget
     */
    CombinedPanel(KFilePlacesModel *places, FavoritesModel *favorites, QWidget *parent = nullptr);

    FolderTree *folderTree() const;

    /// Stable keys of collapsed sections, including KDE Places groups.
    QStringList collapsedSections() const;
    /// @param sections keys of the sections to collapse; the others expand
    void setCollapsedSections(const QStringList &sections);

Q_SIGNALS:
    /// A favorite, place or folder was clicked.
    void urlActivated(const QUrl &url);
    /// A favorite, place or folder was middle-clicked.
    void tabRequested(const QUrl &url);
    /// Files were dropped on a place or a tree folder.
    void urlsDropped(const QUrl &destination, QDropEvent *event);
    /// A tree folder was right-clicked.
    void folderContextMenuRequested(const QUrl &url, const QPoint &globalPos);

protected:
    /// Turns a middle-click on a favorite into tabRequested().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /**
     * Scrolls the revealed tree folder into view. While the tree is still
     * listing, rows above it move it, so this repeats as the scroll range
     * changes until the user scrolls the panel.
     */
    void scrollToRevealed();

    /// Offers Open in New Tab, Rename and Remove for the favorite at @p pos.
    void showFavoriteMenu(const QPoint &pos);

    FavoritesModel *m_favorites = nullptr;
    QListView *m_favoritesView = nullptr;
    QWidget *m_placesContainer = nullptr;
    QList<PlacesSection *> m_placesSections;
    FolderTree *m_folderTree = nullptr;
    QToolButton *m_favoritesHeader = nullptr;
    QToolButton *m_treeHeader = nullptr;
    QScrollArea *m_scroll = nullptr;
    bool m_favoritesVisible = true;
    bool m_foldersVisible = true;
    /// Whether the panel keeps the revealed folder in view.
    bool m_followRevealed = false;
};
