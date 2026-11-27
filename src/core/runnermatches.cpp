// SPDX-License-Identifier: GPL-3.0-or-later

#include "runnermatches.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <algorithm>

namespace RunnerMatches
{
namespace
{
const QString WorkspacePrefix = QStringLiteral("workspace:");
const QString LocationPrefix = QStringLiteral("location:");

/// How well @p text matches @p query, or nothing.
std::optional<std::pair<int, double>> score(const QString &text, const QString &query)
{
    if (text.compare(query, Qt::CaseInsensitive) == 0) {
        return std::pair{int(Highest), 1.0};
    }
    if (text.startsWith(query, Qt::CaseInsensitive)) {
        return std::pair{int(High), 0.9};
    }
    const int at = text.indexOf(query, 0, Qt::CaseInsensitive);
    if (at < 0) {
        return std::nullopt;
    }
    const bool wordStart = !text.at(at - 1).isLetterOrNumber();
    return std::pair{int(Moderate), wordStart ? 0.7 : 0.5};
}
}

QString locationId(const QUrl &url)
{
    const QByteArray hash = QCryptographicHash::hash(url.toString(QUrl::FullyEncoded).toUtf8(), QCryptographicHash::Sha256).toHex();
    return LocationPrefix + QString::fromLatin1(hash.left(24));
}

QList<Target> targets(const WorkspaceCatalog::Catalog &catalog, const QList<Favorite> &favorites, const Options &options)
{
    QList<Target> result;
    if (!options.enabled) {
        return result;
    }
    if (options.workspaces) {
        for (const WorkspaceCatalog::Workspace &workspace : catalog.workspaces) {
            if (workspace.id.isEmpty() || workspace.name.isEmpty() || options.hiddenWorkspaces.contains(workspace.id)) {
                continue;
            }
            result.append({Kind::Workspace, WorkspacePrefix + workspace.id, workspace.name, workspace.id, {}});
        }
    }
    if (options.favorites) {
        for (const auto &[url, title] : favorites) {
            const QString id = locationId(url);
            if (!url.isValid() || title.isEmpty() || options.hiddenFavorites.contains(id)) {
                continue;
            }
            result.append({Kind::Location, id, title, {}, url});
        }
    }
    return result;
}

QList<Match> match(const QList<Target> &targets, const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.size() < 2) {
        return {};
    }
    QList<Match> result;
    for (const Target &target : targets) {
        auto best = score(target.text, trimmed);
        if (target.kind == Kind::Location) {
            const auto byName = score(target.url.adjusted(QUrl::StripTrailingSlash).fileName(), trimmed);
            if (byName && (!best || byName->second > best->second)) {
                best = byName;
            }
        }
        if (best && available(target)) {
            result.append({target, best->first, best->second});
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Match &a, const Match &b) {
        return a.relevance > b.relevance;
    });
    return result;
}

std::optional<Target> resolve(const QList<Target> &targets, const QString &id)
{
    const auto it = std::find_if(targets.cbegin(), targets.cend(), [&id](const Target &target) {
        return target.id == id;
    });
    if (it == targets.cend() || !available(*it)) {
        return std::nullopt;
    }
    return *it;
}

bool available(const Target &target)
{
    if (target.kind == Kind::Workspace) {
        return !target.workspaceId.isEmpty();
    }
    if (!target.url.isLocalFile()) {
        return target.url.isValid();
    }
    return QFileInfo(target.url.toLocalFile()).isDir();
}

QString subtext(const Target &target)
{
    if (target.kind == Kind::Workspace) {
        return {};
    }
    return target.url.toDisplayString(QUrl::PreferLocalFile | QUrl::RemoveUserInfo);
}

QStringList arguments(const Target &target)
{
    if (target.kind == Kind::Workspace) {
        return {QStringLiteral("--workspace"), target.workspaceId};
    }
    return {target.url.toString(QUrl::FullyEncoded)};
}
}
