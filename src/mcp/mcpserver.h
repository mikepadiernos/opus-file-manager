// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <functional>

/**
 * A Model Context Protocol server for Plasma Opus. It answers JSON-RPC 2.0
 * messages, one per line on the stdio transport, with tools that read and,
 * when allowed, change local files inside the configured folders.
 *
 * Tool names, descriptions and results are protocol text for MCP clients and
 * their language models, so they are not translated.
 */
class McpServer
{
public:
    enum class Access {
        Off,
        Read,
        Write,
    };

    struct Options {
        Access access = Access::Read;
        /// Absolute local folders that tools may use; empty for the home folder.
        QStringList folders;
        /// Where each tool call is recorded, without file contents; empty for no log.
        QString auditLogPath;
        /// Opens a local folder in Plasma Opus; returns an error message or an empty string. Unset hides the tool.
        std::function<QString(const QString &folder)> openFolder;
        QString version;
    };

    explicit McpServer(Options options);

    /// Handles one message line and returns the response line, or an empty array for notifications.
    QByteArray handleLine(const QByteArray &line);

    /// Handles one JSON-RPC message and returns its response, or an empty object for a notification.
    QJsonObject handle(const QJsonObject &message);

    /// The canonical folders tools may use.
    QStringList folders() const;

    /// The protocol versions this server speaks, newest first.
    static QStringList protocolVersions();

    /// The largest text a read returns, and the default.
    static constexpr qint64 MaximumReadBytes = 1024 * 1024;
    static constexpr qint64 DefaultReadBytes = 256 * 1024;
    /// The largest text a write accepts.
    static constexpr qint64 MaximumWriteBytes = 10 * 1024 * 1024;
    /// The most entries a listing or search returns.
    static constexpr int MaximumEntries = 1000;

private:
    struct Result {
        /// Structured output, also sent as JSON text when content is empty.
        QJsonValue value = {};
        /// Text blocks sent instead of the JSON text, such as a file's contents.
        QStringList content = {};
        QString error = {};
    };
    struct Checked {
        QString path;
        QString error;
        /// Whether the path names an existing entry, even when it can't be used.
        bool exists = false;
    };
    enum class Check {
        /// The entry must exist; a symbolic link must also point inside the folders.
        Follow,
        /// The entry must exist; a symbolic link itself is used.
        Entry,
        /// The entry must not exist, and its parent folder must.
        New,
    };

    QJsonObject initialize(const QJsonObject &params) const;
    QJsonArray tools() const;
    QJsonObject callTool(const QJsonObject &params);
    Result runTool(const QString &name, const QJsonObject &arguments);
    Checked check(const QJsonValue &value, Check check, bool allowFolderRoot = true) const;
    bool inside(const QString &path) const;
    bool isRoot(const QString &path) const;
    void audit(const QString &tool, const QJsonObject &arguments, const Result &result) const;

    Result listFolder(const QJsonObject &arguments) const;
    Result fileInfo(const QJsonObject &arguments) const;
    Result readTextFile(const QJsonObject &arguments) const;
    Result searchFiles(const QJsonObject &arguments) const;
    Result openInPlasmaOpus(const QJsonObject &arguments) const;
    Result createFolder(const QJsonObject &arguments) const;
    Result writeTextFile(const QJsonObject &arguments) const;
    Result copy(const QJsonObject &arguments) const;
    Result move(const QJsonObject &arguments) const;
    Result moveToTrash(const QJsonObject &arguments) const;

    Options m_options;
    QStringList m_folders;
};
