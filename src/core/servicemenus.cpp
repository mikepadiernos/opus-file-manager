// SPDX-License-Identifier: GPL-3.0-or-later

#include "servicemenus.h"

#include <KConfigGroup>
#include <KDesktopFile>
#include <KFileUtils>
#include <KPluginMetaData>
#include <KService>

#include <QStandardPaths>

#include <algorithm>

QList<ServiceMenuEntry> ServiceMenus::installed()
{
    QList<ServiceMenuEntry> entries;
    QStringList keys;

    const QStringList folders =
        QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kio/servicemenus"), QStandardPaths::LocateDirectory);
    const QStringList files = KFileUtils::findAllUniqueFiles(folders, {QStringLiteral("*.desktop")});
    for (const QString &file : files) {
        const QString submenu = KDesktopFile(file).desktopGroup().readEntry("X-KDE-Submenu");
        const QList<KServiceAction> actions = KService(file).actions();
        for (const KServiceAction &action : actions) {
            if (action.noDisplay() || action.isSeparator() || keys.contains(action.name())) {
                continue;
            }
            keys.append(action.name());
            entries.append({action.name(), action.text(), submenu, action.icon(), true});
        }
    }

    const QList<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfileitemaction"));
    for (const KPluginMetaData &plugin : plugins) {
        if (keys.contains(plugin.pluginId())) {
            continue;
        }
        keys.append(plugin.pluginId());
        // KFileItemActions keeps this plugin hidden until the user shows it.
        const bool shown = plugin.pluginId() != QLatin1String("hidefileitemaction");
        entries.append({plugin.pluginId(), plugin.name(), QString(), plugin.iconName(), shown});
    }

    std::sort(entries.begin(), entries.end(), [](const ServiceMenuEntry &a, const ServiceMenuEntry &b) {
        return QString::localeAwareCompare(a.text, b.text) < 0;
    });
    return entries;
}
