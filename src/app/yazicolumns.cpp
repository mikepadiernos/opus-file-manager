// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazicolumns.h"

#include "folderview.h"
#include "namefilter.h"
#include "symbolicicon.h"
#include "transferqueue.h"
#include "yazikeys.h"
#include "yazimode.h"
#include "yazipreview.h"

#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItem>
#include <KLocalizedString>

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QScopedValueRollback>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int ParentColumn = 0;
constexpr int CurrentColumn = 1;

QUrl stripped(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash);
}

/// About half the rows @p view shows at once.
int halfPageRows(const QAbstractItemView *view)
{
    QModelIndex probe = view->currentIndex();
    if (!probe.isValid() && view->model()) {
        probe = view->model()->index(0, 0, view->rootIndex());
    }
    int rowHeight = probe.isValid() ? view->visualRect(probe).height() : 0;
    if (rowHeight <= 0) {
        rowHeight = view->fontMetrics().height();
    }
    return qMax(1, view->viewport()->height() / qMax(1, rowHeight) / 2);
}
}

YaziColumns::YaziColumns(QWidget *parent)
    : QWidget(parent)
    , m_model(new KDirModel(this))
    , m_proxy(new KDirSortFilterProxyModel(this))
{
    setObjectName(QStringLiteral("yaziColumns"));
    resetLister();
    m_proxy->setSortFoldersFirst(true);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->sort(KDirModel::Name, Qt::AscendingOrder);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &YaziColumns::highlightCurrent);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, &YaziColumns::highlightCurrent);

    m_parentColumn = new QListView(this);
    m_parentColumn->setObjectName(QStringLiteral("yaziParentColumn"));
    m_parentColumn->setAccessibleName(i18nc("@info:accessibility", "Parent folder"));
    m_parentColumn->setAccessibleDescription(i18nc("@info:accessibility", "The items next to the current folder; choose one to open it"));
    m_parentColumn->setMinimumWidth(YaziMode::MinimumParentWidth);
    m_parentColumn->setModel(m_proxy);
    m_parentColumn->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parentColumn->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_parentColumn->setSelectionMode(QAbstractItemView::SingleSelection);
    m_parentColumn->setUniformItemSizes(true);
    m_parentColumn->setIconSize(QSize(16, 16));
    m_parentColumn->setTextElideMode(Qt::ElideMiddle);
    m_parentColumn->installEventFilter(this);
    m_parentColumn->viewport()->installEventFilter(this);
    connect(m_parentColumn, &QAbstractItemView::clicked, this, &YaziColumns::request);
    connect(m_parentColumn, &QAbstractItemView::activated, this, &YaziColumns::request);

    m_previewSlot = new QWidget(this);
    m_previewSlot->setObjectName(QStringLiteral("yaziPreviewSlot"));
    m_previewSlot->setAccessibleName(i18nc("@info:accessibility", "Preview"));
    m_previewSlot->setAccessibleDescription(i18nc("@info:accessibility", "A preview of the focused item in the current folder"));
    m_previewSlot->setMinimumWidth(YaziMode::MinimumPreviewWidth);
    m_previewLayout = new QVBoxLayout(m_previewSlot);
    m_previewLayout->setContentsMargins(0, 0, 0, 0);
    m_previewSlot->hide();

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("yaziColumnsSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_parentColumn);
    m_splitter->addWidget(m_previewSlot);
    // Columns are hidden rather than squeezed, so the columns never make the
    // pane wider than its folder view alone would.
    m_splitter->setMinimumWidth(1);
    m_splitter->installEventFilter(this);
    connect(m_splitter, &QSplitter::splitterMoved, this, &YaziColumns::rememberRatios);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (now && m_previewSlot->isAncestorOf(now)) {
            Q_EMIT activated();
        }
    });

    m_indicator = new QLabel(i18nc("@info:status short name of the Yazi browsing mode", "Yazi"), this);
    m_indicator->setObjectName(QStringLiteral("yaziModeIndicator"));
    m_indicator->setAccessibleName(i18nc("@info:accessibility", "Yazi mode"));
    m_indicator->setAccessibleDescription(i18nc("@info:accessibility", "This pane shows its folder in columns with the parent folder"));
    m_indicator->setToolTip(i18nc("@info:tooltip", "Yazi mode is on in this pane"));
    m_indicator->setBackgroundRole(QPalette::Highlight);
    m_indicator->setForegroundRole(QPalette::HighlightedText);
    m_indicator->setAutoFillBackground(true);
    m_indicator->setContentsMargins(6, 0, 6, 0);

    m_selectionLabel = new QLabel(this);
    m_selectionLabel->setObjectName(QStringLiteral("yaziSelectionIndicator"));
    m_selectionLabel->setToolTip(i18nc("@info:tooltip", "File actions in Yazi mode act on the selected items"));
    m_selectionLabel->hide();

    m_filterLabel = new QLabel(this);
    m_filterLabel->setObjectName(QStringLiteral("yaziFilterIndicator"));
    m_filterLabel->setTextFormat(Qt::PlainText);
    m_filterLabel->setToolTip(i18nc("@info:tooltip", "The current column only shows the items that match this filter"));
    m_filterLabel->hide();
    m_filterClear = new QToolButton(this);
    m_filterClear->setObjectName(QStringLiteral("yaziFilterClear"));
    m_filterClear->setIcon(SymbolicIcon::fromTheme(QStringLiteral("edit-clear")));
    m_filterClear->setToolTip(i18nc("@info:tooltip", "Clear the filter"));
    m_filterClear->setAccessibleName(i18nc("@action:button", "Clear Filter"));
    m_filterClear->setAutoRaise(true);
    m_filterClear->setFocusPolicy(Qt::NoFocus);
    m_filterClear->hide();
    connect(m_filterClear, &QToolButton::clicked, this, &YaziColumns::filterClearRequested);

    m_searchLabel = new QLabel(this);
    m_searchLabel->setObjectName(QStringLiteral("yaziSearchIndicator"));
    m_searchLabel->setTextFormat(Qt::PlainText);
    // A long outcome is clipped rather than widening the pane; the tooltip shows all of it.
    m_searchLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_searchLabel->setMinimumWidth(fontMetrics().averageCharWidth() * 12);

    m_searchLabel->hide();
    m_searchCancel = new QToolButton(this);
    m_searchCancel->setObjectName(QStringLiteral("yaziSearchCancel"));
    m_searchCancel->setIcon(SymbolicIcon::fromTheme(QStringLiteral("process-stop")));
    m_searchCancel->setToolTip(i18nc("@info:tooltip", "Cancel the search (Escape)"));
    m_searchCancel->setAccessibleName(i18nc("@action:button", "Cancel Search"));
    m_searchCancel->setAutoRaise(true);
    m_searchCancel->setFocusPolicy(Qt::NoFocus);
    m_searchCancel->hide();
    connect(m_searchCancel, &QToolButton::clicked, this, &YaziColumns::searchCancelRequested);
    m_searchStatusTimeout = new QTimer(this);
    m_searchStatusTimeout->setSingleShot(true);
    m_searchStatusTimeout->setInterval(SearchStatusTimeout);
    connect(m_searchStatusTimeout, &QTimer::timeout, this, &YaziColumns::clearSearchStatus);

    m_sortLabel = new QLabel(this);
    m_sortLabel->setObjectName(QStringLiteral("yaziSortIndicator"));
    m_sortLabel->hide();

    m_transferIndicator = new QToolButton(this);
    m_transferIndicator->setObjectName(QStringLiteral("yaziTransferIndicator"));
    m_transferIndicator->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-list-details")));
    m_transferIndicator->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_transferIndicator->setAutoRaise(true);
    m_transferIndicator->setFocusPolicy(Qt::NoFocus);
    m_transferIndicator->hide();
    connect(m_transferIndicator, &QToolButton::clicked, this, &YaziColumns::transferQueueRequested);
    m_transferFailureTimeout = new QTimer(this);
    m_transferFailureTimeout->setSingleShot(true);
    m_transferFailureTimeout->setInterval(TransferFailureTimeout);
    connect(m_transferFailureTimeout, &QTimer::timeout, this, &YaziColumns::updateTransferIndicator);

    m_statusRow = new QWidget(this);
    m_statusRow->setObjectName(QStringLiteral("yaziStatusRow"));
    m_statusLayout = new QHBoxLayout(m_statusRow);
    m_statusLayout->setContentsMargins(2, 2, 2, 2);
    m_statusLayout->setSpacing(6);
    m_statusLayout->addWidget(m_indicator);
    m_statusLayout->addWidget(m_selectionLabel);
    m_statusLayout->addWidget(m_filterLabel);
    m_statusLayout->addWidget(m_filterClear);
    m_statusLayout->addWidget(m_searchLabel, 1);
    m_statusLayout->addWidget(m_searchCancel);
    m_statusLayout->addStretch(1);
    m_statusLayout->addWidget(m_transferIndicator);
    m_statusLayout->addWidget(m_sortLabel);

    m_keys = new YaziKeys(this);
    m_keys->setFocusFilter([this](QWidget *widget) {
        return acceptsKeys(widget);
    });
    m_keys->setEscapeOverride([this] {
        return m_searchRunning;
    });
    m_keys->watch(m_parentColumn);
    m_keys->watch(m_previewSlot);
    connect(m_keys, &YaziKeys::stateChanged, this, &YaziColumns::updateIndicator);
    connect(m_keys, &YaziKeys::escapePressed, this, &YaziColumns::searchCancelRequested);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter, 1);
    layout->addWidget(m_statusRow);
}

