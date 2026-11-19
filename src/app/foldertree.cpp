// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldertree.h"

#include "contentheight.h"
#include "ignorelist.h"

#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDir>
#include <QHeaderView>
#include <QMouseEvent>
#include <QTreeView>
#include <QVBoxLayout>

namespace
{
QUrl normalized(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

bool isAtOrBelow(const QUrl &url, const QUrl &root)
{
    return normalized(url) == normalized(root) || root.isParentOf(url);
}

/// Sorted folders below one root, without ignored names; the root item is titled Home or Root.
class RootProxyModel : public KDirSortFilterProxyModel
{
public:
    RootProxyModel(const QUrl &root, QObject *parent)
        : KDirSortFilterProxyModel(parent)
        , m_ignoreList(IgnoreList::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files"))))
    {
        if (normalized(root) == normalized(QUrl::fromLocalFile(QDir::homePath()))) {
            m_rootTitle = i18nc("@item:intable the user's home folder", "Home");
        } else if (normalized(root) == QUrl::fromLocalFile(QStringLiteral("/"))) {
            m_rootTitle = i18nc("@item:intable the filesystem root", "Root");
        }
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == Qt::DisplayRole && !index.parent().isValid() && !m_rootTitle.isEmpty()) {
            return m_rootTitle;
        }
        return KDirSortFilterProxyModel::data(index, role);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        return KDirSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent)
            && !m_ignoreList.ignores(index.data(KDirModel::FileItemRole).value<KFileItem>().name());
    }

private:
    QString m_rootTitle;
    IgnoreList m_ignoreList;
};

QUrl urlAt(const QModelIndex &index)
{
    return index.data(KDirModel::FileItemRole).value<KFileItem>().url();
}
}

FolderTree::FolderTree(const QList<QUrl> &roots, QWidget *parent)
    : QWidget(parent)
    , m_roots(roots)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    for (const QUrl &root : roots) {
        auto *model = new KDirModel(this);
        model->dirLister()->setDirOnlyMode(true);
        model->dirLister()->setAutoErrorHandlingEnabled(false);
        auto *proxy = new RootProxyModel(root, this);
        proxy->setSourceModel(model);
        proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

        auto *tree = new QTreeView(this);
        tree->setModel(proxy);
        tree->setHeaderHidden(true);
        for (int column = 1; column < KDirModel::ColumnCount; ++column) {
            tree->setColumnHidden(column, true);
        }
        tree->setSortingEnabled(true);
        tree->sortByColumn(KDirModel::Name, Qt::AscendingOrder);
        tree->setUniformRowHeights(true);
        tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        fitHeightToContents(tree);
        tree->viewport()->installEventFilter(this);
        layout->addWidget(tree);

        connect(tree, &QTreeView::clicked, this, [this](const QModelIndex &index) {
            Q_EMIT urlActivated(urlAt(index));
        });
        connect(model, &KDirModel::expand, tree, [tree, proxy](const QModelIndex &index) {
            tree->expand(proxy->mapFromSource(index));
        });
        model->openUrl(root, KDirModel::ShowRoot);
        m_trees.append(tree);
    }
    layout->addStretch();
}

QList<QUrl> FolderTree::roots() const
{
    return m_roots;
}

QList<QUrl> FolderTree::expandedUrls() const
{
    QList<QUrl> urls;
    for (const QTreeView *tree : m_trees) {
        QList<QModelIndex> pending{QModelIndex()};
        while (!pending.isEmpty()) {
            const QModelIndex parent = pending.takeFirst();
            for (int row = 0; row < tree->model()->rowCount(parent); ++row) {
                const QModelIndex child = tree->model()->index(row, 0, parent);
                if (tree->isExpanded(child)) {
                    urls.append(urlAt(child));
                    pending.append(child);
                }
            }
        }
    }
    return urls;
}

void FolderTree::setExpandedUrls(const QList<QUrl> &urls)
{
    for (int i = 0; i < m_trees.size(); ++i) {
        auto *proxy = static_cast<KDirSortFilterProxyModel *>(m_trees.at(i)->model());
        auto *model = static_cast<KDirModel *>(proxy->sourceModel());
        for (const QUrl &url : urls) {
            if (isAtOrBelow(url, m_roots.at(i))) {
                model->expandToUrl(url);
            }
        }
    }
}

bool FolderTree::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(event)->button() == Qt::MiddleButton) {
        for (const QTreeView *tree : std::as_const(m_trees)) {
            if (watched == tree->viewport()) {
                const QModelIndex index = tree->indexAt(static_cast<QMouseEvent *>(event)->position().toPoint());
                if (index.isValid()) {
                    Q_EMIT tabRequested(urlAt(index));
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
