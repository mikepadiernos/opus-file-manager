// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudaccount.h"

#include <KLocalizedString>

#include <QHostAddress>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace
{
const QString orderKey = QStringLiteral("Order");
const QString remotePhp = QStringLiteral("/remote.php/");

bool validId(const QString &id)
{
    return !QUuid::fromString(id).isNull() && QUuid::fromString(id).toString(QUuid::WithoutBraces) == id;
}

bool hasControlCharacter(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.category() == QChar::Other_Control;
    });
}

bool isLoopback(const QString &host)
{
    if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

bool isPlain(const QUrl &server)
{
    return server.scheme() == QLatin1String("http");
}

/// The server's base path, such as "/nextcloud", or an empty string.
QString basePath(const QUrl &server)
{
    QString path = server.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

QString identity(const NextcloudAccounts::Account &account)
{
    QUrl server = account.server;
    server.setHost(server.host().toLower());
    return server.toString(QUrl::StripTrailingSlash) + QLatin1Char('\n') + account.user;
}

QString userFromPath(const QString &path)
{
    static const QString files = QStringLiteral("/remote.php/dav/files/");
    const int start = path.indexOf(files);
    if (start < 0) {
        return {};
    }
    return path.mid(start + files.size()).section(QLatin1Char('/'), 0, 0);
}
}

QString NextcloudAccounts::groupName()
{
    return QStringLiteral("NextcloudAccounts");
}

QString NextcloudAccounts::defaultRealm()
{
    return QStringLiteral("Nextcloud");
}

QUrl NextcloudAccounts::serverUrl(const QString &text, QString *error)
{
    const QString trimmed = text.trimmed();
    QUrl url(trimmed.contains(QLatin1String("://")) ? trimmed : QStringLiteral("https://") + trimmed, QUrl::StrictMode);
    if (trimmed.isEmpty()) {
        url.clear();
    }
    if (url.isValid()) {
        // A link into the web interface or WebDAV folder names the same server.
        QString path = url.path();
        for (const QString &marker : {remotePhp, QStringLiteral("/index.php/"), QStringLiteral("/ocs/")}) {
            const int index = path.indexOf(marker);
            if (index >= 0) {
                path.truncate(index);
            }
        }
        if (path.endsWith(QLatin1String("/index.php"))) {
            path.chop(10);
        }
        while (path.endsWith(QLatin1Char('/'))) {
            path.chop(1);
        }
        url.setPath(path);
        url.setScheme(url.scheme().toLower());
        url.setHost(url.host().toLower());
        url.setFragment(QString());
        url.setQuery(QString());
    }
    const QString problem = trimmed.isEmpty() ? i18nc("@info", "Enter the address of the Nextcloud server.")
                                              : (url.isValid() ? serverError(url) : i18nc("@info", "The server address is not valid."));
    if (error) {
        *error = problem;
    }
    return problem.isEmpty() ? url : QUrl();
}

QString NextcloudAccounts::serverError(const QUrl &server)
{
    if (!server.isValid() || server.host().isEmpty()) {
        return i18nc("@info", "The server address is not valid.");
    }
    if (!server.userInfo().isEmpty()) {
        return i18nc("@info", "Leave the user name and password out of the server address. Plasma Opus asks for them when it signs in.");
    }
    if (server.hasQuery() || server.hasFragment()) {
        return i18nc("@info", "The server address is not valid.");
    }
    const QString scheme = server.scheme();
    if (scheme == QLatin1String("http")) {
        return isLoopback(server.host()) ? QString()
                                         : i18nc("@info", "Use an https:// address. Plain http would send the app password unencrypted with every request.");
    }
    if (scheme != QLatin1String("https")) {
        return i18nc("@info", "Use an https:// address.");
    }
    return {};
}

QString NextcloudAccounts::validationError(const Account &account)
{
    const QString server = serverError(account.server);
    if (!server.isEmpty()) {
        return server;
    }
    if (account.server.path().endsWith(QLatin1Char('/'))) {
        return i18nc("@info", "The server address is not valid.");
    }
    if (account.user.trimmed().isEmpty() || account.user != account.user.trimmed() || account.user.contains(QLatin1Char('/'))
        || hasControlCharacter(account.user)) {
        return i18nc("@info", "The account has no valid user name.");
    }
    if (hasControlCharacter(account.loginName) || hasControlCharacter(account.name) || hasControlCharacter(account.realm)) {
        return i18nc("@info", "The account details contain control characters.");
    }
    return {};
}

QString NextcloudAccounts::title(const Account &account)
{
    const QString name = account.name.trimmed();
    if (!name.isEmpty()) {
        return name;
    }
    return i18nc("@item %1 is a user name, %2 a server name", "%1 on %2", loginName(account), account.server.host());
}

QString NextcloudAccounts::loginName(const Account &account)
{
    return account.loginName.isEmpty() ? account.user : account.loginName;
}

QString NextcloudAccounts::realm(const Account &account)
{
    return account.realm.isEmpty() ? defaultRealm() : account.realm;
}

QUrl NextcloudAccounts::davUrl(const Account &account, const QString &path)
{
    QUrl url;
    url.setScheme(isPlain(account.server) ? QStringLiteral("webdav") : QStringLiteral("webdavs"));
    url.setHost(account.server.host());
    url.setPort(account.server.port());
    url.setUserName(loginName(account));
    url.setPath(basePath(account.server) + QStringLiteral("/remote.php/dav/") + path);
    return url;
}

QUrl NextcloudAccounts::filesUrl(const Account &account)
{
    return davUrl(account, QStringLiteral("files/") + account.user + QLatin1Char('/'));
}

QUrl NextcloudAccounts::webUrl(const Account &account, const QString &path)
{
    QUrl url = account.server;
    url.setPath(basePath(account.server) + QLatin1Char('/') + path);
    return url;
}

bool NextcloudAccounts::isNextcloudProvider(const QString &provider)
{
    return provider == QLatin1String("nextcloud") || provider == QLatin1String("owncloud");
}

QList<NextcloudAccounts::Account> NextcloudAccounts::fromOnlineAccounts(const QList<OnlineAccounts::Account> &accounts)
{
    QList<Account> result;
    for (const OnlineAccounts::Account &online : accounts) {
        if (!online.enabled || !isNextcloudProvider(online.provider)) {
            continue;
        }
        for (const OnlineAccounts::Service &service : online.services) {
            if (!service.enabled || service.type != QLatin1String("dav-storage")) {
                continue;
            }
            const QUrl dav = OnlineAccounts::davUrl(service.host, service.storagePath, online.displayName);
            const int remote = dav.path().indexOf(remotePhp);
            if (dav.isEmpty() || remote < 0) {
                continue;
            }
            Account account;
            account.id = QStringLiteral("kaccounts-%1").arg(online.id);
            account.name = online.displayName;
            account.server.setScheme(dav.scheme() == QLatin1String("webdav") ? QStringLiteral("http") : QStringLiteral("https"));
            account.server.setHost(dav.host());
            account.server.setPort(dav.port());
            account.server.setPath(dav.path().left(remote));
            account.loginName = online.displayName.trimmed();
            account.user = userFromPath(dav.path());
            if (account.user.isEmpty()) {
                account.user = account.loginName;
            }
            if (account.loginName == account.user) {
                account.loginName.clear();
            }
            account.source = Source::OnlineAccounts;
            account.onlineAccountId = online.id;
            if (validationError(account).isEmpty()) {
                result.append(account);
            }
            break;
        }
    }
    return result;
}

QList<NextcloudAccounts::Account> NextcloudAccounts::merged(const QList<Account> &saved, const QList<Account> &online)
{
    QList<Account> result = saved;
    QSet<QString> known;
    for (const Account &account : saved) {
        known.insert(identity(account));
    }
    for (const Account &account : online) {
        if (!known.contains(identity(account))) {
            known.insert(identity(account));
            result.append(account);
        }
    }
    return result;
}

std::optional<NextcloudAccounts::Account> NextcloudAccounts::accountForUrl(const QList<Account> &accounts, const QUrl &url)
{
    if (url.scheme() != QLatin1String("webdavs") && url.scheme() != QLatin1String("webdav")) {
        return std::nullopt;
    }
    const QString path = url.path().endsWith(QLatin1Char('/')) ? url.path() : url.path() + QLatin1Char('/');
    for (const Account &account : accounts) {
        const QUrl files = filesUrl(account);
        if (files.scheme() == url.scheme() && files.host().compare(url.host(), Qt::CaseInsensitive) == 0 && files.port() == url.port()
            && path.startsWith(files.path())) {
            return account;
        }
    }
    return std::nullopt;
}

QList<NextcloudAccounts::Account> NextcloudAccounts::load(const KConfigGroup &group, QString *error)
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
    QList<Account> accounts;
    QSet<QString> seenIds;
    QSet<QString> identities;
    int skipped = 0;
    for (const QString &id : std::as_const(ids)) {
        if (seenIds.contains(id) || !group.hasGroup(id)) {
            continue;
        }
        seenIds.insert(id);
        const KConfigGroup entry = group.group(id);
        Account account;
        account.id = id;
        account.name = entry.readEntry("Name", QString());
        account.server = QUrl(entry.readEntry("Server", QString()), QUrl::StrictMode);
        account.user = entry.readEntry("User", QString());
        account.loginName = entry.readEntry("LoginName", QString());
        account.realm = entry.readEntry("Realm", QString());
        if (!validId(id) || !validationError(account).isEmpty() || identities.contains(identity(account))) {
            ++skipped;
            continue;
        }
        identities.insert(identity(account));
        accounts.append(account);
    }
    if (skipped > 0 && error) {
        *error =
            i18ncp("@info", "One saved Nextcloud account is invalid and was skipped.", "%1 saved Nextcloud accounts are invalid and were skipped.", skipped);
    }
    return accounts;
}

