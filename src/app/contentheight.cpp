// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentheight.h"

#include <QAbstractItemView>
#include <QListView>
#include <QMetaObject>
#include <QScrollBar>
#include <QTreeView>

namespace
{
/**
 * Height of all visible rows. List views sum their rows, since views such
 * as KFilePlacesView override sizeHint() with a fixed height.
 */
int contentHeight(QAbstractItemView *view)
{
    const auto *list = qobject_cast<QListView *>(view);
    if (!list) {
        return view->sizeHint().height();
    }
    const QModelIndex root = list->rootIndex();
    int height = list->spacing();
    for (int row = 0; row < list->model()->rowCount(root); ++row) {
        if (!list->isRowHidden(row)) {
            height += list->sizeHintForIndex(list->model()->index(row, 0, root)).height() + list->spacing();
        }
    }
    return height;
}
}

void updateContentHeight(QAbstractItemView *view)
{
    QMetaObject::invokeMethod(
        view,
        [view] {
            view->setFixedHeight(contentHeight(view));
        },
        Qt::QueuedConnection);
}

void fitHeightToContents(QAbstractItemView *view)
{
    view->setFrameShape(QFrame::NoFrame);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    const auto refit = [view] {
        updateContentHeight(view);
    };
    QAbstractItemModel *model = view->model();
    QObject::connect(model, &QAbstractItemModel::rowsInserted, view, refit);
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, view, refit);
    QObject::connect(model, &QAbstractItemModel::modelReset, view, refit);
    QObject::connect(model, &QAbstractItemModel::layoutChanged, view, refit);
    QObject::connect(model, &QAbstractItemModel::dataChanged, view, refit);
    // Also catches rows the view shows or hides itself, which the model never reports.
    QObject::connect(view->verticalScrollBar(), &QScrollBar::rangeChanged, view, refit);
    if (auto *tree = qobject_cast<QTreeView *>(view)) {
        QObject::connect(tree, &QTreeView::expanded, view, refit);
        QObject::connect(tree, &QTreeView::collapsed, view, refit);
    }
    refit();
}
