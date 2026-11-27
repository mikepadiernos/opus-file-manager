// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>
#include <utility>

/// Decides what the Plasma Opus KRunner runner offers: saved workspaces and
/// Favorites, and nothing from history, searches or recent locations.
namespace RunnerMatches
{
enum class Kind {
    Workspace,
    Location,
};

/// Something KRunner can open in Plasma Opus.
struct Target {
    Kind kind = Kind::Workspace;
    /// The stable id KRunner returns on Run. It never contains a URL, so
    /// credentials and paths are not passed back and forth.
    QString id;
    QString text;
    /// For a workspace, its id; for a location, its URL.
    QString workspaceId;
    QUrl url;
    bool operator==(const Target &other) const = default;
};

/// KRunner's category relevance values.
enum CategoryRelevance {
    Moderate = 50,
    High = 70,
    Highest = 100,
};

struct Match {
    Target target;
    int categoryRelevance = Moderate;
    double relevance = 0;
};

struct Options {
    /// Whether the runner offers anything at all.
    bool enabled = true;
    bool workspaces = true;
    bool favorites = true;
    /// The ids of the workspaces left out.
    QStringList hiddenWorkspaces;
    /// The target ids, as locationId() gives them, of the Favorites left out.
    QStringList hiddenFavorites;
};

/// A Favorite: its URL and shown title.
using Favorite = std::pair<QUrl, QString>;

/// The target id of the Favorite at @p url. It never contains the URL.
QString locationId(const QUrl &url);

/// The targets the runner may offer.
QList<Target> targets(const WorkspaceCatalog::Catalog &catalog, const QList<Favorite> &favorites, const Options &options);

/// The targets whose text matches @p query, best first. Queries shorter
/// than two characters match nothing, and local folders that no longer exist
/// are left out.
QList<Match> match(const QList<Target> &targets, const QString &query);

/// The target with @p id, if it is still offered and available.
std::optional<Target> resolve(const QList<Target> &targets, const QString &id);

/// Whether @p target can still be opened: a local Favorite must be an
/// existing folder. Remote Favorites are not checked.
bool available(const Target &target);

/// The location shown under a match, without user name or password.
QString subtext(const Target &target);

/// The arguments that open @p target in Plasma Opus.
QStringList arguments(const Target &target);
}
