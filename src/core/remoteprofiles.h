// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

/**
 * Saved connections to SSH, SFTP and WebDAV servers. Profiles only hold the
 * address and how to sign in: passwords are never stored here, because KIO
 * asks for them and KPasswdServer keeps them in KWallet when the user agrees.
 */
namespace RemoteProfiles
{
enum class Protocol {
    Sftp,
    WebDav,
    WebDavs,
};

enum class Authentication {
    /// KIO asks for a password when the server requires one.
    Password,
    /// The SSH agent or a private key signs in; SFTP only.
    KeyOrAgent,
};

struct Profile {
    QString id;
    QString name;
    Protocol protocol = Protocol::Sftp;
    QString host;
    /// 0 uses the protocol's default port.
    int port = 0;
    QString user;
    /// An absolute remote folder; empty opens the server's default folder.
    QString path;
    Authentication authentication = Authentication::Password;
    /// A private key for SSH terminals; empty uses the SSH configuration and agent.
    QString identityFile;
};

/// All protocols, in menu order.
QList<Protocol> protocols();
/// The KIO scheme: sftp, webdav or webdavs.
QString scheme(Protocol protocol);
/// The translated protocol name shown in menus.
QString name(Protocol protocol);
/// The port used when a profile doesn't set one.
int defaultPort(Protocol protocol);
/// Whether the protocol also offers an SSH terminal.
bool supportsSsh(Protocol protocol);

/// Why @p profile can't be used, or an empty string when it is valid.
QString validationError(const Profile &profile);
/// A warning about sending credentials unencrypted, or an empty string.
QString securityWarning(const Profile &profile);
/// The URL that opens @p profile in a pane. It never contains a password.
QUrl url(const Profile &profile);
/// The arguments that open an SSH session for @p profile, ending with "--" and the host.
QStringList sshArguments(const Profile &profile);

/// Whether a KIO worker handles a scheme.
using ProtocolCheck = std::function<bool(const QString &scheme)>;
/// Asks KIO whether a worker is installed for the scheme.
bool isKnownProtocol(const QString &scheme);
/// Why @p protocol can't be opened, naming the package to install, or an empty string.
QString unavailableReason(Protocol protocol, const ProtocolCheck &check = isKnownProtocol);

/**
 * Reads the saved profiles in their saved order, skipping invalid entries.
 * @param error receives a message when some entries were skipped
 */
QList<Profile> load(const KConfigGroup &group, QString *error = nullptr);
/**
 * Replaces the saved profiles, assigning identifiers to new ones in @p profiles.
 * @return false when a profile is invalid or two profiles share a name, leaving
 * the saved profiles unchanged, or when the configuration can't be written
 */
bool save(KConfigGroup group, QList<Profile> &profiles, QString *error = nullptr);
}
