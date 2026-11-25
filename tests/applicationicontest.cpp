// SPDX-License-Identifier: GPL-3.0-or-later

#include <KIconLoader>
#include <KIconTheme>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QPalette>
#include <QStandardPaths>
#include <QTest>

class ApplicationIconTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KIconTheme::forceThemeForTests(QStringLiteral("opusfm-accent-test"));
    }

    void followsAccent()
    {
        KIconLoader loader;
        QPalette palette;
        palette.setColor(QPalette::Accent, QColor(210, 45, 50));
        palette.setColor(QPalette::Highlight, QColor(210, 45, 50));
        loader.setCustomPalette(palette);
        const QImage red = loader.loadIcon(QStringLiteral("org.kde.opusfilemanager"), KIconLoader::NoGroup, 256).toImage();
        QVERIFY(!red.isNull());

        palette.setColor(QPalette::Accent, QColor(30, 180, 90));
        palette.setColor(QPalette::Highlight, QColor(30, 180, 90));
        loader.setCustomPalette(palette);
        const QImage green = loader.loadIcon(QStringLiteral("org.kde.opusfilemanager"), KIconLoader::NoGroup, 256).toImage();
        QVERIFY(!green.isNull());

        const QColor redFolder = red.pixelColor(30, 145);
        const QColor greenFolder = green.pixelColor(30, 145);
        QVERIFY(redFolder.red() > redFolder.green());
        QVERIFY(greenFolder.green() > greenFolder.red());
        QCOMPARE(red.pixelColor(128, 110), green.pixelColor(128, 110));
        QVERIFY(red.pixelColor(128, 110).red() > 240);
    }

    void occupiesDesktopIconSize()
    {
        KIconLoader loader;
        const QImage image = loader.loadIcon(QStringLiteral("org.kde.opusfilemanager"), KIconLoader::NoGroup, 48).toImage();
        QVERIFY(!image.isNull());

        QRect bounds;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y).alpha() > 0) {
                    bounds |= QRect(x, y, 1, 1);
                }
            }
        }
        QVERIFY(bounds.width() >= 40);
        QVERIFY(bounds.height() >= 35);
    }

    void smallSizesUseDedicatedArtwork_data()
    {
        QTest::addColumn<int>("size");
        QTest::newRow("16") << 16;
        QTest::newRow("22") << 22;
        QTest::newRow("32") << 32;
    }

    void smallSizesUseDedicatedArtwork()
    {
        QFETCH(int, size);
        KIconLoader loader;
        const QString path = loader.iconPath(QStringLiteral("org.kde.opusfilemanager"), -size);
        QVERIFY2(path.endsWith(QStringLiteral("/%1x%1/apps/org.kde.opusfilemanager.svg").arg(size)), qPrintable(path));

        const QImage image = loader.loadIcon(QStringLiteral("org.kde.opusfilemanager"), KIconLoader::NoGroup, size).toImage();
        QCOMPARE(image.size(), QSize(size, size));
        const QRect bounds = opaqueBounds(image);
        QVERIFY(bounds.width() >= size * 7 / 8);
        QVERIFY(bounds.height() >= size * 3 / 4);

        // The mark at the centre of the folder stays white whatever the accent.
        bool hasWhite = false;
        for (int y = size / 2; y < size; ++y) {
            for (int x = size / 4; x < size * 3 / 4; ++x) {
                const QColor c = image.pixelColor(x, y);
                hasWhite |= c.alpha() == 255 && c.red() > 240 && c.green() > 240 && c.blue() > 240;
            }
        }
        QVERIFY(hasWhite);
    }

    void symbolicIconFollowsTextColour()
    {
        KIconLoader loader;
        const QString path = loader.iconPath(QStringLiteral("org.kde.opusfilemanager-symbolic"), -16);
        QVERIFY2(path.endsWith(QStringLiteral("/symbolic/apps/org.kde.opusfilemanager-symbolic.svg")), qPrintable(path));

        QPalette palette;
        palette.setColor(QPalette::WindowText, QColor(200, 30, 40));
        palette.setColor(QPalette::Text, QColor(200, 30, 40));
        loader.setCustomPalette(palette);
        const QImage image = loader.loadIcon(QStringLiteral("org.kde.opusfilemanager-symbolic"), KIconLoader::NoGroup, 16).toImage();
        QVERIFY(!image.isNull());
        const QColor body = image.pixelColor(2, 12);
        QCOMPARE(body.alpha(), 255);
        QVERIFY(body.red() > 180 && body.green() < 60);
        // The ring is cut out of the body, so the background shows through.
        QVERIFY(image.pixelColor(8, 7).alpha() < 128);
    }

    void logosSuitTheirBackgrounds_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << QStringLiteral(":/branding/plasma-opus-logo.svg") << false;
        QTest::newRow("dark") << QStringLiteral(":/branding/plasma-opus-logo-dark.svg") << true;
    }

    void logosSuitTheirBackgrounds()
    {
        QFETCH(QString, file);
        QFETCH(bool, dark);

        QFile source(file);
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QByteArray svg = source.readAll();
        QVERIFY(svg.contains("SPDX-License-Identifier: GPL-3.0-or-later"));
        QVERIFY(svg.contains("<title>Plasma Opus</title>"));
        // The wordmark is outlined, so it renders without the font installed.
        QVERIFY(!svg.contains("<text"));

        QImageReader reader(file);
        QCOMPARE(reader.size(), QSize(768, 160));
        const QImage image = reader.read();
        QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));

        // Average the wordmark "Plasma", which uses the variant's text colour.
        qreal lightness = 0;
        int count = 0;
        for (int y = 30; y < 120; ++y) {
            for (int x = 180; x < 495; ++x) {
                const QColor c = image.pixelColor(x, y);
                if (c.alpha() == 255) {
                    lightness += c.lightnessF();
                    ++count;
                }
            }
        }
        QVERIFY(count > 1000);
        lightness /= count;
        QVERIFY2(dark ? lightness > 0.9 : lightness < 0.2, qPrintable(QString::number(lightness)));
    }

private:
    static QRect opaqueBounds(const QImage &image)
    {
        QRect bounds;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y).alpha() > 0) {
                    bounds |= QRect(x, y, 1, 1);
                }
            }
        }
        return bounds;
    }
};

QTEST_MAIN(ApplicationIconTest)

#include "applicationicontest.moc"
