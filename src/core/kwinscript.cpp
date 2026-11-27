// SPDX-License-Identifier: GPL-3.0-or-later

#include "kwinscript.h"

#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

#include <algorithm>

namespace KWinScript
{
QString pluginId()
{
    return QStringLiteral("opusfilemanager");
}

int interfaceVersion()
{
    return 1;
}

QString interfaceName()
{
    return QStringLiteral("org.kde.opusfilemanager.KWin1");
}

QString objectPath()
{
    return QStringLiteral("/KWin");
}

QString processShortcutName()
{
    return QStringLiteral("Plasma Opus: Process Window Requests");
}

QString enabledKey()
{
    return pluginId() + QLatin1String("Enabled");
}

QString packageRoot()
{
    return QStringLiteral("kwin/scripts");
}

QString installedPackagePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + packageRoot() + QLatin1Char('/') + pluginId();
}

namespace
{
QJsonObject rectToJson(const QRect &rect)
{
    return {{QStringLiteral("x"), rect.x()},
            {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()},
            {QStringLiteral("height"), rect.height()}};
}

std::optional<QRect> rectFromJson(const QJsonValue &value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    for (const QString &key : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")}) {
        if (!object.value(key).isDouble()) {
            return std::nullopt;
        }
    }
    const QRect rect(object.value(QStringLiteral("x")).toInt(),
                     object.value(QStringLiteral("y")).toInt(),
                     object.value(QStringLiteral("width")).toInt(),
                     object.value(QStringLiteral("height")).toInt());
    if (!rect.isValid()) {
        return std::nullopt;
    }
    return rect;
}

constexpr int MinimumPercent = 10;
constexpr int MaximumPercent = 100;
}

QJsonObject toJson(const Placement &placement)
{
    QJsonObject object{{QStringLiteral("screen"), placement.screen},
                       {QStringLiteral("desktops"), QJsonArray::fromStringList(placement.desktops)},
                       {QStringLiteral("onAllDesktops"), placement.onAllDesktops},
                       {QStringLiteral("geometry"), rectToJson(placement.geometry)},
                       {QStringLiteral("keepAbove"), placement.keepAbove}};
    object.insert(QStringLiteral("tile"), placement.tile.isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(rectToJson(placement.tile)));
    return object;
}

std::optional<Placement> placementFromJson(const QJsonObject &object)
{
    Placement placement;
    const auto geometry = rectFromJson(object.value(QStringLiteral("geometry")));
    if (!geometry) {
        return std::nullopt;
    }
    placement.geometry = *geometry;
    placement.screen = object.value(QStringLiteral("screen")).toString();
    const QJsonValue desktops = object.value(QStringLiteral("desktops"));
    if (!desktops.isUndefined() && !desktops.isArray()) {
        return std::nullopt;
    }
    for (const QJsonValue &desktop : desktops.toArray()) {
        if (!desktop.isString()) {
            return std::nullopt;
        }
        placement.desktops.append(desktop.toString());
    }
    placement.onAllDesktops = object.value(QStringLiteral("onAllDesktops")).toBool();
    placement.keepAbove = object.value(QStringLiteral("keepAbove")).toBool();
    const QJsonValue tile = object.value(QStringLiteral("tile"));
    if (!tile.isNull() && !tile.isUndefined()) {
        const auto tileRect = rectFromJson(tile);
        if (!tileRect) {
            return std::nullopt;
        }
        placement.tile = *tileRect;
    }
    return placement;
}

std::optional<Placement> placementFromJson(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return placementFromJson(document.object());
}

Options readOptions(const KConfigGroup &group)
{
    Options options;
    options.restoreWindows = group.readEntry("RestoreWindows", options.restoreWindows);
    options.tileCompanions = group.readEntry("TileCompanions", options.tileCompanions);
    options.followDesktops = group.readEntry("FollowDesktops", options.followDesktops);
    return options;
}

void writeOptions(KConfigGroup &group, const Options &options)
{
    group.writeEntry("RestoreWindows", options.restoreWindows);
    group.writeEntry("TileCompanions", options.tileCompanions);
    group.writeEntry("FollowDesktops", options.followDesktops);
}

QString edgeName(Edge edge)
{
    switch (edge) {
    case Edge::Top:
        return QStringLiteral("top");
    case Edge::Bottom:
        return QStringLiteral("bottom");
    case Edge::Left:
        return QStringLiteral("left");
    case Edge::Right:
        return QStringLiteral("right");
    }
    return QStringLiteral("top");
}

std::optional<Edge> edgeFromName(const QString &name)
{
    for (Edge edge : {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right}) {
        if (edgeName(edge) == name) {
            return edge;
        }
    }
    return std::nullopt;
}

DropDown readDropDown(const KConfigGroup &group)
{
    DropDown dropDown;
    dropDown.enabled = group.readEntry("DropDownEnabled", dropDown.enabled);
    dropDown.edge = edgeFromName(group.readEntry("DropDownEdge", edgeName(dropDown.edge))).value_or(Edge::Top);
    dropDown.width = std::clamp(group.readEntry("DropDownWidth", dropDown.width), MinimumPercent, MaximumPercent);
    dropDown.height = std::clamp(group.readEntry("DropDownHeight", dropDown.height), MinimumPercent, MaximumPercent);
    dropDown.screen = group.readEntry("DropDownScreen", QString());
    dropDown.slide = group.readEntry("DropDownSlide", dropDown.slide);
    dropDown.hideOnFocusLoss = group.readEntry("DropDownHideOnFocusLoss", dropDown.hideOnFocusLoss);
    dropDown.screenEdge = group.readEntry("DropDownScreenEdge", dropDown.screenEdge);
    dropDown.touchEdge = group.readEntry("DropDownTouchEdge", dropDown.touchEdge);
    dropDown.workspaceId = group.readEntry("DropDownWorkspace", QString());
    return dropDown;
}

