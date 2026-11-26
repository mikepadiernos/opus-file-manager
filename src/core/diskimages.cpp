// SPDX-License-Identifier: GPL-3.0-or-later

#include "diskimages.h"

#include <KPluginMetaData>

#include <QMimeDatabase>

namespace DiskImages
{
QStringList mimeTypes()
{
    // Matches the list in dolphin-plugins' mountiso/mountisoaction.cpp.
    return {QStringLiteral("application/vnd.efi.iso"),
            QStringLiteral("application/vnd.efi.img"),
            QStringLiteral("application/x-cd-image"),
            QStringLiteral("application/x-raw-disk-image")};
}

bool isDiskImage(const QString &mimeType)
{
    if (mimeTypes().contains(mimeType)) {
        return true;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    return type.isValid() && mimeTypes().contains(type.name());
}

QString pluginId()
{
    return QStringLiteral("mountisoaction");
}

QString pluginNamespace()
{
    return QStringLiteral("kf6/kfileitemaction");
}

bool isPluginInstalled()
{
    return KPluginMetaData::findPluginById(pluginNamespace(), pluginId()).isValid();
}
}
