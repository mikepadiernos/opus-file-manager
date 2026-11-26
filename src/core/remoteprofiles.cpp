// SPDX-License-Identifier: GPL-3.0-or-later

#include "remoteprofiles.h"

#include <KLocalizedString>
#include <KProtocolInfo>

#include <QDir>
#include <QSet>
#include <QUuid>

namespace
{
const QString orderKey = QStringLiteral("Order");

bool hasControlCharacter(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.category() == QChar::Other_Control;
    });
}

bool hasSpace(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.isSpace();
    });
}

QString normalizedHost(const QString &host)
{
    QUrl url;
    url.setHost(host, QUrl::StrictMode);
    return url.host();
}

QString authenticationKey(RemoteProfiles::Authentication authentication)
{
    return authentication == RemoteProfiles::Authentication::KeyOrAgent ? QStringLiteral("key") : QStringLiteral("password");
}

bool protocolFromScheme(const QString &scheme, RemoteProfiles::Protocol *protocol)
{
    for (RemoteProfiles::Protocol candidate : RemoteProfiles::protocols()) {
        if (RemoteProfiles::scheme(candidate) == scheme) {
            *protocol = candidate;
            return true;
        }
    }
    return false;
}

bool validId(const QString &id)
{
    return !QUuid::fromString(id).isNull() && QUuid::fromString(id).toString(QUuid::WithoutBraces) == id;
}
}

QList<RemoteProfiles::Protocol> RemoteProfiles::protocols()
{
    return {Protocol::Sftp, Protocol::WebDavs, Protocol::WebDav};
}

QString RemoteProfiles::scheme(Protocol protocol)
{
    switch (protocol) {
    case Protocol::Sftp:
        return QStringLiteral("sftp");
    case Protocol::WebDav:
        return QStringLiteral("webdav");
    case Protocol::WebDavs:
        return QStringLiteral("webdavs");
    }
    return {};
}

QString RemoteProfiles::name(Protocol protocol)
{
    switch (protocol) {
    case Protocol::Sftp:
        return i18nc("@item:inlistbox remote protocol", "SSH (SFTP)");
    case Protocol::WebDav:
        return i18nc("@item:inlistbox remote protocol", "WebDAV (unencrypted)");
    case Protocol::WebDavs:
        return i18nc("@item:inlistbox remote protocol", "WebDAV over HTTPS");
    }
    return {};
}

int RemoteProfiles::defaultPort(Protocol protocol)
{
    switch (protocol) {
    case Protocol::Sftp:
        return 22;
    case Protocol::WebDav:
        return 80;
    case Protocol::WebDavs:
        return 443;
    }
    return 0;
}

bool RemoteProfiles::supportsSsh(Protocol protocol)
{
    return protocol == Protocol::Sftp;
}

QString RemoteProfiles::validationError(const Profile &profile)
{
    if (profile.name.trimmed().isEmpty() || hasControlCharacter(profile.name)) {
        return i18nc("@info", "Enter a name without line breaks.");
    }
    if (profile.host.isEmpty() || hasSpace(profile.host) || hasControlCharacter(profile.host) || profile.host.startsWith(QLatin1Char('-'))
        || profile.host.contains(QLatin1Char('/')) || profile.host.contains(QLatin1Char('@')) || profile.host.contains(QLatin1Char('\\'))
        || normalizedHost(profile.host).isEmpty()) {
        return i18nc("@info", "Enter a host name or IP address without spaces, slashes or a user name, not starting with a hyphen.");
    }
    if (profile.port < 0 || profile.port > 65535) {
        return i18nc("@info", "Enter a port from 1 to 65535, or leave it at the default.");
    }
    if (hasSpace(profile.user) || hasControlCharacter(profile.user) || profile.user.startsWith(QLatin1Char('-')) || profile.user.contains(QLatin1Char('@'))
        || profile.user.contains(QLatin1Char(':')) || profile.user.contains(QLatin1Char('/'))) {
        return i18nc("@info", "Enter a user name without spaces, @, : or /, not starting with a hyphen.");
    }
    if ((!profile.path.isEmpty() && !profile.path.startsWith(QLatin1Char('/'))) || hasControlCharacter(profile.path)) {
        return i18nc("@info", "Enter the remote folder as an absolute path starting with /, or leave it empty.");
    }
    if (profile.authentication == Authentication::KeyOrAgent && !supportsSsh(profile.protocol)) {
        return i18nc("@info", "Sign-in with an SSH key or agent is only available for SSH connections.");
    }
    if (profile.authentication == Authentication::KeyOrAgent && !profile.identityFile.isEmpty()
        && (!QDir::isAbsolutePath(profile.identityFile) || hasControlCharacter(profile.identityFile))) {
        return i18nc("@info", "Enter the private key as an absolute path, or leave it empty to use the SSH configuration and agent.");
    }
    return {};
}

QString RemoteProfiles::securityWarning(const Profile &profile)
{
    if (profile.protocol == Protocol::WebDav) {
        return i18nc("@info", "Unencrypted WebDAV sends your password and files in plain text. Use WebDAV over HTTPS unless the network is trusted.");
    }
    return {};
}

QUrl RemoteProfiles::url(const Profile &profile)
{
    QUrl url;
    url.setScheme(scheme(profile.protocol));
    url.setHost(normalizedHost(profile.host));
    if (profile.port > 0) {
        url.setPort(profile.port);
    }
    if (!profile.user.isEmpty()) {
        url.setUserName(profile.user);
    }
    url.setPath(profile.path.isEmpty() ? QStringLiteral("/") : QDir::cleanPath(profile.path));
    if (profile.protocol == Protocol::Sftp && profile.path.isEmpty()) {
        // KIO's SFTP worker opens the user's home folder when the path is empty.
        url.setPath({});
    }
    return url;
}

