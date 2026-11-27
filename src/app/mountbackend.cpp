// SPDX-License-Identifier: GPL-3.0-or-later

#include "mountbackend.h"
#include "settings.h"

#include <KIO/FileSystemFreeSpaceJob>
#include <KLocalizedString>
#include <Solid/Block>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/NetworkShare>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace
{
QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString fallbackName(const MountDevice &device)
{
    if (device.mountPoint == QLatin1String("/")) {
        return i18nc("@item the root file system", "Root");
    }
    const QString name = QFileInfo(device.mountPoint).fileName();
    return name.isEmpty() ? device.device : name;
}

QString fallbackIcon(MountTable::Kind kind)
{
    switch (kind) {
    case MountTable::Kind::Network:
        return QStringLiteral("folder-network");
    case MountTable::Kind::Fuse:
        return QStringLiteral("folder-remote");
    case MountTable::Kind::Local:
        break;
    }
    return QStringLiteral("drive-harddisk");
}

Solid::OpticalDrive *opticalDriveOf(const Solid::Device &device, Solid::Device *holder)
{
    for (Solid::Device parent = device; parent.isValid(); parent = parent.parent()) {
        if (parent.is<Solid::OpticalDrive>()) {
            *holder = parent;
            return holder->as<Solid::OpticalDrive>();
        }
    }
    return nullptr;
}
}

QString mountErrorText(MountOperation operation, int solidError, const QString &details)
{
    QString text;
    switch (solidError) {
    case Solid::NoError:
        return {};
    case Solid::UnauthorizedOperation:
        switch (operation) {
        case MountOperation::Mount:
            text = i18nc("@info", "You are not allowed to mount this file system.");
            break;
        case MountOperation::Unmount:
            text = i18nc("@info", "You are not allowed to unmount this file system.");
            break;
        case MountOperation::Eject:
            text = i18nc("@info", "You are not allowed to eject this disc.");
            break;
        }
        break;
    case Solid::UserCanceled:
        return i18nc("@info", "Authorization was cancelled, so nothing was changed.");
    case Solid::DeviceBusy:
        text = i18nc("@info", "The file system is busy. Close the files and programs that use it, then try again.");
        break;
    default:
        switch (operation) {
        case MountOperation::Mount:
            text = i18nc("@info", "Could not mount the file system.");
            break;
        case MountOperation::Unmount:
            text = i18nc("@info", "Could not unmount the file system.");
            break;
        case MountOperation::Eject:
            text = i18nc("@info", "Could not eject the disc.");
            break;
        }
        break;
    }
    const QString trimmed = details.trimmed();
    return trimmed.isEmpty() ? text : i18nc("@info %1 is a sentence, %2 the system's message", "%1 The system reported: %2", text, trimmed);
}

int mountProcessErrorType(const QString &message)
{
    if (MountTable::isBusyError(message)) {
        return Solid::DeviceBusy;
    }
    const QString lower = message.toLower();
    if (lower.contains(QLatin1String("only root")) || lower.contains(QLatin1String("must be superuser")) || lower.contains(QLatin1String("not permitted"))
        || lower.contains(QLatin1String("permission denied"))) {
        return Solid::UnauthorizedOperation;
    }
    return Solid::OperationFailed;
}

SystemMountBackend::SystemMountBackend(QObject *parent)
    : MountBackend(parent)
{
    auto *notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, &SystemMountBackend::refresh);
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &SystemMountBackend::refresh);
}

QList<MountDevice> SystemMountBackend::devices() const
{
    return m_devices;
}

bool SystemMountBackend::isRunning(const QString &id) const
{
    return m_running.contains(id);
}