YaziColumns::~YaziColumns()
{
    m_lister->disconnect(this);
    // Deleting the columns can move the focus; the handlers use them.
    disconnect(qApp, nullptr, this, nullptr);
    m_keys->disconnect(this);
}

void YaziColumns::resetLister()
{
    if (m_lister) {
        m_lister->disconnect(this);
    }
    m_lister = new KDirLister(this);
    m_lister->setAutoErrorHandlingEnabled(false);
    m_lister->setShowHiddenFiles(m_showHiddenFiles);
    // The model deletes the lister it replaces and drops its rows without
    // announcing a reset, so detach the proxy until the swap is done.
    m_proxy->setSourceModel(nullptr);
    m_model->setDirLister(m_lister);
    m_proxy->setSourceModel(m_model);
    connect(m_lister, &KDirLister::completed, this, &YaziColumns::highlightCurrent);
}

void YaziColumns::setCurrentView(FolderView *view)
{
    if (view == m_view) {
        return;
    }
    takeCurrentView();
    m_view = view;
    if (!m_view) {
        return;
    }
    m_splitter->insertWidget(CurrentColumn, m_view);
    m_splitter->setCollapsible(CurrentColumn, false);
    m_splitter->setStretchFactor(ParentColumn, 1);
    m_splitter->setStretchFactor(CurrentColumn, 3);
    m_splitter->setStretchFactor(CurrentColumn + 1, 2);
    m_view->show();
    m_view->installEventFilter(this);
    m_parentColumn->setFont(m_view->font());
    m_keys->watch(m_view->currentView());
    connect(m_view, &FolderView::viewModeChanged, this, [this] {
        restoreCurrentColumnLabels();
        labelCurrentColumn();
        updateTabOrder();
        m_keys->watch(m_view->currentView());
    });
    connect(m_view, &FolderView::selectionChanged, this, &YaziColumns::selectionChanged);
    connect(m_view, &FolderView::sortingChanged, this, &YaziColumns::updateSortIndicator);
    m_keys->setEnabled(true);
    labelCurrentColumn();
    if (YaziPreview *builtIn = preview()) {
        builtIn->setView(m_view);
    }
    updateTabOrder();
    applyColumnWidths();
    resetSelectionState();
    updateSortIndicator();
}

