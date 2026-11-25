// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QString>
#include <QVariantMap>

#include <type_traits>

/**
 * The appearance options in effect: the saved global settings with an
 * in-memory layer on top, such as a theme profile and overrides attached to
 * the active workspace. The layer is never saved, so the settings dialog
 * keeps editing the global values.
 *
 * Read appearance options through these accessors rather than Settings so
 * that attached appearance applies everywhere.
 */
namespace EffectiveAppearance
{
/// Replaces the layer of setting values, by entry name, in the types the settings use.
void setLayer(const QVariantMap &values);
const QVariantMap &layer();

template<typename T>
T value(const QString &entry, T global)
{
    const QVariantMap &values = layer();
    if (values.isEmpty()) {
        return global;
    }
    const auto it = values.constFind(entry);
    if (it == values.cend()) {
        return global;
    }
    if constexpr (std::is_enum_v<T>) {
        return static_cast<T>(it->toInt());
    } else {
        return it->template value<T>();
    }
}

inline auto defaultViewMode()
{
    return value(QStringLiteral("DefaultViewMode"), Settings::defaultViewMode());
}

inline auto colorScheme()
{
    return value(QStringLiteral("ColorScheme"), Settings::colorScheme());
}

inline auto accentColor()
{
    return value(QStringLiteral("AccentColor"), Settings::accentColor());
}

inline auto customColors()
{
    return value(QStringLiteral("CustomColors"), Settings::customColors());
}

inline auto paneTint()
{
    return value(QStringLiteral("PaneTint"), Settings::paneTint());
}

inline auto paneTintColor()
{
    return value(QStringLiteral("PaneTintColor"), Settings::paneTintColor());
}

inline auto paneTintStrength()
{
    return value(QStringLiteral("PaneTintStrength"), Settings::paneTintStrength());
}

inline auto backgroundBlur()
{
    return value(QStringLiteral("BackgroundBlur"), Settings::backgroundBlur());
}

inline auto windowOpacity()
{
    return value(QStringLiteral("WindowOpacity"), Settings::windowOpacity());
}

inline auto paneOpacity()
{
    return value(QStringLiteral("PaneOpacity"), Settings::paneOpacity());
}

inline auto panelOpacity()
{
    return value(QStringLiteral("PanelOpacity"), Settings::panelOpacity());
}

inline auto cornerRadius()
{
    return value(QStringLiteral("CornerRadius"), Settings::cornerRadius());
}

inline auto customCornerRadius()
{
    return value(QStringLiteral("CustomCornerRadius"), Settings::customCornerRadius());
}

inline auto dividers()
{
    return value(QStringLiteral("Dividers"), Settings::dividers());
}

inline auto revealDividersOnHover()
{
    return value(QStringLiteral("RevealDividersOnHover"), Settings::revealDividersOnHover());
}

inline auto treeLines()
{
    return value(QStringLiteral("TreeLines"), Settings::treeLines());
}

inline auto treeLineColors()
{
    return value(QStringLiteral("TreeLineColors"), Settings::treeLineColors());
}

inline auto treeLineColor()
{
    return value(QStringLiteral("TreeLineColor"), Settings::treeLineColor());
}

inline auto treeLinePalette()
{
    return value(QStringLiteral("TreeLinePalette"), Settings::treeLinePalette());
}

inline auto stripeDetailsRows()
{
    return value(QStringLiteral("StripeDetailsRows"), Settings::stripeDetailsRows());
}

inline auto hoverHighlight()
{
    return value(QStringLiteral("HoverHighlight"), Settings::hoverHighlight());
}

inline auto selectionStyle()
{
    return value(QStringLiteral("SelectionStyle"), Settings::selectionStyle());
}

inline auto folderColorStyle()
{
    return value(QStringLiteral("FolderColorStyle"), Settings::folderColorStyle());
}

inline auto folderColorRules()
{
    return value(QStringLiteral("FolderColorRules"), Settings::folderColorRules());
}

inline auto fileListFont()
{
    return value(QStringLiteral("FileListFont"), Settings::fileListFont());
}

inline auto folderTreeFont()
{
    return value(QStringLiteral("FolderTreeFont"), Settings::folderTreeFont());
}

inline auto tabFont()
{
    return value(QStringLiteral("TabFont"), Settings::tabFont());
}

inline auto statusBarFont()
{
    return value(QStringLiteral("StatusBarFont"), Settings::statusBarFont());
}

inline auto spacingScale()
{
    return value(QStringLiteral("SpacingScale"), Settings::spacingScale());
}

inline auto tabShape()
{
    return value(QStringLiteral("TabShape"), Settings::tabShape());
}

inline auto tabIndicator()
{
    return value(QStringLiteral("TabIndicator"), Settings::tabIndicator());
}

inline auto tabColorStrength()
{
    return value(QStringLiteral("TabColorStrength"), Settings::tabColorStrength());
}

inline auto statusBarStyle()
{
    return value(QStringLiteral("StatusBarStyle"), Settings::statusBarStyle());
}

inline auto animations()
{
    return value(QStringLiteral("Animations"), Settings::animations());
}

inline auto useCustomStyleSheet()
{
    return value(QStringLiteral("UseCustomStyleSheet"), Settings::useCustomStyleSheet());
}

inline auto customStyleSheet()
{
    return value(QStringLiteral("CustomStyleSheet"), Settings::customStyleSheet());
}
}