void SystemMountBackend::refresh()
{
    const QList<MountTable::Entry> entries = MountTable::merge(
        MountTable::parseMountInfo(readAll(QStringLiteral("/proc/self/mountinfo"))),
        MountTable::parseFstab(readAll(QStringLiteral("/etc/fstab"))),
        [](const QString &spec) {
            return MountTable::resolveDeviceSpec(spec);
        },
        !Settings::mountHideSystem());
    QList<MountDevice> devices;
    QHash<QString, qsizetype> byMountPoint;
    QHash<QString, qsizetype> byDevice;
    for (const MountTable::Entry &entry : entries) {
        MountDevice device;
        device.id = entry.mountPoint;
        device.device = entry.device;
        device.mountPoint = entry.mountPoint;
        device.type = entry.type;
        device.kind = entry.kind;
        device.mounted = entry.mounted;
        device.inFstab = entry.inFstab;
        device.system = entry.system;
        device.canMount = !entry.system && !entry.mounted && entry.userMountable;
        device.canUnmount =
            !entry.system && entry.mounted && entry.mountPoint != QLatin1String("/") && (entry.userMountable || entry.type.startsWith(QLatin1String("fuse")));
        byMountPoint.insert(entry.mountPoint, devices.size());
        if (entry.device.startsWith(QLatin1Char('/'))) {
            byDevice.insert(entry.device, devices.size());
        }
        devices.append(device);
    }

    const QList<Solid::Device> solidDevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &solid : solidDevices) {
        const auto *access = solid.as<Solid::StorageAccess>();
        if (!access || access->isIgnored()) {
            continue;
        }
        if (!m_watched.contains(solid.udi())) {
            m_watched.insert(solid.udi());
            connect(access, &Solid::StorageAccess::accessibilityChanged, this, &SystemMountBackend::onAccessibilityChanged);
        }
        const auto *volume = solid.as<Solid::StorageVolume>();
        const auto *block = solid.as<Solid::Block>();
        const QString path = access->filePath();
        qsizetype index = path.isEmpty() ? -1 : byMountPoint.value(QDir::cleanPath(path), -1);
        if (index < 0 && block) {
            index = byDevice.value(block->device(), -1);
        }
        if (index < 0) {
            // Mounted somewhere the mount manager hides, such as a system path.
            if (access->isAccessible()) {
                continue;
            }
            if (volume && volume->usage() != Solid::StorageVolume::FileSystem && volume->usage() != Solid::StorageVolume::Encrypted) {
                continue;
            }
            if (!volume && !solid.is<Solid::NetworkShare>()) {
                continue;
            }
            MountDevice device;
            device.device = block ? block->device() : QString();
            device.type = volume ? volume->fsType() : QString();
            device.kind = solid.is<Solid::NetworkShare>() ? MountTable::Kind::Network : MountTable::Kind::Local;
            index = devices.size();
            devices.append(device);
        }
        MountDevice &device = devices[index];
        device.id = solid.udi();
        device.udi = solid.udi();
        device.name = volume && !volume->label().isEmpty() ? volume->label() : solid.description();
        device.iconName = solid.icon();
        for (Solid::Device parent = solid; parent.isValid(); parent = parent.parent()) {
            if (parent.is<Solid::OpticalDrive>()) {
                device.canEject = true;
            }
            if (const auto *drive = parent.as<Solid::StorageDrive>()) {
                device.removable = drive->isRemovable() || drive->isHotpluggable();
                break;
            }
        }
        device.canMount = !device.system && !device.mounted;
        device.canUnmount = !device.system && device.mounted && device.mountPoint != QLatin1String("/");
    }

    for (MountDevice &device : devices) {
        if (device.name.isEmpty()) {
            device.name = fallbackName(device);
        }
        if (device.iconName.isEmpty()) {
            device.iconName = fallbackIcon(device.kind);
        }
        const auto size = m_sizes.constFind(device.mountPoint);
        if (device.mounted && size != m_sizes.cend()) {
            device.size = size->first;
            device.available = size->second;
        }
    }
    std::stable_sort(devices.begin(), devices.end(), [](const MountDevice &a, const MountDevice &b) {
        if (a.mountPoint.isEmpty() != b.mountPoint.isEmpty()) {
            return b.mountPoint.isEmpty();
        }
        return a.mountPoint == b.mountPoint ? QString::localeAwareCompare(a.name, b.name) < 0 : a.mountPoint < b.mountPoint;
    });

    if (devices != m_devices) {
        m_devices = devices;
        Q_EMIT changed();
    }
    requestSizes();
}

void SystemMountBackend::refreshSizes()
{
    m_sizes.clear();
    requestSizes();
}

void SystemMountBackend::requestSizes()
{
    for (const MountDevice &device : std::as_const(m_devices)) {
        const QString mountPoint = device.mountPoint;
        if (!device.mounted || m_sizes.contains(mountPoint) || m_sizeRequests.contains(mountPoint)) {
            continue;
        }
        m_sizeRequests.insert(mountPoint);
        auto *job = KIO::fileSystemFreeSpace(QUrl::fromLocalFile(mountPoint));
        connect(job, &KJob::result, this, [this, job, mountPoint] {
            m_sizeRequests.remove(mountPoint);
            const std::pair<qint64, qint64> size =
                job->error() ? std::pair<qint64, qint64>(-1, -1) : std::pair<qint64, qint64>(qint64(job->size()), qint64(job->availableSize()));
            m_sizes.insert(mountPoint, size);
            bool updated = false;
            for (MountDevice &device : m_devices) {
                if (device.mounted && device.mountPoint == mountPoint && (device.size != size.first || device.available != size.second)) {
                    device.size = size.first;
                    device.available = size.second;
                    updated = true;
                }
            }
            if (updated) {
                Q_EMIT changed();
            }
        });
    }
}

void SystemMountBackend::onAccessibilityChanged(bool accessible, const QString &udi)
{
    Q_UNUSED(accessible)
    Q_UNUSED(udi)
    refresh();
}

