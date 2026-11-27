// SPDX-License-Identifier: GPL-3.0-or-later

#include "lanshare.h"
#include "settings.h"

#include <KLocalizedString>

#include <QCollator>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMimeDatabase>
#include <QNetworkInterface>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QSet>
#include <QSslConfiguration>
#include <QSslServer>
#include <QSslSocket>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QXmlStreamWriter>

#include <algorithm>

namespace
{
constexpr qsizetype maxHeadBytes = 16 * 1024;
constexpr qsizetype maxHeaders = 100;
constexpr qint64 maxRequestBody = 1024 * 1024;
constexpr qint64 chunkBytes = 64 * 1024;
constexpr qint64 maxQueuedBytes = 256 * 1024;
constexpr qint64 freeSpaceMargin = 16 * 1024 * 1024;
constexpr int idleTimeout = 60 * 1000;
constexpr int maxConnections = 32;
constexpr int sessionsInterval = 500;
constexpr int opensslTimeout = 30 * 1000;

using Header = std::pair<QByteArray, QByteArray>;
using Headers = QList<Header>;

QByteArray reasonPhrase(int status)
{
    switch (status) {
    case 100:
        return "Continue";
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 206:
        return "Partial Content";
    case 207:
        return "Multi-Status";
    case 301:
        return "Moved Permanently";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 411:
        return "Length Required";
    case 413:
        return "Content Too Large";
    case 416:
        return "Range Not Satisfiable";
    case 431:
        return "Request Header Fields Too Large";
    case 503:
        return "Service Unavailable";
    case 505:
        return "HTTP Version Not Supported";
    case 507:
        return "Insufficient Storage";
    default:
        return "Internal Server Error";
    }
}

QByteArray httpDate(const QDateTime &time)
{
    return QLocale::c().toString(time.toUTC(), QStringLiteral("ddd, dd MMM yyyy hh:mm:ss 'GMT'")).toLatin1();
}

QByteArray encodeSegment(const QString &segment)
{
    return QUrl::toPercentEncoding(segment);
}

bool sameToken(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i) {
        difference |= a.at(i) ^ b.at(i);
    }
    return difference == 0;
}

QHostAddress withoutScope(QHostAddress address)
{
    address.setScopeId(QString());
    return address;
}

QString peerName(const QHostAddress &address)
{
    bool ok = false;
    const quint32 ipv4 = address.toIPv4Address(&ok);
    return ok ? QHostAddress(ipv4).toString() : withoutScope(address).toString();
}

LanShare::Entry entryFor(const QFileInfo &info, const QString &name)
{
    return {name, info.isDir(), info.isDir() ? 0 : info.size(), info.lastModified()};
}

bool hidden(const QString &segment)
{
    return segment.startsWith(QLatin1Char('.'));
}

QByteArray jsString(const QString &text)
{
    QByteArray json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    json = json.mid(1, json.size() - 2);
    return json.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026");
}
}

namespace LanShare
{
QList<InterfaceAddress> interfaceAddresses()
{
    QList<InterfaceAddress> ipv4;
    QList<InterfaceAddress> ipv6;
    const auto contains = [](const QList<InterfaceAddress> &list, const QHostAddress &address) {
        return std::any_of(list.cbegin(), list.cend(), [&address](const InterfaceAddress &item) {
            return item.address == address;
        });
    };
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const auto entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            const QHostAddress address = withoutScope(entry.ip());
            if (address.isNull() || address.isLoopback() || address.isMulticast()) {
                continue;
            }
            if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                if (!address.isLinkLocal() && !contains(ipv6, address)) {
                    ipv6.append({address, interface.name()});
                }
            } else if (address.protocol() == QAbstractSocket::IPv4Protocol && !contains(ipv4, address)) {
                ipv4.append({address, interface.name()});
            }
        }
    }
    return ipv4 + ipv6;
}

QList<QHostAddress> localAddresses()
{
    QList<QHostAddress> result;
    const auto addresses = interfaceAddresses();
    for (const InterfaceAddress &address : addresses) {
        result.append(address.address);
    }
    return result;
}

QList<NetworkInterface> networkInterfaces()
{
    QList<NetworkInterface> result;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags().testFlag(QNetworkInterface::IsLoopBack) || interface.name().isEmpty()) {
            continue;
        }
        QString kind;
        switch (interface.type()) {
        case QNetworkInterface::Ethernet:
            kind = i18nc("@item kind of network interface", "Ethernet");
            break;
        case QNetworkInterface::Wifi:
            kind = i18nc("@item kind of network interface", "Wi-Fi");
            break;
        case QNetworkInterface::Virtual:
            kind = i18nc("@item kind of network interface", "virtual");
            break;
        case QNetworkInterface::Ppp:
            kind = i18nc("@item kind of network interface", "point-to-point");
            break;
        default:
            break;
        }
        const QString name = interface.name();
        const QString label = kind.isEmpty() ? name : i18nc("@item %1 is a network interface, %2 its kind", "%1 (%2)", name, kind);
        const auto flags = interface.flags();
        result.append({name, label, flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning)});
    }
    return result;
}

bool chosenByDefault(const InterfaceAddress &address, const QStringList &excludedInterfaces)
{
    if (!isPrivateAddress(address.address) && !address.address.isLoopback()) {
        return false;
    }
    return address.interface.isEmpty() || !excludedInterfaces.contains(address.interface);
}

QString defaultUploadFolder()
{
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return downloads.isEmpty() ? QDir::homePath() : downloads;
}

