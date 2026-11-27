// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

/// Disk images that Dolphin's Mount ISO plugin, from dolphin-plugins, mounts
/// as loop devices through UDisks2.
namespace DiskImages
{
/// The MIME types the plugin offers Mount and Unmount for.
QStringList mimeTypes();

/// Whether @p mimeType, or the type it is an alias of, is a mountable disk image.
bool isDiskImage(const QString &mimeType);

/// The Mount ISO plugin's ID, as listed in KFileItemActions' exclude list.
QString pluginId();

/// The plugin namespace the Mount ISO plugin is installed in.
QString pluginNamespace();

/// Whether Dolphin's Mount ISO plugin is installed.
bool isPluginInstalled();
}
