// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudaccount.h"
#include "nextcloudassets.h"
#include "nextcloudorganize.h"

#include <KIO/AuthInfo>

#include <QByteArray>
#include <QFlags>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <memory>
#include <optional>

class KJob;
class QTimer;

namespace KIO
{
class Job;
}

/// Requests to a Nextcloud server's documented interfaces, and parsers for
/// their responses.
namespace Nextcloud
{
/// What the server and its apps offer, from ocs/v2.php/cloud/capabilities.
struct Capabilities {
    QString version;
    int major = 0;
    /// The name the server's theming gives the product, such as "Nextcloud".
    QString productName;
    /// Whether the response included the capabilities of the user's apps,
    /// which the server only reports to a signed-in user.
    bool authenticated = false;
    bool sharing = false;
    bool systemTags = false;
    bool dav = false;
    /// Whether WebDAV SEARCH is expected to work. Nextcloud doesn't report
    /// it, so it is inferred from the dav capability on Nextcloud 15 or later.
    bool davSearch = false;

    bool operator==(const Capabilities &) const = default;
};

struct User {
    QString id;
    QString displayName;
};

/// The first step of Login Flow v2.
struct LoginFlow {
    /// The page where the user signs in and grants access.
    QUrl loginUrl;
    QUrl pollUrl;
    QString token;
};

/// What Login Flow v2 returns once the user granted access.
struct LoginResult {
    QUrl server;
    QString loginName;
    QString appPassword;
};

/// A permission letter from the oc:permissions WebDAV property.
enum class Permission {
    Shared = 0x1, // S
    Shareable = 0x2, // R
    Mounted = 0x4, // M
    Readable = 0x8, // G
    Delete = 0x10, // D
    Rename = 0x20, // N
    Move = 0x40, // V
    Write = 0x80, // W
    CreateFile = 0x100, // C
    CreateFolder = 0x200, // K
};
Q_DECLARE_FLAGS(Permissions, Permission)

/// The permissions of a folder and its items, by item name.
struct FolderPermissions {
    std::optional<Permissions> folder;
    QHash<QString, Permissions> items;
};

/// An operation the server can forbid.
enum class Operation {
    Rename,
    Move,
    Delete,
    /// Creating or pasting items into a folder.
    Create,
    CreateFolder,
};

/// The result of checking an account.
struct Status {
    Capabilities capabilities;
    bool capabilitiesKnown = false;
    User user;
    /// Whether the account's sign-in worked.
    bool signedIn = false;
    /// Whether the server rejected the saved sign-in, so the user must sign in again.
    bool accessExpired = false;
    /// Whether the Photos app answers, or unknown.
    std::optional<bool> photos;
    QString error;
};

/// Parses a capabilities response. Returns nullopt and sets @p error when it isn't one.
std::optional<Capabilities> parseCapabilities(const QByteArray &json, QString *error = nullptr);
/// Parses an ocs/v2.php/cloud/user response.
std::optional<User> parseUser(const QByteArray &json, QString *error = nullptr);
/// The message of a failed OCS response, or an empty string.
QString ocsMessage(const QByteArray &json);
/**
 * Parses the response that starts Login Flow v2. The login page must be on
 * @p server's host, and both URLs must be https unless @p server isn't.
 */
std::optional<LoginFlow> parseLoginFlow(const QByteArray &json, const QUrl &server, QString *error = nullptr);
/// Parses the poll response that completes Login Flow v2.
std::optional<LoginResult> parseLoginResult(const QByteArray &json, QString *error = nullptr);

/// The permissions in an oc:permissions value, such as "RGDNVCK".
Permissions parsePermissions(const QString &letters);
/**
 * The permissions in a PROPFIND multistatus response for @p folder: the
 * folder's own and those of the items directly in it.
 */
std::optional<FolderPermissions> parseFolderPermissions(const QByteArray &xml, const QUrl &folder, QString *error = nullptr);
/// The PROPFIND body that asks for oc:permissions.
QString permissionsRequest();
/// Why the server forbids @p operation for an item or folder with @p permissions, or an empty string.
QString deniedReason(Operation operation, Permissions permissions);
/**
 * Why the server forbids @p operation on the items named @p names in the
 * folder that @p permissions describe, or an empty string when it allows it
 * or doesn't say. Create and CreateFolder check the folder itself; the other
 * operations check each named item.
 */
QString deniedReason(Operation operation, const FolderPermissions &permissions, const QStringList &names);

/// Whether a failed request means the saved sign-in no longer works.
bool isAccessExpired(int kioError, int httpStatus);
/// The HTTP status of a finished KIO job, or 0 when it has none.
int httpStatus(KJob *job);
/// A message for a failed request, from its KIO error, HTTP status and error text.
QString errorMessage(int kioError, int httpStatus, const QString &errorText);
/// One line per finding of @p status, for showing to the user.
QStringList describe(const Status &status);

/// How KDE's password server finds the app password of @p account.
KIO::AuthInfo authInfo(const NextcloudAccounts::Account &account);
/// The headers that sign in with @p loginName and @p password, joined for KIO's customHTTPHeader.
QString authorizationHeader(const QString &loginName, const QString &password);

/// Looks up or stores the app password of an account.
class CredentialStore
{
public:
    virtual ~CredentialStore() = default;
    /// The saved password of @p account, if any.
    virtual std::optional<QString> password(const NextcloudAccounts::Account &account) = 0;
    /// Saves @p password for @p account, in KDE Wallet when it's enabled.
    virtual void store(const NextcloudAccounts::Account &account, const QString &password) = 0;
    /// Forgets the password of @p account for this session.
    virtual void forget(const NextcloudAccounts::Account &account) = 0;
};

/// Keeps app passwords in KDE's password server, which saves them in KDE Wallet.
class PasswordServerStore : public CredentialStore
{
public:
    explicit PasswordServerStore(qlonglong windowId = 0);
    std::optional<QString> password(const NextcloudAccounts::Account &account) override;
    void store(const NextcloudAccounts::Account &account, const QString &password) override;
    void forget(const NextcloudAccounts::Account &account) override;

private:
    qlonglong m_windowId;
};
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Nextcloud::Permissions)

