// SPDX-License-Identifier: GPL-3.0-or-later

#include "kdeconnectshare.h"
#include "settings.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace KdeConnectShare
{
namespace
{
KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("KdeConnectShare"));
}
}

Selection classify(const QList<QUrl> &urls)
{
    Selection selection;
    QSet<QString> seen;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            selection.rejected.append({url, Reason::Remote});
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        if (!info.exists()) {
            selection.rejected.append({url, Reason::Missing});
        } else if (info.isDir()) {
            selection.rejected.append({url, Reason::Folder});
        } else if (!info.isFile()) {
            selection.rejected.append({url, Reason::NotRegularFile});
        } else {
            const QString path = info.absoluteFilePath();
            if (!seen.contains(path)) {
                seen.insert(path);
                selection.files.append(QUrl::fromLocalFile(path));
            }
        }
    }
    return selection;
}

QList<Device> eligible(const QList<Device> &devices)
{
    QList<Device> result;
    std::copy_if(devices.cbegin(), devices.cend(), std::back_inserter(result), [](const Device &device) {
        return unavailableReason(device).isEmpty();
    });
    std::stable_sort(result.begin(), result.end(), [](const Device &a, const Device &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return result;
}

QString unavailableReason(const Device &device)
{
    if (!device.paired) {
        return i18nc("@info KDE Connect device state", "Not paired");
    }
    if (!device.reachable) {
        return i18nc("@info KDE Connect device state", "Offline");
    }
    if (!device.canShare) {
        return i18nc("@info KDE Connect device state", "File sharing is turned off");
    }
    return {};
}

QString reasonText(Reason reason)
{
    switch (reason) {
    case Reason::Folder:
        return i18nc("@info why an item cannot be sent", "Folders cannot be sent");
    case Reason::Remote:
        return i18nc("@info why an item cannot be sent", "Only local files can be sent");
    case Reason::Missing:
        return i18nc("@info why an item cannot be sent", "The item no longer exists");
    case Reason::NotRegularFile:
        return i18nc("@info why an item cannot be sent", "Not a regular file");
    }
    return {};
}

QString preferredDevice(const QList<Device> &devices, const QString &previous)
{
    const auto listed = [&devices](const QString &id) {
        return std::any_of(devices.cbegin(), devices.cend(), [&id](const Device &device) {
            return device.id == id;
        });
    };
    const QList<Device> candidates = eligible(devices);
    const bool previousEligible = std::any_of(candidates.cbegin(), candidates.cend(), [&previous](const Device &device) {
        return device.id == previous;
    });
    if (!previous.isEmpty() && previousEligible) {
        return previous;
    }
    if (!candidates.isEmpty()) {
        return candidates.first().id;
    }
    if (!previous.isEmpty() && listed(previous)) {
        return previous;
    }
    return devices.isEmpty() ? QString() : devices.first().id;
}

QString rememberedDevice()
{
    return Settings::kdeConnectRememberDevice() ? stateGroup().readEntry("LastDevice", QString()) : QString();
}

void rememberDevice(const QString &deviceId)
{
    if (!Settings::kdeConnectRememberDevice()) {
        forgetDevice();
        return;
    }
    KConfigGroup state = stateGroup();
    state.writeEntry("LastDevice", deviceId);
    state.sync();
}

void forgetDevice()
{
    KConfigGroup state = stateGroup();
    if (state.hasKey("LastDevice")) {
        state.deleteEntry("LastDevice");
        state.sync();
    }
}
}
