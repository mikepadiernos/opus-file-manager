// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfig>
#include <KConfigGroup>

#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class YaziModeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void sessionEntryRoundTripsAndDefaultsToOff()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("staterc"));
        {
            KConfig config(path, KConfig::SimpleConfig);
            KConfigGroup group = config.group(QStringLiteral("Pane"));
            QVERIFY(!YaziMode::readSession(group));
            YaziMode::writeSession(group, true);
            QVERIFY(YaziMode::readSession(group));
            config.sync();
        }
        KConfig config(path, KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Pane"));
        QVERIFY(YaziMode::readSession(group));
        QCOMPARE(group.readEntry("YaziMode", QString()), QStringLiteral("true"));
        YaziMode::writeSession(group, false);
        QVERIFY(!YaziMode::readSession(group));
    }

    void workspaceEntryRoundTrips()
    {
        QJsonObject object;
        YaziMode::writeWorkspace(object, true);
        QCOMPARE(object.value(QStringLiteral("yaziMode")), QJsonValue(true));
        bool enabled = false;
        QVERIFY(YaziMode::readWorkspace(object, enabled));
        QVERIFY(enabled);

        YaziMode::writeWorkspace(object, false);
        QVERIFY(YaziMode::readWorkspace(object, enabled));
        QVERIFY(!enabled);
    }

    void missingWorkspaceEntryIsOff()
    {
        bool enabled = true;
        QVERIFY(YaziMode::readWorkspace(QJsonObject(), enabled));
        QVERIFY(!enabled);
    }

    void invalidWorkspaceEntryIsRejected_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("number") << QJsonValue(1);
        QTest::newRow("string") << QJsonValue(QStringLiteral("true"));
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("object") << QJsonValue(QJsonObject());
    }

    void invalidWorkspaceEntryIsRejected()
    {
        QFETCH(QJsonValue, value);
        const QJsonObject object{{QStringLiteral("yaziMode"), value}};
        bool enabled = true;
        QVERIFY(!YaziMode::readWorkspace(object, enabled));
        QVERIFY(enabled);
    }

    void parentFolder_data()
    {
        QTest::addColumn<QUrl>("current");
        QTest::addColumn<QUrl>("parent");
        QTest::newRow("local") << QUrl::fromLocalFile(QStringLiteral("/home/me/Documents")) << QUrl::fromLocalFile(QStringLiteral("/home/me/"));
        QTest::newRow("trailing slash") << QUrl::fromLocalFile(QStringLiteral("/home/me/")) << QUrl::fromLocalFile(QStringLiteral("/home/"));
        QTest::newRow("top level") << QUrl::fromLocalFile(QStringLiteral("/tmp")) << QUrl::fromLocalFile(QStringLiteral("/"));
        QTest::newRow("root") << QUrl::fromLocalFile(QStringLiteral("/")) << QUrl();
        QTest::newRow("remote") << QUrl(QStringLiteral("sftp://host/dir/sub")) << QUrl(QStringLiteral("sftp://host/dir/"));
        QTest::newRow("remote root") << QUrl(QStringLiteral("sftp://host/")) << QUrl();
        QTest::newRow("empty") << QUrl() << QUrl();
    }

    void parentFolder()
    {
        QFETCH(QUrl, current);
        QFETCH(QUrl, parent);
        QCOMPARE(YaziMode::parentFolder(current).adjusted(QUrl::StripTrailingSlash), parent.adjusted(QUrl::StripTrailingSlash));
        QCOMPARE(YaziMode::parentFolder(current).isEmpty(), parent.isEmpty());
    }

    void sessionPreviewEntryRoundTrips()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Pane"));
        QCOMPARE(YaziMode::readSessionPreview(group), -1);
        YaziMode::writeSessionPreview(group, 0);
        QCOMPARE(YaziMode::readSessionPreview(group), 0);
        QCOMPARE(group.readEntry("YaziPreview", QString()), QStringLiteral("false"));
        YaziMode::writeSessionPreview(group, 1);
        QCOMPARE(YaziMode::readSessionPreview(group), 1);
        YaziMode::writeSessionPreview(group, -1);
        QVERIFY(!group.hasKey("YaziPreview"));
        QCOMPARE(YaziMode::readSessionPreview(group), -1);
    }

    void workspacePreviewEntryRoundTrips()
    {
        QJsonObject object;
        YaziMode::writeWorkspacePreview(object, -1);
        QVERIFY(object.isEmpty());
        int preview = 1;
        QVERIFY(YaziMode::readWorkspacePreview(object, preview));
        QCOMPARE(preview, -1);

        YaziMode::writeWorkspacePreview(object, 0);
        QCOMPARE(object.value(QStringLiteral("yaziPreview")), QJsonValue(false));
        QVERIFY(YaziMode::readWorkspacePreview(object, preview));
        QCOMPARE(preview, 0);
        YaziMode::writeWorkspacePreview(object, 1);
        QCOMPARE(object.value(QStringLiteral("yaziPreview")), QJsonValue(true));
        QVERIFY(YaziMode::readWorkspacePreview(object, preview));
        QCOMPARE(preview, 1);
    }

    void invalidWorkspacePreviewEntryIsRejected_data()
    {
        invalidWorkspaceEntryIsRejected_data();
    }

    void invalidWorkspacePreviewEntryIsRejected()
    {
        QFETCH(QJsonValue, value);
        const QJsonObject object{{QStringLiteral("yaziPreview"), value}};
        int preview = 1;
        QVERIFY(!YaziMode::readWorkspacePreview(object, preview));
        QCOMPARE(preview, 1);
    }

    void columnWidths_data()
    {
        QTest::addColumn<int>("available");
        QTest::addColumn<bool>("preview");
        QTest::addColumn<QList<int>>("ratios");
        QTest::addColumn<int>("handle");
        QTest::addColumn<QList<int>>("widths");
        const QList<int> defaults{1, 3, 2};
        QTest::newRow("empty") << 0 << true << defaults << 0 << QList<int>{0, 0, 0};
        QTest::newRow("negative") << -10 << true << defaults << 0 << QList<int>{0, 0, 0};
        QTest::newRow("very narrow") << 200 << true << defaults << 0 << QList<int>{0, 200, 0};
        QTest::newRow("just below two columns") << 319 << true << defaults << 0 << QList<int>{0, 319, 0};
        QTest::newRow("two columns at minimum") << 320 << true << defaults << 0 << QList<int>{100, 220, 0};
        QTest::newRow("just below three columns") << 479 << true << defaults << 0 << QList<int>{119, 360, 0};
        QTest::newRow("three columns at minimum") << 480 << true << defaults << 0 << QList<int>{100, 220, 160};
        QTest::newRow("medium") << 600 << true << defaults << 0 << QList<int>{100, 300, 200};
        QTest::newRow("wide") << 1200 << true << defaults << 0 << QList<int>{200, 600, 400};
        QTest::newRow("wide without preview") << 1200 << false << defaults << 0 << QList<int>{300, 900, 0};
        QTest::newRow("narrow without preview") << 300 << false << defaults << 0 << QList<int>{0, 300, 0};
        QTest::newRow("handles") << 600 << true << defaults << 6 << QList<int>{100, 293, 195};
        QTest::newRow("handles hide the preview") << 491 << true << defaults << 6 << QList<int>{121, 364, 0};
        QTest::newRow("handles hide the parent") << 325 << true << defaults << 6 << QList<int>{0, 325, 0};
        QTest::newRow("equal ratios") << 900 << true << QList<int>{1, 1, 1} << 0 << QList<int>{300, 300, 300};
        QTest::newRow("zero ratios count as one") << 900 << true << QList<int>{0, 0, -4} << 0 << QList<int>{300, 300, 300};
        QTest::newRow("wide parent keeps the minimums") << 700 << true << QList<int>{5, 1, 1} << 0 << QList<int>{320, 220, 160};
        QTest::newRow("dragged pixel ratios") << 1000 << true << QList<int>{150, 450, 300} << 0 << QList<int>{166, 501, 333};
    }

    void columnWidths()
    {
        QFETCH(int, available);
        QFETCH(bool, preview);
        QFETCH(QList<int>, ratios);
        QFETCH(int, handle);
        QFETCH(QList<int>, widths);
        const YaziMode::ColumnWidths actual = YaziMode::columnWidths(available, preview, {ratios.at(0), ratios.at(1), ratios.at(2)}, handle);
        QCOMPARE((QList<int>{actual.parent, actual.current, actual.preview}), widths);
        if (available > 0) {
            const int shown = (actual.parent > 0) + (actual.current > 0) + (actual.preview > 0);
            QCOMPARE(actual.parent + actual.current + actual.preview + handle * (shown - 1), available);
        }
    }

    void columnWidthsNeverSqueezeTheCurrentColumn()
    {
        for (int handle : {0, 1, 6}) {
            bool hadPreview = false;
            bool hadParent = false;
            for (int available = 1; available <= 2000; ++available) {
                const YaziMode::ColumnWidths widths = YaziMode::columnWidths(available, true, {}, handle);
                const int shown = (widths.parent > 0) + 1 + (widths.preview > 0);
                QCOMPARE(widths.parent + widths.current + widths.preview + handle * (shown - 1), available);
                if (shown > 1) {
                    QVERIFY2(widths.current >= YaziMode::MinimumCurrentWidth, qPrintable(QString::number(available)));
                }
                QVERIFY(widths.parent == 0 || widths.parent >= YaziMode::MinimumParentWidth);
                QVERIFY(widths.preview == 0 || widths.preview >= YaziMode::MinimumPreviewWidth);
                // The preview column hides before the parent column, and neither comes back as the width grows.
                QVERIFY(widths.preview == 0 || widths.parent > 0);
                QVERIFY(!hadPreview || widths.preview > 0);
                QVERIFY(!hadParent || widths.parent > 0);
                hadPreview = widths.preview > 0;
                hadParent = widths.parent > 0;
            }
            QVERIFY(hadPreview);
        }
    }
};

QTEST_GUILESS_MAIN(YaziModeTest)

#include "yazimodetest.moc"
