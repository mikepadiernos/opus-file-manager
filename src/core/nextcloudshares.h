// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDate>
#include <QFlags>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace NextcloudAccounts
{
struct Account;
}

/**
 * The shares of the Nextcloud sharing app, from its OCS interface at
 * ocs/v2.php/apps/files_sharing/api/v1/shares. Share passwords are only
 * sent to the server, never kept: a share only records whether it has one.
 */
namespace Nextcloud
{
enum class ShareType {
    User = 0,
    Group = 1,
    PublicLink = 3,
    Email = 4,
    Federated = 6,
    Team = 7,
    Talk = 10,
    /// A type Plasma Opus doesn't manage, such as a share with a deck board.
    Other = -1,
};

/// A permission bit of a share.
enum class SharePermission {
    Read = 1,
    Update = 2,
    Create = 4,
    Delete = 8,
    Share = 16,
};
Q_DECLARE_FLAGS(SharePermissions, SharePermission)

struct Share {
    /// The server's numeric ID of the share.
    QString id;
    ShareType type = ShareType::Other;
    /// The item's path in the files of the user who listed it, such as /Photos/a.jpg.
    QString path;
    bool folder = false;
    QString owner;
    QString ownerDisplayName;
    /// Who the share is with; empty for public links.
    QString shareWith;
    QString shareWithDisplayName;
    SharePermissions permissions;
    /// When the share expires, or an invalid date when it doesn't.
    QDate expiration;
    /// Whether a password protects the share. The password itself is never kept.
    bool hasPassword = false;
    /// The public link, for link shares.
    QUrl url;
    QString label;
    QString note;
    /// Whether the user may change or revoke the share, as the server reports.
    bool canEdit = true;
    bool canDelete = true;

    bool operator==(const Share &) const = default;
};

enum class ShareScope {
    /// The shares of one file or folder, including reshares by others.
    Item,
    /// The shares others made with the user.
    SharedWithMe,
    /// The shares the user made.
    SharedByMe,
};

struct ShareListing {
    ShareScope scope = ShareScope::Item;
    /// The item's path, for ShareScope::Item.
    QString path;
    QList<Share> shares;
    QString error;
    /// Whether the server doesn't offer sharing, such as when the sharing app is disabled.
    bool unavailable = false;
};

/// What the sharing app allows, from the files_sharing capabilities.
struct ShareCapabilities {
    /// Whether the sharing interface is enabled.
    bool api = false;
    bool publicLinks = false;
    /// Whether public links must have a password.
    bool passwordEnforced = false;
    /// Whether public links must expire, and within how many days.
    bool expireEnforced = false;
    int expireDays = 0;
    /// Whether public links to folders may accept uploads.
    bool publicUpload = false;
    bool resharing = false;
    bool groupSharing = true;
    /// The permissions the server gives new user and group shares.
    SharePermissions defaultPermissions = SharePermissions::fromInt(31);

    bool operator==(const ShareCapabilities &) const = default;
};

/// A share to create.
struct NewShare {
    /// The item's path in the user's files, such as /Photos/a.jpg.
    QString path;
    ShareType type = ShareType::PublicLink;
    /// The user or group, for user and group shares.
    QString shareWith;
    /// Empty lets the server choose.
    SharePermissions permissions;
    /// An invalid date sets no expiry.
    QDate expiration;
    /// Sent once and never kept; empty sets none.
    QString password;
};

/// Changes to a share; only the set fields change.
struct ShareUpdate {
    std::optional<SharePermissions> permissions;
    /// An invalid date removes the expiry.
    std::optional<QDate> expiration;
    /// Sent once and never kept; an empty password removes it.
    std::optional<QString> password;

    bool isEmpty() const
    {
        return !permissions && !expiration && !password;
    }
};

/// The ShareType of an OCS share_type value.
ShareType shareType(int value);
/// A name for @p type, such as "Public link".
QString shareTypeName(ShareType type);
/// A short description of @p permissions, such as "View, edit".
QString describePermissions(SharePermissions permissions, bool folder);

/// Parses one share of an OCS response's data. Password hashes are never kept.
std::optional<Share> parseShare(const QByteArray &json, QString *error = nullptr);
/// Parses an OCS response listing shares.
std::optional<QList<Share>> parseShares(const QByteArray &json, QString *error = nullptr);
/// Parses the files_sharing part of a capabilities response.
std::optional<ShareCapabilities> parseShareCapabilities(const QByteArray &json, QString *error = nullptr);

/// The path of @p file in the files of @p account, such as /Photos/a.jpg; empty for the root or an item elsewhere.
QString sharePath(const NextcloudAccounts::Account &account, const QUrl &file);
/// The webdav URL of the item that @p share shares, in the files of @p account.
QUrl shareFileUrl(const NextcloudAccounts::Account &account, const Share &share);
/// The OCS URL of the shares, or of the share @p id; empty when @p id isn't a share ID.
QUrl sharesUrl(const NextcloudAccounts::Account &account, const QString &id = QString());

/// The form-encoded body that creates @p share. Sets @p error and returns nothing when it is invalid.
std::optional<QByteArray> createShareBody(const NewShare &share, QString *error = nullptr);
/// The form-encoded body that applies @p update.
QByteArray updateShareBody(const ShareUpdate &update);
/**
 * Whether applying @p update to @p share gives more people access, or the
 * same people more access or for longer: added permissions, a removed or
 * later expiry, or a removed password.
 */
bool broadensAccess(const Share &share, const ShareUpdate &update);
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Nextcloud::SharePermissions)
