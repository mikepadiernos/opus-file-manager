// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIcon>
#include <QStringList>

/**
 * Icons that draw the theme's "-symbolic" variant at small sizes. At
 * MaxSize and below the variant is used when the theme has one; larger
 * sizes, and themes without the variant, draw the regular icon.
 */
namespace SymbolicIcon
{
/// Largest edge, in logical pixels, drawn from the symbolic variant.
inline constexpr int MaxSize = 24;

/**
 * Wraps a theme icon so small sizes use its symbolic variant.
 * @param icon the icon to wrap; larger sizes keep drawing it unchanged
 * @returns the wrapped icon, or @p icon itself when it has no theme name,
 *          is already symbolic, or the theme has no symbolic variant
 */
QIcon wrap(const QIcon &icon);

/**
 * Wraps an item icon and preserves its emblem overlays on symbolic variants.
 * Unnamed icons that differ from the themed icon are left intact as previews.
 */
QIcon wrap(const QIcon &icon, const QString &iconName, const QStringList &overlays);

/**
 * Looks up a theme icon and wraps it with wrap().
 * @param name the regular icon name, without the "-symbolic" suffix
 * @returns the wrapped theme icon
 */
QIcon fromTheme(const QString &name);
}
