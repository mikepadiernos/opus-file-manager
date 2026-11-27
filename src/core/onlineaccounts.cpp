// SPDX-License-Identifier: GPL-3.0-or-later

#include "onlineaccounts.h"

#include <KLocalizedString>

#include <QStandardPaths>

#include <algorithm>

#ifdef OPUSFM_HAVE_ACCOUNTS
#include <Accounts/Account>
#include <Accounts/Manager>
#include <Accounts/Provider>
#include <Accounts/Service>
#endif

namespace
{
const QString GoogleDrive = QStringLiteral("google-drive");
const QString DavStorage = QStringLiteral("dav-storage");
const QString DavFiles = QStringLiteral("/dav/files/");

QString userFromStoragePath(const QString &path)
{
    const int index = path.indexOf(DavFiles);
    if (index < 0) {
        return {};
    }
    return path.mid(index + DavFiles.size()).section(QLatin1Char('/'), 0, 0);
}

#ifdef OPUSFM_HAVE_ACCOUNTS
QString translated(const QString &catalog, const QString &text)
{
    if (catalog.isEmpty() || text.isEmpty()) {
        return text;
    }
    return i18nd(catalog.toUtf8().constData(), text.toUtf8().constData());
}
#endif
}

namespace OnlineAccounts
{
bool isFileService(const QString &serviceType)
{
    return serviceType == GoogleDrive || serviceType == DavStorage;
}

bool hasFileStorage(const Account &account)
{
    return std::any_of(account.services.cbegin(), account.services.cend(), [](const Service &service) {
        return isFileService(service.type);
    });
}

QString settingsId(const Account &account)
{
    return QString::number(account.id);
}

QList<Account> shownAccounts(const QList<Account> &accounts, const QStringList &hidden)
{
    QList<Account> result;
    for (const Account &account : accounts) {
        if (hasFileStorage(account) && !hidden.contains(settingsId(account))) {
            result.append(account);
        }
    }
    return result;
}

QUrl davUrl(const QString &host, const QString &storagePath, const QString &accountName)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QUrl server(trimmed.contains(QLatin1String("://")) ? trimmed : QStringLiteral("https://") + trimmed);
    const QString scheme = server.scheme().toLower();
    if (!server.isValid() || server.host().isEmpty()
        || (scheme != QLatin1String("https") && scheme != QLatin1String("http") && scheme != QLatin1String("webdavs") && scheme != QLatin1String("webdav"))) {
        return {};
    }

    QString base = server.path();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    QString path = storagePath.trimmed();
    if (!path.isEmpty() && !path.startsWith(QLatin1Char('/'))) {
        path.prepend(QLatin1Char('/'));
    }
    if (!base.isEmpty() && path != base && !path.startsWith(base + QLatin1Char('/'))) {
        path.prepend(base);
    }
    if (!path.endsWith(QLatin1Char('/'))) {
        path.append(QLatin1Char('/'));
    }

    QUrl url;
    url.setScheme(scheme == QLatin1String("http") || scheme == QLatin1String("webdav") ? QStringLiteral("webdav") : QStringLiteral("webdavs"));
    url.setHost(server.host());
    url.setPort(server.port());
    url.setPath(path);
    QString user = userFromStoragePath(path);
    if (user.isEmpty()) {
        user = accountName.section(QLatin1Char('@'), 0, 0).trimmed();
    }
    if (!user.isEmpty()) {
        url.setUserName(user);
    }
    return url;
}

QList<Location> locations(const Account &account, const ProtocolCheck &isKnownProtocol)
{
    QList<Location> result;
    for (const Service &service : account.services) {
        if (!isFileService(service.type)) {
            continue;
        }
        Location location;
        location.title = service.displayName.isEmpty() ? service.name : service.displayName;
        location.iconName = service.iconName.isEmpty() ? account.iconName : service.iconName;
        if (service.type == GoogleDrive) {
            const QString name = account.displayName.trimmed();
            if (name.isEmpty() || name.contains(QLatin1Char('/'))) {
                location.unavailableReason = i18nc("@info:tooltip", "The account has no name Google Drive can be opened by.");
            } else if (!isKnownProtocol(QStringLiteral("gdrive"))) {
                location.unavailableReason = i18nc("@info:tooltip", "Install kio-gdrive to browse Google Drive.");
            } else {
                location.url.setScheme(QStringLiteral("gdrive"));
                location.url.setPath(QLatin1Char('/') + name + QLatin1Char('/'));
            }
        } else {
            location.url = davUrl(service.host, service.storagePath, account.displayName);
            if (location.url.isEmpty()) {
                location.unavailableReason = i18nc("@info:tooltip", "The account has no valid server address.");
            } else if (!isKnownProtocol(location.url.scheme())) {
                location.unavailableReason = i18nc("@info:tooltip", "No KIO worker handles WebDAV. Install the KIO WebDAV worker.");
            }
        }
        if (!account.enabled) {
            location.unavailableReason = i18nc("@info:tooltip", "The account is turned off in Online Accounts.");
        } else if (!service.enabled) {
            location.unavailableReason = i18nc("@info:tooltip", "File access is turned off for this account in Online Accounts.");
        }
        if (!location.unavailableReason.isEmpty()) {
            location.url.clear();
        }
        result.append(location);
    }
    if (result.isEmpty()) {
        Location none;
        none.title = i18nc("@item", "No file access");
        none.iconName = account.iconName;
        none.unavailableReason = i18nc("@info:tooltip", "This account provides no file storage that Plasma Opus can open.");
        result.append(none);
    }
    return result;
}