FolderView *YaziColumns::takeCurrentView()
{
    FolderView *view = m_view;
    if (!view) {
        return nullptr;
    }
    if (YaziPreview *builtIn = preview()) {
        builtIn->setView(nullptr);
    }
    view->removeEventFilter(this);
    view->disconnect(this);
    m_keys->setEnabled(false);
    const QList<QAbstractItemView *> itemViews = view->findChildren<QAbstractItemView *>();
    for (QAbstractItemView *items : itemViews) {
        items->removeEventFilter(m_keys);
    }
    restoreCurrentColumnLabels();
    m_view = nullptr;
    resetSelectionState();
    updateSortIndicator();
    view->setParent(parentWidget());
    return view;
}

FolderView *YaziColumns::currentView() const
{
    return m_view;
}

void YaziColumns::setUrl(const QUrl &url)
{
    if (stripped(url) != stripped(m_url)) {
        resetSelectionState();
    }
    m_url = url;
    m_pendingRequest.clear();
    const QUrl parent = YaziMode::parentFolder(url);
    if (!parent.isEmpty() && stripped(parent) == stripped(m_parentUrl)) {
        highlightCurrent();
        return;
    }
    m_parentUrl = parent;
    if (parent.isEmpty()) {
        if (!m_lister->url().isEmpty()) {
            resetLister();
        }
        return;
    }
    m_lister->openUrl(parent);
}

QUrl YaziColumns::url() const
{
    return m_url;
}

QUrl YaziColumns::parentUrl() const
{
    return m_parentUrl;
}

void YaziColumns::setShowHiddenFiles(bool show)
{
    if (show == m_showHiddenFiles) {
        return;
    }
    m_showHiddenFiles = show;
    m_lister->setShowHiddenFiles(show);
    m_lister->emitChanges();
    if (YaziPreview *builtIn = preview()) {
        builtIn->setShowHiddenFiles(show);
    }
}

QListView *YaziColumns::parentColumn() const
{
    return m_parentColumn;
}

QWidget *YaziColumns::previewSlot() const
{
    return m_previewSlot;
}

void YaziColumns::setPreviewWidget(QWidget *widget)
{
    if (widget == m_previewWidget) {
        return;
    }
    QWidget *focus = QApplication::focusWidget();
    if (m_previewWidget && focus && (focus == m_previewWidget || m_previewWidget->isAncestorOf(focus)) && m_view) {
        m_view->currentView()->setFocus();
    }
    delete m_previewWidget;
    m_previewWidget = widget;
    if (m_previewWidget) {
        m_previewLayout->addWidget(m_previewWidget);
        m_previewWidget->show();
    }
    if (YaziPreview *builtIn = preview()) {
        builtIn->setShowHiddenFiles(m_showHiddenFiles);
        builtIn->setContent(m_previewContent);
        builtIn->setView(m_view);
    }
    updateTabOrder();
    applyColumnWidths();
}

QWidget *YaziColumns::previewWidget() const
{
    return m_previewWidget;
}

void YaziColumns::setPreviewEnabled(bool enabled)
{
    if (enabled == previewEnabled()) {
        return;
    }
    setPreviewWidget(enabled ? new YaziPreview : nullptr);
}

bool YaziColumns::previewEnabled() const
{
    return m_previewWidget != nullptr;
}

YaziPreview *YaziColumns::preview() const
{
    return qobject_cast<YaziPreview *>(m_previewWidget.data());
}

void YaziColumns::setPreviewContent(YaziMode::PreviewContent content)
{
    m_previewContent = content;
    if (YaziPreview *builtIn = preview()) {
        builtIn->setContent(content);
    }
}

YaziMode::PreviewContent YaziColumns::previewContent() const
{
    return m_previewContent;
}

YaziMode::Ratios YaziColumns::ratios() const
{
    return m_ratios;
}

