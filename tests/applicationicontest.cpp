// SPDX-License-Identifier: GPL-3.0-or-later

#include <KIconLoader>
#include <KIconTheme>
#include <QImage>
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
};

QTEST_MAIN(ApplicationIconTest)

#include "applicationicontest.moc"
