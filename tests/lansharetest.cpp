// SPDX-License-Identifier: GPL-3.0-or-later

#include "lanshare.h"
#include "settings.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace
{
const QString token = QStringLiteral("testtoken");

struct Response {
    int status = 0;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
};

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class LanShareTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        QDir root(m_dir.path());
        QVERIFY(root.mkpath(QStringLiteral("shared/sub")));
        QVERIFY(root.mkpath(QStringLiteral("uploads")));
        writeFile(path(QStringLiteral("shared/hello.txt")), "Hello, world!");
        writeFile(path(QStringLiteral("shared/sub/inner.txt")), "inner");
        writeFile(path(QStringLiteral("shared/.hidden")), "hidden");
        writeFile(path(QStringLiteral("shared/<b>.txt")), "markup");
        writeFile(path(QStringLiteral("secret.txt")), "secret");
        QVERIFY(QFile::link(path(QStringLiteral("secret.txt")), path(QStringLiteral("shared/escape.txt"))));
        QVERIFY(QFile::link(path(QStringLiteral("shared/hello.txt")), path(QStringLiteral("shared/link.txt"))));
    }

    void cleanup()
    {
        m_server.stop();
    }

    void splitsPathSegments()
    {
        QCOMPARE(LanShare::pathSegments("/"), QStringList());
        QCOMPARE(LanShare::pathSegments("/a/b%20c/"), (QStringList{QStringLiteral("a"), QStringLiteral("b c")}));
        QCOMPARE(LanShare::pathSegments("/a/%C3%A9"), (QStringList{QStringLiteral("a"), QStringLiteral("é")}));
        QVERIFY(!LanShare::pathSegments("a/b"));
        QVERIFY(!LanShare::pathSegments("/a//b"));
        QVERIFY(!LanShare::pathSegments("/a/../b"));
        QVERIFY(!LanShare::pathSegments("/a/%2E%2E/b"));
        QVERIFY(!LanShare::pathSegments("/a/b%2Fc"));
        QVERIFY(!LanShare::pathSegments("/a/b%00c"));
        QVERIFY(!LanShare::pathSegments("/a/%FF"));
    }

    void parsesRanges()
    {
        QVERIFY(!LanShare::parseRange("", 10));
        QVERIFY(!LanShare::parseRange("items=0-1", 10));
        QVERIFY(!LanShare::parseRange("bytes=0-1,3-4", 10));
        QVERIFY(!LanShare::parseRange("bytes=5-2", 10));
        QVERIFY(!LanShare::parseRange("bytes=a-2", 10));
        const auto check = [](const QByteArray &header, qint64 first, qint64 last) {
            const auto range = LanShare::parseRange(header, 10);
            QVERIFY(range);
            QCOMPARE(range->first, first);
            QCOMPARE(range->last, last);
        };
        check("bytes=0-4", 0, 4);
        check("bytes=7-", 7, 9);
        check("bytes=-3", 7, 9);
        check("bytes=-30", 0, 9);
        check("bytes=5-100", 5, 9);
        const auto unsatisfiable = LanShare::parseRange("bytes=10-", 10);
        QVERIFY(unsatisfiable && unsatisfiable->first > unsatisfiable->last);
    }

    void namesRootsAndUploads()
    {
        QCOMPARE(LanShare::rootNames({QStringLiteral("/a/Photos"), QStringLiteral("/b/photos"), QStringLiteral("/c/.config")}),
                 (QStringList{QStringLiteral("Photos"), QStringLiteral("photos (2)"), QStringLiteral("config")}));
        QCOMPARE(LanShare::uploadName(QStringLiteral("  ..notes.txt ")), QStringLiteral("notes.txt"));
        QCOMPARE(LanShare::uploadName(QStringLiteral("a\\b\tc")), QStringLiteral("a_b_c"));
        QCOMPARE(LanShare::uploadName(QStringLiteral("..")), QString());
        QCOMPARE(LanShare::uploadName(QString(300, QLatin1Char('x'))), QString());

        const QString folder = path(QStringLiteral("shared"));
        QCOMPARE(LanShare::uniqueName(folder, QStringLiteral("new.txt")), QStringLiteral("new.txt"));
        QCOMPARE(LanShare::uniqueName(folder, QStringLiteral("hello.txt")), QStringLiteral("hello (1).txt"));
        QCOMPARE(LanShare::uniqueName(folder, QStringLiteral("sub")), QStringLiteral("sub (1)"));
    }

    void classifiesAddresses()
    {
        QVERIFY(LanShare::isPrivateAddress(QHostAddress(QStringLiteral("192.168.1.20"))));
        QVERIFY(LanShare::isPrivateAddress(QHostAddress(QStringLiteral("10.1.2.3"))));
        QVERIFY(LanShare::isPrivateAddress(QHostAddress(QStringLiteral("172.20.0.1"))));
        QVERIFY(LanShare::isPrivateAddress(QHostAddress(QStringLiteral("fd12::1"))));
        QVERIFY(!LanShare::isPrivateAddress(QHostAddress(QStringLiteral("172.32.0.1"))));
        QVERIFY(!LanShare::isPrivateAddress(QHostAddress(QStringLiteral("8.8.8.8"))));
        QVERIFY(!LanShare::isPrivateAddress(QHostAddress(QStringLiteral("2001:db8::1"))));

        QCOMPARE(LanShare::shareUrl(QHostAddress(QStringLiteral("192.168.1.20")), 8080, token, false), QStringLiteral("http://192.168.1.20:8080/testtoken/"));
        QCOMPARE(LanShare::shareUrl(QHostAddress(QStringLiteral("fd12::1")), 443, token, true), QStringLiteral("https://[fd12::1]:443/testtoken/"));

        const QString first = LanShare::generateToken();
        QCOMPARE(first.size(), 22);
        QVERIFY(first != LanShare::generateToken());
        QVERIFY(!first.contains(QLatin1Char('/')) && !first.contains(QLatin1Char('+')));
    }

    void resolvesInsideRoots()
    {
        const LanShare::Root root{QStringLiteral("shared"), QFileInfo(path(QStringLiteral("shared"))).canonicalFilePath(), false};
        QCOMPARE(LanShare::resolve(root, {QStringLiteral("hello.txt")}), root.path + QStringLiteral("/hello.txt"));
        QCOMPARE(LanShare::resolve(root, {QStringLiteral("link.txt")}), root.path + QStringLiteral("/hello.txt"));
        QCOMPARE(LanShare::resolve(root, {}), root.path);
        QVERIFY(LanShare::resolve(root, {QStringLiteral("escape.txt")}).isEmpty());
        QVERIFY(LanShare::resolve(root, {QStringLiteral(".hidden")}).isEmpty());
        QVERIFY(LanShare::resolve(root, {QStringLiteral("missing")}).isEmpty());
        const LanShare::Root file{QStringLiteral("hello.txt"), root.path + QStringLiteral("/hello.txt"), false};
        QVERIFY(LanShare::resolve(file, {QStringLiteral("x")}).isEmpty());
    }

    void servesFiles()
    {
        startServer();
        Response response = get("/testtoken/shared/hello.txt");
        QCOMPARE(response.status, 200);
        QCOMPARE(response.body, QByteArray("Hello, world!"));
        QCOMPARE(response.headers.value("content-type"), QByteArray("text/plain"));
        QCOMPARE(response.headers.value("content-security-policy"), QByteArray("sandbox"));
        QCOMPARE(response.headers.value("connection"), QByteArray("close"));
        QCOMPARE(response.headers.value("referrer-policy"), QByteArray("no-referrer"));

        response = request("HEAD /testtoken/shared/hello.txt HTTP/1.1\r\nHost: x\r\n\r\n");
        QCOMPARE(response.status, 200);
        QCOMPARE(response.headers.value("content-length"), QByteArray("13"));
        QVERIFY(response.body.isEmpty());

        response = request("GET /testtoken/shared/hello.txt HTTP/1.1\r\nRange: bytes=7-\r\n\r\n");
        QCOMPARE(response.status, 206);
        QCOMPARE(response.body, QByteArray("world!"));
        QCOMPARE(response.headers.value("content-range"), QByteArray("bytes 7-12/13"));

        response = request("GET /testtoken/shared/hello.txt HTTP/1.1\r\nRange: bytes=20-\r\n\r\n");
        QCOMPARE(response.status, 416);
        QCOMPARE(response.headers.value("content-range"), QByteArray("bytes */13"));

        QCOMPARE(m_server.sessions().size(), 1);
        QCOMPARE(m_server.sessions().first().peer, QStringLiteral("127.0.0.1"));
        QVERIFY(m_server.sessions().first().requests >= 4);
    }

    void streamsLargeFiles()
    {
        QByteArray content(3 * 1024 * 1024 + 17, Qt::Uninitialized);
        for (qsizetype i = 0; i < content.size(); ++i) {
            content[i] = char(i % 251);
        }
        writeFile(path(QStringLiteral("shared/large.bin")), content);
        startServer();
        const Response response = get("/testtoken/shared/large.bin");
        QFile::remove(path(QStringLiteral("shared/large.bin")));
        QCOMPARE(response.status, 200);
        QCOMPARE(response.body.size(), content.size());
        QVERIFY(response.body == content);
    }

    void listsFolders()
    {
        startServer();
        Response response = get("/testtoken/");
        QCOMPARE(response.status, 200);
        QVERIFY(response.body.contains("href=\"shared/\""));

        response = get("/testtoken/shared");
        QCOMPARE(response.status, 301);
        QCOMPARE(response.headers.value("location"), QByteArray("/testtoken/shared/"));

        response = get("/testtoken/shared/");
        QCOMPARE(response.status, 200);
        QVERIFY(response.headers.value("content-type").startsWith("text/html"));
        QVERIFY(response.headers.value("content-security-policy").contains("default-src 'none'"));
        QVERIFY(response.body.contains("href=\"sub/\""));
        QVERIFY(response.body.contains("href=\"hello.txt\""));
        QVERIFY(response.body.contains("href=\"%3Cb%3E.txt\">&lt;b&gt;.txt<"));
        QVERIFY(!response.body.contains(".hidden"));
        QVERIFY(!response.body.contains("escape.txt"));
        QVERIFY(!response.body.contains("<script"));
        QVERIFY(response.body.indexOf("sub/") < response.body.indexOf("hello.txt"));
    }

    void rejectsOutsideRequests()
    {
        startServer();
        QCOMPARE(get("/wrongtoken/shared/hello.txt").status, 404);
        QCOMPARE(get("/").status, 404);
        QCOMPARE(get("/testtoken/missing/").status, 404);
        QCOMPARE(get("/testtoken/shared/../secret.txt").status, 404);
        QCOMPARE(get("/testtoken/shared/%2E%2E/secret.txt").status, 404);
        QCOMPARE(get("/testtoken/shared/escape.txt").status, 404);
        QCOMPARE(get("/testtoken/shared/.hidden").status, 404);
        QCOMPARE(get("/testtoken/shared/hello.txt/").status, 404);
        QCOMPARE(request("DELETE /testtoken/shared/hello.txt HTTP/1.1\r\n\r\n").status, 405);
        QCOMPARE(request("GET /testtoken/ SPDY/3\r\n\r\n").status, 505);
        QCOMPARE(request("GET /testtoken/ HTTP/1.1\r\nX: " + QByteArray(20000, 'a') + "\r\n\r\n").status, 431);
        QCOMPARE(request("PUT /testtoken/shared/new.txt HTTP/1.1\r\nContent-Length: 1\r\n\r\nx").status, 403);
        QVERIFY(!QFile::exists(path(QStringLiteral("shared/new.txt"))));
    }

    void answersWebDav()
    {
        startServer();
        Response response = request("OPTIONS /testtoken/ HTTP/1.1\r\n\r\n");
        QCOMPARE(response.status, 200);
        QCOMPARE(response.headers.value("dav"), QByteArray("1"));
        QVERIFY(!response.headers.value("allow").contains("PUT"));

        const QByteArray body = "<?xml version=\"1.0\"?><propfind xmlns=\"DAV:\"><allprop/></propfind>";
        response = request("PROPFIND /testtoken/shared/ HTTP/1.1\r\nDepth: 1\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        QCOMPARE(response.status, 207);
        QVERIFY(response.body.contains("<D:href>/testtoken/shared/</D:href>"));
        QVERIFY(response.body.contains("<D:href>/testtoken/shared/sub/</D:href>"));
        QVERIFY(response.body.contains("<D:href>/testtoken/shared/hello.txt</D:href>"));
        QVERIFY(response.body.contains("<D:getcontentlength>13</D:getcontentlength>"));
        QVERIFY(response.body.contains("<D:collection/>"));

        response = request("PROPFIND /testtoken/shared/ HTTP/1.1\r\nDepth: 0\r\n\r\n");
        QCOMPARE(response.status, 207);
        QVERIFY(!response.body.contains("hello.txt"));
    }

    void acceptsUploads()
    {
        writeFile(path(QStringLiteral("uploads/a b.txt")), "original");
        startServer(path(QStringLiteral("uploads")));
        QSignalSpy uploaded(&m_server, &LanShareServer::uploaded);

        Response response = get("/testtoken/uploads/");
        QCOMPARE(response.status, 200);
        QVERIFY(response.body.contains("<script nonce="));
        QVERIFY(request("OPTIONS /testtoken/ HTTP/1.1\r\n\r\n").headers.value("allow").contains("PUT"));

        response = request("PUT /testtoken/uploads/a%20b.txt HTTP/1.1\r\nContent-Length: 7\r\nExpect: 100-continue\r\n\r\nupdated");
        QCOMPARE(response.status, 201);
        QCOMPARE(response.headers.value("location"), QByteArray("/testtoken/uploads/a%20b%20%281%29.txt"));
        QCOMPARE(readFile(path(QStringLiteral("uploads/a b.txt"))), QByteArray("original"));
        QCOMPARE(readFile(path(QStringLiteral("uploads/a b (1).txt"))), QByteArray("updated"));
        QCOMPARE(uploaded.size(), 1);
        QCOMPARE(QFileInfo(uploaded.first().first().toString()).fileName(), QStringLiteral("a b (1).txt"));

        response = request("PUT /testtoken/uploads/.profile HTTP/1.1\r\nContent-Length: 2\r\n\r\nok");
        QCOMPARE(response.status, 201);
        QVERIFY(QFile::exists(path(QStringLiteral("uploads/profile"))));

        QCOMPARE(request("PUT /testtoken/uploads/b.txt HTTP/1.1\r\n\r\n").status, 411);
        QCOMPARE(request("PUT /testtoken/uploads/b.txt HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n").status, 411);
        QCOMPARE(request("PUT /testtoken/uploads/missing/b.txt HTTP/1.1\r\nContent-Length: 1\r\n\r\nx").status, 404);
        QCOMPARE(request("PUT /testtoken/shared/b.txt HTTP/1.1\r\nContent-Length: 1\r\n\r\nx").status, 403);
        QVERIFY(!QFile::exists(path(QStringLiteral("uploads/b.txt"))));

        const QStringList leftovers = QDir(path(QStringLiteral("uploads"))).entryList({QStringLiteral(".opus-upload-*")}, QDir::Files | QDir::Hidden);
        QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(QLatin1Char(','))));
    }

    void servesOverTls()
    {
        if (const QString reason = LanShare::tlsUnavailableReason(); !reason.isEmpty()) {
            QSKIP(qPrintable(reason));
        }
        const LanShare::Certificate certificate = LanShare::createCertificate({QHostAddress(QHostAddress::LocalHost)});
        QVERIFY2(certificate.error.isEmpty(), qPrintable(certificate.error));
        QCOMPARE(LanShare::certificateFingerprint(certificate.certificate).size(), 32 * 3 - 1);

        LanShareServer::Options options = baseOptions();
        options.tls = true;
        options.certificate = certificate;
        const QString error = m_server.start(options);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(m_server.urls().first().startsWith(QStringLiteral("https://127.0.0.1:")));

        QSslCertificate peer;
        const Response response = request("GET /testtoken/shared/hello.txt HTTP/1.1\r\n\r\n", &peer);
        QCOMPARE(response.status, 200);
        QCOMPARE(response.body, QByteArray("Hello, world!"));
        QCOMPARE(LanShare::certificateFingerprint(peer), LanShare::certificateFingerprint(certificate.certificate));

        LanShareServer::Options missing = baseOptions();
        missing.tls = true;
        QVERIFY(!m_server.start(missing).isEmpty());
        QVERIFY(!m_server.isRunning());
    }

    void stopsAfterTimeLimit()
    {
        LanShareServer::Options options = baseOptions();
        options.timeoutSeconds = 1;
        QVERIFY(m_server.start(options).isEmpty());
        QVERIFY(m_server.stopsAt().isValid());
        QSignalSpy stopped(&m_server, &LanShareServer::stopped);
        QVERIFY(stopped.wait(5000));
        QVERIFY(!m_server.isRunning());
        QVERIFY(!stopped.first().first().toString().isEmpty());
        QVERIFY(m_server.urls().isEmpty());
    }

    void servesOnlyTheChosenClients()
    {
        const QByteArray propfind = "PROPFIND /testtoken/shared/ HTTP/1.1\r\nDepth: 1\r\n\r\n";
        LanShareServer::Options options = baseOptions();
        options.protocol = LanShare::Protocol::Browsers;
        QVERIFY(m_server.start(options).isEmpty());
        Response response = request("OPTIONS /testtoken/ HTTP/1.1\r\n\r\n");
        QCOMPARE(response.status, 200);
        QVERIFY(!response.headers.contains("dav"));
        QVERIFY(!response.headers.value("allow").contains("PROPFIND"));
        QCOMPARE(request(propfind).status, 405);
        QCOMPARE(get("/testtoken/shared/").status, 200);
        QCOMPARE(get("/testtoken/shared/hello.txt").body, QByteArray("Hello, world!"));

        options.protocol = LanShare::Protocol::WebDav;
        QVERIFY(m_server.start(options).isEmpty());
        response = request("OPTIONS /testtoken/ HTTP/1.1\r\n\r\n");
        QCOMPARE(response.headers.value("dav"), QByteArray("1"));
        QVERIFY(response.headers.value("allow").contains("PROPFIND"));
        QCOMPARE(request(propfind).status, 207);
        response = get("/testtoken/shared/");
        QCOMPARE(response.status, 405);
        QVERIFY(!response.body.contains("<html"));
        QCOMPARE(get("/testtoken/").status, 405);
        QCOMPARE(get("/testtoken/shared/hello.txt").body, QByteArray("Hello, world!"));

        options.protocol = LanShare::Protocol::BrowsersAndWebDav;
        QVERIFY(m_server.start(options).isEmpty());
        QCOMPARE(request(propfind).status, 207);
        QCOMPARE(get("/testtoken/shared/").status, 200);
    }

    void choosesAddressesByInterface()
    {
        const LanShare::InterfaceAddress wifi{QHostAddress(QStringLiteral("192.168.1.2")), QStringLiteral("wlan0")};
        const LanShare::InterfaceAddress vpn{QHostAddress(QStringLiteral("10.8.0.2")), QStringLiteral("tun0")};
        const LanShare::InterfaceAddress publicAddress{QHostAddress(QStringLiteral("8.8.8.8")), QStringLiteral("eth0")};
        const LanShare::InterfaceAddress unknown{QHostAddress(QHostAddress::LocalHost), QString()};
        QVERIFY(LanShare::chosenByDefault(wifi, {}));
        QVERIFY(LanShare::chosenByDefault(vpn, {}));
        QVERIFY(!LanShare::chosenByDefault(publicAddress, {}));
        QVERIFY(LanShare::chosenByDefault(unknown, {}));
        const QStringList excluded{QStringLiteral("tun0")};
        QVERIFY(LanShare::chosenByDefault(wifi, excluded));
        QVERIFY(!LanShare::chosenByDefault(vpn, excluded));
        QVERIFY(LanShare::chosenByDefault(unknown, excluded));

        QList<QHostAddress> expected;
        const auto addresses = LanShare::interfaceAddresses();
        for (const LanShare::InterfaceAddress &address : addresses) {
            QVERIFY(!address.interface.isEmpty());
            expected.append(address.address);
        }
        QCOMPARE(LanShare::localAddresses(), expected);
        const auto interfaces = LanShare::networkInterfaces();
        for (const LanShare::NetworkInterface &interface : interfaces) {
            QVERIFY(!interface.name.isEmpty());
            QVERIFY(interface.label.startsWith(interface.name));
        }
    }

    void readsTheDefaultsFromTheSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        Settings::self()->setDefaults();
        Settings::self()->save();
        LanShare::Defaults defaults = LanShare::defaultsFromSettings();
        QCOMPARE(defaults.protocol, LanShare::Protocol::BrowsersAndWebDav);
        QVERIFY(defaults.tls);
        QVERIFY(defaults.excludedInterfaces.isEmpty());
        QCOMPARE(defaults.port, quint16(0));
        QCOMPARE(defaults.timeoutMinutes, 30);
        QVERIFY(!defaults.allowUploads);
        QCOMPARE(defaults.uploadFolder, LanShare::defaultUploadFolder());
        QVERIFY(!defaults.uploadFolder.isEmpty());

        Settings::setLanShareProtocol(Settings::EnumLanShareProtocol::WebDav);
        Settings::setLanShareHttps(false);
        Settings::setLanShareExcludedInterfaces({QStringLiteral("tun0")});
        Settings::setLanSharePort(8443);
        Settings::setLanShareTimeoutMinutes(90);
        Settings::setLanShareAllowUploads(true);
        Settings::setLanShareUploadFolder(QUrl::fromLocalFile(path(QStringLiteral("uploads"))));
        Settings::self()->save();
        defaults = LanShare::defaultsFromSettings();
        QCOMPARE(defaults.protocol, LanShare::Protocol::WebDav);
        QVERIFY(!defaults.tls);
        QCOMPARE(defaults.excludedInterfaces, QStringList{QStringLiteral("tun0")});
        QCOMPARE(defaults.port, quint16(8443));
        QCOMPARE(defaults.timeoutMinutes, 90);
        QVERIFY(defaults.allowUploads);
        QCOMPARE(defaults.uploadFolder, path(QStringLiteral("uploads")));

        Settings::setLanShareProtocol(Settings::EnumLanShareProtocol::Browsers);
        defaults = LanShare::defaultsFromSettings();
        QCOMPARE(defaults.protocol, LanShare::Protocol::Browsers);
    }

    void validatesOptions()
    {
        LanShareServer::Options options = baseOptions();
        options.paths = {path(QStringLiteral("missing"))};
        QVERIFY(!m_server.start(options).isEmpty());
        options = baseOptions();
        options.addresses.clear();
        QVERIFY(!m_server.start(options).isEmpty());
        options = baseOptions();
        options.uploadFolder = path(QStringLiteral("shared/hello.txt"));
        QVERIFY(!m_server.start(options).isEmpty());
        options = baseOptions();
        options.token.clear();
        QVERIFY(m_server.start(options).isEmpty());
        QCOMPARE(m_server.token().size(), 22);
    }

