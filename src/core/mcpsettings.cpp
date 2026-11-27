// SPDX-License-Identifier: GPL-3.0-or-later

#include "mcpsettings.h"

#include "appidentity.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace McpSettings
{
QString auditLogPath()
{
    // The server and Plasma Opus have different application names, so the folder is named explicitly.
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QLatin1Char('/') + AppIdentity::componentName()
        + QStringLiteral("/mcp-audit.log");
}

Folders resolveFolders(const QStringList &configured, const QStringList &gates)
{
    Folders result;
    const QStringList folders = configured.isEmpty() ? QStringList{QDir::homePath()} : configured;
    for (const QString &entry : folders) {
        QString folder = entry;
        if (folder == QLatin1String("~") || folder.startsWith(QLatin1String("~/"))) {
            folder.replace(0, 1, QDir::homePath());
        }
        const QString canonical = QDir::isAbsolutePath(folder) ? QFileInfo(folder).canonicalFilePath() : QString();
        if (canonical.isEmpty() || !QFileInfo(canonical).isDir()) {
            result.skipped.append(entry);
            continue;
        }
        if (result.used.contains(canonical)) {
            continue;
        }
        result.used.append(canonical);
        const bool gated = std::any_of(gates.cbegin(), gates.cend(), [&canonical](const QString &gate) {
            return canonical == gate || canonical.startsWith(gate.endsWith(QLatin1Char('/')) ? gate : gate + QLatin1Char('/'));
        });
        if (gated) {
            result.gated.append(canonical);
        }
    }
    return result;
}

QString clientConfiguration()
{
    return QStringLiteral(R"({ "mcpServers": { "plasma-opus": { "command": "opus-file-manager-mcp" } } })");
}
}
