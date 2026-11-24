// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"

namespace IndexedSearch
{
/// Searches the index, optionally limiting matches and depth to workspaceRoots.
LiveSearch::Result find(const QString &currentFolder,
                        const QString &text,
                        LiveSearch::Field field,
                        const LiveSearch::Cancel &cancel = {},
                        const LiveSearch::Progress &progress = {},
                        const PathFilter &filter = {},
                        const QStringList *workspaceRoots = nullptr);
}