Defaults defaultsFromSettings()
{
    Defaults defaults;
    switch (Settings::lanShareProtocol()) {
    case Settings::EnumLanShareProtocol::Browsers:
        defaults.protocol = Protocol::Browsers;
        break;
    case Settings::EnumLanShareProtocol::WebDav:
        defaults.protocol = Protocol::WebDav;
        break;
    default:
        defaults.protocol = Protocol::BrowsersAndWebDav;
        break;
    }
    defaults.tls = Settings::lanShareHttps();
    defaults.excludedInterfaces = Settings::lanShareExcludedInterfaces();
    defaults.port = quint16(std::clamp(Settings::lanSharePort(), 0, 65535));
    defaults.timeoutMinutes = std::clamp(Settings::lanShareTimeoutMinutes(), 1, 24 * 60);
    defaults.allowUploads = Settings::lanShareAllowUploads();
    const QUrl folder = Settings::lanShareUploadFolder();
    defaults.uploadFolder = folder.isLocalFile() && !folder.toLocalFile().isEmpty() ? folder.toLocalFile() : defaultUploadFolder();
    return defaults;
}

bool isPrivateAddress(const QHostAddress &address)
{
    static const QList<std::pair<QHostAddress, int>> subnets = {
        QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8")),
        QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12")),
        QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("100.64.0.0/10")),
        QHostAddress::parseSubnet(QStringLiteral("169.254.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("fc00::/7")),
        QHostAddress::parseSubnet(QStringLiteral("fe80::/10")),
    };
    bool ok = false;
    const quint32 ipv4 = address.toIPv4Address(&ok);
    const QHostAddress plain = ok ? QHostAddress(ipv4) : withoutScope(address);
    return std::any_of(subnets.cbegin(), subnets.cend(), [&plain](const auto &subnet) {
        return plain.isInSubnet(subnet);
    });
}

QString generateToken()
{
    QByteArray bytes(16, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), bytes.size() / int(sizeof(quint32)));
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString shareUrl(const QHostAddress &address, quint16 port, const QString &token, bool tls)
{
    const QHostAddress plain = withoutScope(address);
    const QString host = plain.protocol() == QAbstractSocket::IPv6Protocol ? QLatin1Char('[') + plain.toString() + QLatin1Char(']') : plain.toString();
    return QStringLiteral("%1://%2:%3/%4/").arg(tls ? QStringLiteral("https") : QStringLiteral("http"), host, QString::number(port), token);
}

QStringList rootNames(const QStringList &paths)
{
    QStringList names;
    QSet<QString> used;
    for (const QString &path : paths) {
        QString base = QFileInfo(QDir::cleanPath(path)).fileName();
        if (base.isEmpty() || hidden(base)) {
            base = base.isEmpty() ? i18nc("@item name of the shared root folder", "Root") : base.mid(1);
        }
        QString name = base;
        for (int i = 2; used.contains(name.toCaseFolded()); ++i) {
            name = QStringLiteral("%1 (%2)").arg(base).arg(i);
        }
        used.insert(name.toCaseFolded());
        names.append(name);
    }
    return names;
}

std::optional<QStringList> pathSegments(const QByteArray &path)
{
    if (!path.startsWith('/')) {
        return std::nullopt;
    }
    QByteArray trimmed = path.mid(1);
    if (trimmed.endsWith('/')) {
        trimmed.chop(1);
    }
    QStringList segments;
    if (trimmed.isEmpty()) {
        return segments;
    }
    const auto parts = trimmed.split('/');
    for (const QByteArray &part : parts) {
        const QByteArray decoded = QByteArray::fromPercentEncoding(part);
        const QString segment = QString::fromUtf8(decoded);
        if (decoded.isEmpty() || decoded == "." || decoded == ".." || decoded.contains('/') || decoded.contains('\0') || segment.toUtf8() != decoded) {
            return std::nullopt;
        }
        segments.append(segment);
    }
    return segments;
}

bool isInside(const QString &path, const QString &root)
{
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    if (canonicalPath.isEmpty() || canonicalRoot.isEmpty()) {
        return false;
    }
    if (canonicalPath == canonicalRoot || canonicalRoot == QLatin1String("/")) {
        return true;
    }
    return canonicalPath.startsWith(canonicalRoot + QLatin1Char('/'));
}

QString resolve(const Root &root, const QStringList &segments)
{
    if (std::any_of(segments.cbegin(), segments.cend(), hidden)) {
        return {};
    }
    const QFileInfo rootInfo(root.path);
    if (!segments.isEmpty() && !rootInfo.isDir()) {
        return {};
    }
    const QString path = segments.isEmpty() ? root.path : root.path + QLatin1Char('/') + segments.join(QLatin1Char('/'));
    if (!isInside(path, root.path)) {
        return {};
    }
    return QFileInfo(path).canonicalFilePath();
}

QString uploadName(const QString &name)
{
    QString clean;
    clean.reserve(name.size());
    for (const QChar c : name) {
        if (c == QLatin1Char('/') || c == QLatin1Char('\\') || c.category() == QChar::Other_Control) {
            clean.append(QLatin1Char('_'));
        } else {
            clean.append(c);
        }
    }
    clean = clean.trimmed();
    while (clean.startsWith(QLatin1Char('.'))) {
        clean.remove(0, 1);
    }
    clean = clean.trimmed();
    if (clean.isEmpty() || clean.toUtf8().size() > 255) {
        return {};
    }
    return clean;
}

QString uniqueName(const QString &folder, const QString &name)
{
    const QDir dir(folder);
    if (!QFileInfo::exists(dir.filePath(name)) && !QFileInfo(dir.filePath(name)).isSymLink()) {
        return name;
    }
    QString suffix = QMimeDatabase().suffixForFileName(name);
    if (suffix.isEmpty()) {
        suffix = QFileInfo(name).suffix();
    }
    const QString base = suffix.isEmpty() ? name : name.left(name.size() - suffix.size() - 1);
    for (int i = 1;; ++i) {
        const QString candidate = suffix.isEmpty() ? QStringLiteral("%1 (%2)").arg(base).arg(i) : QStringLiteral("%1 (%2).%3").arg(base).arg(i).arg(suffix);
        if (!QFileInfo::exists(dir.filePath(candidate)) && !QFileInfo(dir.filePath(candidate)).isSymLink()) {
            return candidate;
        }
    }
}

std::optional<Range> parseRange(const QByteArray &header, qint64 size)
{
    const QByteArray value = header.trimmed();
    if (!value.startsWith("bytes=")) {
        return std::nullopt;
    }
    const QByteArray spec = value.mid(6).trimmed();
    const qsizetype dash = spec.indexOf('-');
    if (dash < 0 || spec.contains(',')) {
        return std::nullopt;
    }
    const auto number = [](const QByteArray &text, bool *ok) -> qint64 {
        *ok = !text.isEmpty() && std::all_of(text.cbegin(), text.cend(), [](char c) {
            return c >= '0' && c <= '9';
        });
        return *ok ? text.toLongLong(ok) : 0;
    };
    const QByteArray from = spec.left(dash).trimmed();
    const QByteArray to = spec.mid(dash + 1).trimmed();
    const Range unsatisfiable{1, 0};
    bool ok = false;
    if (from.isEmpty()) {
        const qint64 suffix = number(to, &ok);
        if (!ok) {
            return std::nullopt;
        }
        if (suffix == 0 || size == 0) {
            return unsatisfiable;
        }
        return Range{std::max<qint64>(0, size - suffix), size - 1};
    }
    const qint64 first = number(from, &ok);
    if (!ok) {
        return std::nullopt;
    }
    qint64 last = size - 1;
    if (!to.isEmpty()) {
        last = number(to, &ok);
        if (!ok || last < first) {
            return std::nullopt;
        }
    }
    if (first >= size) {
        return unsatisfiable;
    }
    return Range{first, std::min(last, size - 1)};
}

QByteArray listingPage(const QString &title, const QList<Entry> &entries, bool canGoUp, bool uploads, const QByteArray &nonce)
{
    const QLocale locale = QLocale::system();
    const auto escape = [](const QString &text) {
        return text.toHtmlEscaped().toUtf8();
    };

    QByteArray html;
    html +=
        "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<meta name=\"referrer\" content=\"no-referrer\"><title>";
    html += escape(title);
    html += "</title><style nonce=\"" + nonce + "\">";
    html +=
        "body{font-family:system-ui,sans-serif;margin:2em auto;max-width:60em;padding:0 1em;color:#232629;background:#fcfcfc}"
        "@media (prefers-color-scheme:dark){body{color:#fcfcfc;background:#202326}a{color:#3daee9}}"
        "h1{font-size:1.4em;word-break:break-all}table{border-collapse:collapse;width:100%}"
        "th,td{text-align:left;padding:.4em .6em;border-bottom:1px solid #8884}"
        "td.size,th.size{text-align:right;white-space:nowrap}td.date{white-space:nowrap}"
        "a{color:#2980b9;text-decoration:none;word-break:break-all}a:hover{text-decoration:underline}"
        "form{margin:1em 0}footer{margin-top:2em;font-size:.85em;opacity:.7}";
    html += "</style></head><body><h1>";
    html += escape(title);
    html += "</h1>\n";
    if (uploads) {
        html += "<form id=\"upload\"><label>";
        html += escape(i18nc("@label upload form on a shared web page", "Upload files:"));
        html += " <input type=\"file\" id=\"files\" multiple></label> <span id=\"status\" role=\"status\"></span></form>\n";
    }
    html += "<table><thead><tr><th>";
    html += escape(i18nc("@title:column", "Name"));
    html += "</th><th class=\"size\">";
    html += escape(i18nc("@title:column", "Size"));
    html += "</th><th>";
    html += escape(i18nc("@title:column", "Modified"));
    html += "</th></tr></thead><tbody>\n";
    if (canGoUp) {
        html += "<tr><td><a href=\"../\">";
        html += escape(i18nc("@action:button link to the parent folder", "Parent Folder"));
        html += "</a></td><td></td><td></td></tr>\n";
    }
    for (const Entry &entry : entries) {
        html += "<tr><td><a href=\"" + encodeSegment(entry.name) + (entry.isDir ? "/" : "") + "\">";
        html += escape(entry.isDir ? entry.name + QLatin1Char('/') : entry.name);
        html += "</a></td><td class=\"size\">";
        if (!entry.isDir) {
            html += escape(locale.formattedDataSize(entry.size));
        }
        html += "</td><td class=\"date\">";
        if (entry.modified.isValid()) {
            html += escape(locale.toString(entry.modified, QLocale::ShortFormat));
        }
        html += "</td></tr>\n";
    }
    html += "</tbody></table>\n<footer>";
    html += escape(i18nc("@info footer of a shared web page", "Shared temporarily with Plasma Opus"));
    html += "</footer>\n";
    if (uploads) {
        html += "<script nonce=\"" + nonce + "\">\n";
        html += "const uploading=" + jsString(i18nc("@info:status %1 is a file name", "Uploading %1…")) + ";\n";
        html += "const failed=" + jsString(i18nc("@info:status %1 is a file name, %2 an HTTP status code", "Couldn't upload %1 (error %2)")) + ";\n";
        html +=
            "const input=document.getElementById('files'),status=document.getElementById('status');\n"
            "input.addEventListener('change',async()=>{\n"
            " for(const file of input.files){\n"
            "  status.textContent=uploading.replace('%1',file.name);\n"
            "  let response;\n"
            "  try{response=await fetch(encodeURIComponent(file.name),{method:'PUT',body:file});}catch(e){response={ok:false,status:0};}\n"
            "  if(!response.ok){status.textContent=failed.replace('%1',file.name).replace('%2',response.status);return;}\n"
            " }\n"
            " location.reload();\n"
            "});\n"
            "</script>\n";
    }
    html += "</body></html>\n";
    return html;
}

QByteArray propfind(const QByteArray &href, const QList<Entry> &entries)
{
    QByteArray body;
    QXmlStreamWriter xml(&body);
    xml.writeStartDocument();
    xml.writeNamespace(QStringLiteral("DAV:"), QStringLiteral("D"));
    xml.writeStartElement(QStringLiteral("DAV:"), QStringLiteral("multistatus"));
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const Entry &entry = entries.at(i);
        QByteArray link = href;
        if (i > 0) {
            link += encodeSegment(entry.name) + (entry.isDir ? "/" : "");
        }
        xml.writeStartElement(QStringLiteral("DAV:"), QStringLiteral("response"));
        xml.writeTextElement(QStringLiteral("DAV:"), QStringLiteral("href"), QString::fromLatin1(link));
        xml.writeStartElement(QStringLiteral("DAV:"), QStringLiteral("propstat"));
        xml.writeStartElement(QStringLiteral("DAV:"), QStringLiteral("prop"));
        xml.writeTextElement(QStringLiteral("DAV:"), QStringLiteral("displayname"), entry.name);
        xml.writeStartElement(QStringLiteral("DAV:"), QStringLiteral("resourcetype"));
        if (entry.isDir) {
            xml.writeEmptyElement(QStringLiteral("DAV:"), QStringLiteral("collection"));
        }
        xml.writeEndElement();
        if (!entry.isDir) {
            xml.writeTextElement(QStringLiteral("DAV:"), QStringLiteral("getcontentlength"), QString::number(entry.size));
        }
        if (entry.modified.isValid()) {
            xml.writeTextElement(QStringLiteral("DAV:"), QStringLiteral("getlastmodified"), QString::fromLatin1(httpDate(entry.modified)));
        }
        xml.writeEndElement();
        xml.writeTextElement(QStringLiteral("DAV:"), QStringLiteral("status"), QStringLiteral("HTTP/1.1 200 OK"));
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    return body;
}

QString certificateFingerprint(const QSslCertificate &certificate)
{
    if (certificate.isNull()) {
        return {};
    }
    return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex(':').toUpper());
}

