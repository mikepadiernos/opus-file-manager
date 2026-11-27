// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/**
 * The user's default file manager: the default application for folders,
 * and the per-user D-Bus activation file that starts Plasma Opus for
 * `org.freedesktop.FileManager1` requests while no file manager runs.
 * Nothing here changes unless the user asks, and each change reports what
 * failed.
 */
namespace DefaultFileManager
{
/// The MIME type of folders, whose default application is the default file manager.
QString folderMimeType();

/// The default application for folders.
struct Handler {
    /// The desktop entry name, without the .desktop suffix, or an empty string when none is set.
    QString desktopName;
    /// The application's name, for display.
    QString name;
};

/// The user's default application for folders, as KDE applications resolve it.
Handler folderHandler();

/// Whether @p desktopName, with or without the .desktop suffix, is Plasma Opus.
bool isPlasmaOpus(const QString &desktopName);

/**
 * The user's desktop-specific mimeapps.list, such as kde-mimeapps.list, that
 * sets another default application for folders and so takes precedence over
 * mimeapps.list.
 * @param configHome the user's configuration folder
 * @param currentDesktops the session's desktops, as in XDG_CURRENT_DESKTOP
 * @param desktopId the default that mimeapps.list sets, with the .desktop suffix
 * @returns the overriding file, or an empty string when there is none
 */
QString overridingList(const QString &configHome, const QString &currentDesktops, const QString &desktopId);

/**
 * Makes Plasma Opus the default application for folders in the user's
 * mimeapps.list, as System Settings › Default Applications does, then checks
 * that the file was written and that no desktop-specific list overrides it.
 * @returns an error message, or an empty string on success
 */
QString setPlasmaOpusAsDefault();

/// The per-user activation file of the FileManager1 service in the data folder @p dataHome.
QString activationFile(const QString &dataHome);

/// The activation file's content, which starts @p executable for FileManager1 requests.
QString activationFileContent(const QString &executable);

enum class Activation {
    /// There is no activation file.
    Missing,
    /// The file starts the given executable.
    Installed,
    /// The file starts another Plasma Opus executable that exists, such as another installation.
    Elsewhere,
    /// The file starts a Plasma Opus executable that no longer exists.
    Broken,
    /// The file was not written by Plasma Opus, so it is left alone.
    Foreign,
};

/**
 * The state of the activation file @p file.
 * @param executable the Plasma Opus executable that is running
 * @param target receives the executable the file starts, when it is a Plasma Opus file
 */
Activation activationState(const QString &file, const QString &executable, QString *target = nullptr);

/**
 * Writes the activation file, replacing an earlier Plasma Opus one but not
 * a file written by another program.
 * @returns an error message, or an empty string on success
 */
QString installActivationFile(const QString &file, const QString &executable);

/**
 * Removes the activation file when Plasma Opus wrote it.
 * @returns an error message, or an empty string on success
 */
QString removeActivationFile(const QString &file);

/// Asks the session bus to read its activation files again; dbus-broker needs it.
void reloadBusConfiguration();
}
