// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

class KConfigGroup;

/**
 * Opt-in setup of the Plasma Opus file chooser as the user's XDG portal
 * FileChooser backend, and its restoration. Setup writes per-user portal
 * metadata and a D-Bus activation file, and prefers Plasma Opus in the
 * user's kde-portals.conf, keeping the previous chooser as the fallback.
 * The previous configuration is kept in a backup group so restoring can put
 * it back.
 */
namespace PortalSetup
{
/// The portal name used in kde-portals.conf and the .portal file.
QString portalName();
/// The bus name of the backend.
QString dbusName();
QString fileChooserInterface();

struct Paths {
    QString dataHome;
    QString configHome;
    /// System configuration folders, in xdg-desktop-portal's search order.
    QStringList configDirs;
    /// System data folders, in xdg-desktop-portal's search order.
    QStringList dataDirs;
    /// The backend executable the activation file starts.
    QString helperExecutable;

    /// Paths for the current user, following the XDG base directory variables.
    static Paths fromEnvironment(const QString &helperExecutable);

    QString portalFile() const;
    QString serviceFile() const;
    /// The user's kde-portals.conf.
    QString configFile() const;
};

QString portalFileContent();
QString serviceFileContent(const QString &executable);

/**
 * The value of @p key in the [preferred] group of portal configuration
 * text, or an empty string when it is not set.
 */
QString preferredEntry(const QString &config, const QString &key);

/**
 * Sets @p key in the [preferred] group, keeping the rest of the text,
 * including comments, unchanged.
 * @param value the new value; an empty string removes the key
 */
QString setPreferredEntry(const QString &config, const QString &key, const QString &value);

/**
 * A FileChooser preference listing Plasma Opus first and then @p fallback,
 * the previous preference.
 */
QString preferOpus(const QString &fallback);

/// A preference without Plasma Opus, or an empty string when nothing remains.
QString withoutOpus(const QString &value);

/**
 * The portal configuration xdg-desktop-portal uses when the user has no
 * kde-portals.conf: the user's portals.conf, or the first kde-portals.conf or
 * portals.conf in the system configuration and data folders, or an empty string.
 */
QString effectiveSystemConfig(const Paths &paths);

enum class State {
    /// None of the setup is present.
    NotInstalled,
    /// The portal and activation files exist and kde-portals.conf prefers Plasma Opus.
    Installed,
    /// Some of the setup is present, such as after the user edited kde-portals.conf.
    Partial,
};
State state(const Paths &paths);

/**
 * Installs the portal metadata and activation file and prefers Plasma Opus
 * for the FileChooser portal. Running it again refreshes the files without
 * replacing the backup of the original configuration.
 * @param backup the state group keeping the previous configuration
 * @returns an error message, or an empty string on success
 */
QString install(const Paths &paths, KConfigGroup &backup);

/**
 * Restores the previous chooser: puts back the previous kde-portals.conf when
 * the user has not changed it since setup, or otherwise removes only Plasma
 * Opus from the preference, and removes the portal and activation files.
 * @returns an error message, or an empty string on success
 */
QString uninstall(const Paths &paths, KConfigGroup &backup);

/// Whether xdg-desktop-portal is running or can be started on the session bus.
bool portalServiceAvailable();

/**
 * Restarts the xdg-desktop-portal user service so that it reads the portal
 * files and configuration again. File dialogs it shows close.
 * @returns an error message, or an empty string on success
 */
QString restartPortal();
}
