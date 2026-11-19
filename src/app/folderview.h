// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldermetadata.h"
#include "listingexport.h"
#include "namefilter.h"

#include <KFileItem>

#include <QSet>
#include <QUrl>
#include <QWidget>

class KDirLister;
class KDirModel;
class KDirSortFilterProxyModel;
class KFilePreviewGenerator;
class KFileItemDelegate;
class DetailsView;
class NameFilterProxyModel;
class QHeaderView;
class QAbstractItemView;
class QListView;
class QStackedWidget;
class QTimer;
namespace KIO
{
class DirectorySizeJob;
}

/**
 * One folder listing: details, icon and compact views sharing a
 * KDirModel. Drops are delegated to KIO::DropJob; drags come from the
 * model. The view never navigates on its own; it asks via signals.
 */
class FolderView : public QWidget
{
    Q_OBJECT

public:
    enum class ViewMode {
        Details,
        Icons,
        Compact,
    };
    Q_ENUM(ViewMode)

    explicit FolderView(QWidget *parent = nullptr);
    ~FolderView() override;

    QUrl url() const;
    bool isLoading() const;
    void setUrl(const QUrl &url);
    void reload();

    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);
    bool selectionMarkersVisible() const;
    void setSelectionMarkersVisible(bool visible);
    bool selectionMode() const;
    void setSelectionMode(bool enabled);

    bool showHiddenFiles() const;
    void setShowHiddenFiles(bool show);

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool enabled);
    void setRemotePreviewsEnabled(bool enabled);
    bool previewsShown() const;
    void setPreviewPlugins(const QStringList &plugins);
    QStringList previewPlugins() const;
    void setIconSizes(int details, int icons);
    void setItemSpacing(int detailsSpace, int iconsSpace);
    void setDragFromAnywhere(bool enabled);
    void setColumnVisible(int column, bool visible);
    bool isColumnVisible(int column) const;
    void setRatingColumn(bool enabled);
    void setSorting(int column, Qt::SortOrder order);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;
    void setSortFoldersFirst(bool foldersFirst);
    bool sortFoldersFirst() const;
    void setShowToolTips(bool show);
    bool showToolTips() const;

    /// The folder being listed, or a null item while loading.
    KFileItem rootItem() const;
    KFileItemList items() const;
    KFileItemList itemsInViewOrder() const;
    /// Captures the filtered listing using the configured details columns.
    ListingExport::Table listingForExport() const;
    KFileItemList selectedItems() const;
    /// Selected items, or the listed folder itself when nothing is selected.
    KFileItemList selectedItemsOrRoot() const;
    KFileItem currentItem() const;
    int itemCount() const;

    void selectAll();
    int applySelectionPattern(const QString &pattern, bool select);
    void clearSelection();
    void selectUrls(const QList<QUrl> &urls);
    bool expandForHistorySelection(const QList<QUrl> &urls);
    void cancelHistoryExpansion();

    QAbstractItemView *currentView() const;

    NameFilter nameFilter() const;
    /**
     * Hides items the filter does not match. The filter stays set when the
     * view lists another folder.
     * @param filter the filter to apply; an empty filter shows everything
     */
    void setNameFilter(const NameFilter &filter);

    /**
     * Activates, as a double-click would, the first item the name filter
     * matched, or the first visible item when no item in the filter's
     * scope is shown.
     */
    void activateFirstItem();

    bool isFlat() const;
    /**
     * Lists every file below the folder in one list, without folders, or
     * goes back to the folder's own items. url() stays the folder either way.
     * In a flat list the Type column shows each item's relative path, and
     * nothing can be dropped.
     * @param flat true for the flat list
     */
    void setFlat(bool flat);
    /// Reloads the current folder's labels, colours, ratings, notes and pins.
    void reloadMetadata();
    bool hasFolderFormat() const;
    QString formatRuleDescription() const;
    QString formatRuleSource() const;
    bool hasLocalFolderFormat() const;
    void clearLocalFolderFormat();
    bool formatLocked() const;
    void setFormatLocked(bool locked);
    bool formatIncludesDescendants() const;
    void setFormatIncludesDescendants(bool include);
    bool automaticFolderSizes() const;
    void setAutomaticFolderSizes(bool enabled);
    void clearFolderSizePolicy();
    void calculateFolderSizes();
    void refreshFolderSizePolicy();
    void reloadListingSettings();
    QString groupBy() const;
    void setGroupBy(const QString &group);
    void reloadSettings();

