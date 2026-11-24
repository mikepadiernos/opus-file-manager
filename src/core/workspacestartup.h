// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <KConfigGroup>
#include <QString>

namespace WorkspaceStartup
{
/// StartupWorkspace value that starts without an active workspace.
inline constexpr auto NoWorkspace = "none";

/**
 * Chooses the workspace that is active when Plasma Opus starts.
 * @param catalog the saved workspace catalog
 * @param setting the StartupWorkspace setting: empty for the last used
 *        workspace, "none" for no workspace, or a workspace UUID
 * @param warning set when the chosen workspace is unavailable and the last
 *        used workspace is kept instead; cleared otherwise
 * @returns the UUID to make active, or an empty string for no workspace
 */
QString resolve(const WorkspaceCatalog::Catalog &catalog, const QString &setting, QString *warning = nullptr);

/**
 * Makes the startup workspace active in the saved catalog. Only the active
 * workspace changes; layouts, sessions and startup folders are left alone.
 * @param group the "Workspaces" group of a configuration without pending
 *        changes
 * @param setting the StartupWorkspace setting
 * @param warning describes an unavailable startup workspace or a catalog that
 *        could not be read or saved; cleared otherwise
 * @returns whether the catalog is valid and holds the startup workspace
 */
bool apply(KConfigGroup group, const QString &setting, QString *warning = nullptr);
}