Certificate createCertificate(const QList<QHostAddress> &addresses)
{
    Certificate result;
    const QString openssl = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    if (openssl.isEmpty()) {
        result.error = i18nc("@info", "Install openssl to share over HTTPS.");
        return result;
    }
    QTemporaryDir dir;
    if (!dir.isValid()) {
        result.error = i18nc("@info", "Couldn't create a temporary folder for the certificate.");
        return result;
    }
    const QString keyPath = dir.filePath(QStringLiteral("key.pem"));
    const QString certificatePath = dir.filePath(QStringLiteral("certificate.pem"));
    QStringList arguments = {QStringLiteral("req"),
                             QStringLiteral("-x509"),
                             QStringLiteral("-newkey"),
                             QStringLiteral("ec"),
                             QStringLiteral("-pkeyopt"),
                             QStringLiteral("ec_paramgen_curve:prime256v1"),
                             QStringLiteral("-nodes"),
                             QStringLiteral("-days"),
                             QStringLiteral("1"),
                             QStringLiteral("-subj"),
                             QStringLiteral("/CN=Plasma Opus"),
                             QStringLiteral("-keyout"),
                             keyPath,
                             QStringLiteral("-out"),
                             certificatePath};
    QStringList alternativeNames;
    for (const QHostAddress &address : addresses) {
        alternativeNames.append(QStringLiteral("IP:") + withoutScope(address).toString());
    }
    if (!alternativeNames.isEmpty()) {
        arguments << QStringLiteral("-addext") << QStringLiteral("subjectAltName=") + alternativeNames.join(QLatin1Char(','));
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(openssl, arguments);
    if (!process.waitForFinished(opensslTimeout) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        process.kill();
        const QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
        result.error = i18nc("@info %1 is an error message", "Couldn't create a certificate: %1", output.isEmpty() ? process.errorString() : output);
        return result;
    }

    QFile keyFile(keyPath);
    QFile certificateFile(certificatePath);
    if (keyFile.open(QIODevice::ReadOnly) && certificateFile.open(QIODevice::ReadOnly)) {
        result.key = QSslKey(keyFile.readAll(), QSsl::Ec);
        result.certificate = QSslCertificate(certificateFile.readAll());
    }
    if (result.key.isNull() || result.certificate.isNull()) {
        result.key = QSslKey();
        result.certificate = QSslCertificate();
        result.error = i18nc("@info", "Couldn't read the new certificate.");
    }
    return result;
}

QString tlsUnavailableReason()
{
    if (!QSslSocket::supportsSsl()) {
        return i18nc("@info", "Qt was built without TLS support.");
    }
    if (QStandardPaths::findExecutable(QStringLiteral("openssl")).isEmpty()) {
        return i18nc("@info", "Install openssl to share over HTTPS.");
    }
    return {};
}
}

