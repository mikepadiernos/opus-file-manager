// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "yazimode.h"

#include <QItemSelection>
#include <QList>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QUrl>
#include <QWidget>

#include <optional>

class FolderView;
class KDirLister;
class KDirModel;
class KDirSortFilterProxyModel;
class KFileItem;
class NameFilter;
class QAbstractItemView;
class QHBoxLayout;
class QLabel;
class QListView;
class QModelIndex;
class QSplitter;
class QTimer;
class QToolButton;
class QVBoxLayout;
class YaziKeys;
class YaziPreview;

/**
 * The columns a pane shows in Yazi mode: a read-only listing of the parent
 * folder, the pane's own FolderView as the current column, and an optional
 * preview column. The FolderView is borrowed, not owned: the pane lends it
 * with setCurrentView() when entering the mode and takes it back with
 * takeCurrentView() when leaving, so its model, selection, scroll position,
 * filter, sorting and view mode are untouched.
 *
 * The columns share the width as YaziMode::columnWidths() allocates it: a
 * narrow pane hides the preview column first and then the parent column,
 * and shows them again when it widens. Dragging a handle changes the
 * columns' ratios, which are kept while columns are hidden.
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
    /**
     * Shows a YaziPreview of the current column's focused item in the preview
     * column, or removes the preview widget. Enabling keeps a preview widget
     * that is already set.
     */
    void setPreviewEnabled(bool enabled);
    /// Whether the preview column has a widget, even if the width hides it.
    bool previewEnabled() const;
    /// The built-in preview of the focused item, or nullptr when the column has none or another widget.
    YaziPreview *preview() const;
    /// The relative widths of the columns; dragging a handle updates them.
    YaziMode::Ratios ratios() const;
    void setRatios(const YaziMode::Ratios &ratios);
    QSplitter *splitter() const;
    /// The label that tells the user the pane is in Yazi mode, with the key preset and any pending keys.
    QLabel *modeIndicator() const;
    /// The label after the mode indicator that counts the selected items while several are selected or marked.
    QLabel *selectionIndicator() const;
    /**
     * Shows @p filter's text in the status row while it isn't empty, with a
     * button that asks to clear it; see filterClearRequested().
     */
    void setFilter(const NameFilter &filter);
    /// The label that shows the pane's filter; hidden while there is none.
    QLabel *filterIndicator() const;
    QToolButton *filterClearButton() const;
    /// The label at the end of the status row that shows how the current column is sorted.
    QLabel *sortIndicator() const;
    /**
     * Shows a search's progress or outcome in the status row. While
     * @p running, a cancel button and Escape in the columns ask to cancel it;
     * see searchCancelRequested(). An outcome is shown for
     * SearchStatusTimeout milliseconds.
     * @param status the search panel's status text, such as its progress or a reason it can't search
     * @param running whether the search is still running
     */
    void setSearchStatus(const QString &status, bool running);
    void clearSearchStatus();
    bool isSearchRunning() const;
    QLabel *searchIndicator() const;
    QToolButton *searchCancelButton() const;
    /// How long a search's outcome stays in the status row, in milliseconds.
    static constexpr int SearchStatusTimeout = 10000;
    /// Adds @p widget to the row below the columns, after the mode, selection, filter and search indicators.
    void addStatusWidget(QWidget *widget, int stretch = 0);

    /**
     * The keys of the mode. They apply while the columns have a current view
     * and the focus is in the parent column, the current column's items or
     * the preview, but never in an inline editor or another typing widget.
     */
    YaziKeys *keys() const;
    /**
     * Moves the focused item of the parent column when it has the focus, and
     * otherwise of the current column, as the view's own keys would.
     * @param command one of the YaziMode::Command movement commands
     * @param count how many times to move; jumps to the first or last item ignore it
     */
    void moveFocus(YaziMode::Command command, int count = 1);
    enum class Column {
        Parent,
        Current,
        Preview,
    };
    /// Focuses @p column; returns false when the column is hidden or has nothing to focus.
    bool focusColumn(Column column);
    /// The column that has the keyboard focus, if the focus is in one.
    std::optional<Column> focusedColumn() const;
    /// The parent column's focused item when it has the focus, otherwise the current column's focused item.
    KFileItem focusedItem() const;
    /**
     * Opens the parent column's focused item as choosing it would. For the
     * folder the current column shows, focuses the current column instead.
     */
    void openParentItem();

    /**
     * Selects or deselects the current column's focused item and starts
     * marking: until the selection is cleared, moving the focus keeps the
     * selection instead of selecting the focused item. When nothing is
     * marked yet, the focused item becomes the only selected item.
     * @param moveDown whether to move the focus down after each toggle
     * @param count how many items to toggle when moving down
     */
    void toggleSelection(bool moveDown, int count = 1);
    /**
     * Starts range (visual) selection at the current column's focused item,
     * or ends it and keeps the selected items. While it is on, moving the
     * focus selects every item between where it started and the focused
     * item, in addition to the items marked before.
     */
    void toggleVisualSelection();
    /// Selects every item of the current column and starts marking.
    void selectAll();
    /// Whether moving the focus keeps the selection, after a selection command.
    bool isMarking() const;
    /// Whether range selection is on.
    bool isVisual() const;

