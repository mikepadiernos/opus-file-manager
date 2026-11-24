// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>

class QModelIndex;
class QPainter;
class QRect;
class QTreeView;

/**
 * Branch lines for the tree pane and inline folder expansion, drawn by
 * Plasma Opus so that solid and dotted lines look the same in every widget
 * style. Expand indicators are still drawn by the widget style.
 */
namespace TreeLines
{
enum class Style {
    FollowStyle,
    Hidden,
    Solid,
    Dotted,
};

/// The tree line style configured in Settings.
Style configured();

/**
 * Draws the branch area of a row, as QTreeView::drawBranches() does.
 * @return false when the widget style should draw the branches itself
 */
bool drawBranches(const QTreeView *view, QPainter *painter, const QRect &rect, const QModelIndex &index);
/// Draws branches with an explicit style, for previews and tests.
bool drawBranches(const QTreeView *view, QPainter *painter, const QRect &rect, const QModelIndex &index, Style style);
}