Q_SIGNALS:
    void urlChanged(const QUrl &url);
    void redirected(const QUrl &url);
    void loadingChanged(bool loading);
    void listingReady();
    void historyExpansionFinished();
    void listingFailed(const QString &message);
    void itemActivated(const KFileItem &item);
    void selectionChanged();
    void itemCountChanged();
    void itemsOrderChanged();
    void contextMenuRequested(const QPoint &globalPos, const KFileItemList &items);
    void goUpRequested();
    /// A view received focus or a click; the pane should become active.
    void activated();
    /// Text was typed in a view with no shortcut modifier; it should filter.
    void typeAheadRequested(const QString &text);
    void selectionModeChanged(bool enabled);
    void copySelectionRequested();
    void cutSelectionRequested();
    void trashSelectionRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupView(QAbstractItemView *view);
    /// Lists m_folder, or everything below it in flat mode.
    void openListing();
    void onActivated(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void showColumnMenu(const QPoint &pos);
    void loadFolderFormat();
    void saveFolderFormat();
    FolderMetadata::Format currentFormat() const;
    void updatePreviews();
    bool handleDrop(QAbstractItemView *view, QEvent *event);
    void finishHistoryExpansionWhenIdle();
    void queueFolderSizes(const KFileItemList &items);
    void startFolderSizeJobs();
    void cancelFolderSizeJobs();
    void scheduleFolderSizeRefresh();
    void updateFolderSizeRefreshTimer();
    void applyDetailsPreferences();
    void applyCompactPreferences();
    KFileItem itemAt(QAbstractItemView *view, const QPoint &viewportPos) const;

    KDirLister *m_lister = nullptr;
    KDirModel *m_model = nullptr;
    NameFilterProxyModel *m_proxy = nullptr;
    /// The folder shown, which in flat mode is not what the lister lists.
    QUrl m_folder;
    bool m_flat = false;
    bool m_loading = false;
    bool m_historyExpanding = false;
    quint64 m_historyExpansionGeneration = 0;
    QSet<QUrl> m_historyListingJobs;
    bool m_applyingFolderFormat = false;
    bool m_formatLocked = false;
    bool m_lockedAutomaticFolderSizes = false;
    QString m_formatRuleSource;
    bool m_ruleCalculatesFolderSizes = false;
    bool m_ruleHasFolderSizePolicy = false;
    KFileItemList m_sizeQueue;
    QSet<QUrl> m_sizePending;
    QSet<KIO::DirectorySizeJob *> m_sizeJobs;
    quint64 m_sizeJobGeneration = 0;
    bool m_sizeRefreshScheduled = false;
    QTimer *m_sizeRefreshTimer = nullptr;
    QStackedWidget *m_stack = nullptr;
    DetailsView *m_details = nullptr;
    QListView *m_icons = nullptr;
    QListView *m_compact = nullptr;
    QWidget *m_selectionBar = nullptr;
    bool m_selectionMarkers = false;
    bool m_selectionMode = false;
    bool m_defaultSelectionMarkers = false;
    bool m_defaultSelectionMode = false;
    bool m_markerPress = false;
    KFilePreviewGenerator *m_detailsPreviews = nullptr;
    KFilePreviewGenerator *m_iconPreviews = nullptr;
    KFilePreviewGenerator *m_compactPreviews = nullptr;
    KFileItemDelegate *m_iconDelegate = nullptr;
    ViewMode m_viewMode = ViewMode::Details;
    bool m_previewsEnabled = true;
    bool m_remotePreviewsEnabled = false;
};
