// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>
#include <KXmlGuiWindow>

#include <QHash>
#include <QList>
#include <QUrl>

class CombinedPanel;
class FavoritesModel;
class FolderTree;
class KFileCopyToMenu;
class KFileItemActions;
class KFilePlacesModel;
class KFilePlacesView;
class KNewFileMenu;
class KSelectAction;
class KToggleAction;
class Pane;
class PreviewPanel;
class TabColors;
class QDockWidget;
class QLabel;
class QMenu;
class QSplitter;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Opens each URL in a new tab of the active pane; restores the
    /// session (or the home folder) when the list is empty.
    void openUrls(const QList<QUrl> &urls);

    Pane *activePane() const;
    Pane *inactivePane() const;

    /**
     * Splits the panes top/bottom instead of side by side. When stacked,
     * the top pane's tabs run along its bottom edge and the bottom pane's
     * tabs along its top edge, so the two tab bars meet in the middle.
     * @param stacked true for top/bottom, false for side by side
     */
    void setStackedLayout(bool stacked);
    bool isStackedLayout() const;

    /// Which side panels the window shows.
    enum class PanelLayout {
        /// One panel with Favorites, the Places sections and a folder tree.
        Combined,
        /// Separate Places and Folders panels.
        Separate,
    };

    /**
     * Shows either the combined panel or the separate Places and Folders
     * panels, and remembers the choice for new windows.
     * @param layout the panels to show
     */
    void setPanelLayout(PanelLayout layout);
    PanelLayout panelLayout() const;

    FavoritesModel *favorites() const;
    TabColors *tabColors() const;

    bool isDualLayout() const;

    /**
     * Applies the current Settings to the panes, actions and status bar.
     * Called on start and whenever the settings dialog saves.
     */
    void applySettings();

protected:
    bool queryClose() override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void readProperties(const KConfigGroup &group) override;
    void saveProperties(KConfigGroup &group) override;

private:
    void setupActions();
    void setupPanes();
    void setupPlaces();
    void setupPanels();
    void setupStatusBar();
    void applyPanelSettings();

    void setActivePane(Pane *pane);
    void connectPane(Pane *pane);
    void updateActions();
    void updateStatusBar();
    void updateFreeSpace();
    void updateWindowTitle();
    void updateTabBars();
    void showStatusMessage(const QString &message);
    void recordFolderVisit(Pane *pane, const QUrl &url);
    void savePopularFolderVisits();
    void populatePopularFolders();
    void populateSelectionFilters();
    void applySelectionFilter(const QString &pattern);
    void selectByPattern(bool select);
    void saveSelectionFilter();
    void calculateOccupiedSpace();
    void jumpBack();
    void testArchiveIntegrity();
    void testArchiveFile(const QString &fileName, const QString &password = QString());

    void restoreSession();
    void saveSession();

    void showContextMenu(Pane *pane, const QPoint &globalPos, const KFileItemList &items);
    void addTransferMenu(QMenu *menu, const KFileItemList &items, bool move);
    void activateItem(const KFileItem &item);
    void activatePaneItem(Pane *pane, const KFileItem &item);
    void updatePreviewPanel();
    void openGallery();

    void copyToOtherPane();
    void moveToOtherPane();
    void cut();
    void copy();
    void paste();
    void rename();
    void trash();
    void deletePermanently();
    void showProperties();
    void openTerminal();
    void newFolder();
    void showNewMenu();
    void toggleLayout(bool dual);
    void showSettings();
    /// Applies the settings that only take effect in a new window.
    void applyViewDefaults();
    /// Stores whether the user left the Places panel shown.
    void rememberPlacesShown(bool shown);
    void toggleViewMode();
    void setShowHiddenFiles(bool show);
    void setPreviewsEnabled(bool enabled);
    /// Reveals the active pane's folder in the trees and matches its hidden files setting.
    void syncFolderTrees();
    /// Shows the active pane's folder flat or not, and remembers the choice for that folder.
    void setFlat(bool flat);
    /// Shows the context menu for a folder in a tree.
    void showFolderTreeMenu(const QUrl &url, const QPoint &globalPos);
    /// Adds the selected folders, or the current folder, to Favorites.
    void addToFavorites();
    /**
     * Extracts each selected archive into a new folder named after it in
     * the other pane's folder, never overwriting an existing one.
     */
    void extractToOtherPane();
    void extractProtectedToOtherPane();
    /**
     * Asks how to compress the selected local items, then writes the archive
     * in the background, never replacing an existing file.
     */
    void compress();
    void setSelectionLabel(const QString &label);
    void setSelectionColor(const QString &color);
    void setSelectionRating(int rating);
    void setSelectionNote();
    void toggleSelectionPin();
    void editFolderLook(const QUrl &folder);
    /**
     * Copies the top-level entries of an archive into a new folder.
     * @param archive the archive's browse URL from Archives::browseUrl()
     * @param target the folder to create; it must not exist
     */
    void extractArchive(const QUrl &archive, const QUrl &target);

    KFilePlacesModel *m_placesModel = nullptr;
    KFilePlacesView *m_placesView = nullptr;
    FavoritesModel *m_favorites = nullptr;
    TabColors *m_tabColors = nullptr;
    QDockWidget *m_placesDock = nullptr;
    QDockWidget *m_foldersDock = nullptr;
    QDockWidget *m_combinedDock = nullptr;
    FolderTree *m_folderTree = nullptr;
    CombinedPanel *m_combinedPanel = nullptr;
    KSelectAction *m_panelLayoutAction = nullptr;
    KToggleAction *m_flatAction = nullptr;
    /// Folders shown flat, as URL strings; kept in the state config.
    QStringList m_flatLocations;
    PanelLayout m_panelLayout = PanelLayout::Combined;
    KFileItemActions *m_fileItemActions = nullptr;
    KFileCopyToMenu *m_copyToMenu = nullptr;
    KNewFileMenu *m_newFileMenu = nullptr;
    QSplitter *m_splitter = nullptr;
    Pane *m_left = nullptr;
    Pane *m_right = nullptr;
    Pane *m_active = nullptr;
    PreviewPanel *m_previewPanel = nullptr;
    QLabel *m_itemsLabel = nullptr;
    QLabel *m_messageLabel = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;
    KToggleAction *m_dualLayoutAction = nullptr;
    KToggleAction *m_stackedLayoutAction = nullptr;
    KToggleAction *m_hiddenFilesAction = nullptr;
    KToggleAction *m_previewsAction = nullptr;
    KToggleAction *m_previewPanelAction = nullptr;
    QAction *m_viewModeAction = nullptr;
    QAction *m_selectionModeAction = nullptr;
    QAction *m_selectionMarkersAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_jumpBackAction = nullptr;
    QMenu *m_selectionFiltersMenu = nullptr;
    QMenu *m_popularFoldersMenu = nullptr;
    QHash<QString, int> m_popularFolderVisits;
    QHash<Pane *, QUrl> m_lastFolderByPane;
    QHash<Pane *, QUrl> m_jumpBackPoints;
    bool m_jumpingBack = false;
    bool m_autoFocusFolders = true;
    bool m_sessionRestored = false;
};
