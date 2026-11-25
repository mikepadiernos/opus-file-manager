// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazicolumns.h"

#include "folderview.h"
#include "yazimode.h"

#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItem>
#include <KLocalizedString>

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QSplitter>
#include <QVBoxLayout>

namespace
{
constexpr int ParentColumn = 0;
constexpr int CurrentColumn = 1;

QUrl stripped(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash);
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
    m_previewLayout = new QVBoxLayout(m_previewSlot);
    m_previewLayout->setContentsMargins(0, 0, 0, 0);
    m_previewSlot->hide();

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("yaziColumnsSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_parentColumn);
    m_splitter->addWidget(m_previewSlot);

    m_indicator = new QLabel(i18nc("@info:status short name of the Yazi browsing mode", "Yazi"), this);
    m_indicator->setObjectName(QStringLiteral("yaziModeIndicator"));
    m_indicator->setAccessibleName(i18nc("@info:accessibility", "Yazi mode"));
    m_indicator->setAccessibleDescription(i18nc("@info:accessibility", "This pane shows its folder in columns with the parent folder"));
    m_indicator->setToolTip(i18nc("@info:tooltip", "Yazi mode is on in this pane"));
    m_indicator->setBackgroundRole(QPalette::Highlight);
    m_indicator->setForegroundRole(QPalette::HighlightedText);
    m_indicator->setAutoFillBackground(true);
    m_indicator->setContentsMargins(6, 0, 6, 0);

    m_statusRow = new QWidget(this);
    m_statusRow->setObjectName(QStringLiteral("yaziStatusRow"));
    m_statusLayout = new QHBoxLayout(m_statusRow);
    m_statusLayout->setContentsMargins(2, 2, 2, 2);
    m_statusLayout->setSpacing(6);
    m_statusLayout->addWidget(m_indicator);
    m_statusLayout->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter, 1);
    layout->addWidget(m_statusRow);
}

YaziColumns::~YaziColumns()
{
    m_lister->disconnect(this);
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
}

FolderView *YaziColumns::takeCurrentView()
{
    FolderView *view = m_view;
    m_view = nullptr;
    if (view) {
        view->setParent(parentWidget());
    }
    return view;
}

FolderView *YaziColumns::currentView() const
{
    return m_view;
}

void YaziColumns::setUrl(const QUrl &url)
{
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
    delete m_previewWidget;
    m_previewWidget = widget;
    if (m_previewWidget) {
        m_previewLayout->addWidget(m_previewWidget);
        m_previewWidget->show();
    }
    m_previewSlot->setVisible(m_previewWidget != nullptr);
}

QWidget *YaziColumns::previewWidget() const
{
    return m_previewWidget;
}

QSplitter *YaziColumns::splitter() const
{
    return m_splitter;
}

QLabel *YaziColumns::modeIndicator() const
{
    return m_indicator;
}

void YaziColumns::addStatusWidget(QWidget *widget, int stretch)
{
    m_statusLayout->insertWidget(m_statusLayout->count() - 1, widget, stretch);
}

bool YaziColumns::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_parentColumn && event->type() == QEvent::FocusIn)
        || (watched == m_parentColumn->viewport() && event->type() == QEvent::MouseButtonPress)) {
        Q_EMIT activated();
    }
    return QWidget::eventFilter(watched, event);
}

void YaziColumns::highlightCurrent()
{
    if (m_url.isEmpty()) {
        return;
    }
    const QModelIndex index = m_proxy->mapFromSource(m_model->indexForUrl(stripped(m_url)));
    if (!index.isValid() || index == m_parentColumn->currentIndex()) {
        return;
    }
    m_parentColumn->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
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