/**
 * Runs requests against Nextcloud servers through KIO, so that KIO verifies
 * TLS and proxies apply. Background requests never prompt: an invalid
 * certificate or a missing password fails the request with a message.
 */
class NextcloudClient : public QObject
{
    Q_OBJECT

public:
    explicit NextcloudClient(QObject *parent = nullptr);
    ~NextcloudClient() override;

    /// Where app passwords are looked up; nullptr sends no credentials. Not owned.
    void setCredentialStore(Nextcloud::CredentialStore *store);
    /// How often Login Flow asks whether the user granted access.
    void setPollInterval(int milliseconds);
    /// How long Login Flow waits for the user; the server's token expires after 20 minutes.
    void setLoginTimeout(int milliseconds);

    /// Checks the server's version and capabilities, the sign-in and the Photos app.
    void check(const NextcloudAccounts::Account &account);
    /// Starts Login Flow v2 on @p server. Emits loginStarted() with the page to open.
    void startLogin(const QUrl &server);
    void cancelLogin();
    bool isLoggingIn() const;
    /// Reads the permissions of @p folder and its items.
    void fetchPermissions(const NextcloudAccounts::Account &account, const QUrl &folder);
    /// Asks the server to delete the account's app password.
    void revokeAppPassword(const NextcloudAccounts::Account &account);
    /// Reads the server's properties and metadata of the file or folder @p item.
    void fetchMetadata(const NextcloudAccounts::Account &account, const QUrl &item);
    /**
     * Asks the server for its preview of the file @p fileId, shown at @p size,
     * instead of downloading the file. Fails when the server has no preview or
     * it is larger than Nextcloud::PreviewByteLimit.
     */
    void fetchPreview(const NextcloudAccounts::Account &account, const QString &fileId, const QSize &size, const QUrl &item);
    /// Marks @p item as one of the user's favourites on the server, or no longer one.
    void setFavorite(const NextcloudAccounts::Account &account, const QUrl &item, bool favorite);
    /// Lists the server's collaborative tags that the user can see.
    void fetchSystemTags(const NextcloudAccounts::Account &account);
    /// Lists the collaborative tags of the file or folder @p fileId.
    void fetchFileTags(const NextcloudAccounts::Account &account, const QString &fileId);
    /// Assigns the tag @p tagId to @p fileId, or removes it when @p assign is false.
    void setFileTag(const NextcloudAccounts::Account &account, const QString &fileId, const QString &tagId, bool assign);
    /// Creates a collaborative tag, or finds the existing one. Ask the user before calling this.
    void createTag(const NextcloudAccounts::Account &account, const QString &name);
    /// Lists the user's favourites, at most @p limit. Returns the ID that assetsListed() reports.
    int listFavorites(const NextcloudAccounts::Account &account, int limit);
    /// Lists the user's files with the tag @p tagId, at most @p limit. Returns the ID that assetsListed() reports.
    int listTagged(const NextcloudAccounts::Account &account, const QString &tagId, int limit);
    /// Searches the user's files on the server. Returns the ID that assetsListed() reports.
    int search(const NextcloudAccounts::Account &account, const Nextcloud::SearchQuery &query);

Q_SIGNALS:
    void checked(const QString &accountId, const Nextcloud::Status &status);
    void loginStarted(const QUrl &loginUrl);
    /// Access was granted. @p account has no id; @p appPassword must only be passed to a CredentialStore.
    void loginFinished(const NextcloudAccounts::Account &account, const QString &appPassword, const Nextcloud::Status &status);
    void loginFailed(const QString &error);
    void permissionsFetched(const QUrl &folder, const Nextcloud::FolderPermissions &permissions, const QString &error);
    void revoked(const QString &accountId, const QString &error);
    void metadataFetched(const QUrl &item, const Nextcloud::FileMetadata &metadata, const QString &error);
    /// The server's preview of @p item, or a null image and why there is none.
    void previewFetched(const QUrl &item, const QImage &image, const QString &error);
    void favoriteSet(const QUrl &item, bool favorite, const QString &error);
    void systemTagsFetched(const QString &accountId, const QList<Nextcloud::SystemTag> &tags, const QString &error);
    void fileTagsFetched(const QString &fileId, const QList<Nextcloud::SystemTag> &tags, const QString &error);
    void fileTagSet(const QString &fileId, const QString &tagId, bool assigned, const QString &error);
    /// A tag was created or already existed; @p tag has no ID when @p error says why it couldn't be.
    void tagCreated(const QString &accountId, const Nextcloud::SystemTag &tag, const QString &error);
    /// The results of the listing or search that returned @p request.
    void assetsListed(int request, const Nextcloud::AssetResults &results);

private:
    struct Check;
    void runCheck(const std::shared_ptr<Check> &check);
    void poll();
    void finishLogin(const Nextcloud::LoginResult &result);
    void failLogin(const QString &error);
    KIO::Job *ocsGet(const QUrl &url, const QString &loginName, const QString &password);
    void prepare(KIO::Job *job, const QString &loginName, const QString &password);
    std::optional<QString> password(const NextcloudAccounts::Account &account) const;
    int listFiles(const NextcloudAccounts::Account &account, const QString &tagId, int limit);

    Nextcloud::CredentialStore *m_store = nullptr;
    int m_pollInterval = 2000;
    int m_loginTimeout = 20 * 60 * 1000;
    QTimer *m_pollTimer;
    std::optional<Nextcloud::LoginFlow> m_login;
    QUrl m_loginServer;
    qint64 m_loginDeadline = 0;
    QPointer<KJob> m_loginJob;
    int m_loginGeneration = 0;
    int m_lastRequest = 0;
};
