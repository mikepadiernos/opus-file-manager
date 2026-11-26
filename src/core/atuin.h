// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/**
 * Optional Atuin command-history search in the embedded terminal. Plasma
 * Opus only checks that Atuin and its history exist; it never installs or
 * initializes Atuin, reads its history database or starts a sync.
 */
namespace Atuin
{
/**
 * The atuin executable, found in PATH or in the folders Atuin's installers
 * use, such as ~/.atuin/bin, or an empty string when it is not installed.
 */
QString executable();
/// Atuin's config.toml, honouring ATUIN_CONFIG_DIR and XDG_CONFIG_HOME.
QString configFile();
/**
 * Where Atuin keeps its history database: ATUIN_DB_PATH, then db_path in
 * config.toml, then history.db in Atuin's data folder.
 */
QString databasePath();
/// Why history search can't start, or an empty string when it can.
QString unavailableReason();
/**
 * The input typed into an idle shell to open Atuin's interactive search.
 * Atuin prints the chosen command instead of running it. A leading space
 * keeps the line out of shell history where leading spaces are ignored.
 */
QString searchInput(const QString &executable);
}
