// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KSharedConfig>

#include <QStringList>

namespace SettingsBackup
{
struct Preview {
    QStringList included;
    QStringList excluded;
};

QString create(const QString &archivePath, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig);
QString inspect(const QString &archivePath, Preview *preview);
QString pendingRestorePath(const QString &dataDirectory);
QString stageRestore(const QString &archivePath, const QString &dataDirectory);
QString applyPendingRestore(const QString &dataDirectory, const KSharedConfig::Ptr &appConfig, const KSharedConfig::Ptr &stateConfig);
}
