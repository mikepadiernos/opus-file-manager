// SPDX-License-Identifier: GPL-3.0-or-later

#include "previewcontent.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class PreviewContentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void textAndLimits()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("text.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("hello\n"), 6);
        file.close();
        const QUrl url = QUrl::fromLocalFile(file.fileName());
        PreviewContent content = Preview::load(url, QStringLiteral("text/plain"));
        QCOMPARE(content.kind, PreviewContent::Kind::Text);
        QCOMPARE(content.text, QStringLiteral("hello\n"));
        QVERIFY(!content.truncated);
        content = Preview::load(url, QStringLiteral("text/plain"), 5);
        QCOMPARE(content.text, QStringLiteral("hello"));
        QVERIFY(content.truncated);
        content = Preview::load(url, QStringLiteral("text/plain"), 6);
        QCOMPARE(content.text, QStringLiteral("hello\n"));
        QVERIFY(!content.truncated);

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray large(1024 * 1024 + 1, 'a');
        QCOMPARE(file.write(large), large.size());
        file.close();
        content = Preview::load(url, QStringLiteral("text/plain"));
        QCOMPARE(content.kind, PreviewContent::Kind::Text);
        QCOMPARE(content.text.size(), 1024 * 1024);
        QVERIFY(content.truncated);

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QByteArray unicode(1024 * 1024 - 1, 'a');
        unicode.append("\xe2\x82\xac", 3);
        QCOMPARE(file.write(unicode), unicode.size());
        file.close();
        content = Preview::load(url, QStringLiteral("text/plain"));
        QCOMPARE(content.kind, PreviewContent::Kind::Text);
        QVERIFY(content.truncated);
        QVERIFY(!content.text.contains(QChar::ReplacementCharacter));

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("a\0b", 3), 3);
        file.close();
        QCOMPARE(Preview::load(url, QStringLiteral("text/plain")).kind, PreviewContent::Kind::Unsupported);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(QByteArray::fromHex("fffe")), 2);
        file.close();
        QCOMPARE(Preview::load(url, QStringLiteral("text/plain")).kind, PreviewContent::Kind::Unsupported);
    }

    void imageAndErrors()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("image.png"));
        QImage image(12, 8, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        const QUrl url = QUrl::fromLocalFile(path);
        PreviewContent content = Preview::load(url, QStringLiteral("image/png"));
        QCOMPARE(content.kind, PreviewContent::Kind::Image);
        QCOMPARE(content.image.size(), QSize(12, 8));

        QImage large(3000, 1000, QImage::Format_RGB32);
        large.fill(Qt::blue);
        QVERIFY(large.save(path));
        content = Preview::load(url, QStringLiteral("image/png"));
        QCOMPARE(content.kind, PreviewContent::Kind::Image);
        QCOMPARE(content.image.width(), 2048);
        QVERIFY(content.image.height() <= 2048);
        QCOMPARE(Preview::loadImage(url, 0).image.size(), QSize(3000, 1000));
        QCOMPARE(Preview::loadImage(url, 96).image.size(), QSize(96, 32));
        QFile largeFile(path);
        QVERIFY(largeFile.open(QIODevice::ReadWrite));
        QVERIFY(largeFile.resize(8 * 1024 * 1024 + 1));
        largeFile.close();
        QCOMPARE(Preview::load(url, QStringLiteral("image/png")).kind, PreviewContent::Kind::TooLarge);
        QCOMPARE(Preview::loadImage(url, 0).image.size(), QSize(3000, 1000));

        QImage oversized(8000, 5001, QImage::Format_Mono);
        oversized.fill(Qt::color0);
        QVERIFY(oversized.save(path));
        QCOMPARE(Preview::load(url, QStringLiteral("image/png")).kind, PreviewContent::Kind::TooLarge);
        QCOMPARE(Preview::loadImage(url, 0).kind, PreviewContent::Kind::TooLarge);

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.resize(8 * 1024 * 1024 + 1));
        file.close();
        QCOMPARE(Preview::load(url, QStringLiteral("image/png")).kind, PreviewContent::Kind::TooLarge);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(128 * 1024 * 1024 + 1));
        file.close();
        QCOMPARE(Preview::loadImage(url, 0).kind, PreviewContent::Kind::TooLarge);
        QVERIFY(file.remove());
        content = Preview::load(url, QStringLiteral("image/png"));
        QCOMPARE(content.kind, PreviewContent::Kind::Error);
        QVERIFY(!content.text.isEmpty());
        QCOMPARE(Preview::load(QUrl(QStringLiteral("sftp://example.invalid/file.txt")), QStringLiteral("text/plain")).kind, PreviewContent::Kind::Remote);
        QCOMPARE(Preview::load(url, QStringLiteral("application/pdf")).kind, PreviewContent::Kind::Unsupported);
        QCOMPARE(Preview::load(QUrl::fromLocalFile(dir.path()), QStringLiteral("text/plain")).kind, PreviewContent::Kind::Unsupported);
    }
};

QTEST_GUILESS_MAIN(PreviewContentTest)

#include "previewcontenttest.moc"
