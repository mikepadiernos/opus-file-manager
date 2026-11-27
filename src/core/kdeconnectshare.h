// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

/// Decides what KDE Connect can send and to which devices. Plasma Opus only
/// asks KDE Connect's share plugin to send local regular files to a paired,
/// reachable device; it never pairs devices or changes KDE Connect settings.
namespace KdeConnectShare
{
/// A device as reported by the KDE Connect daemon.
struct Device {
    QString id;
    QString name;
    bool reachable = false;
    bool paired = false;
    /// Whether the device has the share plugin loaded.
    bool canShare = false;

    bool operator==(const Device &other) const = default;
};

/// Why a selected item cannot be sent.
enum class Reason {
    Folder,
    Remote,
    Missing,
    NotRegularFile,
};

struct Rejected {
    QUrl url;
    Reason reason;
};

/// The selection split into files KDE Connect can send and items it cannot.
struct Selection {
    QList<QUrl> files;
    QList<Rejected> rejected;
};

/// Splits @p urls into local regular files, following symbolic links, and
/// unsupported items. Folders, remote KIO locations and missing items are
/// rejected. Duplicate files are sent once.
Selection classify(const QList<QUrl> &urls);

/// The paired, reachable devices with the share plugin, sorted by name.
QList<Device> eligible(const QList<Device> &devices);

/// Why a device that is not eligible cannot receive files, or an empty string.
QString unavailableReason(const Device &device);

/// A short, localized description of why @p rejected cannot be sent.
QString reasonText(Reason reason);

/// The device to choose among @p devices: @p previous when it can receive
/// files, otherwise the first device that can, otherwise @p previous when it
/// is listed, otherwise the first device. Empty when none is listed.
QString preferredDevice(const QList<Device> &devices, const QString &previous);

/// The device the files were last sent to, or an empty string when none is
/// remembered or the KdeConnectRememberDevice setting is off.
QString rememberedDevice();
/// Remembers @p deviceId as the last device used, in the state config, only
/// while the KdeConnectRememberDevice setting is on; otherwise forgets it.
void rememberDevice(const QString &deviceId);
/// Forgets the last device used.
void forgetDevice();
}
