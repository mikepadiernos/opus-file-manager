// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KSharedConfig>

#include <QList>
#include <QStringList>

namespace SettingsBackup
{
struct Preview {
    QStringList included;
    QStringList excluded;
};

struct WorkspaceProfilesPreview {
    /// A label for each profile in the backup.
    QStringList profiles;
    /// The identity of each profile, in the same order as @c profiles.
    QStringList ids;
    /// The labels of profiles whose name or identity matches an existing workspace.
    QStringList conflicts;
    /// The identities of the conflicting profiles.
    QStringList conflictIds;
};

QString create(const QString &archivePath, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig);
QString createWorkspaceProfiles(const QString &archivePath,
                                const KSharedConfig::Ptr &appConfig,
                                const KSharedConfig::Ptr &stateConfig,
                                const QStringList &workspaceIds);
QString inspect(const QString &archivePath, Preview *preview);
QString inspectWorkspaceProfiles(const QString &archivePath, const KSharedConfig::Ptr &appConfig, WorkspaceProfilesPreview *preview);
/**
 * Imports workspace profiles from a backup without changing other settings.
 * @param replaceConflicts whether a profile replaces an existing workspace with
 *        the same name or identity, instead of being skipped
 * @param imported receives the names of the imported profiles
 * @param workspaceIds the identities of the profiles to import, as listed by
 *        inspectWorkspaceProfiles(); nullptr imports every profile
 * @returns an error message, or an empty string on success
 */
QString importWorkspaceProfiles(const QString &archivePath,
                                const KSharedConfig::Ptr &appConfig,
                                bool replaceConflicts,
                                QStringList *imported = nullptr,
                                const QStringList *workspaceIds = nullptr);
QString pendingRestorePath(const QString &dataDirectory);
QString stageRestore(const QString &archivePath, const QString &dataDirectory);
QString applyPendingRestore(const QString &dataDirectory, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig);
}
