// SPDX-License-Identifier: GPL-3.0-or-later

#include "kwinscript.h"

#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace
{
// A minimal stand-in for the KWin 6 scripting API that main.js uses. It
// records D-Bus calls and answers Sync with the global syncReply.
const char *Harness = R"JS(
var calls = [];
var syncReply = "";
var shortcuts = {};
var screenEdges = [];
var touchEdges = [];
var edgeCallback = null;
var KWin = {MaximizeArea: 2, ElectricTop: 0, ElectricRight: 2, ElectricBottom: 4, ElectricLeft: 6};

function makeSignal() {
    var handlers = [];
    return {
        connect: function (handler) { handlers.push(handler); },
        emit: function () {
            var args = arguments;
            handlers.forEach(function (handler) { handler.apply(null, args); });
        }
    };
}

function callDBus(service, path, iface, method) {
    var args = Array.prototype.slice.call(arguments, 4);
    var callback = null;
    if (args.length > 0 && typeof args[args.length - 1] === "function") {
        callback = args.pop();
    }
    calls.push({service: service, path: path, iface: iface, method: method, args: args});
    if (method === "Sync" && callback) {
        callback(syncReply);
    }
}

function registerShortcut(name, text, keys, callback) { shortcuts[name] = callback; }
function registerScreenEdge(edge, callback) { screenEdges.push(edge); edgeCallback = callback; return true; }
function unregisterScreenEdge(edge) { screenEdges = screenEdges.filter(function (e) { return e !== edge; }); return true; }
function registerTouchScreenEdge(edge, callback) { touchEdges.push(edge); return true; }
function unregisterTouchScreenEdge(edge) { touchEdges = touchEdges.filter(function (e) { return e !== edge; }); return true; }

var screens = [{name: "DP-1"}, {name: "HDMI-A-1"}];
var desktops = [{id: "d1", name: "Work"}, {id: "d2", name: "Play"}];
var nextId = 1;

function makeWindow(props) {
    var window = {
        internalId: "{w" + (nextId++) + "}",
        desktopFileName: "org.kde.opusfilemanager",
        pid: 42,
        normalWindow: true,
        dialog: false,
        transient: false,
        minimized: false,
        output: screens[0],
        desktops: [desktops[0]],
        onAllDesktops: false,
        keepAbove: false,
        noBorder: false,
        skipTaskbar: false,
        skipPager: false,
        skipSwitcher: false,
        tile: null,
        frameGeometry: {x: 100, y: 100, width: 800, height: 600},
        interactiveMoveResizeFinished: makeSignal(),
        desktopsChanged: makeSignal(),
        outputChanged: makeSignal(),
        keepAboveChanged: makeSignal(),
        tileChanged: makeSignal()
    };
    for (var key in props) {
        window[key] = props[key];
    }
    return window;
}

var tiles = [];
var workspace = {
    windows: [],
    screens: screens,
    desktops: desktops,
    currentDesktop: desktops[0],
    activeScreen: screens[0],
    activeWindow: null,
    windowAdded: makeSignal(),
    windowRemoved: makeSignal(),
    currentDesktopChanged: makeSignal(),
    desktopsChanged: makeSignal(),
    windowList: function () { return this.windows; },
    clientArea: function () { return {x: 0, y: 0, width: 1920, height: 1080}; },
    sendClientToScreen: function (window, output) { window.output = output; },
    tilingForScreen: function () {
        return {
            bestTileForPosition: function (x, y) {
                for (var i = 0; i < tiles.length; ++i) {
                    var g = tiles[i].absoluteGeometry;
                    if (x >= g.x && x < g.x + g.width && y >= g.y && y < g.y + g.height) {
                        return tiles[i];
                    }
                }
                return null;
            }
        };
    }
};

function callsTo(method) { return calls.filter(function (call) { return call.method === method; }); }
function allArgumentsAreStrings() {
    return calls.every(function (call) {
        return call.args.every(function (arg) { return typeof arg === "string"; });
    });
}
)JS";

QJsonObject reply(const QJsonObject &overrides = {})
{
    QJsonObject request{
        {QStringLiteral("interface"), KWinScript::interfaceVersion()},
        {QStringLiteral("pid"), 42},
        {QStringLiteral("options"),
         QJsonObject{{QStringLiteral("restoreWindows"), true}, {QStringLiteral("tileCompanions"), false}, {QStringLiteral("followDesktops"), false}}},
        {QStringLiteral("companionPids"), QJsonArray()},
        {QStringLiteral("dropDown"), QJsonObject{{QStringLiteral("enabled"), false}}},
        {QStringLiteral("commands"), QJsonArray()},
    };
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        request.insert(it.key(), it.value());
    }
    return request;
}