class LanShareConnection : public QObject
{
public:
    LanShareConnection(LanShareServer *server, QTcpSocket *socket)
        : QObject(server)
        , m_server(server)
        , m_socket(socket)
        , m_peer(peerName(socket->peerAddress()))
    {
        socket->setParent(this);
        m_idle.setSingleShot(true);
        m_idle.setInterval(idleTimeout);
        connect(&m_idle, &QTimer::timeout, this, &LanShareConnection::abort);
        connect(socket, &QIODevice::readyRead, this, &LanShareConnection::readData);
        connect(socket, &QIODevice::bytesWritten, this, &LanShareConnection::wrote);
        if (auto *ssl = qobject_cast<QSslSocket *>(socket)) {
            connect(ssl, &QSslSocket::encryptedBytesWritten, this, &LanShareConnection::wrote);
        }
        connect(socket, &QAbstractSocket::disconnected, this, &LanShareConnection::finished);
        connect(socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
            if (error != QAbstractSocket::RemoteHostClosedError) {
                abort();
            }
        });
        m_idle.start();
    }

    QString peer() const
    {
        return m_peer;
    }

    void start()
    {
        if (m_socket->bytesAvailable() > 0) {
            readData();
        }
    }

    void refuse(int status)
    {
        respond(status);
    }

    void abort()
    {
        if (m_finished) {
            return;
        }
        m_socket->abort();
        finished();
    }

