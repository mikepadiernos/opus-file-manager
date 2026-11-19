// SPDX-License-Identifier: GPL-3.0-or-later

#include "detailsview.h"

#include <KDirModel>
#include <KFileItemDelegate>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QStyleOptionViewItem>

DetailsView::DetailsView(QWidget *parent)
    : QTreeView(parent)
    , m_band(new QRubberBand(QRubberBand::Rectangle, viewport()))
{
}

void DetailsView::setDragFromAnywhere(bool enabled)
{
    m_dragFromAnywhere = enabled;
}

bool DetailsView::isOnEmptyPartOfRow(const QPoint &pos) const
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        return false;
    }
    const QModelIndex name = index.siblingAtColumn(KDirModel::Name);
    auto *delegate = qobject_cast<KFileItemDelegate *>(itemDelegateForIndex(name));
    if (!delegate) {
        return false;
    }
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(name);
    return !delegate->shape(option, name).contains(pos);
}

void DetailsView::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (m_dragFromAnywhere || event->button() != Qt::LeftButton || !isOnEmptyPartOfRow(pos)) {
        QTreeView::mousePressEvent(event);
        return;
    }
    m_bandPending = true;
    m_bandOrigin = pos;
    m_bandPressedIndex = indexAt(pos);
    m_bandKeepsSelection = event->modifiers() & Qt::ControlModifier;
    setFocus(Qt::MouseFocusReason);
}

void DetailsView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_bandPending) {
        QTreeView::mouseMoveEvent(event);
        return;
    }
    const QPoint pos = event->position().toPoint();
    if (!m_band->isVisible() && (pos - m_bandOrigin).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }
    m_band->setGeometry(QRect(m_bandOrigin, pos).normalized());
    m_band->show();
    selectRowsInBand(pos, m_bandKeepsSelection);
}

void DetailsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_bandPending) {
        QTreeView::mouseReleaseEvent(event);
        return;
    }
    if (!m_band->isVisible() && m_bandPressedIndex.isValid()) {
        selectionModel()->setCurrentIndex(m_bandPressedIndex,
                                          (m_bandKeepsSelection ? QItemSelectionModel::Toggle : QItemSelectionModel::ClearAndSelect)
                                              | QItemSelectionModel::Rows);
    }
    m_band->hide();
    m_bandPending = false;
}

void DetailsView::selectRowsInBand(const QPoint &pos, bool keepSelection)
{
    const QRect band = QRect(m_bandOrigin, pos).normalized();
    QItemSelection rows;
    for (QModelIndex index = indexAt(QPoint(0, band.top())); index.isValid(); index = indexBelow(index)) {
        const QRect row = visualRect(index);
        if (row.top() > band.bottom()) {
            break;
        }
        if (row.bottom() >= band.top()) {
            rows.select(index, index);
        }
    }
    if (rows.isEmpty() && !keepSelection) {
        selectionModel()->clearSelection();
        return;
    }
    selectionModel()->select(rows, (keepSelection ? QItemSelectionModel::Select : QItemSelectionModel::ClearAndSelect) | QItemSelectionModel::Rows);
}