void YaziColumns::setRatios(const YaziMode::Ratios &ratios)
{
    m_ratios = ratios;
    applyColumnWidths();
}

void YaziColumns::applyColumnWidths()
{
    const bool preview = m_previewWidget != nullptr;
    if (!m_splitter->isVisible()) {
        // Without a width yet, show what is wanted; showing the splitter sizes it.
        m_parentColumn->setVisible(true);
        m_previewSlot->setVisible(preview);
        return;
    }
    const YaziMode::ColumnWidths widths = YaziMode::columnWidths(m_splitter->contentsRect().width(), preview, m_ratios, m_splitter->handleWidth());
    const bool parentShown = widths.parent > 0;
    const bool previewShown = widths.preview > 0;
    QWidget *focus = QApplication::focusWidget();
    if (focus && m_view
        && ((!parentShown && (focus == m_parentColumn || m_parentColumn->isAncestorOf(focus))) || (!previewShown && m_previewSlot->isAncestorOf(focus)))) {
        m_view->currentView()->setFocus();
    }
    const bool layoutChanged = m_parentColumn->isVisible() != parentShown || m_previewSlot->isVisible() != previewShown;
    m_parentColumn->setVisible(parentShown);
    m_previewSlot->setVisible(previewShown);
    if (layoutChanged) {
        // Show or hide the handles now rather than on the next layout request,
        // so the sizes below share the width without the hidden columns' handles.
        m_splitter->refresh();
    }
    QList<int> sizes;
    for (int i = 0; i < m_splitter->count(); ++i) {
        const QWidget *column = m_splitter->widget(i);
        sizes.append(column == m_parentColumn ? widths.parent : column == m_previewSlot ? widths.preview : widths.current);
    }
    m_splitter->setSizes(sizes);
}

void YaziColumns::rememberRatios()
{
    if (!m_view || m_parentColumn->isHidden()) {
        return;
    }
    const int parent = m_parentColumn->width();
    const int current = m_view->width();
    if (!m_previewSlot->isHidden()) {
        m_ratios = {parent, current, m_previewSlot->width()};
    } else {
        // Keep the hidden preview column's share in proportion to the others.
        const qint64 before = qint64(qMax(1, m_ratios.parent)) + qMax(1, m_ratios.current);
        const int preview = int(qMax<qint64>(1, qint64(qMax(1, m_ratios.preview)) * (parent + current) / before));
        m_ratios = {parent, current, preview};
    }
    // Enforces the minimum width of the current column, which the splitter doesn't know.
    applyColumnWidths();
}

void YaziColumns::updateTabOrder()
{
    if (!m_view) {
        return;
    }
    QWidget *current = m_view->currentView();
    QWidget::setTabOrder(m_parentColumn, current);
    if (m_previewWidget && m_previewWidget->focusPolicy() != Qt::NoFocus) {
        QWidget::setTabOrder(current, m_previewWidget);
    }
}

void YaziColumns::labelCurrentColumn()
{
    if (!m_view) {
        return;
    }
    const QString name = i18nc("@info:accessibility", "Current folder");
    const QString description = i18nc("@info:accessibility", "The items in the folder this pane shows");
    for (QWidget *widget : {static_cast<QWidget *>(m_view), static_cast<QWidget *>(m_view->currentView())}) {
        m_savedAccessibility.append({widget, widget->accessibleName(), widget->accessibleDescription()});
        widget->setAccessibleName(name);
        widget->setAccessibleDescription(description);
    }
}

void YaziColumns::restoreCurrentColumnLabels()
{
    // Restore in reverse so a widget labelled twice gets its original text back.
    for (auto it = m_savedAccessibility.crbegin(); it != m_savedAccessibility.crend(); ++it) {
        if (it->widget) {
            it->widget->setAccessibleName(it->name);
            it->widget->setAccessibleDescription(it->description);
        }
    }
    m_savedAccessibility.clear();
}

QSplitter *YaziColumns::splitter() const
{
    return m_splitter;
}

QLabel *YaziColumns::modeIndicator() const
{
    return m_indicator;
}

QLabel *YaziColumns::selectionIndicator() const
{
    return m_selectionLabel;
}

void YaziColumns::addStatusWidget(QWidget *widget, int stretch)
{
    // Before the stretch that keeps the transfer and sort indicators at the end of the row.
    m_statusLayout->insertWidget(m_statusLayout->indexOf(m_transferIndicator) - 1, widget, stretch);
}

void YaziColumns::setTransferQueue(TransferQueue *queue)
{
    if (queue == m_transferQueue) {
        return;
    }
    if (m_transferQueue) {
        m_transferQueue->disconnect(this);
    }
    m_transferQueue = queue;
    m_reportedFailures.clear();
    m_transferFailure.clear();
    m_transferFailureTimeout->stop();
    if (m_transferQueue) {
        // Failures from before the queue was followed were reported elsewhere.
        const auto entries = m_transferQueue->entries();
        for (const QueuedTransfer *entry : entries) {
            if (entry->state() == QueuedTransfer::State::Failed) {
                m_reportedFailures.append(entry);
            }
        }
        connect(m_transferQueue, &TransferQueue::changed, this, &YaziColumns::updateTransferIndicator);
    }
    updateTransferIndicator();
}

TransferQueue *YaziColumns::transferQueue() const
{
    return m_transferQueue;
}