private:
    enum class State {
        Head,
        Body,
        Upload,
        Sending,
        Closing,
    };

    struct Request {
        QByteArray method;
        QByteArray path;
        QHash<QByteArray, QByteArray> headers;
    };

    void readData()
    {
        m_idle.start();
        if (m_state == State::Head) {
            m_buffer += m_socket->read(maxHeadBytes + 1 - m_buffer.size());
            const qsizetype end = m_buffer.indexOf("\r\n\r\n");
            if (end < 0) {
                if (m_buffer.size() > maxHeadBytes) {
                    respond(431);
                }
                return;
            }
            const QByteArray head = m_buffer.left(end);
            m_buffer.remove(0, end + 4);
            parseHead(head);
            return;
        }
        if (m_state == State::Body || m_state == State::Upload) {
            consumeBody();
            return;
        }
        // Ignore anything a client sends after its request.
        m_socket->readAll();
    }

    void parseHead(const QByteArray &head)
    {
        const QList<QByteArray> lines = head.split('\n');
        const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
        if (requestLine.size() != 3 || lines.size() > maxHeaders + 1) {
            respond(400);
            return;
        }
        if (!requestLine.at(2).startsWith("HTTP/1.")) {
            respond(505);
            return;
        }
        m_request.method = requestLine.at(0);
        QByteArray target = requestLine.at(1);
        const qsizetype query = target.indexOf('?');
        if (query >= 0) {
            target.truncate(query);
        }
        m_request.path = target;
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon <= 0) {
                respond(400);
                return;
            }
            m_request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
        }

        LanShareSession &session = m_server->session(m_peer);
        ++session.requests;
        session.lastSeen = QDateTime::currentDateTime();
        m_server->touchSessions();

        if (m_request.headers.contains("transfer-encoding")) {
            respond(411);
            return;
        }
        qint64 length = 0;
        if (m_request.headers.contains("content-length")) {
            bool ok = false;
            length = m_request.headers.value("content-length").toLongLong(&ok);
            if (!ok || length < 0) {
                respond(400);
                return;
            }
        } else if (m_request.method == "PUT") {
            respond(411);
            return;
        }
        m_remaining = length;
        if (m_request.method == "PUT") {
            startUpload();
            return;
        }
        if (length > maxRequestBody) {
            respond(413);
            return;
        }
        m_state = State::Body;
        consumeBody();
    }

    void consumeBody()
    {
        const QByteArray data = m_buffer + m_socket->read(std::max<qint64>(0, m_remaining - m_buffer.size()));
        m_buffer.clear();
        const QByteArray body = data.left(m_remaining);
        m_remaining -= body.size();
        if (!body.isEmpty()) {
            m_server->session(m_peer).bytesReceived += body.size();
            m_server->touchSessions();
        }
        if (m_state == State::Upload) {
            if (!body.isEmpty() && m_upload->write(body) != body.size()) {
                discardUpload();
                respond(507);
                return;
            }
            if (m_remaining == 0) {
                finishUpload();
            }
            return;
        }
        if (m_remaining == 0) {
            route();
        }
    }

    struct Target {
        bool found = false;
        bool virtualRoot = false;
        const LanShare::Root *root = nullptr;
        QStringList segments;
        bool trailingSlash = false;
    };

    Target target() const
    {
        Target result;
        const std::optional<QStringList> segments = LanShare::pathSegments(m_request.path);
        if (!segments || segments->isEmpty() || !sameToken(segments->first().toUtf8(), m_server->token().toUtf8())) {
            return result;
        }
        result.trailingSlash = m_request.path.endsWith('/');
        if (segments->size() == 1) {
            result.found = true;
            result.virtualRoot = true;
            return result;
        }
        for (const LanShare::Root &root : std::as_const(m_server->m_roots)) {
            if (root.name == segments->at(1)) {
                result.found = true;
                result.root = &root;
                result.segments = segments->mid(2);
                return result;
            }
        }
        return result;
    }

    QByteArray href(const Target &target, bool isDir) const
    {
        QByteArray link = '/' + m_server->token().toUtf8() + '/';
        if (target.root) {
            link += encodeSegment(target.root->name);
            for (const QString &segment : target.segments) {
                link += '/' + encodeSegment(segment);
            }
            if (isDir) {
                link += '/';
            }
        }
        return link;
    }

    QByteArray allowed() const
    {
        QByteArray methods = "OPTIONS, GET, HEAD";
        if (servesWebDav()) {
            methods += ", PROPFIND";
        }
        if (!m_server->m_options.uploadFolder.isEmpty()) {
            methods += ", PUT";
        }
        return methods;
    }

    bool servesWebDav() const
    {
        return m_server->m_options.protocol != LanShare::Protocol::Browsers;
    }

    bool servesBrowsers() const
    {
        return m_server->m_options.protocol != LanShare::Protocol::WebDav;
    }

    void route()
    {
        const Target t = target();
        const QByteArray &method = m_request.method;
        if (!t.found) {
            respond(404);
            return;
        }
        if (method == "OPTIONS") {
            Headers headers = {{"Allow", allowed()}};
            if (servesWebDav()) {
                headers += {{"DAV", "1"}, {"MS-Author-Via", "DAV"}};
            }
            respond(200, {}, {}, headers);
            return;
        }
        if (method != "GET" && method != "HEAD" && !(method == "PROPFIND" && servesWebDav())) {
            respond(405, {}, {}, {{"Allow", allowed()}});
            return;
        }

        QList<LanShare::Entry> entries;
        bool isDir = true;
        QString path;
        QString title;
        bool uploads = false;
        if (t.virtualRoot) {
            for (const LanShare::Root &root : std::as_const(m_server->m_roots)) {
                entries.append(entryFor(QFileInfo(root.path), root.name));
            }
            title = i18nc("@title shared web page listing all shared items", "Shared Items");
        } else {
            path = LanShare::resolve(*t.root, t.segments);
            if (path.isEmpty()) {
                respond(404);
                return;
            }
            const QFileInfo info(path);
            isDir = info.isDir();
            if (!isDir && !info.isFile()) {
                respond(403);
                return;
            }
            if (!isDir && t.trailingSlash) {
                respond(404);
                return;
            }
            uploads = t.root->writable;
            title = (QStringList{t.root->name} + t.segments).join(QLatin1Char('/'));
            if (isDir) {
                const QFileInfoList children = QDir(path).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::NoSort);
                for (const QFileInfo &child : children) {
                    if (!hidden(child.fileName()) && (child.isDir() || child.isFile()) && LanShare::isInside(child.absoluteFilePath(), t.root->path)) {
                        entries.append(entryFor(child, child.fileName()));
                    }
                }
            }
        }
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(entries.begin(), entries.end(), [&collator](const LanShare::Entry &a, const LanShare::Entry &b) {
            if (a.isDir != b.isDir) {
                return a.isDir;
            }
            return collator.compare(a.name, b.name) < 0;
        });

        if (method == "PROPFIND") {
            const QByteArray depth = m_request.headers.value("depth", "1");
            QList<LanShare::Entry> described;
            const QFileInfo self(path);
            described.append(t.virtualRoot ? LanShare::Entry{m_server->token(), true, 0, {}}
                                           : entryFor(self, t.segments.isEmpty() ? t.root->name : t.segments.last()));
            if (isDir && depth != "0") {
                described += entries;
            }
            respond(207, LanShare::propfind(href(t, isDir), described), "application/xml; charset=utf-8");
            return;
        }

        if (isDir) {
            if (!servesBrowsers()) {
                const QByteArray explanation = i18nc("@info shown to a browser that opens a share for WebDAV clients only",
                                                     "This share is for file managers and other WebDAV clients. Open this address in one of them to browse it.")
                                                   .toUtf8()
                    + '\n';
                respond(405, explanation, "text/plain; charset=utf-8", {{"Allow", allowed()}});
                return;
            }
            if (!t.trailingSlash) {
                respond(301, {}, {}, {{"Location", m_request.path + '/'}});
                return;
            }
            QByteArray nonce(16, Qt::Uninitialized);
            QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(nonce.data()), nonce.size() / int(sizeof(quint32)));
            nonce = nonce.toBase64();
            const QByteArray policy = "default-src 'none'; style-src 'nonce-" + nonce + "'; script-src 'nonce-" + nonce
                + "'; connect-src 'self'; img-src 'self' data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
            respond(200,
                    LanShare::listingPage(title, entries, !t.virtualRoot, uploads, nonce),
                    "text/html; charset=utf-8",
                    {{"Content-Security-Policy", policy}});
            return;
        }
        sendFile(path);
    }

    void sendFile(const QString &path)
    {
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::ReadOnly)) {
            respond(403);
            return;
        }
        const qint64 size = m_file.size();
        const QFileInfo info(path);
        Headers headers = {
            {"Content-Type", QMimeDatabase().mimeTypeForFile(info).name().toLatin1()},
            {"Accept-Ranges", "bytes"},
            {"Last-Modified", httpDate(info.lastModified())},
            {"Content-Security-Policy", "sandbox"},
        };
        int status = 200;
        qint64 first = 0;
        qint64 length = size;
        if (const auto range = LanShare::parseRange(m_request.headers.value("range"), size)) {
            if (range->first > range->last) {
                m_file.close();
                respond(416, {}, {}, {{"Content-Range", "bytes */" + QByteArray::number(size)}});
                return;
            }
            status = 206;
            first = range->first;
            length = range->last - range->first + 1;
            headers.append(
                {"Content-Range", "bytes " + QByteArray::number(range->first) + '-' + QByteArray::number(range->last) + '/' + QByteArray::number(size)});
        }
        writeHead(status, length, headers);
        if (m_request.method == "HEAD" || length == 0 || !m_file.seek(first)) {
            m_file.close();
            close();
            return;
        }
        m_remaining = length;
        m_state = State::Sending;
        ++m_server->session(m_peer).active;
        m_counted = true;
        m_server->touchSessions();
        sendMore();
    }

    qint64 queued() const
    {
        qint64 bytes = m_socket->bytesToWrite();
        if (const auto *ssl = qobject_cast<const QSslSocket *>(m_socket)) {
            bytes += ssl->encryptedBytesToWrite();
        }
        return bytes;
    }

    void wrote()
    {
        m_idle.start();
        if (m_state == State::Sending) {
            sendMore();
        }
    }

    void sendMore()
    {
        while (m_state == State::Sending && m_remaining > 0 && queued() < maxQueuedBytes) {
            const QByteArray chunk = m_file.read(std::min(chunkBytes, m_remaining));
            if (chunk.isEmpty()) {
                abort();
                return;
            }
            m_socket->write(chunk);
            m_remaining -= chunk.size();
            LanShareSession &session = m_server->session(m_peer);
            session.bytesSent += chunk.size();
            session.lastSeen = QDateTime::currentDateTime();
            m_server->touchSessions();
        }
        if (m_state == State::Sending && m_remaining == 0) {
            m_file.close();
            close();
        }
    }

    void startUpload()
    {
        const Target t = target();
        if (!t.found) {
            respond(404);
            return;
        }
        if (t.virtualRoot || !t.root->writable || t.segments.isEmpty() || t.trailingSlash) {
            respond(t.virtualRoot || !t.root->writable ? 403 : 405, {}, {}, {{"Allow", allowed()}});
            return;
        }
        const QString folder = LanShare::resolve(*t.root, t.segments.mid(0, t.segments.size() - 1));
        const QString name = LanShare::uploadName(t.segments.last());
        if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
            respond(404);
            return;
        }
        if (name.isEmpty()) {
            respond(400);
            return;
        }
        const QStorageInfo storage(folder);
        if (storage.isValid() && storage.bytesAvailable() >= 0 && m_remaining + freeSpaceMargin > storage.bytesAvailable()) {
            respond(507);
            return;
        }
        m_upload = std::make_unique<QFile>(folder + QStringLiteral("/.opus-upload-") + LanShare::generateToken());
        if (!m_upload->open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            m_upload.reset();
            respond(403);
            return;
        }
        m_uploadFolder = folder;
        m_uploadName = name;
        m_uploadTarget = t;
        m_uploadTarget.segments.removeLast();
        if (m_request.headers.value("expect").toLower() == "100-continue") {
            m_socket->write("HTTP/1.1 100 Continue\r\n\r\n");
        }
        m_state = State::Upload;
        ++m_server->session(m_peer).active;
        m_counted = true;
        m_server->touchSessions();
        consumeBody();
    }

    void finishUpload()
    {
        const bool written = m_upload->flush();
        m_upload->close();
        if (!written || m_upload->error() != QFileDevice::NoError) {
            discardUpload();
            respond(507);
            return;
        }
        QString name;
        bool renamed = false;
        for (int attempt = 0; attempt < 100 && !renamed; ++attempt) {
            name = LanShare::uniqueName(m_uploadFolder, m_uploadName);
            // QFile::rename never replaces an existing file.
            renamed = m_upload->rename(QDir(m_uploadFolder).filePath(name));
        }
        if (!renamed) {
            discardUpload();
            respond(500);
            return;
        }
        m_upload.reset();
        m_uploadTarget.segments.append(name);
        const QString path = QDir(m_uploadFolder).filePath(name);
        respond(201, {}, {}, {{"Location", href(m_uploadTarget, false)}});
        Q_EMIT m_server->uploaded(path);
    }

    void discardUpload()
    {
        if (m_upload) {
            m_upload->close();
            m_upload->remove();
            m_upload.reset();
        }
    }

    void writeHead(int status, qint64 length, const Headers &headers)
    {
        QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reasonPhrase(status) + "\r\n";
        head += "Date: " + httpDate(QDateTime::currentDateTimeUtc()) + "\r\n";
        head += "Connection: close\r\n";
        head += "Content-Length: " + QByteArray::number(length) + "\r\n";
        head += "Cache-Control: no-store\r\n";
        head += "Referrer-Policy: no-referrer\r\n";
        head += "X-Content-Type-Options: nosniff\r\n";
        head += "X-Frame-Options: DENY\r\n";
        for (const auto &[name, value] : headers) {
            head += name + ": " + value + "\r\n";
        }
        head += "\r\n";
        m_socket->write(head);
    }

    void respond(int status, const QByteArray &body = {}, const QByteArray &type = {}, const Headers &extra = {})
    {
        if (m_state == State::Closing) {
            return;
        }
        Headers headers = extra;
        QByteArray content = body;
        if (status >= 400 && content.isEmpty()) {
            content = QByteArray::number(status) + ' ' + reasonPhrase(status) + '\n';
            headers.append(Header{"Content-Type", "text/plain; charset=utf-8"});
        } else if (!type.isEmpty()) {
            headers.append(Header{"Content-Type", type});
        }
        writeHead(status, content.size(), headers);
        if (m_request.method != "HEAD") {
            m_socket->write(content);
            m_server->session(m_peer).bytesSent += content.size();
        }
        close();
    }

    void close()
    {
        m_state = State::Closing;
        m_socket->disconnectFromHost();
    }

    void finished()
    {
        if (m_finished) {
            return;
        }
        m_finished = true;
        m_idle.stop();
        discardUpload();
        if (m_counted) {
            --m_server->session(m_peer).active;
        }
        m_server->connectionFinished(this);
    }

    LanShareServer *m_server;
    QTcpSocket *m_socket;
    QString m_peer;
    QTimer m_idle;
    State m_state = State::Head;
    QByteArray m_buffer;
    Request m_request;
    qint64 m_remaining = 0;
    QFile m_file;
    std::unique_ptr<QFile> m_upload;
    QString m_uploadFolder;
    QString m_uploadName;
    Target m_uploadTarget;
    bool m_counted = false;
    bool m_finished = false;
};

