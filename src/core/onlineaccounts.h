// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

/// Turns accounts configured in KDE Online Accounts (KAccounts) into folders
/// Plasma Opus can browse. Plasma Opus only reads the account list; adding,
/// removing and signing in to accounts stays in System Settings, and access
/// tokens and passwords are never read, logged or stored.
namespace OnlineAccounts
{
/// A service an account provides, such as Google Drive or Nextcloud storage.
struct Service {
    /// The service identifier, such as "google-drive".
    QString name;
    /// The service type, such as "google-drive" or "dav-storage".
    QString type;
    QString displayName;
    QString iconName;
    bool enabled = false;
    /// The WebDAV server for "dav-storage" services, with or without a scheme.
    QString host;
    /// The WebDAV path for "dav-storage" services, such as
    /// "/remote.php/dav/files/user".
    QString storagePath;

    bool operator==(const Service &other) const = default;
};

/// An account as stored by KDE Online Accounts.
struct Account {
    quint32 id = 0;
    QString displayName;
    /// The provider identifier, such as "google" or "nextcloud".
    QString provider;
    QString providerDisplayName;
    QString iconName;
    bool enabled = false;
    QList<Service> services;

    bool operator==(const Account &other) const = default;
};

/// A folder an account gives access to, or the reason it cannot be opened.
struct Location {
    QString title;
    QString iconName;
    /// The KIO URL to open. Empty when the location is unavailable.
    QUrl url;
    /// Why the location cannot be opened, or an empty string.
    QString unavailableReason;

    bool isAvailable() const
    {
        return unavailableReason.isEmpty() && url.isValid() && !url.isEmpty();
    }
    bool operator==(const Location &other) const = default;
};

/// Reports whether a KIO worker handles a protocol, such as "gdrive".
using ProtocolCheck = std::function<bool(const QString &protocol)>;

/// Whether @p serviceType gives access to files.
bool isFileService(const QString &serviceType);

/// Whether @p account offers file storage that Plasma Opus can open, so that
/// it can be shown, even if the storage is currently unavailable.
bool hasFileStorage(const Account &account);

/// The id under which the settings remember @p account.
QString settingsId(const Account &account);

/// The accounts of @p accounts to show, in their order: those with file
/// storage whose settingsId() is not in @p hidden. Hiding an account only
/// leaves it out here; the account itself is never changed.
QList<Account> shownAccounts(const QList<Account> &accounts, const QStringList &hidden);

/// The KIO URL of a WebDAV storage service. A host without a scheme, or with
/// "https", uses webdavs; "http" uses webdav. The user name comes from a
/// "/dav/files/<user>" storage path, or else from @p accountName up to any
/// "@". Returns an empty URL when @p host is empty or invalid.
QUrl davUrl(const QString &host, const QString &storagePath, const QString &accountName);

/// The folders @p account gives access to, one per file service, in the
/// order the account lists them. An account without a file service yields a
/// single unavailable location that explains why.
QList<Location> locations(const Account &account, const ProtocolCheck &isKnownProtocol);

/// Whether Plasma Opus was built with KDE Online Accounts support.
bool isSupported();

/// Reads the accounts from KDE Online Accounts, sorted by provider and then
/// name. On failure, or without KDE Online Accounts support, returns an empty
/// list and sets @p error to a localized description.
QList<Account> readAccounts(QString *error);

/// The program and arguments that open the Online Accounts page of System
/// Settings, or an empty list when no launcher is installed.
QStringList managerCommand();
}