void writeDropDown(KConfigGroup &group, const DropDown &dropDown)
{
    group.writeEntry("DropDownEnabled", dropDown.enabled);
    group.writeEntry("DropDownEdge", edgeName(dropDown.edge));
    group.writeEntry("DropDownWidth", std::clamp(dropDown.width, MinimumPercent, MaximumPercent));
    group.writeEntry("DropDownHeight", std::clamp(dropDown.height, MinimumPercent, MaximumPercent));
    group.writeEntry("DropDownScreen", dropDown.screen);
    group.writeEntry("DropDownSlide", dropDown.slide);
    group.writeEntry("DropDownHideOnFocusLoss", dropDown.hideOnFocusLoss);
    group.writeEntry("DropDownScreenEdge", dropDown.screenEdge);
    group.writeEntry("DropDownTouchEdge", dropDown.touchEdge);
    group.writeEntry("DropDownWorkspace", dropDown.workspaceId);
}

QList<Desktop> desktopsFromJson(const QString &json)
{
    QList<Desktop> desktops;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        desktops.append({id, object.value(QStringLiteral("name")).toString()});
    }
    return desktops;
}

QMap<QString, QString> readDesktopWorkspaces(const KConfigGroup &group)
{
    QMap<QString, QString> assignments;
    const QStringList keys = group.keyList();
    for (const QString &key : keys) {
        const QString workspace = group.readEntry(key, QString());
        if (!workspace.isEmpty()) {
            assignments.insert(key, workspace);
        }
    }
    return assignments;
}

void writeDesktopWorkspaces(KConfigGroup &group, const QMap<QString, QString> &assignments)
{
    group.deleteGroup();
    for (auto it = assignments.cbegin(); it != assignments.cend(); ++it) {
        if (!it.key().isEmpty() && !it.value().isEmpty()) {
            group.writeEntry(it.key(), it.value());
        }
    }
}

std::optional<Placement> readPlacement(const KConfigGroup &group, const QString &workspaceId)
{
    if (workspaceId.isEmpty() || !group.hasKey(workspaceId)) {
        return std::nullopt;
    }
    return placementFromJson(group.readEntry(workspaceId, QString()));
}

void writePlacement(KConfigGroup &group, const QString &workspaceId, const Placement &placement)
{
    if (workspaceId.isEmpty()) {
        return;
    }
    group.writeEntry(workspaceId, QString::fromUtf8(QJsonDocument(toJson(placement)).toJson(QJsonDocument::Compact)));
}

QString request(qint64 pid, const Options &options, const DropDown &dropDown, const QList<qint64> &companionPids, const QJsonArray &commands)
{
    QJsonArray pids;
    for (qint64 companion : companionPids) {
        pids.append(companion);
    }
    const QJsonObject object{
        {QStringLiteral("interface"), interfaceVersion()},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("options"),
         QJsonObject{{QStringLiteral("restoreWindows"), options.restoreWindows},
                     {QStringLiteral("tileCompanions"), options.tileCompanions},
                     {QStringLiteral("followDesktops"), options.followDesktops}}},
        {QStringLiteral("companionPids"), pids},
        {QStringLiteral("dropDown"),
         QJsonObject{{QStringLiteral("enabled"), dropDown.enabled},
                     {QStringLiteral("edge"), edgeName(dropDown.edge)},
                     {QStringLiteral("width"), dropDown.width},
                     {QStringLiteral("height"), dropDown.height},
                     {QStringLiteral("screen"), dropDown.screen},
                     {QStringLiteral("screenEdge"), dropDown.screenEdge},
                     {QStringLiteral("touchEdge"), dropDown.touchEdge}}},
        {QStringLiteral("commands"), commands},
    };
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject restoreCommand(const Placement &placement)
{
    return {{QStringLiteral("type"), QStringLiteral("restore")}, {QStringLiteral("placement"), toJson(placement)}};
}

int packageVersion(const QByteArray &metadata)
{
    const QJsonObject plugin = QJsonDocument::fromJson(metadata).object().value(QStringLiteral("KPlugin")).toObject();
    if (plugin.value(QStringLiteral("Id")).toString() != pluginId()) {
        return -1;
    }
    bool ok = false;
    const int version = plugin.value(QStringLiteral("Version")).toString().toInt(&ok);
    return ok && version > 0 ? version : -1;
}

int installedVersion()
{
    QFile file(installedPackagePath() + QLatin1String("/metadata.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return -1;
    }
    return packageVersion(file.readAll());
}

bool isEnabled(const KConfigGroup &plugins)
{
    return plugins.readEntry(enabledKey(), false);
}

Status status(const State &state)
{
    if (!state.kwinRunning) {
        return Status::KWinUnavailable;
    }
    if (!state.scriptingAvailable) {
        return Status::ScriptingUnavailable;
    }
    if (state.installedVersion < 0) {
        return Status::NotInstalled;
    }
    if (state.installedVersion < state.bundledVersion) {
        return Status::UpdateAvailable;
    }
    if (!state.enabled) {
        return Status::Disabled;
    }
    if (!state.loaded || state.runningVersion < 0) {
        return Status::NotRunning;
    }
    if (state.runningVersion != state.installedVersion) {
        return Status::RestartRequired;
    }
    return Status::Running;
}
}
