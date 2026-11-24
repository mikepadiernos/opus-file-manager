// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QList>

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

enum class Coloring {
    Neutral,
    Single,
    Depth,
    Folder,
};

struct Colors {
    Coloring coloring = Coloring::Neutral;
    QColor single;
    /// Colours by nesting depth, repeating after the last one.
    QList<QColor> palette;
};

/// The tree line style configured in Settings.
Style configured();
/// The tree line colours configured in Settings.
Colors configuredColors();

/**
 * Draws the branch area of a row, as QTreeView::drawBranches() does.
 * @return false when the widget style should draw the branches itself
 */
bool drawBranches(const QTreeView *view, QPainter *painter, const QRect &rect, const QModelIndex &index);
/// Draws branches with an explicit style and colours, for previews and tests.
bool drawBranches(const QTreeView *view, QPainter *painter, const QRect &rect, const QModelIndex &index, Style style, const Colors &colors = {});

/**
 * The colour of a local folder from its Plasma Opus metadata, or an invalid
 * colour. Results are cached briefly, since lines are drawn for every row.
 */
QColor folderColor(const QModelIndex &folder);
/// Forgets cached folder colours, such as after a folder colour changes.
void clearFolderColorCache();
}
