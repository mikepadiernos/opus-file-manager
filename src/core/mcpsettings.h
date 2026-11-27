// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

/**
 * Shared knowledge about the opus-file-manager-mcp server's settings, used
 * by the server and by Preferences › Security › MCP so both describe the
 * same folders and log.
 */
namespace McpSettings
{
/// The audit log that the server appends each tool call to.
QString auditLogPath();

/// The folders that the server may use, as it resolves the McpFolders setting.
struct Folders {
    /// Canonical existing folders that tools may use.
    QStringList used;
    /// Configured folders that are missing, not folders or not absolute, so the server skips them.
    QStringList skipped;
    /// Used folders that are password-protected or inside a protected folder, so tools refuse them.
    QStringList gated;
};

/**
 * Resolves @p configured as the server does: an empty list means the home
 * folder, and "~" starts a path in the home folder.
 * @param gates the canonical password-protected folders
 */
Folders resolveFolders(const QStringList &configured, const QStringList &gates);

/// The JSON that adds the server to an MCP client's configuration.
QString clientConfiguration();
}
