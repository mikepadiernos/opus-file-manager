// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

class QDBusMessage;
class QDBusPendingCallWatcher;

/**
 * Plasma Vault's encrypted vaults (CryFS, gocryptfs or EncFS), as reported by
 * its `org.kde.plasmavault` kded module. Plasma Vault owns the passwords,
 * mounting and configuration; Plasma Opus only lists vaults and asks Plasma
 * Vault to open, close or create them.
 */
namespace PlasmaVaults
{
/// Plasma Vault's own status values, sent as a 16-bit number over D-Bus.
enum class Status : quint16 {
    NotInitialized = 0,
    Opened = 1,
    Closed = 2,
    Creating = 3,
    Opening = 4,
    Closing = 5,
    Dismantling = 6,
    Dismantled = 7,
    DeviceMissing = 254,
    Error = 255,
};

struct Vault {
    QString name = {};
    /// The encrypted data folder, which identifies the vault.
    QString device = {};
    QString mountPoint = {};
    Status status = Status::NotInitialized;
    /// Plasma Vault's last error or status message, if any.
    QString message = {};
    QStringList activities = {};
    bool offlineOnly = false;

    bool isOpen() const;
    bool isBusy() const;
    /// The name, or the device when the vault has none.
    QString displayName() const;
};

/// A process using a mount point, through its working folder or an open file.
struct User {
    qint64 pid = 0;
    QString name = {};
};

/// A translated description of a status, such as "Open" or "Closing…".
QString statusText(Status status);
/// The theme icon name for a vault's state.
QString iconName(const Vault &vault);
/**
 * Whether @p status says only whether a vault is open or closed, or is
 * opening or closing, rather than reporting a problem.
 */
bool isLockState(Status status);
/// The vaults whose encrypted data folder is not in @p hidden, in the same order.
QList<Vault> shownVaults(const QList<Vault> &vaults, const QStringList &hidden);
/// Whether @p path is @p mountPoint or inside it.
bool contains(const QString &mountPoint, const QString &path);
/**
 * The processes of this user whose working folder, root or open files are
 * inside @p mountPoint, read from @p procRoot. Processes that can't be read
 * are skipped.
 */
QList<User> usersOf(const QString &mountPoint, const QString &procRoot = QStringLiteral("/proc"));

/// The kded service, object path and interface of Plasma Vault.
inline constexpr QLatin1StringView Service{"org.kde.kded6"};
inline constexpr QLatin1StringView Path{"/modules/plasmavault"};
inline constexpr QLatin1StringView Interface{"org.kde.plasmavault"};
}

QDBusArgument &operator<<(QDBusArgument &argument, const PlasmaVaults::Vault &vault);
const QDBusArgument &operator>>(const QDBusArgument &argument, PlasmaVaults::Vault &vault);

Q_DECLARE_METATYPE(PlasmaVaults::Vault)

/// Plasma Vault, as far as Plasma Opus needs it.
class VaultClient : public QObject
{
    Q_OBJECT

public:
    enum class Request {
        Open,
        Close,
        Configure,
        Create,
    };

    using QObject::QObject;

    /// The vaults Plasma Vault knows, or none with @p error set when it is unavailable.
    virtual QList<PlasmaVaults::Vault> vaults(QString *error) const = 0;
    /**
     * Asks Plasma Vault to open, close or configure the vault at @p device, or
     * to create a new vault, with Plasma Vault's own dialogs. Failures are
     * reported through failed().
     */
    virtual void request(Request request, const QString &device = {}) = 0;

Q_SIGNALS:
    /// Vaults were added or removed, or Plasma Vault started or stopped.
    void changed();
    /// A vault's status or message changed.
    void vaultChanged(const PlasmaVaults::Vault &vault);
    /// A request could not be sent, with a translated reason.
    void failed(const QString &message);
};

/// Talks to Plasma Vault's kded module over the session bus.
class DBusVaultClient : public VaultClient
{
    Q_OBJECT

public:
    explicit DBusVaultClient(QObject *parent = nullptr);

    QList<PlasmaVaults::Vault> vaults(QString *error) const override;
    void request(Request request, const QString &device = {}) override;

private Q_SLOTS:
    void onVaultChanged(const QDBusMessage &message);
};