bool isSupported()
{
#ifdef OPUSFM_HAVE_ACCOUNTS
    return true;
#else
    return false;
#endif
}

QList<Account> readAccounts(QString *error)
{
    QList<Account> result;
#ifdef OPUSFM_HAVE_ACCOUNTS
    Accounts::Manager manager(Accounts::Manager::DisableNotifications);
    const Accounts::AccountIdList ids = manager.accountList();
    if (manager.lastError().type() != Accounts::Error::NoError) {
        if (error) {
            *error = i18nc("@info", "Could not read KDE Online Accounts: %1", manager.lastError().message());
        }
        return {};
    }
    for (const Accounts::AccountId id : ids) {
        Accounts::Account *account = manager.account(id);
        if (!account) {
            continue;
        }
        Account info;
        info.id = id;
        info.displayName = account->displayName();
        info.provider = account->providerName();
        const Accounts::Provider provider = account->provider();
        info.providerDisplayName = translated(provider.trCatalog(), provider.displayName());
        if (info.providerDisplayName.isEmpty()) {
            info.providerDisplayName = info.provider;
        }
        info.iconName = provider.iconName();
        account->selectService();
        info.enabled = account->isEnabled();
        // KAccounts stores the server account-wide; a service may override it.
        const auto davSettings = [account](QString *host, QString *storagePath) {
            const QString serviceHost = account->value(QStringLiteral("dav/host")).toString();
            const QString server = account->value(QStringLiteral("server")).toString();
            const QString path = account->value(QStringLiteral("dav/storagePath")).toString();
            if (!serviceHost.isEmpty() || !server.isEmpty()) {
                *host = serviceHost.isEmpty() ? server : serviceHost;
            }
            if (!path.isEmpty()) {
                *storagePath = path;
            }
        };
        QString accountHost;
        QString accountStoragePath;
        davSettings(&accountHost, &accountStoragePath);
        const Accounts::ServiceList services = account->services();
        for (const Accounts::Service &service : services) {
            Service serviceInfo;
            serviceInfo.name = service.name();
            serviceInfo.type = service.serviceType();
            serviceInfo.displayName = translated(service.trCatalog(), service.displayName());
            serviceInfo.iconName = service.iconName();
            account->selectService(service);
            serviceInfo.enabled = account->isEnabled();
            if (serviceInfo.type == DavStorage) {
                serviceInfo.host = accountHost;
                serviceInfo.storagePath = accountStoragePath;
                davSettings(&serviceInfo.host, &serviceInfo.storagePath);
            }
            info.services.append(serviceInfo);
        }
        account->selectService();
        result.append(info);
    }
    std::stable_sort(result.begin(), result.end(), [](const Account &a, const Account &b) {
        const int provider = QString::localeAwareCompare(a.providerDisplayName, b.providerDisplayName);
        if (provider != 0) {
            return provider < 0;
        }
        const int name = QString::localeAwareCompare(a.displayName, b.displayName);
        return name != 0 ? name < 0 : a.id < b.id;
    });
    if (error) {
        error->clear();
    }
#else
    if (error) {
        *error = i18nc("@info", "Plasma Opus was built without KDE Online Accounts support.");
    }
#endif
    return result;
}

QStringList managerCommand()
{
    const QString kcmshell = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
    if (!kcmshell.isEmpty()) {
        return {kcmshell, QStringLiteral("kcm_kaccounts")};
    }
    const QString systemSettings = QStandardPaths::findExecutable(QStringLiteral("systemsettings"));
    if (!systemSettings.isEmpty()) {
        return {systemSettings, QStringLiteral("kcm_kaccounts")};
    }
    return {};
}
}
