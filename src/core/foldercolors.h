// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QIcon>
#include <QString>
#include <QStringList>

class KFileItem;
class QUrl;

/**
 * Folder colours set in Folder Appearance and stored in a folder's Plasma
 * Opus metadata, and how they are shown on folder icons and names.
 */
namespace FolderColors
{
/// How folder colours are shown, matching the FolderColorStyle setting.
enum class Style {
    /// A coloured variant of the theme's folder icon, or a tinted icon.
    Icon,
    /// A coloured dot on the folder icon.
    Badge,
    /// Coloured folder names; icons are unchanged.
    Name,
    /// Folder colours are not shown.
    Hidden,
};

/// The folder colour style configured in Settings.
Style configured();

/**
 * The colour of a local folder: its own colour from Folder Appearance, or
 * else the colour of the first matching folder colour rule. Invalid when
 * neither applies. Results are cached for two seconds, since views ask for
 * them whenever rows are drawn.
 */
QColor color(const QString &localPath);
/// The colour of @p url when it is a local folder, otherwise invalid.
QColor color(const QUrl &url);
/// Forgets cached folder colours and reads the rules again, such as after a folder colour or the settings change.
void clearCache();

/**
 * The icon theme's coloured variant of a plain folder icon, such as
 * "folder-red", for the label colour nearest to @p color.
 * @returns an empty string for other icons, invalid colours or themes
 *          without the variant
 */
QString themedIconName(const QString &iconName, const QColor &color);

/**
 * Shows @p color on a folder icon in @p style.
 * @param icon the icon as the view would draw it, including overlays
 * @param iconName the item's icon name, used for themed variants
 * @param overlays the item's emblems; icons with emblems are tinted rather
 *        than replaced, so the emblems stay visible
 * @returns @p icon unchanged for an invalid colour or the Name and Hidden styles
 */
QIcon decorate(const QIcon &icon, const QString &iconName, const QStringList &overlays, const QColor &color, Style style);

/// A round dot of @p color, for places that show a colour without an icon.
QIcon dot(const QColor &color);

/**
 * Decorates the icon of a local folder item with its colour in the
 * configured style.
 * @returns @p icon unchanged for files, remote folders and folders without a colour
 */
QIcon decorate(const QIcon &icon, const KFileItem &item);

/**
 * The colour for the name of a local folder item when the configured style
 * colours names, made readable on @p background; otherwise invalid.
 */
QColor nameColor(const KFileItem &item, const QColor &background);
}