void SystemMountBackend::start(const QString &id, MountOperation operation)
{
    if (m_running.contains(id)) {
        return;
    }
    const auto found = std::find_if(m_devices.cbegin(), m_devices.cend(), [&id](const MountDevice &device) {
        return device.id == id;
    });
    m_running.insert(id);
    if (found == m_devices.cend()) {
        finish(id, operation, i18nc("@info", "The file system is no longer available."));
        return;
    }
    const MountDevice device = *found;
    if (device.udi.isEmpty()) {
        startProcess(device, operation);
        return;
    }

    auto solid = std::make_shared<Solid::Device>(device.udi);
    m_operations.insert(id, solid);
    const auto report = [this, id, operation](Solid::ErrorType error, const QVariant &data) {
        QString mountPoint;
        if (error == Solid::NoError && operation == MountOperation::Mount) {
            const Solid::Device current(m_operations.value(id) ? m_operations.value(id)->udi() : QString());
            if (const auto *access = current.as<Solid::StorageAccess>()) {
                mountPoint = access->filePath();
            }
        }
        finish(id, operation, mountErrorText(operation, error, data.toString()), mountPoint);
    };
    if (operation == MountOperation::Eject) {
        auto holder = std::make_shared<Solid::Device>();
        Solid::OpticalDrive *drive = opticalDriveOf(*solid, holder.get());
        if (!drive) {
            finish(id, operation, i18nc("@info", "Only discs in an optical drive can be ejected. Unmount other devices instead."));
            return;
        }
        m_operations.insert(id, holder);
        connect(
            drive,
            &Solid::OpticalDrive::ejectDone,
            this,
            [report](Solid::ErrorType error, const QVariant &data, const QString &) {
                report(error, data);
            },
            Qt::SingleShotConnection);
        if (!drive->eject()) {
            finish(id, operation, mountErrorText(operation, Solid::OperationFailed, {}));
        }
        return;
    }

    auto *access = solid->as<Solid::StorageAccess>();
    if (!access) {
        finish(id, operation, mountErrorText(operation, Solid::OperationFailed, {}));
        return;
    }
    const auto done = [report](Solid::ErrorType error, const QVariant &data, const QString &) {
        report(error, data);
    };
    if (operation == MountOperation::Mount) {
        connect(access, &Solid::StorageAccess::setupDone, this, done, Qt::SingleShotConnection);
        if (!access->setup()) {
            finish(id, operation, mountErrorText(operation, Solid::OperationFailed, {}));
        }
    } else {
        connect(access, &Solid::StorageAccess::teardownDone, this, done, Qt::SingleShotConnection);
        if (!access->teardown()) {
            finish(id, operation, mountErrorText(operation, Solid::OperationFailed, {}));
        }
    }
}

void SystemMountBackend::startProcess(const MountDevice &device, MountOperation operation)
{
    QString program;
    QStringList arguments;
    if (operation == MountOperation::Mount) {
        program = QStandardPaths::findExecutable(QStringLiteral("mount"));
        arguments = {device.mountPoint};
    } else if (operation == MountOperation::Unmount && device.type.startsWith(QLatin1String("fuse"))) {
        program = QStandardPaths::findExecutable(QStringLiteral("fusermount3"));
        if (program.isEmpty()) {
            program = QStandardPaths::findExecutable(QStringLiteral("fusermount"));
        }
        arguments = {QStringLiteral("-u"), device.mountPoint};
    } else if (operation == MountOperation::Unmount) {
        program = QStandardPaths::findExecutable(QStringLiteral("umount"));
        arguments = {device.mountPoint};
    } else {
        finish(device.id, operation, i18nc("@info", "Only discs in an optical drive can be ejected. Unmount other devices instead."));
        return;
    }
    if (program.isEmpty()) {
        finish(device.id, operation, mountErrorText(operation, Solid::OperationFailed, i18nc("@info", "The mount tools are not installed.")));
        return;
    }

    auto *process = new QProcess(this);
    const QString id = device.id;
    const QString mountPoint = device.mountPoint;
    connect(process, &QProcess::finished, this, [this, process, id, operation, mountPoint](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status == QProcess::NormalExit && exitCode == 0) {
            finish(id, operation, {}, operation == MountOperation::Mount ? mountPoint : QString());
            return;
        }
        const QString message = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        finish(id, operation, mountErrorText(operation, mountProcessErrorType(message), message));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, id, operation](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        process->deleteLater();
        finish(id, operation, mountErrorText(operation, Solid::OperationFailed, process->errorString()));
    });
    process->start(program, arguments);
}

void SystemMountBackend::finish(const QString &id, MountOperation operation, const QString &error, const QString &mountPoint)
{
    m_running.remove(id);
    // Release the Solid device after its signal has returned.
    QTimer::singleShot(0, this, [kept = m_operations.take(id)] { });
    refresh();
    Q_EMIT finished(id, operation, error, mountPoint);
}