LanShareServer::LanShareServer(QObject *parent)
    : QObject(parent)
    , m_timeout(new QTimer(this))
    , m_sessionsTimer(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        stop(i18nc("@info", "Sharing stopped because the time limit was reached."));
    });
    m_sessionsTimer->setSingleShot(true);
    m_sessionsTimer->setInterval(sessionsInterval);
    connect(m_sessionsTimer, &QTimer::timeout, this, &LanShareServer::sessionsChanged);
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
            stop();
        });
    }
}

LanShareServer::~LanShareServer()
{
    const QSignalBlocker blocker(this);
    stop();
}

QString LanShareServer::start(const Options &options)
{
    if (m_running) {
        const QSignalBlocker blocker(this);
        stop();
    }
    if (options.paths.isEmpty()) {
        return i18nc("@info", "Choose files or folders to share.");
    }
    if (options.addresses.isEmpty()) {
        return i18nc("@info", "Choose at least one network address.");
    }

    QStringList paths;
    for (const QString &path : options.paths) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty()) {
            return i18nc("@info %1 is a path", "%1 doesn't exist.", path);
        }
        if (!paths.contains(canonical)) {
            paths.append(canonical);
        }
    }
    QString uploadFolder;
    if (!options.uploadFolder.isEmpty()) {
        const QFileInfo info(options.uploadFolder);
        if (!info.isDir() || !info.isWritable()) {
            return i18nc("@info %1 is a path", "Uploads can't be saved in %1.", options.uploadFolder);
        }
        uploadFolder = info.canonicalFilePath();
        if (!paths.contains(uploadFolder)) {
            paths.append(uploadFolder);
        }
    }
    if (options.tls && (options.certificate.certificate.isNull() || options.certificate.key.isNull())) {
        return options.certificate.error.isEmpty() ? i18nc("@info", "Sharing over HTTPS needs a certificate.") : options.certificate.error;
    }

    m_options = options;
    m_options.uploadFolder = uploadFolder;
    if (m_options.token.isEmpty()) {
        m_options.token = LanShare::generateToken();
    }
    const QStringList names = LanShare::rootNames(paths);
    m_roots.clear();
    for (qsizetype i = 0; i < paths.size(); ++i) {
        m_roots.append({names.at(i), paths.at(i), paths.at(i) == uploadFolder});
    }
    m_sessions.clear();

    m_port = options.port;
    for (const QHostAddress &address : options.addresses) {
        QTcpServer *server = nullptr;
        if (options.tls) {
            auto *sslServer = new QSslServer(this);
            QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
            configuration.setLocalCertificate(options.certificate.certificate);
            configuration.setPrivateKey(options.certificate.key);
            configuration.setProtocol(QSsl::TlsV1_2OrLater);
            configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
            sslServer->setSslConfiguration(configuration);
            server = sslServer;
        } else {
            server = new QTcpServer(this);
        }
        server->setMaxPendingConnections(maxConnections);
        m_servers.append(server);
        if (!server->listen(address, m_port)) {
            const QString error = i18nc("@info %1 is a network address, %2 an error message",
                                        "Couldn't share on %1: %2",
                                        withoutScope(address).toString(),
                                        server->errorString());
            const QSignalBlocker blocker(this);
            stop();
            return error;
        }
        m_port = server->serverPort();
        connect(server, &QTcpServer::pendingConnectionAvailable, this, [this, server] {
            accept(server);
        });
    }

    m_running = true;
    if (options.timeoutSeconds > 0) {
        m_timeout->start(options.timeoutSeconds * 1000);
        m_stopsAt = QDateTime::currentDateTime().addSecs(options.timeoutSeconds);
    } else {
        m_stopsAt = {};
    }
    Q_EMIT sessionsChanged();
    return {};
}