bool NextcloudAccounts::save(KConfigGroup group, QList<Account> &accounts, QString *error)
{
    if (error) {
        error->clear();
    }
    QSet<QString> identities;
    QSet<QString> ids;
    for (const Account &account : std::as_const(accounts)) {
        if (account.source != Source::Login) {
            continue;
        }
        const QString problem = validationError(account);
        if (!problem.isEmpty() || identities.contains(identity(account)) || (!account.id.isEmpty() && (!validId(account.id) || ids.contains(account.id)))) {
            if (error) {
                *error = problem.isEmpty() ? i18nc("@info", "The account is already added.")
                                           : i18nc("@info %1 is an account name, %2 the problem", "%1: %2", title(account), problem);
            }
            return false;
        }
        identities.insert(identity(account));
        if (!account.id.isEmpty()) {
            ids.insert(account.id);
        }
    }
    QStringList order;
    for (Account &account : accounts) {
        if (account.source != Source::Login) {
            continue;
        }
        if (account.id.isEmpty()) {
            account.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        order.append(account.id);
    }
    const QStringList existing = group.groupList();
    for (const QString &id : existing) {
        if (!order.contains(id)) {
            group.deleteGroup(id);
        }
    }
    for (const Account &account : std::as_const(accounts)) {
        if (account.source != Source::Login) {
            continue;
        }
        KConfigGroup entry = group.group(account.id);
        entry.writeEntry("Name", account.name.trimmed());
        entry.writeEntry("Server", account.server.toString());
        entry.writeEntry("User", account.user);
        entry.writeEntry("LoginName", account.loginName);
        entry.writeEntry("Realm", account.realm);
    }
    group.writeEntry(orderKey, order);
    if (!group.sync()) {
        if (error) {
            *error = i18nc("@info", "Could not save the Nextcloud accounts.");
        }
        return false;
    }
    return true;
}