QToolButton *YaziColumns::transferIndicator() const
{
    return m_transferIndicator;
}

void YaziColumns::updateTransferIndicator()
{
    const QList<QueuedTransfer *> entries = m_transferQueue ? m_transferQueue->entries() : QList<QueuedTransfer *>();
    int running = 0;
    int paused = 0;
    int waiting = 0;
    qint64 percent = 0;
    QStringList details;
    QList<const QObject *> failures;
    for (const QueuedTransfer *entry : entries) {
        switch (entry->state()) {
        case QueuedTransfer::State::Running:
            ++running;
            percent += qint64(entry->percent());
            details.append(i18nc("@info:tooltip %1 is a transfer, %2 its progress in percent", "%1: %2%", entry->description(), entry->percent()));
            break;
        case QueuedTransfer::State::Paused:
            ++paused;
            details.append(i18nc("@info:tooltip %1 is a transfer", "%1: paused", entry->description()));
            break;
        case QueuedTransfer::State::Waiting:
            ++waiting;
            details.append(i18nc("@info:tooltip %1 is a transfer", "%1: waiting", entry->description()));
            break;
        case QueuedTransfer::State::Failed:
            failures.append(entry);
            if (!m_reportedFailures.contains(entry)) {
                m_transferFailure = entry->failure().isEmpty()
                    ? i18nc("@info:tooltip %1 is a transfer", "%1: failed", entry->description())
                    : i18nc("@info:tooltip %1 is a transfer, %2 why it failed", "%1: %2", entry->description(), entry->failure());
                m_transferFailureTimeout->start();
            }
            break;
        case QueuedTransfer::State::Completed:
        case QueuedTransfer::State::Canceled:
            break;
        }
    }
    m_reportedFailures = failures;
    const bool failed = m_transferFailureTimeout->isActive();

    QStringList parts;
    if (running > 0) {
        parts.append(i18ncp("@info:status %2 is the average progress in percent",
                            "%1 transfer running · %2%",
                            "%1 transfers running · %2%",
                            running,
                            percent / running));
    }
    if (paused > 0) {
        parts.append(parts.isEmpty() ? i18ncp("@info:status", "%1 transfer paused", "%1 transfers paused", paused)
                                     : i18ncp("@info:status after the running transfers", "%1 paused", "%1 paused", paused));
    }
    if (waiting > 0) {
        parts.append(parts.isEmpty() ? i18ncp("@info:status", "%1 transfer waiting", "%1 transfers waiting", waiting)
                                     : i18ncp("@info:status after the running transfers", "%1 waiting", "%1 waiting", waiting));
    }
    if (failed) {
        parts.append(i18nc("@info:status", "Transfer failed"));
        details.append(m_transferFailure);
    } else {
        m_transferFailure.clear();
    }
    if (parts.isEmpty()) {
        m_transferIndicator->hide();
        return;
    }
    const QString text = parts.join(QStringLiteral(" · "));
    m_transferIndicator->setText(text);
    m_transferIndicator->setIcon(
        SymbolicIcon::fromTheme(failed && running + paused + waiting == 0 ? QStringLiteral("dialog-error") : QStringLiteral("view-list-details")));
    m_transferIndicator->setAccessibleName(text);
    details.append(i18nc("@info:tooltip", "Click to show the transfer queue."));
    m_transferIndicator->setToolTip(details.join(QLatin1Char('\n')));
    m_transferIndicator->show();
}

void YaziColumns::setFilter(const NameFilter &filter)
{
    const QString text = filter.text();
    if (text.isEmpty()) {
        m_filterLabel->hide();
        m_filterClear->hide();
        m_filterLabel->clear();
        return;
    }
    constexpr int MaximumShownLength = 40;
    const QString shown = text.size() > MaximumShownLength ? text.left(MaximumShownLength - 1) + QStringLiteral("…") : text;
    m_filterLabel->setText(i18nc("@info:status %1 is the filter text", "Filter: %1", shown));
    m_filterLabel->setAccessibleName(i18nc("@info:accessibility %1 is the filter text", "Filtered by %1", text));
    m_filterLabel->show();
    m_filterClear->show();
}

QLabel *YaziColumns::filterIndicator() const
{
    return m_filterLabel;
}

QToolButton *YaziColumns::filterClearButton() const
{
    return m_filterClear;
}

QLabel *YaziColumns::sortIndicator() const
{
    return m_sortLabel;
}

void YaziColumns::updateSortIndicator()
{
    if (!m_view) {
        m_sortLabel->hide();
        return;
    }
    const int column = m_view->sortColumn();
    const bool ascending = m_view->sortOrder() == Qt::AscendingOrder;
    QString name;
    switch (column) {
    case KDirModel::Name:
        name = i18nc("@info:status sort column", "Name");
        break;
    case KDirModel::Size:
        name = i18nc("@info:status sort column", "Size");
        break;
    case KDirModel::ModifiedTime:
        name = i18nc("@info:status sort column", "Modified");
        break;
    case KDirModel::Type:
        name = i18nc("@info:status sort column", "Type");
        break;
    default:
        if (const QAbstractItemModel *model = m_view->currentView()->model()) {
            name = model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        }
        break;
    }
    if (name.isEmpty()) {
        m_sortLabel->hide();
        return;
    }
    m_sortLabel->setText(ascending ? i18nc("@info:status %1 is the sort column", "%1 ↑", name) : i18nc("@info:status %1 is the sort column", "%1 ↓", name));
    m_sortLabel->setAccessibleName(ascending ? i18nc("@info:accessibility %1 is the sort column", "Sorted by %1, ascending", name)
                                             : i18nc("@info:accessibility %1 is the sort column", "Sorted by %1, descending", name));
    m_sortLabel->setToolTip(m_sortLabel->accessibleName());
    m_sortLabel->show();
}

