// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <QUrl>
#include <QWidget>

class FolderView;
class KDirLister;
class KDirModel;
class KDirSortFilterProxyModel;
class QHBoxLayout;
class QLabel;
class QListView;
class QModelIndex;
class QSplitter;
class QVBoxLayout;

/**
 * The columns a pane shows in Yazi mode: a read-only listing of the parent
 * folder, the pane's own FolderView as the current column, and an optional
 * preview column. The FolderView is borrowed, not owned: the pane lends it
 * with setCurrentView() when entering the mode and takes it back with
 * takeCurrentView() when leaving, so its model, selection, scroll position,
 * filter, sorting and view mode are untouched.
 */
class YaziColumns : public QWidget
{
    Q_OBJECT

public:
    explicit YaziColumns(QWidget *parent = nullptr);
    ~YaziColumns() override;

    /// Places @p view in the current column, between the parent and preview columns.
    void setCurrentView(FolderView *view);
    /// Removes the current column's view, reparenting it to this widget's parent, and returns it.
    FolderView *takeCurrentView();
    FolderView *currentView() const;

    /**
     * Lists the parent of @p url in the parent column and highlights @p url
     * there once it is listed. An empty URL clears the column.
     */
    void setUrl(const QUrl &url);
    QUrl url() const;
    /// The folder the parent column lists; empty at a root.
    QUrl parentUrl() const;
    void setShowHiddenFiles(bool show);

    QListView *parentColumn() const;
    /// The column after the current one, hidden until it has a preview widget.
    QWidget *previewSlot() const;
    /**
     * Shows @p widget in the preview column, taking ownership and deleting the
     * previous preview widget. nullptr empties and hides the column.
     */
    void setPreviewWidget(QWidget *widget);
    QWidget *previewWidget() const;
    QSplitter *splitter() const;
    /// The label that tells the user the pane is in Yazi mode.
    QLabel *modeIndicator() const;
    /// Adds @p widget to the row below the columns, after the mode indicator.
    void addStatusWidget(QWidget *widget, int stretch = 0);

Q_SIGNALS:
    /// A folder in the parent column was chosen; the pane should open it.
    void folderRequested(const QUrl &url);
    /// A file in the parent column was chosen; the pane should open @p folder and select @p item.
    void itemRequested(const QUrl &folder, const QUrl &item);
    /// The parent column received focus or a click; the pane should become active.
    void activated();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void resetLister();
    void highlightCurrent();
    void request(const QModelIndex &index);

    QSplitter *m_splitter = nullptr;
    QListView *m_parentColumn = nullptr;
    KDirLister *m_lister = nullptr;
    KDirModel *m_model = nullptr;
    KDirSortFilterProxyModel *m_proxy = nullptr;
    FolderView *m_view = nullptr;
    QWidget *m_previewSlot = nullptr;
    QVBoxLayout *m_previewLayout = nullptr;
    QPointer<QWidget> m_previewWidget;
    QWidget *m_statusRow = nullptr;
    QHBoxLayout *m_statusLayout = nullptr;
    QLabel *m_indicator = nullptr;
    QUrl m_url;
    QUrl m_parentUrl;
    QUrl m_pendingRequest;
    bool m_showHiddenFiles = false;
};
