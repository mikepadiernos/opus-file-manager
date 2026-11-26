// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfig>
#include <KConfigGroup>
#include <KCoreConfigSkeleton>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <limits>

Q_DECLARE_METATYPE(YaziMode::Preset)

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

    void startupMode_data()
    {
        QTest::addColumn<bool>("defaultMode");
        QTest::addColumn<bool>("restore");
        QTest::addColumn<int>("saved");
        QTest::addColumn<bool>("expected");
        // saved: -1 when no session is restored for the pane, otherwise the saved mode.
        QTest::newRow("restores a saved on") << false << true << 1 << true;
        QTest::newRow("restores a saved off over an on default") << true << true << 0 << false;
        QTest::newRow("default off without a session") << false << true << -1 << false;
        QTest::newRow("default on without a session") << true << true << -1 << true;
        QTest::newRow("not restoring ignores a saved on") << false << false << 1 << false;
        QTest::newRow("not restoring uses an on default") << true << false << 0 << true;
        QTest::newRow("not restoring without a session") << true << false << -1 << true;
    }

    void startupMode()
    {
        QFETCH(bool, defaultMode);
        QFETCH(bool, restore);
        QFETCH(int, saved);
        QFETCH(bool, expected);
        const std::optional<bool> session = saved < 0 ? std::nullopt : std::optional<bool>(saved == 1);
        QCOMPARE(YaziMode::startupMode({defaultMode, restore}, session), expected);
    }

    void activationDefaultsKeepTheModeOff()
    {
        const YaziMode::Activation activation;
        QVERIFY(!activation.defaultMode);
        QVERIFY(activation.restore);
        QVERIFY(!YaziMode::startupMode(activation, std::nullopt));
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
        QTest::newRow("archive root") << QUrl(QStringLiteral("zip:/home/me/photos.zip/")) << QUrl::fromLocalFile(QStringLiteral("/home/me/"));
        QTest::newRow("compressed tar root") << QUrl(QStringLiteral("tar:/home/me/src.tar.gz")) << QUrl::fromLocalFile(QStringLiteral("/home/me/"));
        QTest::newRow("inside an archive") << QUrl(QStringLiteral("zip:/home/me/photos.zip/2024/")) << QUrl(QStringLiteral("zip:/home/me/photos.zip/"));
        QTest::newRow("trash item") << QUrl(QStringLiteral("trash:/0-old/sub")) << QUrl(QStringLiteral("trash:/0-old/"));
        QTest::newRow("trash") << QUrl(QStringLiteral("trash:/")) << QUrl();
        QTest::newRow("desktop") << QUrl(QStringLiteral("desktop:/")) << QUrl();
    }

    void parentItem()
    {
        const QUrl archive(QStringLiteral("zip:/home/me/photos.zip/"));
        QCOMPARE(YaziMode::parentItem(archive), QUrl::fromLocalFile(QStringLiteral("/home/me/photos.zip")));
        const QUrl inside(QStringLiteral("zip:/home/me/photos.zip/2024/"));
        QCOMPARE(YaziMode::parentItem(inside), inside);
        const QUrl local = QUrl::fromLocalFile(QStringLiteral("/home/me/photos.zip"));
        QCOMPARE(YaziMode::parentItem(local), local);
    }

    void support_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<int>("level");
        constexpr int full = int(YaziMode::SupportLevel::Full);
        constexpr int limited = int(YaziMode::SupportLevel::Limited);
        constexpr int unsupported = int(YaziMode::SupportLevel::Unsupported);
        QTest::newRow("local") << QUrl::fromLocalFile(QStringLiteral("/home/me")) << full;
        QTest::newRow("local root") << QUrl::fromLocalFile(QStringLiteral("/")) << full;
        QTest::newRow("sftp") << QUrl(QStringLiteral("sftp://host/home/me/")) << limited;
        QTest::newRow("smb") << QUrl(QStringLiteral("smb://server/share/")) << limited;
        QTest::newRow("trash") << QUrl(QStringLiteral("trash:/")) << limited;
        QTest::newRow("desktop") << QUrl(QStringLiteral("desktop:/")) << limited;
        QTest::newRow("zip") << QUrl(QStringLiteral("zip:/home/me/a.zip/")) << limited;
        QTest::newRow("tar") << QUrl(QStringLiteral("tar:/home/me/a.tar.gz/dir/")) << limited;
        QTest::newRow("remote") << QUrl(QStringLiteral("remote:/")) << unsupported;
        QTest::newRow("recently used") << QUrl(QStringLiteral("recentlyused:/")) << unsupported;
        QTest::newRow("recent documents") << QUrl(QStringLiteral("recentdocuments:/")) << unsupported;
        QTest::newRow("tags") << QUrl(QStringLiteral("tags:/holiday")) << unsupported;
        QTest::newRow("baloo search") << QUrl(QStringLiteral("baloosearch:/documents")) << unsupported;
        QTest::newRow("file name search") << QUrl(QStringLiteral("filenamesearch:?search=a&url=file:///home/me")) << unsupported;
        QTest::newRow("applications") << QUrl(QStringLiteral("applications:/")) << unsupported;
        QTest::newRow("empty") << QUrl() << unsupported;
    }

    void support()
    {
        QFETCH(QUrl, url);
        QFETCH(int, level);
        const YaziMode::Support support = YaziMode::support(url);
        QCOMPARE(int(support.level), level);
        QCOMPARE(support.reason.isEmpty(), level == int(YaziMode::SupportLevel::Full));
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

    void sessionPreviewContentEntryRoundTrips()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Pane"));
        QCOMPARE(YaziMode::readSessionPreviewContent(group), -1);
        YaziMode::writeSessionPreviewContent(group, int(YaziMode::PreviewContent::Information));
        QCOMPARE(group.readEntry("YaziPreviewContent", QString()), QStringLiteral("Information"));
        QCOMPARE(YaziMode::readSessionPreviewContent(group), int(YaziMode::PreviewContent::Information));
        YaziMode::writeSessionPreviewContent(group, int(YaziMode::PreviewContent::Preview));
        QCOMPARE(group.readEntry("YaziPreviewContent", QString()), QStringLiteral("Preview"));
        QCOMPARE(YaziMode::readSessionPreviewContent(group), int(YaziMode::PreviewContent::Preview));
        group.writeEntry("YaziPreviewContent", QStringLiteral("information"));
        QCOMPARE(YaziMode::readSessionPreviewContent(group), int(YaziMode::PreviewContent::Information));
        group.writeEntry("YaziPreviewContent", QStringLiteral("Thumbnails"));
        QCOMPARE(YaziMode::readSessionPreviewContent(group), -1);
        YaziMode::writeSessionPreviewContent(group, -1);
        QVERIFY(!group.hasKey("YaziPreviewContent"));
        QCOMPARE(YaziMode::readSessionPreviewContent(group), -1);
    }

    void workspacePreviewContentEntryRoundTrips()
    {
        QJsonObject object;
        YaziMode::writeWorkspacePreviewContent(object, -1);
        QVERIFY(object.isEmpty());
        int content = 1;
        QVERIFY(YaziMode::readWorkspacePreviewContent(object, content));
        QCOMPARE(content, -1);

        YaziMode::writeWorkspacePreviewContent(object, int(YaziMode::PreviewContent::Information));
        QCOMPARE(object.value(QStringLiteral("yaziPreviewContent")), QJsonValue(QStringLiteral("information")));
        QVERIFY(YaziMode::readWorkspacePreviewContent(object, content));
        QCOMPARE(content, int(YaziMode::PreviewContent::Information));
        YaziMode::writeWorkspacePreviewContent(object, int(YaziMode::PreviewContent::Preview));
        QCOMPARE(object.value(QStringLiteral("yaziPreviewContent")), QJsonValue(QStringLiteral("preview")));
        QVERIFY(YaziMode::readWorkspacePreviewContent(object, content));
        QCOMPARE(content, int(YaziMode::PreviewContent::Preview));
    }

    void invalidWorkspacePreviewContentEntryIsRejected_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("number") << QJsonValue(1);
        QTest::newRow("bool") << QJsonValue(true);
        QTest::newRow("unknown") << QJsonValue(QStringLiteral("thumbnails"));
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("object") << QJsonValue(QJsonObject());
    }

    void invalidWorkspacePreviewContentEntryIsRejected()
    {
        QFETCH(QJsonValue, value);
        const QJsonObject object{{QStringLiteral("yaziPreviewContent"), value}};
        int content = 1;
        QVERIFY(!YaziMode::readWorkspacePreviewContent(object, content));
        QCOMPARE(content, 1);
    }

    void vimKeyTogglesThePreviewContent()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_I), YaziMode::KeyMatcher::Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::TogglePreviewContent);
        QVERIFY(YaziMode::commandId(YaziMode::Command::TogglePreviewContent) != YaziMode::commandId(YaziMode::Command::TogglePreview));
        YaziMode::KeyMatcher standard(YaziMode::presetKeymap(YaziMode::Preset::Standard));
        QVERIFY(standard.feed(Qt::Key_I) != YaziMode::KeyMatcher::Result::Matched);
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

    void columnWidthsWithoutTheParentColumn()
    {
        const int handle = 6;
        bool hadPreview = false;
        for (int available = 1; available <= 2000; ++available) {
            const YaziMode::ColumnWidths widths = YaziMode::columnWidths(available, true, {1, 3, 2}, handle, false);
            QCOMPARE(widths.parent, 0);
            const int shown = 1 + (widths.preview > 0);
            QCOMPARE(widths.current + widths.preview + handle * (shown - 1), available);
            QVERIFY(widths.preview == 0 || widths.preview >= YaziMode::MinimumPreviewWidth);
            if (widths.preview > 0) {
                QVERIFY2(widths.current >= YaziMode::MinimumCurrentWidth, qPrintable(QString::number(available)));
            }
            QVERIFY(!hadPreview || widths.preview > 0);
            hadPreview = widths.preview > 0;
        }
        QVERIFY(hadPreview);
        // The preview column shows in a narrower pane than with the parent column.
        const int narrow = YaziMode::MinimumCurrentWidth + YaziMode::MinimumPreviewWidth + handle;
        QVERIFY(YaziMode::columnWidths(narrow, true, {}, handle, false).preview > 0);
        QCOMPARE(YaziMode::columnWidths(narrow, true, {}, handle).preview, 0);
        QCOMPARE(YaziMode::columnWidths(narrow - 1, true, {}, handle, false).preview, 0);
        // Without the preview either, the current column takes the whole width.
        const YaziMode::ColumnWidths alone = YaziMode::columnWidths(1000, false, {}, handle, false);
        QCOMPARE((QList<int>{alone.parent, alone.current, alone.preview}), (QList<int>{0, 1000, 0}));
        // The current and preview columns keep their share of the ratios.
        const YaziMode::ColumnWidths wide = YaziMode::columnWidths(1000 + handle, true, {1, 3, 2}, handle, false);
        QCOMPARE((QList<int>{wide.parent, wide.current, wide.preview}), (QList<int>{0, 600, 400}));
    }

    void ratiosFromList_data()
    {
        QTest::addColumn<QList<int>>("list");
        QTest::addColumn<QList<int>>("expected");
        QTest::newRow("default") << QList<int>{1, 3, 2} << QList<int>{1, 3, 2};
        QTest::newRow("pixels") << QList<int>{180, 420, 300} << QList<int>{180, 420, 300};
        QTest::newRow("empty") << QList<int>{} << QList<int>{};
        QTest::newRow("two") << QList<int>{1, 3} << QList<int>{};
        QTest::newRow("four") << QList<int>{1, 3, 2, 1} << QList<int>{};
        QTest::newRow("zero") << QList<int>{0, 3, 2} << QList<int>{};
        QTest::newRow("negative") << QList<int>{1, -3, 2} << QList<int>{};
        QTest::newRow("maximum") << QList<int>{1, YaziMode::MaximumRatio, 2} << QList<int>{1, YaziMode::MaximumRatio, 2};
        QTest::newRow("too large") << QList<int>{2 * YaziMode::MaximumRatio, 4 * YaziMode::MaximumRatio, 1}
                                   << QList<int>{YaziMode::MaximumRatio / 2, YaziMode::MaximumRatio, 1};
        QTest::newRow("int max") << QList<int>{std::numeric_limits<int>::max(), 1, 1} << QList<int>{YaziMode::MaximumRatio, 1, 1};
    }

    void ratiosFromList()
    {
        QFETCH(QList<int>, list);
        QFETCH(QList<int>, expected);
        const std::optional<YaziMode::Ratios> ratios = YaziMode::ratiosFromList(list);
        QCOMPARE(ratios.has_value(), !expected.isEmpty());
        if (ratios) {
            QCOMPARE(YaziMode::ratiosToList(*ratios), expected);
        }
    }

    void clampedRatiosKeepEveryColumnPositive()
    {
        QCOMPARE(YaziMode::clampedRatios({0, -4, 2}), (YaziMode::Ratios{1, 1, 2}));
        QCOMPARE(YaziMode::clampedRatios({1, 3, 2}), (YaziMode::Ratios{1, 3, 2}));
        const YaziMode::Ratios large = YaziMode::clampedRatios({4 * YaziMode::MaximumRatio, 2 * YaziMode::MaximumRatio, 1});
        QCOMPARE(large.parent, YaziMode::MaximumRatio);
        QCOMPARE(large.current, YaziMode::MaximumRatio / 2);
        QCOMPARE(large.preview, 1);
        QCOMPARE(YaziMode::ratiosToList(YaziMode::Ratios()), (QList<int>{1, 3, 2}));
    }

    void sessionRatiosEntryRoundTrips()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Pane"));
        QVERIFY(YaziMode::readSessionRatios(group).isEmpty());
        YaziMode::writeSessionRatios(group, {180, 420, 300});
        QCOMPARE(group.readEntry("YaziColumnRatios", QString()), QStringLiteral("180,420,300"));
        QCOMPARE(YaziMode::readSessionRatios(group), (QList<int>{180, 420, 300}));
        for (const QString &invalid : {QStringLiteral("1,3"), QStringLiteral("1,0,2"), QStringLiteral("1,-3,2"), QStringLiteral("wide")}) {
            group.writeEntry("YaziColumnRatios", invalid);
            QVERIFY2(YaziMode::readSessionRatios(group).isEmpty(), qPrintable(invalid));
        }
        group.writeEntry("YaziColumnRatios", QStringLiteral("1,%1,1").arg(2 * YaziMode::MaximumRatio));
        QCOMPARE(YaziMode::readSessionRatios(group), (QList<int>{1, YaziMode::MaximumRatio, 1}));
        YaziMode::writeSessionRatios(group, {});
        QVERIFY(!group.hasKey("YaziColumnRatios"));
        QVERIFY(YaziMode::readSessionRatios(group).isEmpty());
    }

    void workspaceRatiosEntryRoundTrips()
    {
        QJsonObject object;
        YaziMode::writeWorkspaceRatios(object, {});
        QVERIFY(object.isEmpty());
        QList<int> ratios{1, 1, 1};
        QVERIFY(YaziMode::readWorkspaceRatios(object, ratios));
        QVERIFY(ratios.isEmpty());

        YaziMode::writeWorkspaceRatios(object, {2, 5, 3});
        QCOMPARE(object.value(QStringLiteral("yaziColumnRatios")), QJsonValue(QJsonArray{2, 5, 3}));
        QVERIFY(YaziMode::readWorkspaceRatios(object, ratios));
        QCOMPARE(ratios, (QList<int>{2, 5, 3}));

        object.insert(QStringLiteral("yaziColumnRatios"), QJsonArray{1, 2.0 * YaziMode::MaximumRatio, 1});
        QVERIFY(YaziMode::readWorkspaceRatios(object, ratios));
        QCOMPARE(ratios, (QList<int>{1, YaziMode::MaximumRatio, 1}));
    }

    void invalidWorkspaceRatiosEntryIsRejected_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("number") << QJsonValue(1);
        QTest::newRow("string") << QJsonValue(QStringLiteral("1,3,2"));
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("empty") << QJsonValue(QJsonArray());
        QTest::newRow("two") << QJsonValue(QJsonArray{1, 3});
        QTest::newRow("four") << QJsonValue(QJsonArray{1, 3, 2, 1});
        QTest::newRow("zero") << QJsonValue(QJsonArray{1, 0, 2});
        QTest::newRow("negative") << QJsonValue(QJsonArray{1, -3, 2});
        QTest::newRow("fraction") << QJsonValue(QJsonArray{1, 2.5, 2});
        QTest::newRow("beyond int") << QJsonValue(QJsonArray{1, 1e12, 2});
        QTest::newRow("text entry") << QJsonValue(QJsonArray{1, QStringLiteral("3"), 2});
        QTest::newRow("bool entry") << QJsonValue(QJsonArray{1, true, 2});
    }

    void invalidWorkspaceRatiosEntryIsRejected()
    {
        QFETCH(QJsonValue, value);
        const QJsonObject object{{QStringLiteral("yaziColumnRatios"), value}};
        QList<int> ratios{1, 1, 1};
        QVERIFY(!YaziMode::readWorkspaceRatios(object, ratios));
        QCOMPARE(ratios, (QList<int>{1, 1, 1}));
    }

    void sessionParentColumnEntryRoundTrips()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Pane"));
        QCOMPARE(YaziMode::readSessionParentColumn(group), -1);
        YaziMode::writeSessionParentColumn(group, 0);
        QCOMPARE(group.readEntry("YaziParentColumn", QString()), QStringLiteral("false"));
        QCOMPARE(YaziMode::readSessionParentColumn(group), 0);
        YaziMode::writeSessionParentColumn(group, 1);
        QCOMPARE(YaziMode::readSessionParentColumn(group), 1);
        YaziMode::writeSessionParentColumn(group, -1);
        QVERIFY(!group.hasKey("YaziParentColumn"));
        QCOMPARE(YaziMode::readSessionParentColumn(group), -1);
    }

    void workspaceParentColumnEntryRoundTrips()
    {
        QJsonObject object;
        YaziMode::writeWorkspaceParentColumn(object, -1);
        QVERIFY(object.isEmpty());
        int parent = 1;
        QVERIFY(YaziMode::readWorkspaceParentColumn(object, parent));
        QCOMPARE(parent, -1);
        YaziMode::writeWorkspaceParentColumn(object, 0);
        QCOMPARE(object.value(QStringLiteral("yaziParentColumn")), QJsonValue(false));
        QVERIFY(YaziMode::readWorkspaceParentColumn(object, parent));
        QCOMPARE(parent, 0);
        YaziMode::writeWorkspaceParentColumn(object, 1);
        QCOMPARE(object.value(QStringLiteral("yaziParentColumn")), QJsonValue(true));
        QVERIFY(YaziMode::readWorkspaceParentColumn(object, parent));
        QCOMPARE(parent, 1);
    }

    void invalidWorkspaceParentColumnEntryIsRejected_data()
    {
        invalidWorkspaceEntryIsRejected_data();
    }

    void invalidWorkspaceParentColumnEntryIsRejected()
    {
        QFETCH(QJsonValue, value);
        const QJsonObject object{{QStringLiteral("yaziParentColumn"), value}};
        int parent = 1;
        QVERIFY(!YaziMode::readWorkspaceParentColumn(object, parent));
        QCOMPARE(parent, 1);
    }

    void matcherMatchesSingleKeys()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_J), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::MoveDown);
        QCOMPARE(matcher.count(), 1);
        QVERIFY(!matcher.isPending());
        QCOMPARE(matcher.feed(Qt::ShiftModifier | Qt::Key_G), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::MoveLast);
        QCOMPARE(matcher.feed(Qt::ControlModifier | Qt::Key_D), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::HalfPageDown);
        // The keypad modifier doesn't change what a key means.
        QCOMPARE(matcher.feed(Qt::KeypadModifier | Qt::Key_Enter), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::Open);
    }

    void matcherWaitsForTheRestOfASequence()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QVERIFY(matcher.isPending());
        QCOMPARE(matcher.pendingKeys(), YaziMode::KeySequence{Qt::Key_G});
        QCOMPARE(matcher.pendingText(), QStringLiteral("g"));
        // Modifier keys pressed alone, as before Shift+G, keep the sequence.
        QCOMPARE(matcher.feed(Qt::ShiftModifier | Qt::Key_Shift), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::MoveFirst);
        QVERIFY(!matcher.isPending());
        QVERIFY(matcher.pendingText().isEmpty());

        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_H), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::Home);
        QCOMPARE(matcher.feed(Qt::Key_Z), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_P), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::TogglePreview);
    }

    void matcherLetsUnboundKeysThrough()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_W), Result::NoMatch);
        QCOMPARE(matcher.feed(Qt::Key_0), Result::NoMatch);
        QCOMPARE(matcher.feed(Qt::ControlModifier | Qt::Key_C), Result::NoMatch);
        QCOMPARE(matcher.feed(Qt::Key_Shift), Result::NoMatch);
        QVERIFY(!matcher.isPending());

        // Standard keys leave letters, Escape and modified arrows alone.
        YaziMode::KeyMatcher standard(YaziMode::presetKeymap(YaziMode::Preset::Standard));
        QCOMPARE(standard.feed(Qt::Key_J), Result::NoMatch);
        QCOMPARE(standard.feed(Qt::Key_5), Result::NoMatch);
        QCOMPARE(standard.feed(Qt::ShiftModifier | Qt::Key_Down), Result::NoMatch);
        QCOMPARE(standard.feed(Qt::AltModifier | Qt::Key_Left), Result::NoMatch);
        QCOMPARE(standard.feed(Qt::Key_Down), Result::Matched);
        QCOMPARE(standard.command(), YaziMode::Command::MoveDown);
        QCOMPARE(standard.feed(Qt::Key_Left), Result::Matched);
        QCOMPARE(standard.command(), YaziMode::Command::Parent);
    }

    void escapeCancelsOnlyWhatIsPending()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::NoMatch);
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QCOMPARE(matcher.test(Qt::Key_Escape), Result::Cancelled);
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::Cancelled);
        QVERIFY(!matcher.isPending());
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::NoMatch);
        // After cancelling, g starts a new sequence rather than completing gg.
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        // A digit breaks the sequence and starts a count.
        QCOMPARE(matcher.feed(Qt::Key_3), Result::Pending);
        QVERIFY(matcher.pendingKeys().isEmpty());
        QCOMPARE(matcher.pendingCount(), 3);
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::Cancelled);
        QVERIFY(!matcher.isPending());

        QCOMPARE(matcher.feed(Qt::Key_4), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::Cancelled);
        QCOMPARE(matcher.pendingCount(), 0);
    }

    void brokenSequenceRetriesTheKeyOnItsOwn()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_J), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::MoveDown);

        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_Z), Result::Pending);
        QCOMPARE(matcher.pendingText(), QStringLiteral("z"));
        QCOMPARE(matcher.feed(Qt::Key_W), Result::NoMatch);
        QVERIFY(!matcher.isPending());
    }

    void countsRepeatCommands()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_1), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_2), Result::Pending);
        QCOMPARE(matcher.pendingCount(), 12);
        QCOMPARE(matcher.pendingText(), QStringLiteral("12"));
        QCOMPARE(matcher.feed(Qt::Key_J), Result::Matched);
        QCOMPARE(matcher.command(), YaziMode::Command::MoveDown);
        QCOMPARE(matcher.count(), 12);

        QCOMPARE(matcher.feed(Qt::Key_5), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QCOMPARE(matcher.pendingText(), QStringLiteral("5g"));
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Matched);
        QCOMPARE(matcher.count(), 5);

        QCOMPARE(matcher.feed(Qt::Key_2), Result::Pending);
        QCOMPARE(matcher.feed(Qt::ControlModifier | Qt::Key_D), Result::Matched);
        QCOMPARE(matcher.count(), 2);

        // Counts stop growing at the maximum.
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(matcher.feed(Qt::Key_9), Result::Pending);
        }
        QCOMPARE(matcher.pendingCount(), YaziMode::MaximumCount);
        // A count followed by an unbound key is dropped with it.
        QCOMPARE(matcher.feed(Qt::Key_W), Result::NoMatch);
        QVERIFY(!matcher.isPending());
        QCOMPARE(matcher.feed(Qt::Key_K), Result::Matched);
        QCOMPARE(matcher.count(), 1);

        // The standard keys don't take counts.
        YaziMode::KeyMatcher standard(YaziMode::presetKeymap(YaziMode::Preset::Standard));
        QCOMPARE(standard.feed(Qt::Key_3), Result::NoMatch);
        QVERIFY(!standard.isPending());
    }

    void testLeavesTheStateAlone()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.test(Qt::Key_G), Result::Pending);
        QVERIFY(!matcher.isPending());
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        QCOMPARE(matcher.test(Qt::Key_G), Result::Matched);
        QCOMPARE(matcher.test(Qt::Key_B), Result::NoMatch);
        QVERIFY(matcher.isPending());
        QCOMPARE(matcher.pendingKeys(), YaziMode::KeySequence{Qt::Key_G});
        matcher.reset();
        QVERIFY(!matcher.isPending());
    }

    void setKeymapCancelsAPendingSequence()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        QCOMPARE(matcher.feed(Qt::Key_G), Result::Pending);
        matcher.setKeymap(YaziMode::presetKeymap(YaziMode::Preset::Standard));
        QVERIFY(!matcher.isPending());
        QCOMPARE(matcher.feed(Qt::Key_G), Result::NoMatch);
    }

    void presetsHaveNoConflicts_data()
    {
        QTest::addColumn<YaziMode::Preset>("preset");
        QTest::newRow("standard") << YaziMode::Preset::Standard;
        QTest::newRow("vim") << YaziMode::Preset::Vim;
    }

    void presetsHaveNoConflicts()
    {
        QFETCH(YaziMode::Preset, preset);
        const YaziMode::Keymap keymap = YaziMode::presetKeymap(preset);
        QVERIFY(!keymap.bindings.isEmpty());
        QCOMPARE(YaziMode::conflicts(keymap), QList<YaziMode::Conflict>());
        // Every binding can be typed and runs its own command.
        for (const YaziMode::Binding &binding : keymap.bindings) {
            YaziMode::KeyMatcher matcher(keymap);
            for (qsizetype i = 0; i < binding.keys.size(); ++i) {
                const Result result = matcher.feed(binding.keys.at(i));
                QCOMPARE(result, i + 1 < binding.keys.size() ? Result::Pending : Result::Matched);
            }
            QCOMPARE(matcher.command(), binding.command);
            QVERIFY(!YaziMode::commandId(binding.command).isEmpty());
        }
    }

    void vimKeysExtendTheStandardKeys()
    {
        const YaziMode::Keymap standard = YaziMode::presetKeymap(YaziMode::Preset::Standard);
        const YaziMode::Keymap vim = YaziMode::presetKeymap(YaziMode::Preset::Vim);
        QVERIFY(!standard.counts);
        QVERIFY(vim.counts);
        for (const YaziMode::Binding &binding : standard.bindings) {
            QVERIFY(vim.bindings.contains(binding));
        }
    }

    void conflictsFindPrefixesAndUnusableBindings()
    {
        YaziMode::Keymap keymap;
        keymap.bindings = {
            {{Qt::Key_G}, YaziMode::Command::MoveFirst},
            {{Qt::Key_G, Qt::Key_G}, YaziMode::Command::MoveLast},
            {{Qt::Key_J}, YaziMode::Command::MoveDown},
            {{Qt::Key_J}, YaziMode::Command::MoveUp},
            {{}, YaziMode::Command::Open},
            {{Qt::Key_Escape}, YaziMode::Command::Leave},
            {{Qt::Key_5, Qt::Key_J}, YaziMode::Command::PageDown},
            {{Qt::Key_0}, YaziMode::Command::Home},
        };
        // Without counts, digits are ordinary keys.
        QCOMPARE(YaziMode::conflicts(keymap), (QList<YaziMode::Conflict>{{0, 1}, {2, 3}, {4, -1}, {5, -1}}));
        keymap.counts = true;
        // With counts, 5 can't start a binding, though 0 can.
        QCOMPARE(YaziMode::conflicts(keymap), (QList<YaziMode::Conflict>{{0, 1}, {2, 3}, {4, -1}, {5, -1}, {6, -1}}));
    }

    void commandsHaveDistinctIdentifiers()
    {
        QStringList ids;
        for (int i = int(YaziMode::Command::MoveUp); i <= int(YaziMode::LastCommand); ++i) {
            const QString id = YaziMode::commandId(YaziMode::Command(i));
            QVERIFY(!id.isEmpty());
            QVERIFY2(!ids.contains(id), qPrintable(id));
            ids.append(id);
        }
        QCOMPARE(YaziMode::commandId(YaziMode::Command::MoveDown), QStringLiteral("move-down"));
        QCOMPARE(YaziMode::commandId(YaziMode::Command::Trash), QStringLiteral("trash"));
        QVERIFY(YaziMode::commandTakesCount(YaziMode::Command::MoveDown));
        QVERIFY(YaziMode::commandTakesCount(YaziMode::Command::HalfPageUp));
        QVERIFY(YaziMode::commandTakesCount(YaziMode::Command::ToggleSelectionDown));
        QVERIFY(!YaziMode::commandTakesCount(YaziMode::Command::Parent));
        QVERIFY(!YaziMode::commandTakesCount(YaziMode::Command::Leave));
        QVERIFY(!YaziMode::commandTakesCount(YaziMode::Command::Trash));
        QVERIFY(!YaziMode::commandTakesCount(YaziMode::Command::Paste));
    }

    void commandsHaveDistinctTexts()
    {
        QStringList texts;
        for (int i = int(YaziMode::Command::MoveUp); i <= int(YaziMode::LastCommand); ++i) {
            const auto command = YaziMode::Command(i);
            const QString text = YaziMode::commandText(command);
            QVERIFY2(!text.isEmpty(), qPrintable(YaziMode::commandId(command)));
            QVERIFY2(!texts.contains(text), qPrintable(text));
            texts.append(text);
        }
        QCOMPARE(YaziMode::commandText(YaziMode::Command::MoveDown), QStringLiteral("Move down"));
        QCOMPARE(YaziMode::commandText(YaziMode::Command::Trash), QStringLiteral("Move the items to the Trash"));
    }

    void shadowedShortcutsBeginABindingOrACount()
    {
        const QList<YaziMode::ActionShortcuts> actions = {
            {QStringLiteral("open_gallery"), QStringLiteral("Open Gallery"), {QKeySequence(Qt::Key_Space)}},
            {QStringLiteral("add_to_favorites"), QStringLiteral("Add to Favorites"), {QKeySequence(Qt::ControlModifier | Qt::Key_D)}},
            {QStringLiteral("reload"), QStringLiteral("Reload"), {QKeySequence(), QKeySequence(Qt::ControlModifier | Qt::Key_R)}},
            {QStringLiteral("go_up"), QStringLiteral("Up"), {QKeySequence(Qt::Key_Backspace), QKeySequence(Qt::AltModifier | Qt::Key_Up)}},
            {QStringLiteral("select"), QStringLiteral("Select"), {QKeySequence(Qt::ControlModifier | Qt::Key_Space)}},
            {QStringLiteral("digits"),
             QStringLiteral("Digits"),
             {QKeySequence(Qt::Key_1), QKeySequence(Qt::KeypadModifier | Qt::Key_2), QKeySequence(Qt::Key_0)}},
            {QStringLiteral("chord"), QStringLiteral("Chord"), {QKeySequence(Qt::Key_G, Qt::Key_X)}},
            {QStringLiteral("plain"), QStringLiteral("Plain"), {QKeySequence(Qt::Key_Escape), QKeySequence(Qt::Key_W)}},
            {QStringLiteral("last"), QStringLiteral("Last"), {QKeySequence(Qt::ShiftModifier | Qt::Key_G)}},
            {QStringLiteral("hidden"), QStringLiteral("Hidden"), {QKeySequence(Qt::ShiftModifier | Qt::Key_Period)}},
        };
        // The indexes of the bindings that @p keys of @p keymap begin with.
        const auto bindingsStartingWith = [](const YaziMode::Keymap &keymap, QKeyCombination key) {
            QList<int> found;
            for (int i = 0; i < keymap.bindings.size(); ++i) {
                if (keymap.bindings.at(i).keys.first() == key) {
                    found.append(i);
                }
            }
            return found;
        };

        const YaziMode::Keymap standard = YaziMode::presetKeymap(YaziMode::Preset::Standard);
        const QList<int> controlSpace = bindingsStartingWith(standard, Qt::ControlModifier | Qt::Key_Space);
        QCOMPARE(controlSpace.size(), 1);
        QCOMPARE(YaziMode::shadowedShortcuts(standard, actions),
                 (QList<YaziMode::ShadowedShortcut>{
                     {QStringLiteral("select"), QStringLiteral("Select"), QKeySequence(Qt::ControlModifier | Qt::Key_Space), controlSpace}}));

        const YaziMode::Keymap vim = YaziMode::presetKeymap(YaziMode::Preset::Vim);
        const QList<int> g = bindingsStartingWith(vim, QKeyCombination(Qt::Key_G));
        // gg, gh, gp, gc, gv, gT, gt and gw.
        QCOMPARE(g.size(), 8);
        const QList<YaziMode::ShadowedShortcut> expected = {
            {QStringLiteral("open_gallery"), QStringLiteral("Open Gallery"), QKeySequence(Qt::Key_Space), bindingsStartingWith(vim, Qt::Key_Space)},
            {QStringLiteral("add_to_favorites"),
             QStringLiteral("Add to Favorites"),
             QKeySequence(Qt::ControlModifier | Qt::Key_D),
             bindingsStartingWith(vim, Qt::ControlModifier | Qt::Key_D)},
            {QStringLiteral("select"), QStringLiteral("Select"), QKeySequence(Qt::ControlModifier | Qt::Key_Space), controlSpace},
            // Digits from 1 begin a count, whether typed on the keypad or not; 0 only continues one.
            {QStringLiteral("digits"), QStringLiteral("Digits"), QKeySequence(Qt::Key_1), {}},
            {QStringLiteral("digits"), QStringLiteral("Digits"), QKeySequence(Qt::KeypadModifier | Qt::Key_2), {}},
            // Only the first key of a shortcut matters: g begins sequences, so g, x never fires.
            {QStringLiteral("chord"), QStringLiteral("Chord"), QKeySequence(Qt::Key_G, Qt::Key_X), g},
            {QStringLiteral("last"),
             QStringLiteral("Last"),
             QKeySequence(Qt::ShiftModifier | Qt::Key_G),
             bindingsStartingWith(vim, Qt::ShiftModifier | Qt::Key_G)},
            // Shift doesn't change punctuation.
            {QStringLiteral("hidden"), QStringLiteral("Hidden"), QKeySequence(Qt::ShiftModifier | Qt::Key_Period), bindingsStartingWith(vim, Qt::Key_Period)},
        };
        const QList<YaziMode::ShadowedShortcut> shadowed = YaziMode::shadowedShortcuts(vim, actions);
        QCOMPARE(shadowed, expected);
        for (const YaziMode::ShadowedShortcut &shortcut : shadowed) {
            if (shortcut.name != QLatin1String("digits")) {
                QVERIFY2(!shortcut.bindings.isEmpty(), qPrintable(shortcut.shortcut.toString()));
            }
        }
        QVERIFY(YaziMode::shadowedShortcuts(vim, {}).isEmpty());
    }

    void fileCommandsRunWindowActions()
    {
        for (int i = int(YaziMode::Command::MoveUp); i <= int(YaziMode::LastCommand); ++i) {
            const auto command = YaziMode::Command(i);
            const QString id = YaziMode::commandId(command);
            QVERIFY2(!(YaziMode::isFileCommand(command) && YaziMode::isSelectionCommand(command)), qPrintable(id));
            if (YaziMode::commandUsesSelection(command)) {
                QVERIFY2(YaziMode::isFileCommand(command), qPrintable(id));
            }
            if (!YaziMode::commandAction(command).isEmpty()) {
                QVERIFY2(YaziMode::isFileCommand(command), qPrintable(id));
            }
        }
        QCOMPARE(YaziMode::commandAction(YaziMode::Command::Trash), QStringLiteral("move_to_trash"));
        QCOMPARE(YaziMode::commandAction(YaziMode::Command::Copy), QStringLiteral("edit_copy"));
        QCOMPARE(YaziMode::commandAction(YaziMode::Command::Paste), QStringLiteral("edit_paste"));
        QCOMPARE(YaziMode::commandAction(YaziMode::Command::Undo), QStringLiteral("edit_undo"));
        QVERIFY(YaziMode::commandAction(YaziMode::Command::MoveDown).isEmpty());
        QVERIFY(YaziMode::commandUsesSelection(YaziMode::Command::Trash));
        QVERIFY(!YaziMode::commandUsesSelection(YaziMode::Command::Paste));
        QVERIFY(!YaziMode::commandUsesSelection(YaziMode::Command::Undo));
    }

    void noCommandDeletesPermanently()
    {
        const QStringList permanent = YaziMode::permanentDeletionActions();
        QVERIFY(permanent.contains(QStringLiteral("delete")));
        for (int i = int(YaziMode::Command::MoveUp); i <= int(YaziMode::LastCommand); ++i) {
            const auto command = YaziMode::Command(i);
            QVERIFY2(!permanent.contains(YaziMode::commandAction(command)), qPrintable(YaziMode::commandId(command)));
            QVERIFY2(!YaziMode::commandId(command).contains(QStringLiteral("delete")), qPrintable(YaziMode::commandId(command)));
        }
        // Not even a preset binding: in Yazi, D deletes permanently, so the Vim keys leave it unbound.
        for (YaziMode::Preset preset : {YaziMode::Preset::Standard, YaziMode::Preset::Vim}) {
            const YaziMode::Keymap keymap = YaziMode::presetKeymap(preset);
            for (const YaziMode::Binding &binding : keymap.bindings) {
                QVERIFY(!permanent.contains(YaziMode::commandAction(binding.command)));
                QVERIFY(binding.keys != YaziMode::KeySequence{Qt::ShiftModifier | Qt::Key_D});
                QVERIFY(binding.keys != YaziMode::KeySequence{Qt::ShiftModifier | Qt::Key_Delete});
            }
            YaziMode::KeyMatcher matcher(keymap);
            QCOMPARE(matcher.feed(Qt::ShiftModifier | Qt::Key_D), Result::NoMatch);
        }
    }

    void vimKeysSelectAndRunFileActions()
    {
        YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Vim));
        // The command @p keys run, or -1.
        const auto run = [&matcher](const YaziMode::KeySequence &keys) {
            for (qsizetype i = 0; i + 1 < keys.size(); ++i) {
                if (matcher.feed(keys.at(i)) != Result::Pending) {
                    return -1;
                }
            }
            return matcher.feed(keys.last()) == Result::Matched ? int(matcher.command()) : -1;
        };
        using Command = YaziMode::Command;
        QCOMPARE(run({Qt::Key_Space}), int(Command::ToggleSelectionDown));
        QCOMPARE(run({Qt::ControlModifier | Qt::Key_Space}), int(Command::ToggleSelection));
        QCOMPARE(run({Qt::Key_V}), int(Command::VisualSelection));
        QCOMPARE(run({Qt::ShiftModifier | Qt::Key_V}), int(Command::SelectAll));
        QCOMPARE(run({Qt::ControlModifier | Qt::Key_A}), int(Command::SelectAll));
        QCOMPARE(run({Qt::Key_Y, Qt::Key_Y}), int(Command::Copy));
        QCOMPARE(run({Qt::Key_X}), int(Command::Cut));
        QCOMPARE(run({Qt::Key_P}), int(Command::Paste));
        QCOMPARE(run({Qt::Key_R}), int(Command::Rename));
        QCOMPARE(run({Qt::Key_D, Qt::Key_D}), int(Command::Trash));
        QCOMPARE(run({Qt::Key_A}), int(Command::Create));
        QCOMPARE(run({Qt::Key_O}), int(Command::OpenWith));
        QCOMPARE(run({Qt::Key_C}), int(Command::CopyToOtherPane));
        QCOMPARE(run({Qt::Key_M}), int(Command::MoveToOtherPane));
        QCOMPARE(run({Qt::Key_U}), int(Command::Undo));
        QCOMPARE(run({Qt::Key_Period}), int(Command::ToggleHidden));
        // A count repeats Space.
        QCOMPARE(matcher.feed(Qt::Key_3), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_Space), Result::Matched);
        QCOMPARE(matcher.count(), 3);
        // Escape is left to the view, which clears the selection, unless it cancels a sequence.
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::NoMatch);
        QCOMPARE(matcher.feed(Qt::Key_D), Result::Pending);
        QCOMPARE(matcher.feed(Qt::Key_Escape), Result::Cancelled);
    }

    void standardKeysLeaveFileShortcutsToTheWindow()
    {
        const YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Standard));
        // Space stays Open Gallery; the window's file shortcuts pass through.
        for (QKeyCombination key : {QKeyCombination(Qt::Key_Space),
                                    QKeyCombination(Qt::Key_Delete),
                                    QKeyCombination(Qt::Key_F2),
                                    QKeyCombination(Qt::Key_F5),
                                    QKeyCombination(Qt::Key_F6),
                                    QKeyCombination(Qt::Key_F7),
                                    QKeyCombination(Qt::Key_F8),
                                    Qt::ControlModifier | Qt::Key_A,
                                    Qt::ControlModifier | Qt::Key_C,
                                    Qt::ControlModifier | Qt::Key_X,
                                    Qt::ControlModifier | Qt::Key_V,
                                    Qt::ControlModifier | Qt::Key_Z,
                                    Qt::ControlModifier | Qt::Key_H,
                                    QKeyCombination(Qt::Key_D),
                                    QKeyCombination(Qt::Key_Y)}) {
            QCOMPARE(matcher.test(key), Result::NoMatch);
        }
        QCOMPARE(matcher.test(Qt::ControlModifier | Qt::Key_Space), Result::Matched);
    }

    void keysShowAsTyped()
    {
        QCOMPARE(YaziMode::keyText(Qt::Key_G), QStringLiteral("g"));
        QCOMPARE(YaziMode::keyText(Qt::ShiftModifier | Qt::Key_G), QStringLiteral("G"));
        QCOMPARE(YaziMode::keyText(Qt::ControlModifier | Qt::Key_D), QKeySequence(Qt::ControlModifier | Qt::Key_D).toString(QKeySequence::NativeText));
        QCOMPARE(YaziMode::sequenceText({Qt::Key_G, Qt::Key_G}), QStringLiteral("gg"));
        QCOMPARE(YaziMode::sequenceText({Qt::Key_G}, 5), QStringLiteral("5g"));
        QCOMPARE(YaziMode::sequenceText({}, 12), QStringLiteral("12"));
        QCOMPARE(YaziMode::sequenceText({Qt::ControlModifier | Qt::Key_D}, 2),
                 QStringLiteral("2 ") + QKeySequence(Qt::ControlModifier | Qt::Key_D).toString(QKeySequence::NativeText));
        QCOMPARE(YaziMode::normalizedKey(Qt::KeypadModifier | Qt::Key_Up), QKeyCombination(Qt::Key_Up));
    }

    void vimKeysFilterSearchSortAndChangeTabs()
    {
        const YaziMode::Keymap keymap = YaziMode::presetKeymap(YaziMode::Preset::Vim);
        QCOMPARE(YaziMode::conflicts(keymap), QList<YaziMode::Conflict>());
        YaziMode::KeyMatcher matcher(keymap);
        const auto run = [&matcher](const YaziMode::KeySequence &keys) {
            for (qsizetype i = 0; i + 1 < keys.size(); ++i) {
                if (matcher.feed(keys.at(i)) != Result::Pending) {
                    return -1;
                }
            }
            return matcher.feed(keys.last()) == Result::Matched ? int(matcher.command()) : -1;
        };
        using Command = YaziMode::Command;
        QCOMPARE(run({Qt::Key_Slash}), int(Command::Filter));
        QCOMPARE(run({Qt::Key_Backslash}), int(Command::ClearFilter));
        QCOMPARE(run({Qt::Key_S}), int(Command::SearchNames));
        QCOMPARE(run({Qt::ShiftModifier | Qt::Key_S}), int(Command::SearchContents));
        QCOMPARE(run({Qt::Key_Comma, Qt::Key_N}), int(Command::SortByName));
        QCOMPARE(run({Qt::Key_Comma, Qt::Key_S}), int(Command::SortBySize));
        QCOMPARE(run({Qt::Key_Comma, Qt::Key_M}), int(Command::SortByModified));
        QCOMPARE(run({Qt::Key_Comma, Qt::Key_E}), int(Command::SortByType));
        QCOMPARE(run({Qt::Key_Comma, Qt::Key_R}), int(Command::ReverseSort));
        QCOMPARE(run({Qt::Key_T}), int(Command::NewTab));
        QCOMPARE(run({Qt::Key_BracketLeft}), int(Command::PreviousTab));
        QCOMPARE(run({Qt::Key_BracketRight}), int(Command::NextTab));
        QCOMPARE(run({Qt::Key_G, Qt::ShiftModifier | Qt::Key_T}), int(Command::PreviousTab));
        QCOMPARE(run({Qt::Key_G, Qt::Key_T}), int(Command::NextTab));
        QCOMPARE(run({Qt::Key_G, Qt::Key_W}), int(Command::CloseTab));
        // The older g sequences still work beside the tab ones.
        QCOMPARE(run({Qt::Key_G, Qt::Key_G}), int(Command::MoveFirst));
        // Layouts that type / or [ with Shift still reach the commands.
        QCOMPARE(run({Qt::ShiftModifier | Qt::Key_Slash}), int(Command::Filter));
        QCOMPARE(run({Qt::ShiftModifier | Qt::Key_BracketRight}), int(Command::NextTab));
        // A comma waits for the sort key; another key breaks it and runs on its own.
        QCOMPARE(matcher.feed(Qt::Key_Comma), Result::Pending);
        QCOMPARE(matcher.pendingText(), QStringLiteral(","));
        QCOMPARE(matcher.feed(Qt::Key_J), Result::Matched);
        QCOMPARE(matcher.command(), Command::MoveDown);
        // Counts don't repeat these commands.
        for (Command command : {Command::Filter,
                                Command::ClearFilter,
                                Command::SearchNames,
                                Command::SearchContents,
                                Command::SortByName,
                                Command::ReverseSort,
                                Command::NewTab,
                                Command::PreviousTab,
                                Command::NextTab,
                                Command::CloseTab}) {
            QVERIFY2(!YaziMode::commandTakesCount(command), qPrintable(YaziMode::commandId(command)));
            QVERIFY2(!YaziMode::isFileCommand(command), qPrintable(YaziMode::commandId(command)));
            QVERIFY2(YaziMode::commandAction(command).isEmpty(), qPrintable(YaziMode::commandId(command)));
        }
        QCOMPARE(YaziMode::commandId(Command::SortByModified), QStringLiteral("sort-by-modified"));
        QCOMPARE(YaziMode::commandId(Command::CloseTab), QStringLiteral("close-tab"));
    }

    void standardKeysLeaveFilterSearchAndTabKeysAlone()
    {
        const YaziMode::KeyMatcher matcher(YaziMode::presetKeymap(YaziMode::Preset::Standard));
        for (QKeyCombination key : {QKeyCombination(Qt::Key_Slash),
                                    QKeyCombination(Qt::Key_Backslash),
                                    QKeyCombination(Qt::Key_S),
                                    QKeyCombination(Qt::Key_Comma),
                                    QKeyCombination(Qt::Key_T),
                                    QKeyCombination(Qt::Key_BracketLeft),
                                    QKeyCombination(Qt::Key_BracketRight),
                                    Qt::ControlModifier | Qt::Key_I,
                                    Qt::ControlModifier | Qt::Key_F,
                                    Qt::ControlModifier | Qt::Key_T,
                                    Qt::ControlModifier | Qt::Key_W}) {
            QCOMPARE(matcher.test(key), Result::NoMatch);
        }
    }

    void shiftIsIgnoredOnlyForPunctuation()
    {
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_Slash), QKeyCombination(Qt::Key_Slash));
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_Comma), QKeyCombination(Qt::Key_Comma));
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_BraceLeft), QKeyCombination(Qt::Key_BraceLeft));
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::ControlModifier | Qt::Key_Backslash), Qt::ControlModifier | Qt::Key_Backslash);
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_S), Qt::ShiftModifier | Qt::Key_S);
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_Space), Qt::ShiftModifier | Qt::Key_Space);
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_5), Qt::ShiftModifier | Qt::Key_5);
        QCOMPARE(YaziMode::normalizedKey(Qt::ShiftModifier | Qt::Key_Tab), Qt::ShiftModifier | Qt::Key_Tab);
        QCOMPARE(YaziMode::keyText(Qt::Key_BracketRight), QStringLiteral("]"));
        QCOMPARE(YaziMode::sequenceText({Qt::Key_Comma, Qt::Key_N}), QStringLiteral(",n"));
    }

    void defaultSettingsCoverOnlyTheYaziGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KCoreConfigSkeleton settings(KSharedConfig::openConfig(dir.filePath(QStringLiteral("settingsrc")), KConfig::SimpleConfig));
        bool preview = false;
        QList<int> ratios;
        bool workspaceYazi = false;
        bool places = false;
        settings.setCurrentGroup(QStringLiteral("YaziMode"));
        settings.addItemBool(QStringLiteral("YaziPreviewVisible"), preview, true);
        settings.addItemIntList(QStringLiteral("YaziColumnRatios"), ratios, {1, 3, 2});
        settings.setCurrentGroup(QStringLiteral("Workspaces"));
        settings.addItemBool(QStringLiteral("WorkspaceDefaultYaziMode"), workspaceYazi, true);
        settings.setCurrentGroup(QStringLiteral("General"));
        settings.addItemBool(QStringLiteral("ShowPlaces"), places, true);
        settings.load();
        preview = false;
        ratios = {2, 5, 3};

        const QVariantMap defaults = YaziMode::defaultSettings(&settings);
        QCOMPARE(defaults.keys(), QStringList({QStringLiteral("YaziColumnRatios"), QStringLiteral("YaziPreviewVisible")}));
        QCOMPARE(defaults.value(QStringLiteral("YaziPreviewVisible")), QVariant(true));
        QCOMPARE(defaults.value(QStringLiteral("YaziColumnRatios")).value<QList<int>>(), QList<int>({1, 3, 2}));
        // Reading the defaults doesn't change the current values.
        QVERIFY(!preview);
        QCOMPARE(ratios, QList<int>({2, 5, 3}));
    }

private:
    using Result = YaziMode::KeyMatcher::Result;
};

QTEST_GUILESS_MAIN(YaziModeTest)

#include "yazimodetest.moc"
