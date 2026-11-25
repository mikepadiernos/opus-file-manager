// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>

class KConfigGroup;
class QJsonObject;

/**
 * Non-widget helpers for Yazi mode, the optional per-pane column layout that
 * shows a parent-folder column before the pane's folder view and an optional
 * preview column after it.
 */
namespace YaziMode
{
/// Reads a pane's mode from a session group; absent means off.
bool readSession(const KConfigGroup &group);
void writeSession(KConfigGroup &group, bool enabled);

/**
 * Reads whether a pane shows its preview column from a session group.
 * @returns 1 when shown, 0 when hidden and -1 when nothing was saved, as in
 *          sessions saved before the preview column existed
 */
int readSessionPreview(const KConfigGroup &group);
/// Writes a pane's preview visibility; -1 removes the entry.
void writeSessionPreview(KConfigGroup &group, int preview);

/// Writes a pane's mode into a workspace pane-session object.
void writeWorkspace(QJsonObject &object, bool enabled);
/// Reads a pane's mode from a workspace pane-session object into @p enabled.
/// An absent key, as in workspaces saved before Yazi mode existed, means off.
/// Returns false when the value is present but is not a boolean.
bool readWorkspace(const QJsonObject &object, bool &enabled);

/// Writes a pane's preview visibility as a boolean; -1 writes nothing.
void writeWorkspacePreview(QJsonObject &object, int preview);
/**
 * Reads a pane's preview visibility into @p preview: 1 shown, 0 hidden, or
 * -1 when absent, as in workspaces saved before the preview column existed.
 * @returns false when the value is present but is not a boolean
 */
bool readWorkspacePreview(const QJsonObject &object, int &preview);

/// The folder the parent column lists for @p current, or an empty URL when
/// @p current is invalid or has no parent, such as at a root.
QUrl parentFolder(const QUrl &current);

/// The narrowest current column that columnWidths() allows while it shows another column.
constexpr int MinimumCurrentWidth = 220;
/// The narrowest parent column; narrower panes hide it.
constexpr int MinimumParentWidth = 100;
/// The narrowest preview column; narrower panes hide it before the parent column.
constexpr int MinimumPreviewWidth = 160;
/// Without handles, the narrowest width that shows the parent and current columns.
constexpr int TwoColumnWidth = MinimumParentWidth + MinimumCurrentWidth;
/// Without handles, the narrowest width that shows all three columns.
constexpr int ThreeColumnWidth = TwoColumnWidth + MinimumPreviewWidth;

/// The relative widths of the parent, current and preview columns. Values below 1 count as 1.
struct Ratios {
    int parent = 1;
    int current = 3;
    int preview = 2;

    bool operator==(const Ratios &other) const = default;
};

/// Column widths in pixels; 0 means the column is hidden.
struct ColumnWidths {
    int parent = 0;
    int current = 0;
    int preview = 0;

    bool operator==(const ColumnWidths &other) const = default;
};

/**
 * Shares @p available pixels between the columns in proportion to @p ratios,
 * keeping each shown column at least as wide as its minimum. As the width
 * shrinks, the preview column is hidden first, below ThreeColumnWidth, and then
 * the parent column, below TwoColumnWidth, so the current column never gets
 * narrower than MinimumCurrentWidth unless it is the only column left.
 * @param available the width to share, including the handles between columns
 * @param preview whether the preview column is wanted at all
 * @param ratios the relative widths of the columns
 * @param handleWidth the width of each handle between two shown columns
 * @returns the widths; together with the handles they add up to @p available
 */
ColumnWidths columnWidths(int available, bool preview, const Ratios &ratios = {}, int handleWidth = 0);
}
