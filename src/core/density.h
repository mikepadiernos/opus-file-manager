// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/// How tightly items and bars are packed.
namespace Density
{
/// Item density for one view mode; the order matches the settings choices.
enum class Items {
    Normal,
    Compact,
    VeryCompact,
    Custom,
};

/// Bar density for toolbars, location bars and the status bar.
enum class Bars {
    Normal,
    Compact,
    VeryCompact,
};

/// Space above and below each item for Normal, as in Dolphin.
inline constexpr int NormalItemSpace = 2;
inline constexpr int MaxCustomItemSpace = 10;

/**
 * Pixels of space above and below each item.
 * @param density the chosen density
 * @param customSpace the space used for Custom; clamped to 0..MaxCustomItemSpace
 * @returns 2 for Normal, 1 for Compact, 0 for Very compact
 */
int itemSpace(Items density, int customSpace);

/**
 * Margin and spacing around the contents of a toolbar or the status bar.
 * @param density the chosen density
 * @returns -1 for Normal, meaning the style's own margins; 2 for Compact; 0 for Very compact
 */
int barMargin(Bars density);
}