void YaziColumns::setSearchStatus(const QString &status, bool running)
{
    if (status.isEmpty() && !running) {
        clearSearchStatus();
        return;
    }
    m_searchRunning = running;
    m_searchLabel->setText(status.isEmpty() ? i18nc("@info:status", "Searching…") : status);
    m_searchLabel->setToolTip(m_searchLabel->text());
    m_searchLabel->show();
    m_searchCancel->setVisible(running);
    if (running) {
        m_searchStatusTimeout->stop();
    } else {
        m_searchStatusTimeout->start();
    }
}

void YaziColumns::clearSearchStatus()
{
    m_searchRunning = false;
    m_searchStatusTimeout->stop();
    m_searchLabel->hide();
    m_searchLabel->clear();
    m_searchCancel->hide();
}

bool YaziColumns::isSearchRunning() const
{
    return m_searchRunning;
}

QLabel *YaziColumns::searchIndicator() const
{
    return m_searchLabel;
}

QToolButton *YaziColumns::searchCancelButton() const
{
    return m_searchCancel;
}

YaziKeys *YaziColumns::keys() const
{
    return m_keys;
}

void YaziColumns::updateIndicator()
{
    const bool vim = m_keys->preset() == YaziMode::Preset::Vim;
    QString text = vim ? i18nc("@info:status short name of the Yazi browsing mode with Vim-style keys", "Yazi · Vim")
                       : i18nc("@info:status short name of the Yazi browsing mode", "Yazi");
    QString name = vim ? i18nc("@info:accessibility", "Yazi mode with Vim-style keys") : i18nc("@info:accessibility", "Yazi mode");
    const QString pending = m_keys->pendingText();
    if (!pending.isEmpty()) {
        text = i18nc("@info:status %1 is the mode, %2 the keys typed so far of a key sequence", "%1 · %2…", text, pending);
        name = i18nc("@info:accessibility %1 is the mode, %2 the keys typed so far of a key sequence", "%1, waiting for the keys after %2", name, pending);
    }
    m_indicator->setText(text);
    m_indicator->setAccessibleName(name);
}

bool YaziColumns::isEditing(const QAbstractItemView *view)
{
    // QAbstractItemView::state() is protected; an inline editor is a typing widget on the viewport.
    const QList<QWidget *> children = view->viewport()->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
    return std::any_of(children.cbegin(), children.cend(), [](const QWidget *child) {
        return child->isVisible() && YaziKeys::isTypingWidget(child);
    });
}

bool YaziColumns::acceptsKeys(QWidget *widget) const
{
    if (!m_view || !widget) {
        return false;
    }
    if (auto *items = qobject_cast<QAbstractItemView *>(widget); items && isEditing(items)) {
        return false;
    }
    return widget == m_parentColumn || widget == m_view->currentView() || m_previewSlot->isAncestorOf(widget);
}

std::optional<YaziColumns::Column> YaziColumns::focusedColumn() const
{
    QWidget *focus = QApplication::focusWidget();
    if (!focus) {
        return std::nullopt;
    }
    if (focus == m_parentColumn || m_parentColumn->isAncestorOf(focus)) {
        return Column::Parent;
    }
    if (m_view && (focus == m_view || m_view->isAncestorOf(focus))) {
        return Column::Current;
    }
    if (m_previewSlot->isAncestorOf(focus)) {
        return Column::Preview;
    }
    return std::nullopt;
}

KFileItem YaziColumns::focusedItem() const
{
    if (focusedColumn() == Column::Parent) {
        return m_model->itemForIndex(m_proxy->mapToSource(m_parentColumn->currentIndex()));
    }
    return m_view ? m_view->currentItem() : KFileItem();
}

QAbstractItemView *YaziColumns::movementTarget() const
{
    if (focusedColumn() == Column::Parent) {
        return m_parentColumn;
    }
    return m_view ? m_view->currentView() : nullptr;
}

