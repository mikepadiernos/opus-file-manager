// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>

/// One service menu action or file item action plugin.
struct ServiceMenuEntry {
    /// The key KFileItemActions reads in kservicemenurc's [Show] group.
    QString key;
    QString text;
    /// The submenu the action appears in, or an empty string.
    QString submenu;
    QString iconName;
    /// Whether the entry shows when kservicemenurc has no value for it.
    bool shownByDefault = true;
};

/// The service menus KFileItemActions can add to context menus.
namespace ServiceMenus
{
/**
 * Every installed service menu action and file item action plugin, sorted
 * by text. Separators and hidden actions are left out.
 */
QList<ServiceMenuEntry> installed();

/// The KNewStuff configuration Dolphin installs to download service menus.
inline constexpr auto DownloadConfig = "servicemenu.knsrc";
/**
 * The path of DownloadConfig, or an empty string when it is not installed.
 * Plasma Opus reuses Dolphin's provider, so downloads, updates and removals
 * are shared with Dolphin and installed by its `servicemenuinstaller`.
 */
QString downloadConfigPath();

/// The config file shared with Dolphin that hides service menus.
inline constexpr auto ConfigFile = "kservicemenurc";
/// The group in ConfigFile whose entries map keys to shown or hidden.
inline constexpr auto ShowGroup = "Show";
}
