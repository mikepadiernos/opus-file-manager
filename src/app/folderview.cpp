// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"

#include "densitydelegate.h"
#include "detailsview.h"
#include "fileoperations.h"
#include "flatview.h"
#include "ignorelist.h"
#include "symbolicicon.h"

#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItemDelegate>
#include <KFilePreviewGenerator>
#include <KIO/Job>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QListView>
#include <QMimeData>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

/// Sorts like KDirSortFilterProxyModel and hides ignored items and items a NameFilter rejects.
class NameFilterProxyModel : public KDirSortFilterProxyModel
{
public:
    explicit NameFilterProxyModel(QObject *parent)
        : KDirSortFilterProxyModel(parent)
        , m_ignoreList(IgnoreList::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files"))))
    {
    }

    NameFilter nameFilter() const
    {
        return m_filter;
    }

    void setNameFilter(const NameFilter &filter)
    {
        beginFilterChange();
        m_filter = filter;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }

    /// Hides folders and shows relative paths in the Type column; an empty URL ends flat mode.
    void setFlatBase(const QUrl &base)
    {
        beginFilterChange();
        m_flatBase = base;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
        Q_EMIT headerDataChanged(Qt::Horizontal, KDirModel::Type, KDirModel::Type);
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (isRelativePath(index.column()) && role == Qt::DisplayRole) {
            return FlatView::relativeFolder(m_flatBase, itemAt(index).url());
        }
        const QVariant value = KDirSortFilterProxyModel::data(index, role);
        if (role == Qt::DecorationRole && value.typeId() == QMetaType::QIcon) {
            return SymbolicIcon::wrap(value.value<QIcon>());
        }
        return value;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && isRelativePath(section) && role == Qt::DisplayRole) {
            return i18nc("@title:column path below the flat folder", "Relative Path");
        }
        return KDirSortFilterProxyModel::headerData(section, orientation, role);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (!KDirSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent)) {
            return false;
        }
        const auto *model = static_cast<const KDirModel *>(sourceModel());
        const KFileItem item = model->itemForIndex(model->index(sourceRow, 0, sourceParent));
        if (m_flatBase.isValid() && item.isDir()) {
            return false;
        }
        return !m_ignoreList.ignores(item.name()) && m_filter.matches(item.name(), item.isDir());
    }

    bool subSortLessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (isRelativePath(left.column())) {
            const int byPath = QString::localeAwareCompare(FlatView::relativeFolder(m_flatBase, itemAt(left).url()),
                                                           FlatView::relativeFolder(m_flatBase, itemAt(right).url()));
            if (byPath != 0) {
                return byPath < 0;
            }
        }
        return KDirSortFilterProxyModel::subSortLessThan(left, right);
    }

private:
    bool isRelativePath(int column) const
    {
        return m_flatBase.isValid() && column == KDirModel::Type;
    }

    /// The item at a proxy or source index of this model's KDirModel.
    KFileItem itemAt(const QModelIndex &index) const
    {
        const QModelIndex source = index.model() == this ? mapToSource(index) : index;
        return static_cast<const KDirModel *>(sourceModel())->itemForIndex(source);
    }

    QUrl m_flatBase;
    NameFilter m_filter;
    IgnoreList m_ignoreList;
};

namespace
{
/// Room around an icon in the icons view grid for its name.
constexpr int IconGridMargin = 48;
}

