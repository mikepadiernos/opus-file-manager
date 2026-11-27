// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mounttable.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * Decides what Plasma Opus reports to Plasma Activities as used in the
 * current Activity. Plasma Activities is the only context Plasma Opus shares
 * with other applications, such as Plasma Impresario: each application
 * reports its own use and follows Activity changes with its own
 * associations, and none reads or changes another's data.
 */
namespace ActivityUsage
{
/// The mount points of encrypted folders, such as Plasma Vault's CryFS,
/// gocryptfs and EncFS vaults, whose contents are never reported.
QStringList encryptedMountPoints(const QList<MountTable::Mounted> &mounts);

/**
 * The URL to report for @p url, or an empty URL when it must not be reported.
 * Only local files and network locations are reported, without a password,
 * query or fragment. Virtual locations, such as search results, the trash and
 * administrator locations, are never reported, nor are local paths in
 * @p excludedFolders or their subfolders.
 */
QUrl reportableUrl(const QUrl &url, const QStringList &excludedFolders);
}
