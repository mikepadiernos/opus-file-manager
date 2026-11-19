// SPDX-License-Identifier: GPL-3.0-or-later

#include "imageediting.h"

#include <QTest>

class ImageEditingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void geometry()
    {
        QImage image(4, 3, QImage::Format_ARGB32);
        image.fill(Qt::red);
        image.setPixelColor(2, 1, Qt::blue);
        const QImage cropped = ImageEditing::crop(image, QRect(2, 1, 5, 5));
        QCOMPARE(cropped.size(), QSize(2, 2));
        QCOMPARE(cropped.pixelColor(0, 0), QColor(Qt::blue));
        QVERIFY(ImageEditing::crop(image, QRect(8, 8, 1, 1)).isNull());
        QCOMPARE(ImageEditing::resize(image, QSize(8, 6)).size(), QSize(8, 6));
        QVERIFY(ImageEditing::resize(image, QSize(10000, 5000)).isNull());
        QVERIFY(ImageEditing::resize(image, QSize(0, 1)).isNull());
    }

    void adjustments()
    {
        QImage image(1, 1, QImage::Format_ARGB32);
        image.setPixelColor(0, 0, QColor(200, 80, 40, 120));
        QCOMPARE(ImageEditing::adjust(image, {}).pixelColor(0, 0), image.pixelColor(0, 0));
        ImageEditing::Adjustments options;
        options.grayscale = 100;
        const QColor gray = ImageEditing::adjust(image, options).pixelColor(0, 0);
        QCOMPARE(gray.red(), gray.green());
        QCOMPARE(gray.green(), gray.blue());
        QCOMPARE(gray.alpha(), 120);
        options.grayscale = 0;
        options.brightness = 150;
        const QColor brighter = ImageEditing::adjust(image, options).pixelColor(0, 0);
        QVERIFY(brighter.red() > 200);
        QVERIFY(brighter.green() > 80);
        options.saturation = 201;
        QVERIFY(ImageEditing::adjust(image, options).isNull());
    }
};

QTEST_GUILESS_MAIN(ImageEditingTest)
#include "imageeditingtest.moc"