FolderView::FolderView(QWidget *parent)
    : QWidget(parent)
    , m_lister(new KDirLister(this))
    , m_model(new KDirModel(this))
    , m_proxy(new NameFilterProxyModel(this))
    , m_stack(new QStackedWidget(this))
    , m_details(new DetailsView(this))
    , m_icons(new QListView(this))
    , m_detailsDelegate(new DensityDelegate(m_details))
    , m_iconsDelegate(new KFileItemDelegate(m_icons))
{
    m_lister->setAutoErrorHandlingEnabled(false);
    m_lister->setMainWindow(window());
    m_model->setDirLister(m_lister);
    m_model->setJobTransfersVisible(true);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortFoldersFirst(true);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_details->setModel(m_proxy);
    m_details->setRootIsDecorated(false);
    m_details->setItemsExpandable(false);
    m_details->setUniformRowHeights(true);
    m_details->setSortingEnabled(true);
    m_details->sortByColumn(KDirModel::Name, Qt::AscendingOrder);
    m_details->setAllColumnsShowFocus(true);
    m_details->header()->setSectionsMovable(true);
    m_details->header()->setStretchLastSection(false);
    m_details->header()->setSectionResizeMode(KDirModel::Name, QHeaderView::Stretch);
    for (int column : {KDirModel::Permissions, KDirModel::Owner, KDirModel::Group}) {
        m_details->setColumnHidden(column, true);
    }
    setupView(m_details, m_detailsDelegate);

    m_icons->setModel(m_proxy);
    m_icons->setSelectionModel(m_details->selectionModel());
    m_icons->setViewMode(QListView::IconMode);
    m_icons->setResizeMode(QListView::Adjust);
    m_icons->setFlow(QListView::LeftToRight);
    m_icons->setWrapping(true);
    m_icons->setMovement(QListView::Static);
    m_icons->setUniformItemSizes(true);
    m_icons->setWordWrap(true);
    m_icons->setIconSize(QSize(64, 64));
    m_icons->setGridSize(QSize(64 + IconGridMargin, 64 + IconGridMargin));
    m_icons->setSpacing(4);
    setupView(m_icons, m_iconsDelegate);

    m_detailsPreviews = new KFilePreviewGenerator(m_details);
    m_iconPreviews = new KFilePreviewGenerator(m_icons);
    setPreviewsEnabled(true);

    m_stack->addWidget(m_details);
    m_stack->addWidget(m_icons);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    connect(m_lister, &KDirLister::started, this, [this] {
        Q_EMIT loadingChanged(true);
    });
    connect(m_lister, &KDirLister::completed, this, [this] {
        Q_EMIT loadingChanged(false);
        Q_EMIT itemCountChanged();
    });
    connect(m_lister, &KDirLister::canceled, this, [this] {
        Q_EMIT loadingChanged(false);
    });
    connect(m_lister, &KDirLister::redirection, this, [this](const QUrl &, const QUrl &newUrl) {
        if (!m_flat) {
            m_folder = newUrl;
        }
        Q_EMIT redirected(newUrl);
    });
    connect(m_lister, &KDirLister::jobError, this, [this](KIO::Job *job) {
        Q_EMIT loadingChanged(false);
        Q_EMIT listingFailed(job->errorString());
    });
    connect(m_lister, &KDirLister::itemsAdded, this, &FolderView::itemCountChanged);
    connect(m_lister, &KDirLister::itemsDeleted, this, &FolderView::itemCountChanged);
    connect(m_lister, &KDirLister::clear, this, &FolderView::itemCountChanged);

    connect(m_details->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FolderView::selectionChanged);
    connect(m_details->selectionModel(), &QItemSelectionModel::currentChanged, this, &FolderView::selectionChanged);
}

FolderView::~FolderView()
{
    // ~KCoreDirLister emits clear() while ~QWidget deletes children, after this type is gone.
    m_lister->disconnect(this);
}

void FolderView::setupView(QAbstractItemView *view, KFileItemDelegate *delegate)
{
    view->setItemDelegate(delegate);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setDefaultDropAction(Qt::MoveAction);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setFocusPolicy(Qt::StrongFocus);
    view->viewport()->installEventFilter(this);
    view->installEventFilter(this);

    connect(view, &QAbstractItemView::activated, this, &FolderView::onActivated);
    connect(view, &QAbstractItemView::customContextMenuRequested, this, &FolderView::onContextMenu);
}

QUrl FolderView::url() const
{
    return m_folder;
}

void FolderView::setUrl(const QUrl &url)
{
    if (!url.isValid() || url == m_folder) {
        return;
    }
    m_folder = url;
    if (m_flat) {
        m_proxy->setFlatBase(url);
    }
    openListing();
    updatePreviews();
    Q_EMIT urlChanged(url);
}

void FolderView::openListing()
{
    m_lister->openUrl(m_flat ? FlatView::listingUrl(m_folder) : m_folder);
}

void FolderView::reload()
{
    if (m_lister->url().isValid()) {
        m_lister->openUrl(m_lister->url(), KDirLister::Reload);
    }
}

FolderView::ViewMode FolderView::viewMode() const
{
    return m_viewMode;
}

void FolderView::setViewMode(ViewMode mode)
{
    if (mode == m_viewMode) {
        return;
    }
    const bool hadFocus = currentView()->hasFocus();
    m_viewMode = mode;
    m_stack->setCurrentWidget(currentView());
    if (hadFocus) {
        currentView()->setFocus();
    }
}

bool FolderView::showHiddenFiles() const
{
    return m_lister->showHiddenFiles();
}

void FolderView::setShowHiddenFiles(bool show)
{
    if (show == m_lister->showHiddenFiles()) {
        return;
    }
    m_lister->setShowHiddenFiles(show);
    m_lister->emitChanges();
}

bool FolderView::previewsEnabled() const
{
    return m_previewsEnabled;
}

