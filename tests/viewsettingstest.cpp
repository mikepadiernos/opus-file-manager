// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewsettings.h"
#include "filecolorrules.h"

#include <KConfig>

#include <QTemporaryDir>
#include <QTest>

class ViewSettingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsAndRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("settings.rc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("View"));

        const ViewSettings defaults = ViewSettings::fromConfig(group);
        QCOMPARE(defaults.visibleColumns, QStringList({QStringLiteral("type"), QStringLiteral("size"), QStringLiteral("date")}));
        QCOMPARE(defaults.groupBy, QStringLiteral("none"));
        QCOMPARE(defaults.sizeFormat, QStringLiteral("human"));
        QCOMPARE(defaults.dateFormat, QStringLiteral("locale"));
        QCOMPARE(defaults.iconNameLines, 2);

        ViewSettings settings;
        settings.visibleColumns = {QStringLiteral("owner"), QStringLiteral("rating")};
        settings.groupBy = QStringLiteral("ext");
        settings.sizeFormat = QStringLiteral("bytes");
        settings.dateFormat = QStringLiteral("iso");
        settings.showSeconds = true;
        settings.iconNameLines = 4;
        settings.iconInformation = {QStringLiteral("size"), QStringLiteral("friendlyMimeType")};
        settings.writeConfig(group);

        const ViewSettings restored = ViewSettings::fromConfig(group);
        QCOMPARE(restored.visibleColumns, settings.visibleColumns);
        QCOMPARE(restored.groupBy, settings.groupBy);
        QCOMPARE(restored.sizeFormat, settings.sizeFormat);
        QCOMPARE(restored.dateFormat, settings.dateFormat);
        QCOMPARE(restored.showSeconds, settings.showSeconds);
        QCOMPARE(restored.iconNameLines, settings.iconNameLines);
        QCOMPARE(restored.iconInformation, settings.iconInformation);
    }

    void invalidSettingsAreSanitized()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("settings.rc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("View"));
        group.writeEntry("VisibleColumns", QStringList{QStringLiteral("bad"), QStringLiteral("size"), QStringLiteral("size")});
        group.writeEntry("GroupBy", QStringLiteral("invalid"));
        group.writeEntry("SizeFormat", QStringLiteral("invalid"));
        group.writeEntry("DateFormat", QStringLiteral("invalid"));
        group.writeEntry("IconNameLines", 100);
        group.writeEntry("IconInformation", QStringList{QStringLiteral("invalid"), QStringLiteral("size")});

        const ViewSettings settings = ViewSettings::fromConfig(group);
        QCOMPARE(settings.visibleColumns, QStringList({QStringLiteral("size")}));
        QCOMPARE(settings.groupBy, QStringLiteral("none"));
        QCOMPARE(settings.sizeFormat, QStringLiteral("human"));
        QCOMPARE(settings.dateFormat, QStringLiteral("locale"));
        QCOMPARE(settings.iconNameLines, 5);
        QCOMPARE(settings.iconInformation, QStringList({QStringLiteral("size")}));
    }

    void migratesLegacyColumnsWithoutOverwritingExplicitSettings()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfig config(dir.filePath(QStringLiteral("settings.rc")), KConfig::SimpleConfig);
        KConfigGroup legacy = config.group(QStringLiteral("View"));
        KConfigGroup views = config.group(QStringLiteral("Views"));
        legacy.writeEntry("VisibleColumns", QStringList{QStringLiteral("size"), QStringLiteral("rating")});
        views.writeEntry("ShowSizeColumn", false);

        QVERIFY(ViewSettings::migrateColumns(legacy, views));
        QVERIFY(!legacy.hasKey("VisibleColumns"));
        QVERIFY(!views.readEntry("ShowSizeColumn", true));
        QVERIFY(views.readEntry("ShowRatingColumn", false));
        QVERIFY(!views.readEntry("ShowTypeColumn", true));
        QVERIFY(!ViewSettings::migrateColumns(legacy, views));
    }

    void fileColorRulesMatchExtensionsCaseInsensitively()
    {
        const FileColorRules rules({QStringLiteral(".cpp=#123456"), QStringLiteral("tar.gz=#abcdef"), QStringLiteral("../bad=#ffffff")});
        QCOMPARE(rules.colorForFileName(QStringLiteral("Main.CPP")), QColor(QStringLiteral("#123456")));
        QCOMPARE(rules.colorForFileName(QStringLiteral("archive.TAR.GZ")), QColor(QStringLiteral("#abcdef")));
        QCOMPARE(rules.colorForFileName(QStringLiteral("notes.txt")), QColor());
        QCOMPARE(rules.entries(), QStringList({QStringLiteral("cpp=#123456"), QStringLiteral("tar.gz=#abcdef")}));
    }
};

QTEST_GUILESS_MAIN(ViewSettingsTest)

#include "viewsettingstest.moc"
