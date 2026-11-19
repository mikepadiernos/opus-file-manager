// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "namefilter.h"

#include <KFileItem>

#include <QUrl>
#include <QWidget>

class KDirLister;
class KDirModel;
class KDirSortFilterProxyModel;
class KFilePreviewGenerator;
class NameFilterProxyModel;
class QAbstractItemView;
class QListView;
class QStackedWidget;
class QTreeView;

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
    /// Text was typed in a view with no shortcut modifier; it should filter.
    void typeAheadRequested(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupView(QAbstractItemView *view);
    void onActivated(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    bool handleDrop(QAbstractItemView *view, QEvent *event);
    KFileItem itemAt(QAbstractItemView *view, const QPoint &viewportPos) const;

    KDirLister *m_lister = nullptr;
    KDirModel *m_model = nullptr;
    NameFilterProxyModel *m_proxy = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTreeView *m_details = nullptr;
    QListView *m_icons = nullptr;
    KFilePreviewGenerator *m_detailsPreviews = nullptr;
    KFilePreviewGenerator *m_iconPreviews = nullptr;
    ViewMode m_viewMode = ViewMode::Details;
    bool m_previewsEnabled = true;
};
