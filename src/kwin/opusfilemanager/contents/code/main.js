// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plasma Opus window management for KWin 6.
//
// The script acts only on windows of the running Plasma Opus process and
// stays passive until Plasma Opus answers its Sync call over the versioned
// org.kde.opusfilemanager.KWin1 D-Bus interface. Plasma Opus asks it to
// sync by invoking the "Plasma Opus: Process Window Requests" shortcut,
// because KWin scripts cannot export D-Bus methods themselves. All D-Bus
// arguments are strings, since KWin passes JavaScript numbers as doubles.

var scriptVersion = 1;
var interfaceVersion = 1;
var service = "org.kde.opus-file-manager";
var objectPath = "/KWin";
var interfaceName = "org.kde.opusfilemanager.KWin1";
var appId = "org.kde.opusfilemanager";
var processShortcut = "Plasma Opus: Process Window Requests";

var state = {
    pid: 0,
    options: {},
    companionPids: [],
    dropDown: null
};
// Windows whose signals are connected, by internal id.
var watched = {};
// The original flags of windows styled as the drop-down window, by internal id.
var styled = {};
var registeredEdge = -1;
var registeredTouchEdge = -1;

function idOf(window) {
    return String(window.internalId);
}

function isOpus(window) {
    return !!window && window.desktopFileName === appId;
}

function isMain(window) {
    return isOpus(window) && state.pid !== 0 && window.pid === state.pid && window.normalWindow && !window.transient;
}

function isCompanion(window) {
    if (!window || !window.normalWindow || window.dialog || state.pid === 0) {
        return false;
    }
    if (isOpus(window) && window.pid === state.pid) {
        return window.transient;
    }
    return state.companionPids.indexOf(window.pid) >= 0;
}

function findMain() {
    var windows = workspace.windowList();
    for (var i = 0; i < windows.length; ++i) {
        if (isMain(windows[i])) {
            return windows[i];
        }
    }
    return null;
}

function dropDownEnabled() {
    return !!state.dropDown && state.dropDown.enabled === true;
}

// Copies a rectangle into a window's own geometry value, so KWin receives
// the type it expects.
function geometryFor(window, rect) {
    var geometry = window.frameGeometry;
    geometry.x = Math.round(rect.x);
    geometry.y = Math.round(rect.y);
    geometry.width = Math.round(rect.width);
    geometry.height = Math.round(rect.height);
    return geometry;
}

function plainRect(rect) {
    return {x: Math.round(rect.x), y: Math.round(rect.y), width: Math.round(rect.width), height: Math.round(rect.height)};
}

function outputNamed(name) {
    if (!name) {
        return null;
    }
    var screens = workspace.screens;
    for (var i = 0; i < screens.length; ++i) {
        if (screens[i].name === name) {
            return screens[i];
        }
    }
    return null;
}

function desktopWithId(id) {
    var desktops = workspace.desktops;
    for (var i = 0; i < desktops.length; ++i) {
        if (desktops[i].id === id) {
            return desktops[i];
        }
    }
    return null;
}

function call(method) {
    var args = [service, objectPath, interfaceName, method];
    for (var i = 1; i < arguments.length; ++i) {
        args.push(arguments[i]);
    }
    callDBus.apply(null, args);
}

function sync() {
    call("Sync", String(scriptVersion), function (reply) {
        apply(reply);
    });
}

// The tile whose geometry matches @p rect on @p output, if any.
function tileMatching(output, rect) {
    if (!output || !rect || typeof workspace.tilingForScreen !== "function") {
        return null;
    }
    var tiling = workspace.tilingForScreen(output);
    if (!tiling || typeof tiling.bestTileForPosition !== "function") {
        return null;
    }
    var tile = tiling.bestTileForPosition(rect.x + rect.width / 2, rect.y + rect.height / 2);
    if (!tile) {
        return null;
    }
    var geometry = tile.absoluteGeometry;
    var tolerance = 24;
    if (Math.abs(geometry.x - rect.x) > tolerance || Math.abs(geometry.y - rect.y) > tolerance || Math.abs(geometry.width - rect.width) > tolerance
        || Math.abs(geometry.height - rect.height) > tolerance) {
        return null;
    }
    return tile;
}

function placementOf(window) {
    var placement = {
        screen: window.output ? window.output.name : "",
        desktops: [],
        onAllDesktops: !!window.onAllDesktops,
        geometry: plainRect(window.frameGeometry),
        keepAbove: !!window.keepAbove,
        tile: null
    };
    var desktops = window.desktops || [];
    for (var i = 0; i < desktops.length; ++i) {
        placement.desktops.push(desktops[i].id);
    }
    if (window.tile) {
        placement.tile = plainRect(window.tile.absoluteGeometry);
    }
    return placement;
}

