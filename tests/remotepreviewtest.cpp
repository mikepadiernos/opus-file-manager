// SPDX-License-Identifier: GPL-3.0-or-later

#include "previewpanel.h"
#include "settings.h"

#include <KFileItem>
#include <KPluginMetaData>

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QMediaPlayer>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>

class RemotePreviewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        Settings::setTextPreviewLimitKiB(1024);
        QVERIFY(m_server.listen(QHostAddress::LocalHost));
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [socket, request = QByteArray()]() mutable {
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        return;
                    }
                    const QByteArray path = request.split(' ').value(1);
                    QByteArray body;
                    QByteArray mime("text/plain");
                    QByteArray status("200 OK");
                    if (path == "/text.txt") {
                        body = "remote text";
                    } else if (path == "/large.txt") {
                        body = QByteArray(1024 * 1024 + 50, 'x');
                    } else if (path == "/image.png") {
                        QImage image(20, 10, QImage::Format_RGB32);
                        image.fill(Qt::red);
                        QBuffer buffer(&body);
                        buffer.open(QIODevice::WriteOnly);
                        image.save(&buffer, "PNG");
                        mime = "image/png";
                    } else if (path == "/photo.jpg") {
                        QImage image(4, 4, QImage::Format_RGB32);
                        image.fill(Qt::red);
                        QBuffer buffer(&body);
                        buffer.open(QIODevice::WriteOnly);
                        image.save(&buffer, "JPEG");
                        buffer.close();
                        body.insert(2, QByteArray::fromHex("ffe1002745786966000049492a000800000001000f010200050000001a0000000000000041636d6500"));
                        mime = "image/jpeg";
                    } else if (path == "/audio.wav") {
                        body = QByteArray::fromHex("52494646641f000057415645666d74201000000001000100401f0000401f00000100080064617461401f0000");
                        body += QByteArray(8000, char(0x80));
                        mime = "audio/wav";
                    } else {
                        body = "missing";
                        status = "404 Not Found";
                    }
                    socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + mime + "\r\nContent-Length: " + QByteArray::number(body.size())
                                  + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    void cleanup()
    {
        Settings::setTextPreviewLimitKiB(1024);
    }

    void remoteContentAndCleanup()
    {
        PreviewPanel panel;
        panel.resize(400, 400);
        panel.show();
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *footer = panel.findChild<QLabel *>(QStringLiteral("previewFooter"));
        auto *image = panel.findChild<QLabel *>(QStringLiteral("previewImage"));
        auto *message = panel.findChild<QLabel *>(QStringLiteral("previewMessage"));
        QVERIFY(text && footer && image && message);

        panel.setItems({KFileItem(url(QStringLiteral("text.txt")))});
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("remote text"));
        panel.setItems({KFileItem(url(QStringLiteral("large.txt")))});
        QTRY_COMPARE(text->toPlainText().size(), 1024 * 1024);
        QVERIFY(!footer->text().isEmpty());
        panel.setItems({KFileItem(url(QStringLiteral("image.png")))});
        QTRY_VERIFY(image->isVisible() && !image->pixmap().isNull());

        panel.setItems({KFileItem(url(QStringLiteral("audio.wav")))});
        QTRY_VERIFY(panel.findChild<QMediaPlayer *>(QStringLiteral("mediaPlayer")));
        const QString temporary = panel.findChild<QMediaPlayer *>(QStringLiteral("mediaPlayer"))->source().toLocalFile();
        QVERIFY(!temporary.isEmpty());
        QVERIFY(QFile::exists(temporary));
        panel.clear();
        QTRY_VERIFY(!QFile::exists(temporary));

        panel.setItems({KFileItem(url(QStringLiteral("missing.txt")))});
        QTRY_VERIFY(message->isVisible() && message->text().contains(QStringLiteral("Could not")));
    }

    void configuredTextLimitForRemoteAndLocal()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile local(dir.filePath(QStringLiteral("local.txt")));
        QVERIFY(local.open(QIODevice::WriteOnly));
        QCOMPARE(local.write(QByteArray(2500, 'x')), 2500);
        local.close();
        Settings::setTextPreviewLimitKiB(1);
        PreviewPanel panel;
        panel.show();
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *footer = panel.findChild<QLabel *>(QStringLiteral("previewFooter"));
        QVERIFY(text && footer);
        const KFileItem localItem(QUrl::fromLocalFile(local.fileName()));
        panel.setItems({localItem});
        QTRY_COMPARE(text->toPlainText().size(), 1024);
        QVERIFY(footer->text().contains(QStringLiteral("truncated"), Qt::CaseInsensitive));
        Settings::setTextPreviewLimitKiB(2);
        panel.setItems({localItem});
        QTRY_COMPARE(text->toPlainText().size(), 2048);
        panel.setItems({KFileItem(url(QStringLiteral("large.txt")))});
        QTRY_COMPARE(text->toPlainText().size(), 2048);
        QVERIFY(footer->text().contains(QStringLiteral("truncated"), Qt::CaseInsensitive));
        Settings::setTextPreviewLimitKiB(2048);
        panel.setItems({KFileItem(url(QStringLiteral("large.txt")))});
        QTRY_COMPARE(text->toPlainText().size(), 1024 * 1024 + 50);
        QVERIFY(!footer->text().contains(QStringLiteral("truncated"), Qt::CaseInsensitive));
    }

    void obsoleteRemoteResponseDoesNotReplaceSelection()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile local(dir.filePath(QStringLiteral("local.txt")));
        QVERIFY(local.open(QIODevice::WriteOnly));
        QCOMPARE(local.write("current"), 7);
        local.close();
        PreviewPanel panel;
        panel.show();
        panel.setItems({KFileItem(url(QStringLiteral("large.txt")))});
        panel.setItems({KFileItem(QUrl::fromLocalFile(local.fileName()))});
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("current"));
        QTest::qWait(200);
        QCOMPARE(text->toPlainText(), QStringLiteral("current"));
    }

    void remoteExifMetadataAndSelectionChange()
    {
        const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfilemetadata"));
        if (std::none_of(plugins.cbegin(), plugins.cend(), [](const KPluginMetaData &plugin) {
                return plugin.pluginId().contains(QLatin1String("exiv2"));
            })) {
            QSKIP("No EXIF metadata extractor is installed");
        }
        PreviewPanel panel;
        panel.show();
        auto *footer = panel.findChild<QLabel *>(QStringLiteral("previewFooter"));
        QVERIFY(footer);
        panel.setItems({KFileItem(url(QStringLiteral("photo.jpg")))});
        QTRY_VERIFY(footer->text().contains(QStringLiteral("Acme")));
        panel.setItems({KFileItem(url(QStringLiteral("text.txt")))});
        QVERIFY(!footer->text().contains(QStringLiteral("Acme")));
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("remote text"));
        QVERIFY(!footer->text().contains(QStringLiteral("Acme")));
    }

private:
    QUrl url(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(path));
    }

    QTcpServer m_server;
};

QTEST_MAIN(RemotePreviewTest)
#include "remotepreviewtest.moc"