void YaziColumns::moveFocus(YaziMode::Command command, int count)
{
    QAbstractItemView *view = movementTarget();
    if (!view) {
        return;
    }
    using YaziMode::Command;
    Qt::Key key = Qt::Key_unknown;
    int steps = qBound(1, count, YaziMode::MaximumCount);
    switch (command) {
    case Command::MoveUp:
        key = Qt::Key_Up;
        break;
    case Command::MoveDown:
        key = Qt::Key_Down;
        break;
    case Command::MoveFirst:
        key = Qt::Key_Home;
        steps = 1;
        break;
    case Command::MoveLast:
        key = Qt::Key_End;
        steps = 1;
        break;
    case Command::PageUp:
        key = Qt::Key_PageUp;
        break;
    case Command::PageDown:
        key = Qt::Key_PageDown;
        break;
    case Command::HalfPageUp:
        key = Qt::Key_Up;
        steps *= halfPageRows(view);
        break;
    case Command::HalfPageDown:
        key = Qt::Key_Down;
        steps *= halfPageRows(view);
        break;
    default:
        return;
    }
    // The view's own keys move the focus and selection as they always do,
    // in every view mode and through expanded folders. While marking, the
    // selection is put back after each step, so it stays as marked.
    QItemSelectionModel *selection = view->selectionModel();
    const bool keepMarks = m_marking && m_view && view == m_view->currentView() && selection;
    const QScopedValueRollback changing(m_changingSelection, m_changingSelection || keepMarks);
    const QItemSelection marked = keepMarks ? selection->selection() : QItemSelection();
    for (int step = 0; step < steps; ++step) {
        const QPersistentModelIndex before = view->currentIndex();
        m_keys->sendKey(view, key);
        if (keepMarks) {
            selection->select(marked, QItemSelectionModel::ClearAndSelect);
        }
        if (before == view->currentIndex()) {
            break;
        }
    }
    if (keepMarks && m_visual) {
        applyVisualRange();
    }
    if (keepMarks) {
        updateSelectionIndicator();
    }
}

bool YaziColumns::focusColumn(Column column)
{
    QWidget *target = nullptr;
    switch (column) {
    case Column::Parent:
        target = m_parentColumn->isVisible() ? m_parentColumn : nullptr;
        break;
    case Column::Current:
        target = m_view ? m_view->currentView() : nullptr;
        break;
    case Column::Preview:
        target = m_previewSlot->isVisible() && m_previewWidget && m_previewWidget->focusPolicy() != Qt::NoFocus ? m_previewWidget.data() : nullptr;
        break;
    }
    if (!target) {
        return false;
    }
    target->setFocus(Qt::ShortcutFocusReason);
    return true;
}

void YaziColumns::openParentItem()
{
    const QModelIndex index = m_parentColumn->currentIndex();
    const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
    if (item.isNull()) {
        return;
    }
    if (stripped(item.url()) == stripped(m_url)) {
        focusColumn(Column::Current);
        return;
    }
    request(index);
}

void YaziColumns::toggleSelection(bool moveDown, int count)
{
    if (!m_view) {
        return;
    }
    QAbstractItemView *items = m_view->currentView();
    QItemSelectionModel *selection = items->selectionModel();
    const int steps = moveDown ? qBound(1, count, YaziMode::MaximumCount) : 1;
    for (int step = 0; step < steps; ++step) {
        const QModelIndex current = items->currentIndex();
        if (!current.isValid()) {
            break;
        }
        {
            const QScopedValueRollback changing(m_changingSelection, true);
            // Toggling ends range selection and keeps its items marked.
            m_visual = false;
            m_visualAnchor = QPersistentModelIndex();
            m_visualBase.clear();
            // The focused item is selected as the focus moves; the first toggle marks it rather than deselecting it.
            const bool first = !m_marking && selectionIsFocusOnly();
            selection->select(current, (first ? QItemSelectionModel::ClearAndSelect : QItemSelectionModel::Toggle) | QItemSelectionModel::Rows);
            m_marking = selection->hasSelection();
        }
        if (moveDown) {
            moveFocus(YaziMode::Command::MoveDown);
            if (items->currentIndex() == current) {
                break;
            }
        }
    }
    updateSelectionIndicator();
}

void YaziColumns::toggleVisualSelection()
{
    if (!m_view) {
        return;
    }
    if (m_visual) {
        m_visual = false;
        m_visualAnchor = QPersistentModelIndex();
        m_visualBase.clear();
        updateSelectionIndicator();
        return;
    }
    QAbstractItemView *items = m_view->currentView();
    const QModelIndex current = items->currentIndex();
    if (!current.isValid()) {
        return;
    }
    m_visualBase = m_marking ? items->selectionModel()->selection() : QItemSelection();
    m_visualAnchor = current.siblingAtColumn(0);
    m_visual = true;
    m_marking = true;
    applyVisualRange();
    updateSelectionIndicator();
}

void YaziColumns::selectAll()
{
    if (!m_view) {
        return;
    }
    {
        const QScopedValueRollback changing(m_changingSelection, true);
        m_visual = false;
        m_visualAnchor = QPersistentModelIndex();
        m_visualBase.clear();
        m_view->selectAll();
        m_marking = m_view->currentView()->selectionModel()->hasSelection();
    }
    updateSelectionIndicator();
}

bool YaziColumns::isMarking() const
{
    return m_marking;
}

bool YaziColumns::isVisual() const
{
    return m_visual;
}

void YaziColumns::selectionChanged()
{
    if (m_changingSelection) {
        return;
    }
    // Another change, such as a click, Escape or the Edit menu, ends range
    // selection; once only the focused item is left, the selection follows
    // the focus again.
    m_visual = false;
    m_visualAnchor = QPersistentModelIndex();
    m_visualBase.clear();
    if (m_marking && selectionIsFocusOnly()) {
        m_marking = false;
    }
    updateSelectionIndicator();
}

void YaziColumns::resetSelectionState()
{
    m_marking = false;
    m_visual = false;
    m_visualAnchor = QPersistentModelIndex();
    m_visualBase.clear();
    updateSelectionIndicator();
}