Q_SIGNALS:
    /// A folder in the parent column was chosen; the pane should open it.
    void folderRequested(const QUrl &url);
    /// A file in the parent column was chosen; the pane should open @p folder and select @p item.
    void itemRequested(const QUrl &folder, const QUrl &item);
    /// The parent column or the preview column received focus or a click; the pane should become active.
    void activated();
    /// The filter indicator's clear button was clicked; the pane should clear its filter.
    void filterClearRequested();
    /// The search cancel button was clicked, or Escape pressed in the columns while a search runs.
    void searchCancelRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct SavedAccessibility {
        QPointer<QWidget> widget;
        QString name;
        QString description;
    };

    void resetLister();
    void highlightCurrent();
    void request(const QModelIndex &index);
    /// Shows, hides and sizes the columns for the splitter's width.
    void applyColumnWidths();
    void rememberRatios();
    void updateTabOrder();
    void labelCurrentColumn();
    void restoreCurrentColumnLabels();
    void updateIndicator();
    void updateSortIndicator();
    /// Follows a selection change that the selection commands didn't make.
    void selectionChanged();
    void updateSelectionIndicator();
    /// Ends marking and range selection, keeping the selection.
    void resetSelectionState();
    /// Selects the items marked before range selection and the range to the focused item.
    void applyVisualRange();
    /// The rows from @p from to @p to as the current column shows them, in either order.
    QItemSelection rowsBetween(const QModelIndex &from, const QModelIndex &to) const;
    /// Whether the current column's selection is empty or only its focused item.
    bool selectionIsFocusOnly() const;
    /// Whether @p widget takes the keys with the focus in it.
    bool acceptsKeys(QWidget *widget) const;
    /// Whether @p view has an inline editor open, as when renaming in place.
    static bool isEditing(const QAbstractItemView *view);
    /// The column item view the movement keys act on.
    QAbstractItemView *movementTarget() const;

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
    QLabel *m_selectionLabel = nullptr;
    QLabel *m_filterLabel = nullptr;
    QToolButton *m_filterClear = nullptr;
    QLabel *m_searchLabel = nullptr;
    QToolButton *m_searchCancel = nullptr;
    QTimer *m_searchStatusTimeout = nullptr;
    QLabel *m_sortLabel = nullptr;
    bool m_searchRunning = false;
    YaziKeys *m_keys = nullptr;
    QUrl m_url;
    QUrl m_parentUrl;
    QUrl m_pendingRequest;
    bool m_showHiddenFiles = false;
    bool m_marking = false;
    bool m_visual = false;
    /// Set while the selection commands change the selection themselves.
    bool m_changingSelection = false;
    QPersistentModelIndex m_visualAnchor;
    QItemSelection m_visualBase;
    YaziMode::Ratios m_ratios;
    QList<SavedAccessibility> m_savedAccessibility;
};