void LanShareServer::stop(const QString &reason)
{
    const bool wasRunning = m_running;
    m_running = false;
    m_timeout->stop();
    m_stopsAt = {};
    for (QTcpServer *server : std::as_const(m_servers)) {
        server->close();
        server->deleteLater();
    }
    m_servers.clear();
    const auto connections = m_connections;
    for (LanShareConnection *connection : connections) {
        connection->abort();
    }
    m_connections.clear();
    if (wasRunning) {
        Q_EMIT stopped(reason);
    }
}

bool LanShareServer::isRunning() const
{
    return m_running;
}

quint16 LanShareServer::port() const
{
    return m_port;
}

QString LanShareServer::token() const
{
    return m_options.token;
}

bool LanShareServer::tls() const
{
    return m_options.tls;
}

QList<QHostAddress> LanShareServer::addresses() const
{
    return m_options.addresses;
}

QStringList LanShareServer::urls() const
{
    QStringList result;
    if (!m_running) {
        return result;
    }
    for (const QHostAddress &address : m_options.addresses) {
        result.append(LanShare::shareUrl(address, m_port, m_options.token, m_options.tls));
    }
    return result;
}

QList<LanShare::Root> LanShareServer::roots() const
{
    return m_roots;
}

QSslCertificate LanShareServer::certificate() const
{
    return m_options.tls ? m_options.certificate.certificate : QSslCertificate();
}

