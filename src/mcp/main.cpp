// SPDX-License-Identifier: GPL-3.0-or-later

// A Model Context Protocol server that MCP clients start to read, and when
// allowed change, local files through Plasma Opus. It speaks JSON-RPC 2.0 on
// stdin and stdout, one message per line, so nothing else may be written to
// stdout.

#include "appidentity.h"
#include "foldergates.h"
#include "mcpserver.h"
#include "opusfm_version.h"
#include "settings.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <cstdio>
#include <iostream>
#include <string>

namespace
{
QString fileManagerExecutable()
{
    const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(AppIdentity::componentName());
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    return QStandardPaths::findExecutable(AppIdentity::componentName());
}

McpServer::Access accessFromSettings()
{
    switch (Settings::mcpAccess()) {
    case Settings::EnumMcpAccess::Off:
        return McpServer::Access::Off;
    case Settings::EnumMcpAccess::Write:
        return McpServer::Access::Write;
    default:
        return McpServer::Access::Read;
    }
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // Settings are read from Plasma Opus's own configuration.
    QCoreApplication::setApplicationName(AppIdentity::componentName());
    QCoreApplication::setApplicationVersion(QStringLiteral(OPUSFM_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Model Context Protocol server for Plasma Opus, on stdin and stdout"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption readOnly(QStringLiteral("read-only"), QStringLiteral("Only read files, even when changing them is allowed in the settings."));
    parser.addOption(readOnly);
    parser.process(app);

    McpServer::Options options;
    options.access = accessFromSettings();
    if (parser.isSet(readOnly) && options.access == McpServer::Access::Write) {
        options.access = McpServer::Access::Read;
    }
    options.folders = Settings::mcpFolders();
    if (Settings::mcpAuditLog()) {
        options.auditLogPath = QStandardPaths::writableLocation(QStandardPaths::StateLocation) + QStringLiteral("/mcp-audit.log");
    }
    options.version = QStringLiteral(OPUSFM_VERSION_STRING);
    options.protectedFolders = FolderGates::instance()->folders();
    if (!fileManagerExecutable().isEmpty()) {
        options.openFolder = [](const QString &folder) {
            const QString program = fileManagerExecutable();
            if (program.isEmpty() || !QProcess::startDetached(program, {folder})) {
                return QStringLiteral("Plasma Opus could not be started.");
            }
            return QString();
        };
    }

    McpServer server(options);
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        const QByteArray answer = server.handleLine(QByteArray::fromStdString(line));
        if (!answer.isEmpty()) {
            std::cout << answer.toStdString() << '\n' << std::flush;
        }
    }
    return 0;
}
