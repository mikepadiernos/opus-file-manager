// SPDX-License-Identifier: GPL-3.0-or-later

#include "defaultfilemanager.h"

#include "appidentity.h"
#include "filemanagerrequest.h"

#include <KApplicationTrader>
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KService>
#include <KShell>
#include <KSycoca>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
const QString DefaultApplicationsGroup = QStringLiteral("Default Applications");
const QString ServiceGroup = QStringLiteral("D-BUS Service");
const QString ServiceOption = QStringLiteral("--file-manager-service");

/// The first default application for folders that the mimeapps.list file @p path sets.
QString defaultIn(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        return {};
    }
    const KConfig list(path, KConfig::SimpleConfig);
    const QStringList ids = list.group(DefaultApplicationsGroup).readXdgListEntry(DefaultFileManager::folderMimeType());
    return ids.isEmpty() ? QString() : ids.constFirst().trimmed();
}

/// The executable that a Plasma Opus activation file starts, or an empty string for other files.
QString startedExecutable(const QString &file)
{
    const KConfig config(file, KConfig::SimpleConfig);
    const KConfigGroup group = config.group(ServiceGroup);
    if (group.readEntry("Name", QString()) != FileManagerRequest::serviceName()) {
        return {};
    }
    KShell::Errors errors = KShell::NoError;
    const QStringList args = KShell::splitArgs(group.readEntry("Exec", QString()), KShell::NoOptions, &errors);
    if (errors != KShell::NoError || args.isEmpty() || !args.contains(ServiceOption)
        || QFileInfo(args.constFirst()).fileName() != AppIdentity::componentName()) {
        return {};
    }
    return args.constFirst();
}

bool sameFile(const QString &a, const QString &b)
{
    const QString canonicalA = QFileInfo(a).canonicalFilePath();
    return canonicalA.isEmpty() ? QFileInfo(a).absoluteFilePath() == QFileInfo(b).absoluteFilePath() : canonicalA == QFileInfo(b).canonicalFilePath();
}
}

namespace DefaultFileManager
{
QString folderMimeType()
{
    return QStringLiteral("inode/directory");
}

Handler folderHandler()
{
    KSycoca::self()->ensureCacheValid();
    const KService::Ptr service = KApplicationTrader::preferredService(folderMimeType());
    if (!service) {
        return {};
    }
    return {service->desktopEntryName(), service->name()};
}

bool isPlasmaOpus(const QString &desktopName)
{
    return FileManagerRequest::shouldClaim(desktopName);
}

QString overridingList(const QString &configHome, const QString &currentDesktops, const QString &desktopId)
{
    const QStringList desktops = currentDesktops.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &desktop : desktops) {
        const QString path = configHome + QLatin1Char('/') + desktop.trimmed().toLower() + QStringLiteral("-mimeapps.list");
        const QString id = defaultIn(path);
        if (!id.isEmpty()) {
            return id == desktopId ? QString() : path;
        }
    }
    return {};
}

QString setPlasmaOpusAsDefault()
{
    KSycoca::self()->ensureCacheValid();
    const KService::Ptr service = KService::serviceByDesktopName(AppIdentity::desktopFileName());
    if (!service || !service->isValid()) {
        return i18nc("@info",
                     "Plasma Opus is not installed as an application, so it cannot be the default file manager. Install it with install.sh, then try again.");
    }
    KApplicationTrader::setPreferredService(folderMimeType(), service);

    // Read the file back rather than asking KService, whose cache may not have
    // noticed the change yet.
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString list = configHome + QStringLiteral("/mimeapps.list");
    if (defaultIn(list) != service->storageId()) {
        return i18nc("@info", "Could not save the default file manager in %1.", list);
    }
    const QString overriding = overridingList(configHome, QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")), service->storageId());
    if (!overriding.isEmpty()) {
        return i18nc("@info",
                     "Plasma Opus was saved as the default file manager, but %1 sets another default application for folders, which takes precedence. "
                     "Change it in System Settings › Default Applications or remove its inode/directory entry.",
                     overriding);
    }
    return {};
}

QString activationFile(const QString &dataHome)
{
    return dataHome + QStringLiteral("/dbus-1/services/") + AppIdentity::desktopFileName() + QStringLiteral(".FileManager1.service");
}

QString activationFileContent(const QString &executable)
{
    return QStringLiteral("[D-BUS Service]\nName=%1\nExec=%2 %3\n").arg(FileManagerRequest::serviceName(), KShell::quoteArg(executable), ServiceOption);
}

Activation activationState(const QString &file, const QString &executable, QString *target)
{
    if (!QFileInfo::exists(file)) {
        return Activation::Missing;
    }
    const QString started = startedExecutable(file);
    if (started.isEmpty()) {
        return Activation::Foreign;
    }
    if (target) {
        *target = started;
    }
    if (sameFile(started, executable)) {
        return Activation::Installed;
    }
    const QFileInfo info(started);
    return info.isFile() && info.isExecutable() ? Activation::Elsewhere : Activation::Broken;
}

QString installActivationFile(const QString &file, const QString &executable)
{
    if (activationState(file, executable) == Activation::Foreign) {
        return i18nc("@info", "%1 was not written by Plasma Opus, so it was left unchanged.", file);
    }
    const QFileInfo program(executable);
    if (!program.isFile() || !program.isExecutable()) {
        return i18nc("@info", "The Plasma Opus executable %1 does not exist.", executable);
    }
    const QString folder = QFileInfo(file).absolutePath();
    if (!QDir().mkpath(folder)) {
        return i18nc("@info", "Could not create the folder %1.", folder);
    }
    QSaveFile save(file);
    if (!save.open(QIODevice::WriteOnly) || save.write(activationFileContent(program.absoluteFilePath()).toUtf8()) < 0 || !save.commit()) {
        return i18nc("@info", "Could not write %1: %2", file, save.errorString());
    }
    return {};
}

QString removeActivationFile(const QString &file)
{
    const Activation state = activationState(file, QString());
    if (state == Activation::Missing) {
        return {};
    }
    if (state == Activation::Foreign) {
        return i18nc("@info", "%1 was not written by Plasma Opus, so it was left unchanged.", file);
    }
    if (!QFile::remove(file)) {
        return i18nc("@info", "Could not remove %1.", file);
    }
    return {};
}

void reloadBusConfiguration()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.send(QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                                QStringLiteral("/org/freedesktop/DBus"),
                                                QStringLiteral("org.freedesktop.DBus"),
                                                QStringLiteral("ReloadConfig")));
    }
}
}
