// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagedetails.h"

#include <KPluginMetaData>

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <numeric>

class ImageDetailsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void histogramAndColours()
    {
        QImage image(20, 10, QImage::Format_ARGB32);
        image.fill(Qt::red);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 10; x < image.width(); ++x) {
                image.setPixelColor(x, y, Qt::blue);
            }
        }
        const ImageDetails::Data details = ImageDetails::analyze(image, {});
        QCOMPARE(details.histogram.size(), 40);
        QCOMPARE(std::accumulate(details.histogram.cbegin(), details.histogram.cend(), 0), 160 * 80);
        QVERIFY(details.colours.contains(QColor(Qt::red)));
        QVERIFY(details.colours.contains(QColor(Qt::blue)));
        QCOMPARE(details.metadata.size(), 0);
    }

    void extractsCameraMetadata()
    {
        const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfilemetadata"));
        if (std::none_of(plugins.cbegin(), plugins.cend(), [](const KPluginMetaData &plugin) {
                return plugin.pluginId().contains(QLatin1String("exiv2"));
            })) {
            QSKIP("No EXIF metadata extractor is installed");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("photo.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray jpeg = file.readAll();
        file.close();
        QVERIFY(jpeg.startsWith(QByteArray::fromHex("ffd8")));
        jpeg.insert(2, QByteArray::fromHex("ffe1002745786966000049492a000800000001000f010200050000001a0000000000000041636d6500"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(jpeg), jpeg.size());
        file.close();
        const auto details = ImageDetails::analyze(image, path);
        QCOMPARE(details.metadata.value(KFileMetaData::Property::Manufacturer).toString(), QStringLiteral("Acme"));
    }
};

QTEST_GUILESS_MAIN(ImageDetailsTest)
#include "imagedetailstest.moc"
