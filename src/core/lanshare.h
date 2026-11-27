// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>
#include <QStringList>

#include <optional>

class QTcpServer;
class QTimer;

/**
 * Temporarily shares files and folders with browsers and WebDAV clients on the
 * local network. Every link contains a random token, shared items are read
 * only except for an optional upload folder, uploads never replace existing
 * files and sharing stops after a time limit.
 */
namespace LanShare
{
/// A shared file or folder, exposed under @c name.
struct Root {
    QString name;
    QString path;
    bool writable = false;
};

/// An entry in a folder listing.
struct Entry {
    QString name;
    bool isDir = false;
    qint64 size = 0;
    QDateTime modified;
};

/// A byte range of a file, inclusive.
struct Range {
    qint64 first = 0;
    qint64 last = 0;
};

/// A self-signed certificate and its private key, which are never saved.
struct Certificate {
    QSslCertificate certificate;
    QSslKey key;
    QString error;
};

/// The clients a share serves.
enum class Protocol {
    /// Folder pages for browsers and WebDAV for file managers.
    BrowsersAndWebDav,
    /// Folder pages and downloads only; WebDAV requests are refused.
    Browsers,
    /// WebDAV and downloads only; folders have no browser page.
    WebDav,
};

/// A network address and the interface it belongs to.
struct InterfaceAddress {
    QHostAddress address;
    /// The interface name, such as wlp2s0, or empty when unknown.
    QString interface;
};

/// A network interface that can serve shares.
struct NetworkInterface {
    QString name;
    /// The name with the kind of interface, such as "wlp2s0 (Wi-Fi)".
    QString label;
    bool up = false;
};

/// The defaults that Preferences › Remote and Cloud › LAN Sharing stores.
struct Defaults {
    Protocol protocol = Protocol::BrowsersAndWebDav;
    bool tls = true;
    /// Interfaces whose addresses aren't chosen at first.
    QStringList excludedInterfaces;
    /// The port, or 0 to choose one.
    quint16 port = 0;
    int timeoutMinutes = 30;
    bool allowUploads = false;
    /// The folder that receives uploads, never empty.
    QString uploadFolder;
};

/// The addresses of network interfaces that are up, excluding loopback and
/// IPv6 link-local addresses.
QList<QHostAddress> localAddresses();
/// localAddresses() with the interface of each address.
QList<InterfaceAddress> interfaceAddresses();
/// The network interfaces other than loopback, whether up or not.
QList<NetworkInterface> networkInterfaces();
/// Whether @p address is chosen when a share starts: it must be private or
/// loopback, and not on one of the @p excludedInterfaces.
bool chosenByDefault(const InterfaceAddress &address, const QStringList &excludedInterfaces);
/// The defaults saved in the settings.
Defaults defaultsFromSettings();
/// The folder that receives uploads when none is chosen: Downloads, or the
/// home folder.
QString defaultUploadFolder();
/// Whether @p address is private, shared or link-local, so it is normally
/// only reachable from the local network.
bool isPrivateAddress(const QHostAddress &address);
/// A random URL-safe token with 128 bits of entropy.
QString generateToken();
/// The link that opens a share on @p address.
QString shareUrl(const QHostAddress &address, quint16 port, const QString &token, bool tls);

/// Names for @p paths that are unique, ignoring case.
QStringList rootNames(const QStringList &paths);
/// The decoded segments of the absolute @p path, ignoring a trailing slash.
/// Returns nothing when a segment is empty, "." or "..", contains "/" or NUL,
/// or isn't valid UTF-8.
std::optional<QStringList> pathSegments(const QByteArray &path);
/// Whether @p path is @p root or inside it, after resolving symbolic links.
bool isInside(const QString &path, const QString &root);
/// The canonical local path of @p segments below @p root, or an empty string
/// when it doesn't exist, is hidden or leaves the root through a symbolic link.
QString resolve(const Root &root, const QStringList &segments);
/// A safe file name for an upload named @p name, or an empty string.
QString uploadName(const QString &name);
/// @p name, or "name (1).ext" and so on when @p folder already has that name.
QString uniqueName(const QString &folder, const QString &name);
/// The byte range requested by a Range header for a file of @p size bytes.
/// Returns nothing when the header is absent or not a single byte range, and a
/// range with @c first greater than @c last when it can't be satisfied.
std::optional<Range> parseRange(const QByteArray &header, qint64 size);

/// The HTML page listing @p entries of the folder at @p title. When @p uploads
/// is set, the page has an upload form whose script carries @p nonce.
QByteArray listingPage(const QString &title, const QList<Entry> &entries, bool canGoUp, bool uploads, const QByteArray &nonce);
/// The WebDAV multistatus body describing @p entries, whose links start with
/// the encoded @p href of their folder. The first entry describes the folder.
QByteArray propfind(const QByteArray &href, const QList<Entry> &entries);

/// The SHA-256 fingerprint of @p certificate as colon-separated hex.
QString certificateFingerprint(const QSslCertificate &certificate);
/// Creates a certificate for @p addresses valid for a day, using openssl.
Certificate createCertificate(const QList<QHostAddress> &addresses);
/// Whether TLS can be offered, naming what is missing otherwise.
QString tlsUnavailableReason();
}

/// The activity of one client.
struct LanShareSession {
    QString peer;
    int requests = 0;
    int active = 0;
    qint64 bytesSent = 0;
    qint64 bytesReceived = 0;
    QDateTime lastSeen;
};

class LanShareConnection;

/// Serves shared items over HTTP or HTTPS with a subset of WebDAV.
class LanShareServer : public QObject
{
    Q_OBJECT

public:
    struct Options {
        QStringList paths;
        QList<QHostAddress> addresses;
        /// The port on every address, or 0 to choose one.
        quint16 port = 0;
        bool tls = false;
        LanShare::Certificate certificate;
        /// The folder that receives uploads, or empty to refuse them.
        QString uploadFolder;
        LanShare::Protocol protocol = LanShare::Protocol::BrowsersAndWebDav;
        /// Stop after this many seconds, or 0 to keep sharing until stopped.
        int timeoutSeconds = 30 * 60;
        /// The link token, or empty to generate one.
        QString token;
    };

    explicit LanShareServer(QObject *parent = nullptr);
    ~LanShareServer() override;

    /// Starts sharing, returning an error message or an empty string.
    QString start(const Options &options);
    void stop(const QString &reason = QString());

    bool isRunning() const;
    quint16 port() const;
    QString token() const;
    bool tls() const;
    QList<QHostAddress> addresses() const;
    QStringList urls() const;
    QList<LanShare::Root> roots() const;
    /// The certificate used for HTTPS, or a null certificate.
    QSslCertificate certificate() const;
    /// When sharing stops by itself, or an invalid time.
    QDateTime stopsAt() const;
    QList<LanShareSession> sessions() const;

Q_SIGNALS:
    void sessionsChanged();
    void uploaded(const QString &path);
    void stopped(const QString &reason);

private:
    friend class LanShareConnection;

    void accept(QTcpServer *server);
    void connectionFinished(LanShareConnection *connection);
    LanShareSession &session(const QString &peer);
    void touchSessions();

    Options m_options;
    QList<LanShare::Root> m_roots;
    QList<QTcpServer *> m_servers;
    QList<LanShareConnection *> m_connections;
    QHash<QString, LanShareSession> m_sessions;
    QTimer *m_timeout = nullptr;
    QTimer *m_sessionsTimer = nullptr;
    QDateTime m_stopsAt;
    quint16 m_port = 0;
    bool m_running = false;
};