bool YaziColumns::selectionIsFocusOnly() const
{
    if (!m_view) {
        return true;
    }
    const QAbstractItemView *items = m_view->currentView();
    const QModelIndexList rows = items->selectionModel()->selectedRows();
    return rows.isEmpty() || (rows.size() == 1 && rows.first() == items->currentIndex().siblingAtColumn(0));
}

void YaziColumns::applyVisualRange()
{
    if (!m_view || !m_visual) {
        return;
    }
    QAbstractItemView *items = m_view->currentView();
    const QModelIndex current = items->currentIndex();
    if (!m_visualAnchor.isValid() || !current.isValid()) {
        // The item where it started is gone, as after a reload.
        m_visual = false;
        m_visualBase.clear();
        return;
    }
    QItemSelection selected = m_visualBase;
    selected.merge(rowsBetween(m_visualAnchor, current), QItemSelectionModel::Select);
    const QScopedValueRollback changing(m_changingSelection, true);
    items->selectionModel()->select(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

QItemSelection YaziColumns::rowsBetween(const QModelIndex &from, const QModelIndex &to) const
{
    const QModelIndex first = from.siblingAtColumn(0);
    const QModelIndex last = to.siblingAtColumn(0);
    if (!first.isValid() || !last.isValid()) {
        return {};
    }
    if (const auto *tree = qobject_cast<const QTreeView *>(m_view->currentView())) {
        // Expanded folders put rows of other parents in between; walk the rows as shown.
        for (const auto &[start, end] : {std::pair(first, last), std::pair(last, first)}) {
            QItemSelection rows;
            for (QModelIndex index = start; index.isValid(); index = tree->indexBelow(index)) {
                rows.select(index, index);
                if (index == end) {
                    return rows;
                }
            }
        }
        return {};
    }
    if (first.parent() != last.parent()) {
        return {};
    }
    const QAbstractItemModel *model = first.model();
    const int top = qMin(first.row(), last.row());
    const int bottom = qMax(first.row(), last.row());
    return QItemSelection(model->index(top, 0, first.parent()), model->index(bottom, 0, first.parent()));
}

void YaziColumns::updateSelectionIndicator()
{
    if (!m_view) {
        m_selectionLabel->hide();
        return;
    }
    const int count = int(m_view->currentView()->selectionModel()->selectedRows().size());
    if (!m_visual && !m_marking && count < 2) {
        m_selectionLabel->hide();
        return;
    }
    if (m_visual) {
        m_selectionLabel->setText(i18ncp("@info:status range selection in Yazi mode", "Visual · %1 selected", "Visual · %1 selected", count));
        m_selectionLabel->setAccessibleName(i18ncp("@info:accessibility", "Range selection, %1 item selected", "Range selection, %1 items selected", count));
    } else {
        m_selectionLabel->setText(i18ncp("@info:status", "%1 selected", "%1 selected", count));
        m_selectionLabel->setAccessibleName(i18ncp("@info:accessibility", "%1 item selected", "%1 items selected", count));
    }
    m_selectionLabel->show();
}

bool YaziColumns::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_parentColumn && event->type() == QEvent::FocusIn)
        || (watched == m_parentColumn->viewport() && event->type() == QEvent::MouseButtonPress)) {
        Q_EMIT activated();
    } else if (watched == m_parentColumn && event->type() == QEvent::Show) {
        highlightCurrent();
    } else if (watched == m_splitter && (event->type() == QEvent::Show || (event->type() == QEvent::Resize && m_splitter->isVisible()))) {
        applyColumnWidths();
    } else if (watched == m_view && event->type() == QEvent::FontChange) {
        m_parentColumn->setFont(m_view->font());
        // The new font changes the row heights once the column lays out again.
        QMetaObject::invokeMethod(this, &YaziColumns::highlightCurrent, Qt::QueuedConnection);
    }
    return QWidget::eventFilter(watched, event);
}

void YaziColumns::highlightCurrent()
{
    if (m_url.isEmpty()) {
        return;
    }
    const QModelIndex index = m_proxy->mapFromSource(m_model->indexForUrl(stripped(m_url)));
    if (!index.isValid()) {
        return;
    }
    if (index != m_parentColumn->currentIndex() || !m_parentColumn->selectionModel()->isSelected(index)) {
        m_parentColumn->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    }
    m_parentColumn->scrollTo(index);
}

void YaziColumns::request(const QModelIndex &index)
{
    const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
    if (item.isNull()) {
        return;
    }
    const QUrl target = item.url();
    if (stripped(target) == stripped(m_url) || stripped(target) == stripped(m_pendingRequest)) {
        return;
    }
    const QUrl folder = m_parentUrl;
    const bool isFolder = item.isDir();
    m_pendingRequest = target;
    // A click also emits activated() with the same index, and navigating
    // resets this column's model, so request after the click is handled.
    QMetaObject::invokeMethod(
        this,
        [this, target, folder, isFolder] {
            if (stripped(target) != stripped(m_pendingRequest)) {
                return;
            }
            m_pendingRequest.clear();
            if (isFolder) {
                Q_EMIT folderRequested(target);
            } else {
                Q_EMIT itemRequested(folder, target);
            }
        },
        Qt::QueuedConnection);
}