private:
    QString path(const QString &relative) const
    {
        return m_dir.filePath(relative);
    }

    LanShareServer::Options baseOptions() const
    {
        LanShareServer::Options options;
        options.paths = {path(QStringLiteral("shared"))};
        options.addresses = {QHostAddress(QHostAddress::LocalHost)};
        options.token = token;
        options.timeoutSeconds = 0;
        return options;
    }

    void startServer(const QString &uploadFolder = QString())
    {
        LanShareServer::Options options = baseOptions();
        options.uploadFolder = uploadFolder;
        const QString error = m_server.start(options);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(m_server.port() > 0);
    }

    Response get(const QByteArray &target)
    {
        return request("GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
    }

    Response request(const QByteArray &raw, QSslCertificate *peer = nullptr)
    {
        QSslSocket socket;
        QByteArray data;
        QEventLoop loop;
        connect(&socket, &QIODevice::readyRead, &loop, [&] {
            data += socket.readAll();
        });
        connect(&socket, &QAbstractSocket::disconnected, &loop, &QEventLoop::quit);
        connect(&socket, &QAbstractSocket::errorOccurred, &loop, [&](QAbstractSocket::SocketError error) {
            if (error != QAbstractSocket::RemoteHostClosedError) {
                loop.quit();
            }
        });
        QTimer::singleShot(20000, &loop, &QEventLoop::quit);
        if (m_server.tls()) {
            socket.setPeerVerifyMode(QSslSocket::VerifyNone);
            connect(&socket, &QSslSocket::encrypted, &socket, [&] {
                if (peer) {
                    *peer = socket.peerCertificate();
                }
                socket.write(raw);
            });
            socket.connectToHostEncrypted(QStringLiteral("127.0.0.1"), m_server.port());
        } else {
            connect(&socket, &QAbstractSocket::connected, &socket, [&] {
                socket.write(raw);
            });
            socket.connectToHost(QHostAddress(QHostAddress::LocalHost), m_server.port());
        }
        loop.exec();
        data += socket.readAll();

        Response response;
        qsizetype end = data.indexOf("\r\n\r\n");
        while (data.startsWith("HTTP/1.1 100 ") && end >= 0) {
            data.remove(0, end + 4);
            end = data.indexOf("\r\n\r\n");
        }
        if (end < 0) {
            return response;
        }
        const QList<QByteArray> lines = data.left(end).split('\n');
        response.status = lines.first().split(' ').value(1).toInt();
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const qsizetype colon = lines.at(i).indexOf(':');
            response.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
        }
        response.body = data.mid(end + 4);
        return response;
    }

    QTemporaryDir m_dir;
    LanShareServer m_server;
};

QTEST_GUILESS_MAIN(LanShareTest)

#include "lansharetest.moc"
