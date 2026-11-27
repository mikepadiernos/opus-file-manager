// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "onlineaccounts.h"

#include <KConfigGroup>

#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

/**
 * Nextcloud accounts that Plasma Opus can browse. An account only records the
 * server, the user and how it was added: app passwords and other credentials
 * are never stored here. KDE's password server keeps them, in KDE Wallet when
 * it's enabled, and accounts from KDE Online Accounts keep theirs there.
 */
namespace NextcloudAccounts
{
enum class Source {
    /// Added with Nextcloud Login Flow in Plasma Opus.
    Login,
    /// Read from KDE Online Accounts; managed in System Settings.
    OnlineAccounts,
};

struct Account {
    /// A UUID for saved accounts, or "kaccounts-<id>" for Online Accounts.
    QString id;
    /// An optional name; title() falls back to the user and server.
    QString name;
    /// The server's web address, such as https://cloud.example.com or
    /// https://example.com/nextcloud, without a trailing slash.
    QUrl server;
    /// The user id that names the user's WebDAV folder.
    QString user;
    /// The name used to sign in, which can differ from the user id; empty
    /// uses the user id.
    QString loginName;
    /// The authentication realm the server's WebDAV interface reports, used
    /// to find the app password in KDE's password server; empty uses
    /// defaultRealm().
    QString realm;
    Source source = Source::Login;
    /// The KDE Online Accounts id, for accounts from there.
    quint32 onlineAccountId = 0;

    bool operator==(const Account &) const = default;
};

/// The configuration group that holds the saved accounts.
QString groupName();
/// The realm a Nextcloud server reports unless its theming renames it.
QString defaultRealm();

/**
 * The server address typed or pasted as @p text, such as cloud.example.com or
 * a link into the server's web interface or WebDAV folder. Plain http is only
 * accepted for this computer, because the app password is sent with every
 * request.
 * @param error receives why the address can't be used
 * @return the address without a trailing slash, or an empty URL
 */
QUrl serverUrl(const QString &text, QString *error = nullptr);
/// Why @p server can't be a Nextcloud server address, or an empty string.
QString serverError(const QUrl &server);
/// Why @p account can't be used, or an empty string when it is valid.
QString validationError(const Account &account);

/// The name shown in menus: the account's name, or user@host.
QString title(const Account &account);
/// The name used to sign in.
QString loginName(const Account &account);
/// The realm used to find the account's app password.
QString realm(const Account &account);
/// The webdavs:// URL of the user's files. It never contains a password.
QUrl filesUrl(const Account &account);
/// A webdavs:// URL under the server's remote.php/dav/, such as "photos/alice/".
QUrl davUrl(const Account &account, const QString &path);
/// An https:// URL under the server's address, such as "ocs/v2.php/cloud/user".
QUrl webUrl(const Account &account, const QString &path);

/// Whether @p provider is a KDE Online Accounts provider of Nextcloud storage.
bool isNextcloudProvider(const QString &provider);
/**
 * The enabled Nextcloud and ownCloud file storage of @p accounts, in their
 * order. Storage whose WebDAV path isn't under remote.php is left out.
 */
QList<Account> fromOnlineAccounts(const QList<OnlineAccounts::Account> &accounts);
/// @p saved followed by the accounts of @p online that aren't also saved.
QList<Account> merged(const QList<Account> &saved, const QList<Account> &online);
/// The account whose files @p url is in, if any.
std::optional<Account> accountForUrl(const QList<Account> &accounts, const QUrl &url);

/**
 * Reads the saved accounts in their saved order, skipping invalid entries.
 * @param error receives a message when some entries were skipped
 */
QList<Account> load(const KConfigGroup &group, QString *error = nullptr);
/**
 * Replaces the saved accounts, assigning identifiers to new ones in
 * @p accounts. Accounts from Online Accounts are not saved.
 * @return false when an account is invalid or two accounts share a server
 * and user, leaving the saved accounts unchanged, or when the configuration
 * can't be written
 */
bool save(KConfigGroup group, QList<Account> &accounts, QString *error = nullptr);
}
