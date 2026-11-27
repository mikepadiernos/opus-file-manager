// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mounttable.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

#include <memory>

namespace Solid
{
class Device;
}

/// A file system listed by the mount manager.
struct MountDevice {
    /// The Solid device id, or the mount point for file systems Solid does not know.
    QString id;
    QString udi;
    QString name;
    QString device;
    /// The current mount point, or the fstab one when it is not mounted; empty for an unmounted device without one.
    QString mountPoint;
    QString type;
    QString iconName;
    MountTable::Kind kind = MountTable::Kind::Local;
    bool mounted = false;
    bool inFstab = false;
    bool removable = false;
    /// Whether it is a pseudo, snap or other system file system, which can be
    /// listed but is never mounted or unmounted from Plasma Opus.
    bool system = false;
    bool canMount = false;
    bool canUnmount = false;
    bool canEject = false;
    /// The total and available bytes, or -1 while unknown.
    qint64 size = -1;
    qint64 available = -1;

    bool operator==(const MountDevice &other) const = default;
};

enum class MountOperation {
    Mount,
    Unmount,
    Eject,
};

/// Lists file systems and mounts, unmounts or ejects them.
class MountBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual QList<MountDevice> devices() const = 0;
    /// Reads the mount table again; emits changed() when something differs.
    virtual void refresh() = 0;
    /// Reads the size and free space of each mounted file system again.
    virtual void refreshSizes() = 0;
    /// Starts @p operation on the device with @p id; finished() reports the result.
    virtual void start(const QString &id, MountOperation operation) = 0;
    /// Whether an operation on the device with @p id is running.
    virtual bool isRunning(const QString &id) const = 0;

Q_SIGNALS:
    void changed();
    /// @p error is empty on success; @p mountPoint is where the device is mounted after mounting.
    void finished(const QString &id, MountOperation operation, const QString &error, const QString &mountPoint);
};

/// Uses Solid and UDisks2 for devices they manage, with KDE's authorization
/// prompts, and mount(8), umount(8) or fusermount for fstab entries a user may
/// mount and for FUSE mounts.
class SystemMountBackend : public MountBackend
{
    Q_OBJECT

public:
    explicit SystemMountBackend(QObject *parent = nullptr);

    QList<MountDevice> devices() const override;
    void refresh() override;
    void refreshSizes() override;
    void start(const QString &id, MountOperation operation) override;
    bool isRunning(const QString &id) const override;

private:
    void finish(const QString &id, MountOperation operation, const QString &error, const QString &mountPoint = {});
    void startProcess(const MountDevice &device, MountOperation operation);
    void requestSizes();
    void onAccessibilityChanged(bool accessible, const QString &udi);

    QList<MountDevice> m_devices;
    QSet<QString> m_running;
    /// Solid devices kept alive until their operation reports back.
    QHash<QString, std::shared_ptr<Solid::Device>> m_operations;
    QSet<QString> m_watched;
    QHash<QString, std::pair<qint64, qint64>> m_sizes;
    QSet<QString> m_sizeRequests;
};

/// A localized description of why an operation failed, from Solid's error
/// type and details; an empty string for success.
QString mountErrorText(MountOperation operation, int solidError, const QString &details);

/// The Solid error type that matches a message from mount(8), umount(8) or fusermount.
int mountProcessErrorType(const QString &message);