function report(window) {
    if (!isMain(window) || dropDownEnabled() || window.minimized) {
        return;
    }
    call("WindowChanged", JSON.stringify(placementOf(window)));
}

function restore(window, placement) {
    if (!window || !placement || dropDownEnabled()) {
        return;
    }
    var output = outputNamed(placement.screen);
    if (output && window.output !== output) {
        workspace.sendClientToScreen(window, output);
    }
    if (placement.onAllDesktops) {
        window.onAllDesktops = true;
    } else {
        var desktops = [];
        var ids = placement.desktops || [];
        for (var i = 0; i < ids.length; ++i) {
            var desktop = desktopWithId(ids[i]);
            if (desktop) {
                desktops.push(desktop);
            }
        }
        if (desktops.length > 0) {
            window.onAllDesktops = false;
            window.desktops = desktops;
        }
    }
    window.keepAbove = !!placement.keepAbove;
    var tile = placement.tile ? tileMatching(output || window.output, placement.tile) : null;
    if (tile) {
        window.tile = tile;
        return;
    }
    if (window.tile) {
        window.tile = null;
    }
    if (placement.geometry) {
        window.frameGeometry = geometryFor(window, placement.geometry);
    }
}

function placeBeside(window, main) {
    if (!main || main.minimized || dropDownEnabled()) {
        return;
    }
    var area = workspace.clientArea(KWin.MaximizeArea, main);
    var geometry = main.frameGeometry;
    var half = Math.floor(area.width / 2);
    var mainOnLeft = geometry.x + geometry.width / 2 < area.x + area.width / 2;
    var target = mainOnLeft ? {x: area.x + half, y: area.y, width: area.width - half, height: area.height}
                            : {x: area.x, y: area.y, width: half, height: area.height};
    if (main.output && window.output !== main.output) {
        workspace.sendClientToScreen(window, main.output);
    }
    var tile = tileMatching(main.output, target);
    if (tile) {
        window.tile = tile;
    } else {
        window.frameGeometry = geometryFor(window, target);
    }
}

function style(window) {
    var id = idOf(window);
    if (!styled[id]) {
        styled[id] = {
            noBorder: window.noBorder,
            keepAbove: window.keepAbove,
            skipTaskbar: window.skipTaskbar,
            skipPager: window.skipPager,
            skipSwitcher: window.skipSwitcher,
            onAllDesktops: window.onAllDesktops,
            geometry: plainRect(window.frameGeometry)
        };
    }
    window.noBorder = true;
    window.keepAbove = true;
    window.skipTaskbar = true;
    window.skipPager = true;
    window.skipSwitcher = true;
    window.onAllDesktops = true;
}

function unstyle(window) {
    var id = idOf(window);
    var original = styled[id];
    if (!original) {
        return;
    }
    delete styled[id];
    window.noBorder = original.noBorder;
    window.keepAbove = original.keepAbove;
    window.skipTaskbar = original.skipTaskbar;
    window.skipPager = original.skipPager;
    window.skipSwitcher = original.skipSwitcher;
    window.onAllDesktops = original.onAllDesktops;
    window.frameGeometry = geometryFor(window, original.geometry);
}

function dropDownGeometry(area, dropDown) {
    var horizontal = dropDown.edge === "top" || dropDown.edge === "bottom";
    var width = Math.max(1, Math.round(area.width * dropDown.width / 100));
    var height = Math.max(1, Math.round(area.height * dropDown.height / 100));
    var x = area.x + Math.round((area.width - width) / 2);
    var y = area.y + Math.round((area.height - height) / 2);
    if (horizontal) {
        y = dropDown.edge === "top" ? area.y : area.y + area.height - height;
    } else {
        x = dropDown.edge === "left" ? area.x : area.x + area.width - width;
    }
    return {x: x, y: y, width: width, height: height};
}

function showAsDropDown(window) {
    if (!window || !dropDownEnabled()) {
        return;
    }
    var output = outputNamed(state.dropDown.screen) || workspace.activeScreen;
    style(window);
    if (output && window.output !== output) {
        workspace.sendClientToScreen(window, output);
    }
    if (window.tile) {
        window.tile = null;
    }
    var area = workspace.clientArea(KWin.MaximizeArea, output, workspace.currentDesktop);
    window.frameGeometry = geometryFor(window, dropDownGeometry(area, state.dropDown));
    workspace.activeWindow = window;
}

