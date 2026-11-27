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

/// The embedded terminal sessions that history search may use, from the AtuinSession setting.
enum class SessionMode {
    /// Only the current session, which must have an idle shell.
    Current,
    /// The current session, or a new one when it is running a program or there is none.
    NewWhenBusy,
    /// Always a new session, unless the current one has just been opened.
    New,
};
/// Where history search goes for one session.
enum class Session {
    Current,
    New,
    /// Report why the search isn't opened.
    Refuse,
};
/// The configured session mode.
SessionMode sessionMode();
/**
 * Where history search goes in @p mode, given whether there is a current
 * session, whether it has just been opened for the search and whether it is
 * running a program. A new session that is busy is refused rather than
 * replaced, so a shell profile that starts a program can't open sessions
 * repeatedly.
 */
Session session(SessionMode mode, bool hasSession, bool sessionIsNew, bool busy);
}
