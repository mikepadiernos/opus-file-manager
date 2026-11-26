// SPDX-License-Identifier: GPL-3.0-or-later

#include "diskimagebackend.h"
#include "diskimages.h"

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <Solid/Device>
#include <Solid/GenericInterface>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <QAction>
#include <QFileInfo>
#include <QUrl>

#include <memory>

bool SystemDiskImageBackend::pluginInstalled() const
{
    return DiskImages::isPluginInstalled();
}

bool SystemDiskImageBackend::runPluginAction(const QString &file, QWidget *parent)
{
    const KPluginMetaData metaData = KPluginMetaData::findPluginById(DiskImages::pluginNamespace(), DiskImages::pluginId());
    if (!metaData.isValid()) {
        return false;
    }
    const auto result = KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(metaData);
    if (!result) {
        return false;
    }
    const std::unique_ptr<KAbstractFileItemActionPlugin> plugin(result.plugin);
    const KFileItem item(QUrl::fromLocalFile(file));
    const QList<QAction *> actions = plugin->actions(KFileItemListProperties(KFileItemList{item}), parent);
    if (actions.isEmpty()) {
        return false;
    }
    // The plugin offers Mount or Unmount, whichever applies, and blocks
    // until UDisks2 has finished.
    actions.constFirst()->trigger();
    qDeleteAll(actions);
    return true;
}

DiskImageBackend::Status SystemDiskImageBackend::status(const QString &file) const
{
    const QString canonical = QFileInfo(file).canonicalFilePath();
    const QList<Solid::Device> volumes = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const Solid::Device &device : volumes) {
        const auto *generic = device.as<Solid::GenericInterface>();
        if (!generic) {
            continue;
        }
        const QString backingFile = generic->property(QStringLiteral("BackingFile")).toString();
        if (backingFile.isEmpty() || (backingFile != file && backingFile != canonical)) {
            continue;
        }
        Status status{State::Attached, {}};
        const QString uuid = generic->property(QStringLiteral("IdUUID")).toString().toLower();
        const QList<Solid::Device> accessible = uuid.isEmpty()
            ? QList<Solid::Device>{device}
            : Solid::Device::listFromQuery(QStringLiteral("[ StorageVolume.uuid == '%1' AND IS StorageAccess ]").arg(uuid));
        for (const Solid::Device &candidate : accessible) {
            const auto *access = candidate.as<Solid::StorageAccess>();
            if (access && access->isAccessible()) {
                status = {State::Mounted, access->filePath()};
                break;
            }
        }
        return status;
    }
    return {};
}
