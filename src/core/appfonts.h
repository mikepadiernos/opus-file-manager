// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QString>

/**
 * App-local font overrides for file lists, the folder tree, tabs and the
 * status bar. An empty setting follows Plasma's fonts.
 */
namespace AppFonts
{
/**
 * The font stored in a font setting.
 * @returns a default QFont, which widgets resolve entirely from their parent
 *          and so follows Plasma, for an empty or unreadable setting
 */
QFont fromSetting(const QString &value);

/**
 * The stored form of @p font. Pixel sizes are converted to point sizes, so
 * overrides scale with the display like Plasma's fonts.
 */
QString toSetting(const QFont &font);

/// Whether @p font overrides any property of its parent's font.
bool isOverride(const QFont &font);
}