void FolderView::setPreviewsEnabled(bool enabled)
{
    m_previewsEnabled = enabled;
    updatePreviews();
}

void FolderView::setRemotePreviewsEnabled(bool enabled)
{
    m_remotePreviewsEnabled = enabled;
    updatePreviews();
}

bool FolderView::previewsShown() const
{
    return m_detailsPreviews->isPreviewShown();
}

void FolderView::setPreviewPlugins(const QStringList &plugins)
{
    m_detailsPreviews->setEnabledPlugins(plugins);
    m_iconPreviews->setEnabledPlugins(plugins);
}

QStringList FolderView::previewPlugins() const
{
    return m_detailsPreviews->enabledPlugins();
}

void FolderView::setIconSizes(int details, int icons)
{
    m_details->setIconSize(QSize(details, details));
    m_icons->setIconSize(QSize(icons, icons));
    m_icons->setGridSize(QSize(icons + IconGridMargin, icons + IconGridMargin));
}

void FolderView::setItemSpacing(int detailsSpace, int iconsSpace)
{
    m_detailsDelegate->setItemSpace(detailsSpace);
    m_details->doItemsLayout();
    m_icons->setSpacing(2 * iconsSpace);
}

void FolderView::setDragFromAnywhere(bool enabled)
{
    m_details->setDragFromAnywhere(enabled);
}

void FolderView::setColumnVisible(int column, bool visible)
{
    if (column != KDirModel::Name) {
        m_details->setColumnHidden(column, !visible);
    }
}

bool FolderView::isColumnVisible(int column) const
{
    return !m_details->isColumnHidden(column);
}

void FolderView::setSorting(int column, Qt::SortOrder order)
{
    m_details->sortByColumn(column, order);
}

int FolderView::sortColumn() const
{
    return m_details->header()->sortIndicatorSection();
}

Qt::SortOrder FolderView::sortOrder() const
{
    return m_details->header()->sortIndicatorOrder();
}

void FolderView::setSortFoldersFirst(bool foldersFirst)
{
    m_proxy->setSortFoldersFirst(foldersFirst);
}

bool FolderView::sortFoldersFirst() const
{
    return m_proxy->sortFoldersFirst();
}

void FolderView::setShowToolTips(bool show)
{
    m_detailsDelegate->setShowToolTipWhenElided(show);
    m_iconsDelegate->setShowToolTipWhenElided(show);
}

bool FolderView::showToolTips() const
{
    return m_detailsDelegate->showToolTipWhenElided();
}

void FolderView::updatePreviews()
{
    const bool shown = m_previewsEnabled && (m_lister->url().isLocalFile() || m_remotePreviewsEnabled);
    m_detailsPreviews->setPreviewShown(shown);
    m_iconPreviews->setPreviewShown(shown);
}

KFileItem FolderView::rootItem() const
{
    return m_flat ? KFileItem(m_folder) : m_lister->rootItem();
}

KFileItemList FolderView::items() const
{
    return m_lister->items();
}

KFileItemList FolderView::selectedItems() const
{
    KFileItemList result;
    const QModelIndexList rows = m_details->selectionModel()->selectedRows();
    result.reserve(rows.size());
    for (const QModelIndex &index : rows) {
        const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
        if (!item.isNull()) {
            result.append(item);
        }
    }
    return result;
}

KFileItemList FolderView::selectedItemsOrRoot() const
{
    KFileItemList result = selectedItems();
    if (result.isEmpty()) {
        const KFileItem root = rootItem();
        if (!root.isNull()) {
            result.append(root);
        }
    }
    return result;
}

KFileItem FolderView::currentItem() const
{
    return m_model->itemForIndex(m_proxy->mapToSource(m_details->currentIndex()));
}

int FolderView::itemCount() const
{
    return m_proxy->rowCount();
}

void FolderView::selectAll()
{
    currentView()->selectAll();
}

void FolderView::clearSelection()
{
    m_details->selectionModel()->clearSelection();
}

void FolderView::selectUrls(const QList<QUrl> &urls)
{
    QItemSelectionModel *selection = m_details->selectionModel();
    selection->clearSelection();
    bool first = true;
    for (const QUrl &url : urls) {
        const QModelIndex index = m_proxy->mapFromSource(m_model->indexForUrl(url));
        if (!index.isValid()) {
            continue;
        }
        selection->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        if (first) {
            selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            currentView()->scrollTo(index);
            first = false;
        }
    }
}

QAbstractItemView *FolderView::currentView() const
{
    return m_viewMode == ViewMode::Details ? static_cast<QAbstractItemView *>(m_details) : m_icons;
}