QDateTime LanShareServer::stopsAt() const
{
    return m_stopsAt;
}

QList<LanShareSession> LanShareServer::sessions() const
{
    QList<LanShareSession> result = m_sessions.values();
    std::sort(result.begin(), result.end(), [](const LanShareSession &a, const LanShareSession &b) {
        return a.lastSeen > b.lastSeen;
    });
    return result;
}

void LanShareServer::accept(QTcpServer *server)
{
    while (QTcpSocket *socket = server->nextPendingConnection()) {
        auto *connection = new LanShareConnection(this, socket);
        LanShareSession &peer = session(connection->peer());
        peer.lastSeen = QDateTime::currentDateTime();
        touchSessions();
        m_connections.append(connection);
        if (!m_running || m_connections.size() > maxConnections) {
            connection->refuse(503);
            continue;
        }
        connection->start();
    }
}

void LanShareServer::connectionFinished(LanShareConnection *connection)
{
    m_connections.removeOne(connection);
    touchSessions();
    connection->deleteLater();
}

LanShareSession &LanShareServer::session(const QString &peer)
{
    auto it = m_sessions.find(peer);
    if (it == m_sessions.end()) {
        it = m_sessions.insert(peer, LanShareSession{peer, 0, 0, 0, 0, QDateTime::currentDateTime()});
    }
    return it.value();
}

void LanShareServer::touchSessions()
{
    if (!m_sessionsTimer->isActive()) {
        m_sessionsTimer->start();
    }
}