function edgeActivated() {
    call("EdgeActivated");
}

function electricBorder(edge) {
    switch (edge) {
    case "top":
        return KWin.ElectricTop;
    case "bottom":
        return KWin.ElectricBottom;
    case "left":
        return KWin.ElectricLeft;
    case "right":
        return KWin.ElectricRight;
    }
    return -1;
}

function updateEdges() {
    var dropDown = dropDownEnabled() ? state.dropDown : null;
    var wanted = dropDown && dropDown.screenEdge ? electricBorder(dropDown.edge) : -1;
    if (wanted !== registeredEdge) {
        if (registeredEdge !== -1 && typeof unregisterScreenEdge === "function") {
            unregisterScreenEdge(registeredEdge);
        }
        registeredEdge = -1;
        if (wanted !== -1 && registerScreenEdge(wanted, edgeActivated)) {
            registeredEdge = wanted;
        }
    }
    var wantedTouch = dropDown && dropDown.touchEdge ? electricBorder(dropDown.edge) : -1;
    if (wantedTouch !== registeredTouchEdge) {
        if (registeredTouchEdge !== -1 && typeof unregisterTouchScreenEdge === "function") {
            unregisterTouchScreenEdge(registeredTouchEdge);
        }
        registeredTouchEdge = -1;
        if (wantedTouch !== -1 && typeof registerTouchScreenEdge === "function" && registerTouchScreenEdge(wantedTouch, edgeActivated)) {
            registeredTouchEdge = wantedTouch;
        }
    }
}

function reportDesktops() {
    var desktops = [];
    var list = workspace.desktops;
    for (var i = 0; i < list.length; ++i) {
        desktops.push({id: list[i].id, name: list[i].name});
    }
    call("DesktopsChanged", JSON.stringify(desktops));
}

function watch(window) {
    var id = idOf(window);
    if (watched[id]) {
        return;
    }
    watched[id] = true;
    var changed = function () {
        report(window);
    };
    window.interactiveMoveResizeFinished.connect(changed);
    window.desktopsChanged.connect(changed);
    window.outputChanged.connect(changed);
    window.keepAboveChanged.connect(changed);
    window.tileChanged.connect(changed);
}

function apply(reply) {
    var request;
    try {
        request = JSON.parse(reply);
    } catch (error) {
        return;
    }
    // Stay passive with a Plasma Opus that speaks another interface version.
    if (!request || request.interface !== interfaceVersion) {
        state.pid = 0;
        return;
    }
    state.pid = request.pid || 0;
    state.options = request.options || {};
    state.companionPids = request.companionPids || [];
    var wasDropDown = dropDownEnabled();
    state.dropDown = request.dropDown || null;
    updateEdges();
    reportDesktops();

    var main = findMain();
    if (main) {
        watch(main);
        if (dropDownEnabled()) {
            showAsDropDown(main);
        } else if (wasDropDown || styled[idOf(main)]) {
            unstyle(main);
        }
    }
    var commands = request.commands || [];
    for (var i = 0; i < commands.length; ++i) {
        var command = commands[i];
        if (command.type === "restore") {
            restore(main, command.placement);
        } else if (command.type === "report") {
            report(main);
        }
    }
}

workspace.windowAdded.connect(function (window) {
    if (!isOpus(window) && state.companionPids.indexOf(window.pid) < 0) {
        return;
    }
    if (isOpus(window) && window.pid !== state.pid && window.normalWindow && !window.transient) {
        // A Plasma Opus that started after the last sync.
        sync();
        return;
    }
    if (isMain(window)) {
        watch(window);
        if (dropDownEnabled()) {
            showAsDropDown(window);
        }
        return;
    }
    if (state.options.tileCompanions && isCompanion(window)) {
        placeBeside(window, findMain());
    }
});

workspace.windowRemoved.connect(function (window) {
    var id = idOf(window);
    delete watched[id];
    delete styled[id];
});

workspace.currentDesktopChanged.connect(function () {
    if (state.pid !== 0 && state.options.followDesktops && workspace.currentDesktop) {
        call("CurrentDesktopChanged", workspace.currentDesktop.id);
    }
});

workspace.desktopsChanged.connect(function () {
    if (state.pid !== 0) {
        reportDesktops();
    }
});

registerShortcut(processShortcut, processShortcut, "", sync);
sync();
