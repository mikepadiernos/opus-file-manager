// SPDX-License-Identifier: GPL-3.0-or-later

#include "diskimages.h"

#include <QTest>

class DiskImagesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void recognisesThePluginsImageTypes()
    {
        QCOMPARE(DiskImages::mimeTypes().size(), 4);
        QVERIFY(DiskImages::isDiskImage(QStringLiteral("application/x-cd-image")));
        QVERIFY(DiskImages::isDiskImage(QStringLiteral("application/x-raw-disk-image")));
        QVERIFY(DiskImages::isDiskImage(QStringLiteral("application/vnd.efi.iso")));
        QVERIFY(!DiskImages::isDiskImage(QStringLiteral("application/zip")));
        QVERIFY(!DiskImages::isDiskImage(QStringLiteral("not/a-type")));
        QVERIFY(!DiskImages::isDiskImage(QString()));
    }

    void resolvesAliases()
    {
        // shared-mime-info lists application/x-iso9660-image as an alias of the ISO type.
        QVERIFY(DiskImages::isDiskImage(QStringLiteral("application/x-iso9660-image")));
    }

    void namesThePlugin()
    {
        QCOMPARE(DiskImages::pluginId(), QStringLiteral("mountisoaction"));
        QCOMPARE(DiskImages::pluginNamespace(), QStringLiteral("kf6/kfileitemaction"));
    }
};

QTEST_GUILESS_MAIN(DiskImagesTest)

#include "diskimagestest.moc"