NameFilter FolderView::nameFilter() const
{
    return m_proxy->nameFilter();
}

void FolderView::setNameFilter(const NameFilter &filter)
{
    if (filter == m_proxy->nameFilter()) {
        return;
    }
    m_proxy->setNameFilter(filter);
    Q_EMIT itemCountChanged();
}

bool FolderView::isFlat() const
{
    return m_flat;
}

void FolderView::setFlat(bool flat)
{
    if (flat == m_flat) {
        return;
    }
    m_flat = flat;
    m_proxy->setFlatBase(flat ? m_folder : QUrl());
    if (m_folder.isValid()) {
        openListing();
    }
}

void FolderView::activateFirstItem()
{
    const NameFilter filter = m_proxy->nameFilter();
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex index = m_proxy->index(row, 0);
        if (filter.appliesTo(m_model->itemForIndex(m_proxy->mapToSource(index)).isDir())) {
            onActivated(index);
            return;
        }
    }
    if (m_proxy->rowCount() > 0) {
        onActivated(m_proxy->index(0, 0));
    }
}

void FolderView::onActivated(const QModelIndex &index)
{
    const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
    if (!item.isNull()) {
        Q_EMIT itemActivated(item);
    }
}

void FolderView::onContextMenu(const QPoint &pos)
{
    auto *view = qobject_cast<QAbstractItemView *>(sender());
    if (!view) {
        return;
    }
    if (!view->indexAt(pos).isValid()) {
        clearSelection();
    }
    Q_EMIT contextMenuRequested(view->viewport()->mapToGlobal(pos), selectedItems());
}

KFileItem FolderView::itemAt(QAbstractItemView *view, const QPoint &viewportPos) const
{
    return m_model->itemForIndex(m_proxy->mapToSource(view->indexAt(viewportPos)));
}

bool FolderView::handleDrop(QAbstractItemView *view, QEvent *event)
{
    auto *dropEvent = static_cast<QDropEvent *>(event);
    if (m_flat) {
        // Items in a flat list come from many folders, so a drop has no single destination.
        dropEvent->ignore();
        return true;
    }
    const QMimeData *mime = dropEvent->mimeData();
    if (!mime || !mime->hasUrls()) {
        return false;
    }

    const KFileItem target = itemAt(view, dropEvent->position().toPoint());
    const bool onFolder = !target.isNull() && target.isDir();

    if (event->type() != QEvent::Drop) {
        // KIO::DropJob decides between copy, move and link; accept everything.
        dropEvent->setDropAction(Qt::CopyAction);
        dropEvent->accept();
        return true;
    }

    QUrl destination = onFolder ? target.mostLocalUrl() : url();
    if (!destination.isValid()) {
        return false;
    }
    if (!onFolder) {
        // Dropping items onto their own folder is a no-op, not a copy.
        const QList<QUrl> urls = mime->urls();
        const bool allLocal = std::all_of(urls.cbegin(), urls.cend(), [&destination](const QUrl &source) {
            return source.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == destination.adjusted(QUrl::StripTrailingSlash);
        });
        if (allLocal) {
            dropEvent->ignore();
            return true;
        }
    }

    FileOperations::drop(dropEvent, destination, window());
    dropEvent->acceptProposedAction();
    return true;
}

bool FolderView::eventFilter(QObject *watched, QEvent *event)
{
    auto *view = qobject_cast<QAbstractItemView *>(watched);
    if (!view) {
        view = qobject_cast<QAbstractItemView *>(watched->parent());
    }
    if (!view) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::Drop:
        if (watched == view->viewport() && handleDrop(view, event)) {
            return true;
        }
        break;
    case QEvent::FocusIn:
        Q_EMIT activated();
        break;
    case QEvent::MouseButtonPress:
        if (watched == view->viewport()) {
            Q_EMIT activated();
        }
        break;
    case QEvent::KeyPress: {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (watched != view) {
            break;
        }
        const QString text = keyEvent->text();
        if ((keyEvent->modifiers() & ~Qt::ShiftModifier) == Qt::NoModifier && !text.isEmpty() && text.at(0).isPrint() && !text.trimmed().isEmpty()) {
            Q_EMIT typeAheadRequested(text);
            return true;
        }
        if (keyEvent->modifiers() != Qt::NoModifier) {
            break;
        }
        if (view == m_details && keyEvent->key() == Qt::Key_Left) {
            Q_EMIT goUpRequested();
            return true;
        }
        if (view == m_details && keyEvent->key() == Qt::Key_Right) {
            onActivated(view->currentIndex());
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            clearSelection();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}
