// SPDX-License-Identifier: GPL-3.0-or-later

#include "kwinscript.h"
#include "settings.h"

#include <KConfig>
#include <KConfigGroup>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace KWinScript;

class KWinScriptTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void placementRoundTripsThroughJson()
    {
        Placement placement;
        placement.screen = QStringLiteral("DP-1");
        placement.desktops = {QStringLiteral("a"), QStringLiteral("b")};
        placement.geometry = QRect(10, 20, 800, 600);
        placement.keepAbove = true;
        placement.tile = QRect(0, 0, 960, 1080);
        QCOMPARE(placementFromJson(toJson(placement)), std::optional<Placement>(placement));

        placement.tile = QRect();
        placement.onAllDesktops = true;
        const QJsonObject json = toJson(placement);
        QVERIFY(json.value(QStringLiteral("tile")).isNull());
        const QString text = QString::fromUtf8(QJsonDocument(json).toJson());
        QCOMPARE(placementFromJson(text), std::optional<Placement>(placement));
    }

    void rejectsMalformedPlacements()
    {
        QVERIFY(!placementFromJson(QStringLiteral("not json")));
        QVERIFY(!placementFromJson(QStringLiteral("[]")));
        QVERIFY(!placementFromJson(QStringLiteral(R"({"screen":"DP-1"})")));
        QVERIFY(!placementFromJson(QStringLiteral(R"({"geometry":{"x":0,"y":0,"width":0,"height":10}})")));
        QVERIFY(!placementFromJson(QStringLiteral(R"({"geometry":{"x":0,"y":0,"width":"10","height":10}})")));
        QVERIFY(!placementFromJson(QStringLiteral(R"({"geometry":{"x":0,"y":0,"width":10,"height":10},"desktops":"a"})")));
        QVERIFY(!placementFromJson(QStringLiteral(R"({"geometry":{"x":0,"y":0,"width":10,"height":10},"desktops":[1]})")));
        QVERIFY(!placementFromJson(QStringLiteral(R"({"geometry":{"x":0,"y":0,"width":10,"height":10},"tile":{"x":0}})")));
        QVERIFY(placementFromJson(QStringLiteral(R"({"geometry":{"x":0,"y":0,"width":10,"height":10}})")));
    }

    void optionsAndDropDownRoundTripThroughConfig()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("KWinScript"));
        QCOMPARE(readOptions(group), Options());
        QCOMPARE(readDropDown(group), DropDown());

        const Options options{.restoreWindows = false, .tileCompanions = true, .followDesktops = true};
        writeOptions(group, options);
        QCOMPARE(readOptions(group), options);

        DropDown dropDown;
        dropDown.enabled = true;
        dropDown.edge = Edge::Right;
        dropDown.width = 40;
        dropDown.height = 100;
        dropDown.screen = QStringLiteral("HDMI-A-1");
        dropDown.slide = false;
        dropDown.hideOnFocusLoss = true;
        dropDown.screenEdge = true;
        dropDown.touchEdge = true;
        dropDown.workspaceId = QStringLiteral("ws");
        writeDropDown(group, dropDown);
        QCOMPARE(readDropDown(group), dropDown);
    }

    void settingsMatchTheScriptOptions()
    {
        Settings *settings = Settings::self();
        const auto reset = qScopeGuard([settings] {
            settings->setDefaults();
            settings->save();
        });
        settings->setDefaults();
        settings->save();
        const KConfigGroup group = settings->config()->group(QStringLiteral("KWinScript"));
        QCOMPARE(readOptions(group), Options());

        settings->setKWinRestoreWindows(false);
        settings->setKWinTileCompanions(true);
        settings->setKWinFollowDesktops(true);
        settings->save();
        QCOMPARE(readOptions(group), (Options{.restoreWindows = false, .tileCompanions = true, .followDesktops = true}));

        // The script's own writes are what the settings read back.
        KConfigGroup written = settings->config()->group(QStringLiteral("KWinScript"));
        writeOptions(written, Options());
        settings->load();
        QVERIFY(settings->kWinRestoreWindows());
        QVERIFY(!settings->kWinTileCompanions());
        QVERIFY(!settings->kWinFollowDesktops());
    }

    void dropDownClampsSizesAndUnknownEdges()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("KWinScript"));
        group.writeEntry("DropDownWidth", 5);
        group.writeEntry("DropDownHeight", 250);
        group.writeEntry("DropDownEdge", QStringLiteral("diagonal"));
        const DropDown dropDown = readDropDown(group);
        QCOMPARE(dropDown.width, 10);
        QCOMPARE(dropDown.height, 100);
        QCOMPARE(dropDown.edge, Edge::Top);
        for (Edge edge : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) {
            QCOMPARE(edgeFromName(edgeName(edge)), std::optional<Edge>(edge));
        }
    }

    void desktopAssignmentsSkipEmptyEntries()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("KWinVirtualDesktops"));
        group.writeEntry("stale", QStringLiteral("old"));
        writeDesktopWorkspaces(group, {{QStringLiteral("d1"), QStringLiteral("w1")}, {QStringLiteral("d2"), QString()}});
        const QMap<QString, QString> expected{{QStringLiteral("d1"), QStringLiteral("w1")}};
        QCOMPARE(readDesktopWorkspaces(group), expected);
    }

    void desktopsParseFromJson()
    {
        const QList<Desktop> desktops = desktopsFromJson(QStringLiteral(R"([{"id":"a","name":"One"},{"name":"No id"},{"id":"b","name":"Two"}])"));
        const QList<Desktop> expected{{QStringLiteral("a"), QStringLiteral("One")}, {QStringLiteral("b"), QStringLiteral("Two")}};
        QCOMPARE(desktops, expected);
        QVERIFY(desktopsFromJson(QStringLiteral("{}")).isEmpty());
    }

    void placementsAreKeptPerWorkspace()
    {
        KConfig config(QString(), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("KWinWindowPlacements"));
        Placement placement;
        placement.geometry = QRect(1, 2, 300, 400);
        writePlacement(group, QStringLiteral("w1"), placement);
        writePlacement(group, QString(), placement);
        QCOMPARE(readPlacement(group, QStringLiteral("w1")), std::optional<Placement>(placement));
        QVERIFY(!readPlacement(group, QStringLiteral("w2")));
        QVERIFY(!readPlacement(group, QString()));
        QCOMPARE(group.keyList(), QStringList{QStringLiteral("w1")});
    }

    void requestCarriesOptionsAndCommands()
    {
        Placement placement;
        placement.geometry = QRect(0, 0, 100, 100);
        const QString text = request(42, Options(), DropDown(), {7}, QJsonArray{restoreCommand(placement)});
        const QJsonObject object = QJsonDocument::fromJson(text.toUtf8()).object();
        QCOMPARE(object.value(QStringLiteral("interface")).toInt(), interfaceVersion());
        QCOMPARE(object.value(QStringLiteral("pid")).toInteger(), 42);
        QCOMPARE(object.value(QStringLiteral("companionPids")).toArray(), QJsonArray{7});
        QCOMPARE(object.value(QStringLiteral("options")).toObject().value(QStringLiteral("restoreWindows")).toBool(), true);
        QCOMPARE(object.value(QStringLiteral("dropDown")).toObject().value(QStringLiteral("edge")).toString(), QStringLiteral("top"));
        const QJsonObject command = object.value(QStringLiteral("commands")).toArray().first().toObject();
        QCOMPARE(command.value(QStringLiteral("type")).toString(), QStringLiteral("restore"));
        QCOMPARE(placementFromJson(command.value(QStringLiteral("placement")).toObject()), std::optional<Placement>(placement));
    }

    void readsPackageVersions()
    {
        QCOMPARE(packageVersion(R"({"KPlugin":{"Id":"opusfilemanager","Version":"3"}})"), 3);
        QCOMPARE(packageVersion(R"({"KPlugin":{"Id":"other","Version":"3"}})"), -1);
        QCOMPARE(packageVersion(R"({"KPlugin":{"Id":"opusfilemanager","Version":"x"}})"), -1);
        QCOMPARE(packageVersion("garbage"), -1);

        QCOMPARE(installedVersion(), -1);
        QVERIFY(QDir().mkpath(installedPackagePath()));
        QFile metadata(installedPackagePath() + QLatin1String("/metadata.json"));
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(R"({"KPlugin":{"Id":"opusfilemanager","Version":"2"}})");
        metadata.close();
        QCOMPARE(installedVersion(), 2);
        QVERIFY(QDir(installedPackagePath()).removeRecursively());
    }

    void readsTheEnabledEntry()
    {
        KConfig kwinrc(QString(), KConfig::SimpleConfig);
        KConfigGroup plugins = kwinrc.group(QStringLiteral("Plugins"));
        QVERIFY(!isEnabled(plugins));
        plugins.writeEntry(enabledKey(), true);
        QVERIFY(isEnabled(plugins));
        QCOMPARE(enabledKey(), QStringLiteral("opusfilemanagerEnabled"));
    }

    void mapsStateToStatus()
    {
        State state;
        QCOMPARE(status(state), Status::KWinUnavailable);
        state.kwinRunning = true;
        QCOMPARE(status(state), Status::ScriptingUnavailable);
        state.scriptingAvailable = true;
        state.bundledVersion = 2;
        QCOMPARE(status(state), Status::NotInstalled);
        state.installedVersion = 1;
        QCOMPARE(status(state), Status::UpdateAvailable);
        state.installedVersion = 2;
        QCOMPARE(status(state), Status::Disabled);
        state.enabled = true;
        QCOMPARE(status(state), Status::NotRunning);
        state.loaded = true;
        QCOMPARE(status(state), Status::NotRunning);
        state.runningVersion = 1;
        QCOMPARE(status(state), Status::RestartRequired);
        state.runningVersion = 2;
        QCOMPARE(status(state), Status::Running);
    }
};

QTEST_GUILESS_MAIN(KWinScriptTest)

#include "kwinscripttest.moc"
