// SPDX-License-Identifier: GPL-3.0-or-later

#include "treelines.h"

#include "settings.h"

#include <QCursor>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QTreeView>

#include <algorithm>

namespace
{
QColor mixed(const QColor &first, const QColor &second, qreal secondAmount)
{
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * secondAmount,
                            first.greenF() + (second.greenF() - first.greenF()) * secondAmount,
                            first.blueF() + (second.blueF() - first.blueF()) * secondAmount);
}

bool hasMoreSiblings(const QModelIndex &index)
{
    return index.isValid() && index.model()->rowCount(index.parent()) - 1 > index.row();
}

/// Draws a one pixel horizontal or vertical line. Dots follow the painter's
/// coordinates, so they line up across rows.
void drawLine(QPainter *painter, const QPoint &from, const QPoint &to, const QColor &color, bool dotted)
{
    if (to.x() < from.x() || to.y() < from.y()) {
        return;
    }
    const QRect line(from, to);
    if (!dotted) {
        painter->fillRect(line, color);
        return;
    }
    const bool vertical = line.width() == 1;
    const int start = vertical ? line.top() : line.left();
    const int end = vertical ? line.bottom() : line.right();
    for (int position = start + (start & 1); position <= end; position += 2) {
        painter->fillRect(vertical ? QRect(line.left(), position, 1, 1) : QRect(position, line.top(), 1, 1), color);
    }
}
}

TreeLines::Style TreeLines::configured()
{
    switch (Settings::treeLines()) {
    case Settings::EnumTreeLines::Hidden:
        return Style::Hidden;
    case Settings::EnumTreeLines::Solid:
        return Style::Solid;
    case Settings::EnumTreeLines::Dotted:
        return Style::Dotted;
    default:
        return Style::FollowStyle;
    }
}

bool TreeLines::drawBranches(const QTreeView *view, QPainter *painter, const QRect &rect, const QModelIndex &index)
{
    return drawBranches(view, painter, rect, index, configured());
}

bool TreeLines::drawBranches(const QTreeView *view, QPainter *painter, const QRect &rect, const QModelIndex &index, Style style)
{
    if (style == Style::FollowStyle) {
        return false;
    }
    const int indent = view->indentation();
    if (indent <= 0 || !index.isValid()) {
        return true;
    }
    const QModelIndex item = index.siblingAtColumn(0);
    const QModelIndex root = view->rootIndex();
    const int outer = view->rootIsDecorated() ? 0 : 1;
    int level = 0;
    for (QModelIndex parent = item.parent(); parent.isValid() && parent != root; parent = parent.parent()) {
        ++level;
    }
    if (level < outer) {
        return true;
    }

    const bool reverse = view->isRightToLeft();
    const QPalette palette = view->palette();
    const QPalette::ColorGroup group = !view->isEnabled() ? QPalette::Disabled : (view->isActiveWindow() ? QPalette::Active : QPalette::Inactive);
    const bool selected = view->selectionModel() && view->selectionModel()->isSelected(index);
    const bool decorationSelected = view->style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, nullptr, view);
    QColor color = selected && decorationSelected ? mixed(palette.color(group, QPalette::Highlight), palette.color(group, QPalette::HighlightedText), 0.6)
                                                  : mixed(palette.color(group, QPalette::Base), palette.color(group, QPalette::Text), 0.45);
    const bool dotted = style == Style::Dotted;
    const bool lines = style != Style::Hidden;

    // Start with the item's own branch, then go out level by level.
    QRect cell(reverse ? rect.left() : rect.right() + 1 - indent, rect.top(), indent, rect.height());
    const int centerX = cell.left() + cell.width() / 2;
    const int centerY = cell.top() + cell.height() / 2;
    const bool children = view->model()->hasChildren(item);
    if (lines) {
        // Leave room for the expand indicator.
        const int gap = children ? std::min(cell.width(), cell.height()) / 3 : 0;
        const bool firstRoot = item.row() == 0 && item.parent() == root;
        if (!firstRoot) {
            drawLine(painter, QPoint(centerX, cell.top()), QPoint(centerX, centerY - gap - 1), color, dotted);
        }
        if (hasMoreSiblings(item)) {
            drawLine(painter, QPoint(centerX, centerY + gap + 1), QPoint(centerX, cell.bottom()), color, dotted);
        }
        if (reverse) {
            drawLine(painter, QPoint(cell.left(), centerY), QPoint(centerX - gap - 1, centerY), color, dotted);
        } else {
            drawLine(painter, QPoint(centerX + gap + 1, centerY), QPoint(cell.right(), centerY), color, dotted);
        }
    }
    if (children) {
        QStyleOptionViewItem option;
        option.initFrom(view);
        option.rect = cell;
        option.state = (view->isEnabled() ? QStyle::State_Enabled : QStyle::State_None) | (view->hasFocus() ? QStyle::State_Active : QStyle::State_None)
            | QStyle::State_Children;
        option.state.setFlag(QStyle::State_Open, view->isExpanded(item));
        option.state.setFlag(QStyle::State_Selected, selected);
        option.state.setFlag(QStyle::State_MouseOver, view->viewport()->underMouse() && cell.contains(view->viewport()->mapFromGlobal(QCursor::pos())));
        view->style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter, view);
    }
    if (!lines) {
        return true;
    }
    QModelIndex current = item.parent();
    for (--level; level >= outer; --level) {
        cell.moveLeft(reverse ? cell.left() + indent : cell.left() - indent);
        if (hasMoreSiblings(current)) {
            const int x = cell.left() + cell.width() / 2;
            drawLine(painter, QPoint(x, cell.top()), QPoint(x, cell.bottom()), color, dotted);
        }
        current = current.parent();
    }
    return true;
}
