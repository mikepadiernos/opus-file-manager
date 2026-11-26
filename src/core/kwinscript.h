// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * The optional Plasma Opus KWin script: its identity, the versioned D-Bus
 * interface it uses to talk to Plasma Opus, the window state it reports and
 * restores, and the options Plasma Opus sends it.
 *
 * The script lives in `src/kwin/opusfilemanager`. KWin scripts cannot export
 * D-Bus methods, so the script calls Plasma Opus, and Plasma Opus asks it to
 * call by invoking the script's processing shortcut through KGlobalAccel.
 */
namespace KWinScript
{
/// The KPackage plugin id, which is also the script's directory name.
QString pluginId();
/// The version of the D-Bus interface and request format.
int interfaceVersion();
QString interfaceName();
QString objectPath();
/// The shortcut the script registers to process pending requests.
QString processShortcutName();
/// The `[Plugins]` entry in kwinrc that enables the script.
QString enabledKey();
/// The package root, relative to the generic data location.
QString packageRoot();
/// The installed package's directory for the current user.
QString installedPackagePath();

/// A window's screen, virtual desktops, size, tile and keep-above state.
struct Placement {
    QString screen;
    QStringList desktops;
    bool onAllDesktops = false;
    QRect geometry;
    bool keepAbove = false;
    /// The geometry of the tile the window was in, or null when not tiled.
    QRect tile;
    bool operator==(const Placement &other) const = default;
};

QJsonObject toJson(const Placement &placement);
/// Parses a reported placement; std::nullopt when it is malformed.
std::optional<Placement> placementFromJson(const QJsonObject &object);
std::optional<Placement> placementFromJson(const QString &json);

/// What the script does for Plasma Opus.
struct Options {
    /// Restore the window's placement when a workspace is applied.
    bool restoreWindows = true;
    /// Place Plasma Opus's detached windows beside the main window.
    bool tileCompanions = false;
    /// Apply the workspace assigned to the virtual desktop that becomes current.
    bool followDesktops = false;
    bool operator==(const Options &other) const = default;
};

Options readOptions(const KConfigGroup &group);
void writeOptions(KConfigGroup &group, const Options &options);

enum class Edge {
    Top,
    Bottom,
    Left,
    Right
};
QString edgeName(Edge edge);
std::optional<Edge> edgeFromName(const QString &name);

/// The drop-down window, like Yakuake.
struct DropDown {
    bool enabled = false;
    Edge edge = Edge::Top;
    /// Percentages of the screen's work area.
    int width = 100;
    int height = 50;
    /// The output name, or empty for the active screen.
    QString screen;
    bool slide = true;
    bool hideOnFocusLoss = false;
    bool screenEdge = false;
    bool touchEdge = false;
    /// The workspace applied when it is shown, or empty to keep the current one.
    QString workspaceId;
    bool operator==(const DropDown &other) const = default;
};

DropDown readDropDown(const KConfigGroup &group);
void writeDropDown(KConfigGroup &group, const DropDown &dropDown);

struct Desktop {
    QString id;
    QString name;
    bool operator==(const Desktop &other) const = default;
};
QList<Desktop> desktopsFromJson(const QString &json);

/// Workspace ids by virtual desktop id.
QMap<QString, QString> readDesktopWorkspaces(const KConfigGroup &group);
void writeDesktopWorkspaces(KConfigGroup &group, const QMap<QString, QString> &assignments);

/// Placements by workspace id, kept in the state configuration.
std::optional<Placement> readPlacement(const KConfigGroup &group, const QString &workspaceId);
void writePlacement(KConfigGroup &group, const QString &workspaceId, const Placement &placement);

/**
 * The reply to the script's Sync call: the interface version, the Plasma
 * Opus process id, the options and the pending commands.
 */
QString request(qint64 pid, const Options &options, const DropDown &dropDown, const QList<qint64> &companionPids, const QJsonArray &commands);
QJsonObject restoreCommand(const Placement &placement);

/// The package's integer version from its metadata.json, or -1.
int packageVersion(const QByteArray &metadata);
/// The version installed at installedPackagePath(), or -1 when none is.
int installedVersion();

/// Whether the script is enabled in a kwinrc `[Plugins]` group.
bool isEnabled(const KConfigGroup &plugins);

struct State {
    bool kwinRunning = false;
    bool scriptingAvailable = false;
    int bundledVersion = -1;
    int installedVersion = -1;
    bool enabled = false;
    bool loaded = false;
    /// The version that last called Sync, or -1.
    int runningVersion = -1;
};

enum class Status {
    KWinUnavailable,
    ScriptingUnavailable,
    NotInstalled,
    UpdateAvailable,
    Disabled,
    NotRunning,
    RestartRequired,
    Running
};
Status status(const State &state);
}