QStringList RemoteProfiles::sshArguments(const Profile &profile)
{
    QStringList arguments;
    if (profile.port > 0) {
        arguments << QStringLiteral("-p") << QString::number(profile.port);
    }
    if (!profile.user.isEmpty()) {
        arguments << QStringLiteral("-l") << profile.user;
    }
    if (profile.authentication == Authentication::KeyOrAgent && !profile.identityFile.isEmpty()) {
        arguments << QStringLiteral("-i") << profile.identityFile;
    }
    arguments << QStringLiteral("--") << normalizedHost(profile.host);
    return arguments;
}

bool RemoteProfiles::isKnownProtocol(const QString &scheme)
{
    return KProtocolInfo::isKnownProtocol(scheme);
}

QString RemoteProfiles::unavailableReason(Protocol protocol, const ProtocolCheck &check)
{
    if (check(scheme(protocol))) {
        return {};
    }
    if (protocol == Protocol::Sftp) {
        return i18nc("@info", "No KIO worker handles SFTP. Install kio-extras to open SSH connections.");
    }
    return i18nc("@info %1 is a protocol name", "No KIO worker handles %1. Install the KIO package from your distribution.", name(protocol));
}

QList<RemoteProfiles::Profile> RemoteProfiles::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    QStringList ids = group.readEntry(orderKey, QStringList());
    QStringList unordered = group.groupList();
    unordered.sort();
    for (const QString &id : std::as_const(unordered)) {
        if (!ids.contains(id)) {
            ids.append(id);
        }
    }
    QList<Profile> profiles;
    QSet<QString> seenIds;
    QSet<QString> names;
    int skipped = 0;
    for (const QString &id : std::as_const(ids)) {
        if (seenIds.contains(id)) {
            continue;
        }
        seenIds.insert(id);
        if (!group.hasGroup(id)) {
            continue;
        }
        const KConfigGroup entry = group.group(id);
        Profile profile;
        profile.id = id;
        profile.name = entry.readEntry("Name", QString());
        profile.host = entry.readEntry("Host", QString());
        profile.port = entry.readEntry("Port", 0);
        profile.user = entry.readEntry("User", QString());
        profile.path = entry.readEntry("Path", QString());
        profile.identityFile = entry.readEntry("IdentityFile", QString());
        const QString authentication = entry.readEntry("Authentication", authenticationKey(Authentication::Password));
        profile.authentication = authentication == authenticationKey(Authentication::KeyOrAgent) ? Authentication::KeyOrAgent : Authentication::Password;
        const QString key = profile.name.trimmed().toCaseFolded();
        if (!validId(id) || !protocolFromScheme(entry.readEntry("Protocol", QString()), &profile.protocol)
            || (authentication != authenticationKey(Authentication::Password) && authentication != authenticationKey(Authentication::KeyOrAgent))
            || !validationError(profile).isEmpty() || names.contains(key)) {
            ++skipped;
            continue;
        }
        names.insert(key);
        profiles.append(profile);
    }
    if (skipped > 0 && error) {
        *error =
            i18ncp("@info", "One saved remote connection is invalid and was skipped.", "%1 saved remote connections are invalid and were skipped.", skipped);
    }
    return profiles;
}

bool RemoteProfiles::save(KConfigGroup group, QList<Profile> &profiles, QString *error)
{
    if (error) {
        error->clear();
    }
    QSet<QString> names;
    QSet<QString> ids;
    for (const Profile &profile : std::as_const(profiles)) {
        const QString problem = validationError(profile);
        const QString key = profile.name.trimmed().toCaseFolded();
        if (!problem.isEmpty() || names.contains(key) || (!profile.id.isEmpty() && (!validId(profile.id) || ids.contains(profile.id)))) {
            if (error) {
                *error = problem.isEmpty() ? i18nc("@info", "Each remote connection needs a unique name.")
                                           : i18nc("@info %1 is a connection name, %2 the problem", "%1: %2", profile.name.trimmed(), problem);
            }
            return false;
        }
        names.insert(key);
        if (!profile.id.isEmpty()) {
            ids.insert(profile.id);
        }
    }
    QStringList order;
    for (Profile &profile : profiles) {
        if (profile.id.isEmpty()) {
            profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        order.append(profile.id);
    }
    const QStringList existing = group.groupList();
    for (const QString &id : existing) {
        if (!order.contains(id)) {
            group.deleteGroup(id);
        }
    }
    for (const Profile &profile : std::as_const(profiles)) {
        KConfigGroup entry = group.group(profile.id);
        entry.writeEntry("Name", profile.name.trimmed());
        entry.writeEntry("Protocol", scheme(profile.protocol));
        entry.writeEntry("Host", profile.host);
        entry.writeEntry("Port", profile.port);
        entry.writeEntry("User", profile.user);
        entry.writeEntry("Path", profile.path);
        entry.writeEntry("Authentication", authenticationKey(profile.authentication));
        if (profile.authentication == Authentication::KeyOrAgent && !profile.identityFile.isEmpty()) {
            entry.writeEntry("IdentityFile", profile.identityFile);
        } else {
            entry.deleteEntry("IdentityFile");
        }
    }
    group.writeEntry(orderKey, order);
    if (!group.sync()) {
        if (error) {
            *error = i18nc("@info", "Could not save remote connections.");
        }
        return false;
    }
    return true;
}
