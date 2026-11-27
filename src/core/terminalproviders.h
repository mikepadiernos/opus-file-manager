// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QUrl;

/**
 * Terminal applications that can open the active folder in a separate window,
 * and terminal components that the embedded terminal panel can load.
 */
namespace TerminalProviders
{
enum class Provider {
    /// KDE's configured terminal application, through KTerminalLauncherJob.
    SystemDefault,
    Konsole,
    Yakuake,
    Kwi,
};

/// Providers offered by Open Terminal With, in menu order.
QList<Provider> separateProviders();
/// Providers whose terminal component can be embedded in the terminal panel.
QList<Provider> embeddedProviders();

/// The provider Open Terminal Here uses, from the SeparateTerminal setting.
Provider separateProvider();

/// A stable identifier, such as "konsole".
QString id(Provider provider);
/// The translated name shown in menus and messages.
QString name(Provider provider);
QString iconName(Provider provider);
/// The executable a separate terminal runs, or an empty string for the system default.
QString executable(Provider provider);
/// The desktop file name used for startup feedback, or an empty string for the system default.
QString desktopName(Provider provider);
/// The KParts plugin ID in the kf6/parts namespace, or an empty string when it can't be embedded.
QString partPluginId(Provider provider);

/// Whether the provider's executable is in PATH. The system default is always available.
bool isInstalled(Provider provider);
/// Whether the provider's terminal component is installed.
bool isPartInstalled(Provider provider);
/// Why the provider can't open a separate terminal, or an empty string when it can.
QString unavailableReason(Provider provider);
/// Why the provider can't be embedded, or an empty string when it can.
QString partUnavailableReason(Provider provider);

/**
 * The clean absolute path of @p folder when it is an existing local folder,
 * or an empty string when terminals can't start there.
 */
QString localDirectory(const QUrl &folder);
/// The arguments that start @p provider in @p directory. Yakuake and the system default take none.
QStringList launchArguments(Provider provider, const QString &directory);

/// Yakuake's D-Bus service name.
QString yakuakeService();
/**
 * The command Yakuake runs in a new session to enter @p directory. A leading
 * space keeps it out of shell history where HISTCONTROL ignores spaces.
 */
QString yakuakeChangeDirectoryCommand(const QString &directory);
/// The first terminal ID in Yakuake's comma-separated list, or -1 when there is none.
int firstYakuakeTerminalId(const QString &terminalIds);
}
