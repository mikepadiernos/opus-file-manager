// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <KConfigGroup>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <optional>

/// Associates Plasma Activities with a workspace and default folders.
///
/// Each Activity has its own association, independent of the others. An
/// association names a workspace, chooses whether switching to it uses the
/// workspace's own behaviours or explicit ones, and can name a folder for each
/// pane. Associations are kept when their workspace, folders or Activity
/// disappear, so nothing is lost while they are unavailable.
namespace ActivityAssociations
{
/// The configuration group that holds one subgroup per Activity id.
inline constexpr const char *Group = "ActivityAssociations";

struct Association {
    QString activityId;
    /// The Activity's last known name, shown once the Activity is removed.
    QString activityName;
    /// The associated workspace's id, or empty for none.
    QString workspaceId;
    /// Whether switching uses the workspace's own behaviour selection.
    bool inheritBehaviors = true;
    /// The behaviours to apply instead when inheritBehaviors is false.
    WorkspaceCatalog::BehaviorSelection behaviors;
    /// Folders to open in a new tab of the first and second pane.
    QUrl primaryFolder;
    QUrl secondaryFolder;
    /// Whether the Activity no longer exists. Its association is kept.
    bool removed = false;

    /// Whether the association neither names a workspace nor any folder.
    bool isEmpty() const;
    bool operator==(const Association &other) const = default;
};

/// Reads the associations, skipping malformed subgroups.
QList<Association> load(const KConfigGroup &group);

/// Replaces the stored associations. Empty associations are not stored.
void save(KConfigGroup group, const QList<Association> &associations);

const Association *find(const QList<Association> &associations, const QString &activityId);

/// Adds or replaces the association for its Activity, or removes it when empty.
/// @returns false and sets error when the Activity id or a folder is invalid
bool set(QList<Association> &associations, const Association &association, QString &error);

/// Removes the association for @p activityId.
/// @returns whether there was one
bool remove(QList<Association> &associations, const QString &activityId);

/**
 * Updates names and marks associations removed or restored to match the
 * running Activities. Call it only while the Activities service is running;
 * an unavailable service says nothing about which Activities exist.
 * @param activities the existing Activities' names by id
 * @returns whether anything changed
 */
bool reconcile(QList<Association> &associations, const QMap<QString, QString> &activities);

/// What applying an association does.
struct Plan {
    QString activityId;
    /// The workspace to switch to, or empty to keep the current one.
    QString workspaceId;
    QString workspaceName;
    /// The behaviours to use instead of the workspace's own.
    std::optional<WorkspaceCatalog::BehaviorSelection> behaviors;
    /// Folders to open; invalid when the pane has none or it is unavailable.
    QUrl primaryFolder;
    QUrl secondaryFolder;
    /// Parts of the association that cannot be applied, and why.
    QStringList problems;

    bool isEmpty() const;
    bool operator==(const Plan &other) const = default;
};

/// Whether a folder exists. Local folders are checked; remote ones are assumed
/// to exist, since checking them would block.
bool folderAvailable(const QUrl &url);

/**
 * Plans applying @p association. The workspace switch is left out when its
 * workspace is missing or is already @p currentWorkspaceId; pass an empty
 * current id to switch even then. Missing folders are left out. Both are
 * described in Plan::problems.
 */
Plan plan(const Association &association,
          const WorkspaceCatalog::Catalog &catalog,
          const QString &currentWorkspaceId,
          const std::function<bool(const QUrl &)> &available = folderAvailable);
}
