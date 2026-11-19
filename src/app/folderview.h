// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QUrl>
#include <QWidget>

class KDirLister;
class KDirModel;
class KDirSortFilterProxyModel;
class KFileItemDelegate;
class KFilePreviewGenerator;
class QAbstractItemView;
class QListView;
class QStackedWidget;
class QTreeView;
class DetailsView;

/**
 * One folder listing: a details view and an icon view sharing a
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
    };
    Q_ENUM(ViewMode)

    explicit FolderView(QWidget *parent = nullptr);
    ~FolderView() override;

    QUrl url() const;
    void setUrl(const QUrl &url);
    void reload();

    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);

    bool showHiddenFiles() const;
    void setShowHiddenFiles(bool show);

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool enabled);

    /**
     * Allows previews in folders that are not local files. Previews show
     * only when previewsEnabled() and the folder is local or this is set.
     * @param enabled true to allow previews in remote folders
     */
    void setRemotePreviewsEnabled(bool enabled);
    /// Whether previews are being generated for the current folder.
    bool previewsShown() const;

    /**
     * Sets the KIO thumbnailer plugins used for previews.
     * @param plugins plugin ids as listed by KIO::PreviewJob
     */
    void setPreviewPlugins(const QStringList &plugins);
    QStringList previewPlugins() const;

    /**
     * Sets the icon size of each view mode; previews use the same size.
     * @param details icon size of the details view in pixels
     * @param icons icon size of the icons view in pixels
     */
    void setIconSizes(int details, int icons);

    /**
     * Shows or hides a details view column. The Name column always shows.
     * @param column a KDirModel::ModelColumns value
     * @param visible true to show the column
     */
    void setColumnVisible(int column, bool visible);

    /**
     * Whether a drag can start from any part of a details row. When off,
     * pressing outside an item's icon and name starts a selection rectangle.
     * @param enabled true to drag from anywhere in a row
     */
    void setDragFromAnywhere(bool enabled);
    bool isColumnVisible(int column) const;

    /**
     * Sorts the listing.
     * @param column a KDirModel::ModelColumns value
     * @param order ascending or descending
     */
    void setSorting(int column, Qt::SortOrder order);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    void setSortFoldersFirst(bool foldersFirst);
    bool sortFoldersFirst() const;

    /**
     * Shows the full name of shortened items in a tooltip.
     * @param show true to show the tooltips
     */
    void setShowToolTips(bool show);
    bool showToolTips() const;

    /// The folder being listed, or a null item while loading.
    KFileItem rootItem() const;
    KFileItemList items() const;
    KFileItemList selectedItems() const;
    /// Selected items, or the listed folder itself when nothing is selected.
    KFileItemList selectedItemsOrRoot() const;
    KFileItem currentItem() const;
    int itemCount() const;

    void selectAll();
    void clearSelection();
    void selectUrls(const QList<QUrl> &urls);

    QAbstractItemView *currentView() const;

Q_SIGNALS:
    void urlChanged(const QUrl &url);
    void redirected(const QUrl &url);
    void loadingChanged(bool loading);
    void listingFailed(const QString &message);
    void itemActivated(const KFileItem &item);
    void selectionChanged();
    void itemCountChanged();
    void contextMenuRequested(const QPoint &globalPos, const KFileItemList &items);
    void goUpRequested();
    /// A view received focus or a click; the pane should become active.
    void activated();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupView(QAbstractItemView *view, KFileItemDelegate *delegate);
    void onActivated(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    bool handleDrop(QAbstractItemView *view, QEvent *event);
    KFileItem itemAt(QAbstractItemView *view, const QPoint &viewportPos) const;
    void updatePreviews();

    KDirLister *m_lister = nullptr;
    KDirModel *m_model = nullptr;
    KDirSortFilterProxyModel *m_proxy = nullptr;
    QStackedWidget *m_stack = nullptr;
    DetailsView *m_details = nullptr;
    QListView *m_icons = nullptr;
    KFileItemDelegate *m_detailsDelegate = nullptr;
    KFileItemDelegate *m_iconsDelegate = nullptr;
    KFilePreviewGenerator *m_detailsPreviews = nullptr;
    KFilePreviewGenerator *m_iconPreviews = nullptr;
    ViewMode m_viewMode = ViewMode::Details;
    bool m_previewsEnabled = true;
    bool m_remotePreviewsEnabled = false;
};