QString text(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
}

class KWinScriptJsTest : public QObject
{
    Q_OBJECT

private:
    QJSValue run(const QString &code)
    {
        const QJSValue result = m_engine->evaluate(code);
        if (result.isError()) {
            qWarning() << result.toString() << result.property(QStringLiteral("lineNumber")).toInt();
        }
        return result;
    }

    /// Loads the harness, sets the first Sync reply and evaluates main.js.
    bool load(const QString &firstReply, const QString &setup = QString())
    {
        m_engine = std::make_unique<QJSEngine>();
        if (run(QString::fromUtf8(Harness)).isError()) {
            return false;
        }
        m_engine->globalObject().setProperty(QStringLiteral("syncReply"), firstReply);
        if (!setup.isEmpty() && run(setup).isError()) {
            return false;
        }
        QFile script(QStringLiteral(OPUSFM_KWIN_SCRIPT));
        if (!script.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QJSValue result = m_engine->evaluate(QString::fromUtf8(script.readAll()), QStringLiteral(OPUSFM_KWIN_SCRIPT));
        if (result.isError()) {
            qWarning() << result.toString() << result.property(QStringLiteral("lineNumber")).toInt();
            return false;
        }
        return true;
    }

    /// Sets the next Sync reply and invokes the processing shortcut, as Plasma Opus does.
    void process(const QJsonObject &request)
    {
        m_engine->globalObject().setProperty(QStringLiteral("syncReply"), text(request));
        QVERIFY(!run(QStringLiteral("shortcuts[\"%1\"]()").arg(KWinScript::processShortcutName())).isError());
    }

    std::unique_ptr<QJSEngine> m_engine;

private Q_SLOTS:
    void matchesThePackageAndInterface()
    {
        QFile metadata(QStringLiteral(OPUSFM_KWIN_METADATA));
        QVERIFY(metadata.open(QIODevice::ReadOnly));
        const int version = KWinScript::packageVersion(metadata.readAll());
        QVERIFY(version > 0);
        QVERIFY(load(QString()));
        QCOMPARE(run(QStringLiteral("scriptVersion")).toInt(), version);
        QCOMPARE(run(QStringLiteral("interfaceVersion")).toInt(), KWinScript::interfaceVersion());
        QCOMPARE(run(QStringLiteral("interfaceName")).toString(), KWinScript::interfaceName());
        QCOMPARE(run(QStringLiteral("objectPath")).toString(), KWinScript::objectPath());
        QCOMPARE(run(QStringLiteral("processShortcut")).toString(), KWinScript::processShortcutName());
    }

    void staysPassiveWithoutPlasmaOpus()
    {
        QVERIFY(load(QString(), QStringLiteral("workspace.windows.push(makeWindow({}));")));
        QCOMPARE(run(QStringLiteral("callsTo('Sync').length")).toInt(), 1);
        QCOMPARE(run(QStringLiteral("callsTo('Sync')[0].args[0]")).toString(), QStringLiteral("1"));
        QCOMPARE(run(QStringLiteral("callsTo('Sync')[0].service")).toString(), QStringLiteral("org.kde.opus-file-manager"));
        QCOMPARE(run(QStringLiteral("state.pid")).toInt(), 0);
        QCOMPARE(run(QStringLiteral("callsTo('DesktopsChanged').length")).toInt(), 0);
        QVERIFY(run(QStringLiteral("shortcuts['%1'] !== undefined").arg(KWinScript::processShortcutName())).toBool());

        process(reply({{QStringLiteral("interface"), KWinScript::interfaceVersion() + 1}}));
        QCOMPARE(run(QStringLiteral("state.pid")).toInt(), 0);
        QCOMPARE(run(QStringLiteral("callsTo('DesktopsChanged').length")).toInt(), 0);
    }

    void reportsDesktopsAndPlacements()
    {
        QVERIFY(load(text(reply()),
                     QStringLiteral("var main = makeWindow({}); var other = makeWindow({desktopFileName: 'org.kde.dolphin', pid: 7});"
                                    "workspace.windows.push(other, main);")));
        QCOMPARE(run(QStringLiteral("state.pid")).toInt(), 42);
        const QJsonArray desktops = QJsonDocument::fromJson(run(QStringLiteral("callsTo('DesktopsChanged')[0].args[0]")).toString().toUtf8()).array();
        QCOMPARE(desktops.size(), 2);
        QCOMPARE(KWinScript::desktopsFromJson(QString::fromUtf8(QJsonDocument(desktops).toJson())).first().name, QStringLiteral("Work"));

        run(
            QStringLiteral("main.frameGeometry = {x: 5, y: 6, width: 700, height: 500}; main.keepAbove = true;"
                           "main.interactiveMoveResizeFinished.emit(); other.interactiveMoveResizeFinished.emit();"));
        QCOMPARE(run(QStringLiteral("callsTo('WindowChanged').length")).toInt(), 1);
        const auto placement = KWinScript::placementFromJson(run(QStringLiteral("callsTo('WindowChanged')[0].args[0]")).toString());
        QVERIFY(placement);
        QCOMPARE(placement->geometry, QRect(5, 6, 700, 500));
        QCOMPARE(placement->screen, QStringLiteral("DP-1"));
        QCOMPARE(placement->desktops, QStringList{QStringLiteral("d1")});
        QVERIFY(placement->keepAbove);
        QVERIFY(placement->tile.isNull());

        run(QStringLiteral("main.minimized = true; main.interactiveMoveResizeFinished.emit();"));
        QCOMPARE(run(QStringLiteral("callsTo('WindowChanged').length")).toInt(), 1);
        QVERIFY(run(QStringLiteral("allArgumentsAreStrings()")).toBool());
    }

    void restoresAPlacement()
    {
        QVERIFY(load(text(reply()), QStringLiteral("var main = makeWindow({}); workspace.windows.push(main);")));
        KWinScript::Placement placement;
        placement.screen = QStringLiteral("HDMI-A-1");
        placement.desktops = {QStringLiteral("d2")};
        placement.geometry = QRect(50, 60, 1000, 700);
        placement.keepAbove = true;
        process(reply({{QStringLiteral("commands"), QJsonArray{KWinScript::restoreCommand(placement)}}}));
        QCOMPARE(run(QStringLiteral("main.output.name")).toString(), QStringLiteral("HDMI-A-1"));
        QCOMPARE(run(QStringLiteral("main.desktops[0].id")).toString(), QStringLiteral("d2"));
        QVERIFY(run(QStringLiteral("main.keepAbove")).toBool());
        QCOMPARE(run(QStringLiteral("JSON.stringify(main.frameGeometry)")).toString(), QStringLiteral(R"({"x":50,"y":60,"width":1000,"height":700})"));

        run(QStringLiteral("tiles.push({absoluteGeometry: {x: 0, y: 0, width: 960, height: 1080}});"));
        placement.tile = QRect(0, 0, 960, 1080);
        placement.onAllDesktops = true;
        process(reply({{QStringLiteral("commands"), QJsonArray{KWinScript::restoreCommand(placement)}}}));
        QVERIFY(run(QStringLiteral("main.tile === tiles[0]")).toBool());
        QVERIFY(run(QStringLiteral("main.onAllDesktops")).toBool());
    }

    void placesCompanionsBesideTheMainWindow()
    {
        QJsonObject options{{QStringLiteral("restoreWindows"), true}, {QStringLiteral("tileCompanions"), true}, {QStringLiteral("followDesktops"), false}};
        QVERIFY(load(text(reply({{QStringLiteral("options"), options}})),
                     QStringLiteral("var main = makeWindow({frameGeometry: {x: 0, y: 0, width: 900, height: 1080}}); workspace.windows.push(main);")));
        run(
            QStringLiteral("var gallery = makeWindow({transient: true}); workspace.windowAdded.emit(gallery);"
                           "var dialog = makeWindow({transient: true, dialog: true}); workspace.windowAdded.emit(dialog);"));
        QCOMPARE(run(QStringLiteral("JSON.stringify(gallery.frameGeometry)")).toString(), QStringLiteral(R"({"x":960,"y":0,"width":960,"height":1080})"));
        QCOMPARE(run(QStringLiteral("dialog.frameGeometry.x")).toInt(), 100);

        options.insert(QStringLiteral("tileCompanions"), false);
        process(reply({{QStringLiteral("options"), options}}));
        run(QStringLiteral("var second = makeWindow({transient: true}); workspace.windowAdded.emit(second);"));
        QCOMPARE(run(QStringLiteral("second.frameGeometry.x")).toInt(), 100);
    }

    void stylesTheDropDownWindow()
    {
        QVERIFY(load(text(reply()), QStringLiteral("var main = makeWindow({}); workspace.windows.push(main);")));
        const QJsonObject dropDown{{QStringLiteral("enabled"), true},
                                   {QStringLiteral("edge"), QStringLiteral("bottom")},
                                   {QStringLiteral("width"), 50},
                                   {QStringLiteral("height"), 30},
                                   {QStringLiteral("screen"), QStringLiteral("HDMI-A-1")},
                                   {QStringLiteral("screenEdge"), true},
                                   {QStringLiteral("touchEdge"), true}};
        process(reply({{QStringLiteral("dropDown"), dropDown}}));
        QVERIFY(
            run(QStringLiteral("main.noBorder && main.keepAbove && main.skipTaskbar && main.skipPager && main.skipSwitcher && main.onAllDesktops")).toBool());
        QCOMPARE(run(QStringLiteral("main.output.name")).toString(), QStringLiteral("HDMI-A-1"));
        QCOMPARE(run(QStringLiteral("JSON.stringify(main.frameGeometry)")).toString(), QStringLiteral(R"({"x":480,"y":756,"width":960,"height":324})"));
        QVERIFY(run(QStringLiteral("workspace.activeWindow === main")).toBool());
        QCOMPARE(run(QStringLiteral("JSON.stringify(screenEdges)")).toString(), QStringLiteral("[4]"));
        QCOMPARE(run(QStringLiteral("JSON.stringify(touchEdges)")).toString(), QStringLiteral("[4]"));
        run(QStringLiteral("edgeCallback()"));
        QCOMPARE(run(QStringLiteral("callsTo('EdgeActivated').length")).toInt(), 1);

        // The drop-down window does not report placements for workspaces.
        run(QStringLiteral("main.interactiveMoveResizeFinished.emit();"));
        QCOMPARE(run(QStringLiteral("callsTo('WindowChanged').length")).toInt(), 0);

        process(reply());
        QVERIFY(run(QStringLiteral("!main.noBorder && !main.keepAbove && !main.skipTaskbar && !main.onAllDesktops")).toBool());
        QCOMPARE(run(QStringLiteral("JSON.stringify(main.frameGeometry)")).toString(), QStringLiteral(R"({"x":100,"y":100,"width":800,"height":600})"));
        QCOMPARE(run(QStringLiteral("JSON.stringify(screenEdges)")).toString(), QStringLiteral("[]"));
        QCOMPARE(run(QStringLiteral("JSON.stringify(touchEdges)")).toString(), QStringLiteral("[]"));
    }

    void followsVirtualDesktops()
    {
        QJsonObject options{{QStringLiteral("restoreWindows"), true}, {QStringLiteral("tileCompanions"), false}, {QStringLiteral("followDesktops"), true}};
        QVERIFY(load(text(reply({{QStringLiteral("options"), options}}))));
        run(QStringLiteral("workspace.currentDesktop = desktops[1]; workspace.currentDesktopChanged.emit();"));
        QCOMPARE(run(QStringLiteral("callsTo('CurrentDesktopChanged')[0].args[0]")).toString(), QStringLiteral("d2"));
        run(QStringLiteral("workspace.desktopsChanged.emit();"));
        QCOMPARE(run(QStringLiteral("callsTo('DesktopsChanged').length")).toInt(), 2);
        QVERIFY(run(QStringLiteral("allArgumentsAreStrings()")).toBool());
    }

    void syncsWhenANewPlasmaOpusStarts()
    {
        QVERIFY(load(text(reply())));
        run(QStringLiteral("workspace.windowAdded.emit(makeWindow({pid: 99}));"));
        QCOMPARE(run(QStringLiteral("callsTo('Sync').length")).toInt(), 2);
        run(QStringLiteral("workspace.windowAdded.emit(makeWindow({desktopFileName: 'org.kde.konsole', pid: 5}));"));
        QCOMPARE(run(QStringLiteral("callsTo('Sync').length")).toInt(), 2);
    }
};

QTEST_GUILESS_MAIN(KWinScriptJsTest)
#include "kwinscriptjstest.moc"
