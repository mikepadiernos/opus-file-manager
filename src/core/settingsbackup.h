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
    QStringList profiles;
    QStringList conflicts;
};

QString create(const QString &archivePath, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig);
QString createWorkspaceProfiles(const QString &archivePath,
                                const KSharedConfig::Ptr &appConfig,
                                const KSharedConfig::Ptr &stateConfig,
                                const QStringList &workspaceIds);
QString inspect(const QString &archivePath, Preview *preview);
QString inspectWorkspaceProfiles(const QString &archivePath, const KSharedConfig::Ptr &appConfig, WorkspaceProfilesPreview *preview);
QString importWorkspaceProfiles(const QString &archivePath, const KSharedConfig::Ptr &appConfig, bool replaceConflicts, QStringList *imported = nullptr);
QString pendingRestorePath(const QString &dataDirectory);
QString stageRestore(const QString &archivePath, const QString &dataDirectory);
QString applyPendingRestore(const QString &dataDirectory, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig);
}
