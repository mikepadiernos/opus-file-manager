// SPDX-License-Identifier: GPL-3.0-or-later

#include "mcpserver.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace
{
constexpr int ParseError = -32700;
constexpr int InvalidRequest = -32600;
constexpr int MethodNotFound = -32601;
constexpr int InvalidParams = -32602;

/// Entries a search visits before it stops, so a huge tree can't stall the server.
constexpr int SearchVisitLimit = 200000;
constexpr int DefaultSearchResults = 200;

const QStringList ReadTools = {
    QStringLiteral("list_folder"),
    QStringLiteral("file_info"),
    QStringLiteral("read_text_file"),
    QStringLiteral("search_files"),
    QStringLiteral("open_in_plasma_opus"),
};
const QStringList WriteTools = {
    QStringLiteral("create_folder"),
    QStringLiteral("write_text_file"),
    QStringLiteral("copy"),
    QStringLiteral("move"),
    QStringLiteral("move_to_trash"),
};
/// Arguments that name paths, recorded in the audit log.
const QStringList PathArguments = {QStringLiteral("path"), QStringLiteral("folder"), QStringLiteral("source"), QStringLiteral("destination")};

QJsonObject response(const QJsonValue &id)
{
    return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), id}};
}

QJsonObject errorResponse(const QJsonValue &id, int code, const QString &message)
{
    QJsonObject object = response(id);
    object.insert(QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}});
    return object;
}

QJsonObject resultResponse(const QJsonValue &id, const QJsonObject &result)
{
    QJsonObject object = response(id);
    object.insert(QStringLiteral("result"), result);
    return object;
}

QJsonObject stringProperty(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("description"), description}};
}

QJsonObject booleanProperty(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("boolean")}, {QStringLiteral("description"), description}};
}

QJsonObject integerProperty(const QString &description, qint64 maximum)
{
    return {{QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"), description},
            {QStringLiteral("minimum"), 1},
            {QStringLiteral("maximum"), maximum}};
}

QJsonObject inputSchema(const QJsonObject &properties, const QStringList &required)
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), properties},
            {QStringLiteral("required"), QJsonArray::fromStringList(required)},
            {QStringLiteral("additionalProperties"), false}};
}

QJsonObject tool(const QString &name, const QString &title, const QString &description, const QJsonObject &schema, bool readOnly, bool destructive)
{
    return {{QStringLiteral("name"), name},
            {QStringLiteral("title"), title},
            {QStringLiteral("description"), description},
            {QStringLiteral("inputSchema"), schema},
            {QStringLiteral("annotations"),
             QJsonObject{{QStringLiteral("title"), title},
                         {QStringLiteral("readOnlyHint"), readOnly},
                         {QStringLiteral("destructiveHint"), destructive},
                         {QStringLiteral("idempotentHint"), readOnly},
                         {QStringLiteral("openWorldHint"), false}}}};
}

QString typeOf(const QFileInfo &info)
{
    if (info.isSymLink()) {
        return QStringLiteral("symlink");
    }
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    return info.isFile() ? QStringLiteral("file") : QStringLiteral("other");
}

QString timestamp(const QDateTime &time)
{
    return time.isValid() ? time.toUTC().toString(Qt::ISODate) : QString();
}

QJsonObject entry(const QFileInfo &info)
{
    QJsonObject object{{QStringLiteral("name"), info.fileName()}, {QStringLiteral("type"), typeOf(info)}};
    if (info.isFile() && !info.isSymLink()) {
        object.insert(QStringLiteral("size"), info.size());
    }
    object.insert(QStringLiteral("modified"), timestamp(info.lastModified()));
    return object;
}

/// Drops an incomplete UTF-8 sequence that a size limit cut off at the end of @p bytes.
QByteArray trimIncompleteUtf8(QByteArray bytes)
{
    qsizetype lead = bytes.size() - 1;
    int continuation = 0;
    while (lead >= 0 && continuation < 3 && (static_cast<uchar>(bytes.at(lead)) & 0xC0) == 0x80) {
        --lead;
        ++continuation;
    }
    if (lead < 0) {
        return bytes;
    }
    const auto byte = static_cast<uchar>(bytes.at(lead));
    const int length = (byte & 0xE0) == 0xC0 ? 2 : (byte & 0xF0) == 0xE0 ? 3 : (byte & 0xF8) == 0xF0 ? 4 : 1;
    if (continuation + 1 < length) {
        bytes.truncate(lead);
    }
    return bytes;
}

/// Copies a file or folder; symbolic links are copied as links, not followed.
bool copyEntry(const QString &source, const QString &destination, QString *error)
{
    const QFileInfo info(source);
    if (info.isSymLink()) {
        if (!QFile::link(info.readSymLink(), destination)) {
            *error = QStringLiteral("Could not copy the symbolic link %1.").arg(source);
            return false;
        }
        return true;
    }
    if (info.isDir()) {
        if (!QDir().mkdir(destination)) {
            *error = QStringLiteral("Could not create the folder %1.").arg(destination);
            return false;
        }
        const QStringList names = QDir(source).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        for (const QString &name : names) {
            if (!copyEntry(source + QLatin1Char('/') + name, destination + QLatin1Char('/') + name, error)) {
                return false;
            }
        }
        QFile::setPermissions(destination, info.permissions());
        return true;
    }
    QFile file(source);
    if (!file.copy(destination)) {
        *error = QStringLiteral("Could not copy %1: %2").arg(source, file.errorString());
        return false;
    }
    return true;
}

bool within(const QString &folder, const QString &path)
{
    return path == folder || path.startsWith(folder.endsWith(QLatin1Char('/')) ? folder : folder + QLatin1Char('/'));
}

QString escapeLogField(QString field)
{
    return field.replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
        .replace(QLatin1Char('\t'), QStringLiteral("\\t"))
        .replace(QLatin1Char('\n'), QStringLiteral("\\n"));
}
}

McpServer::McpServer(Options options)
    : m_options(std::move(options))
{
    const QStringList configured = m_options.folders.isEmpty() ? QStringList{QDir::homePath()} : m_options.folders;
    for (QString folder : configured) {
        if (folder == QLatin1String("~") || folder.startsWith(QLatin1String("~/"))) {
            folder.replace(0, 1, QDir::homePath());
        }
        if (!QDir::isAbsolutePath(folder)) {
            continue;
        }
        const QString canonical = QFileInfo(folder).canonicalFilePath();
        if (!canonical.isEmpty() && QFileInfo(canonical).isDir() && !m_folders.contains(canonical)) {
            m_folders.append(canonical);
        }
    }
    for (const QString &folder : std::as_const(m_options.protectedFolders)) {
        if (!QDir::isAbsolutePath(folder)) {
            continue;
        }
        const QString canonical = QFileInfo(folder).canonicalFilePath();
        const QString path = canonical.isEmpty() ? QDir::cleanPath(folder) : canonical;
        if (!m_protected.contains(path)) {
            m_protected.append(path);
        }
    }
}

QStringList McpServer::folders() const
{
    return m_folders;
}

QStringList McpServer::protocolVersions()
{
    return {QStringLiteral("2025-06-18"), QStringLiteral("2025-03-26"), QStringLiteral("2024-11-05")};
}

QByteArray McpServer::handleLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return QJsonDocument(errorResponse(QJsonValue::Null, ParseError, QStringLiteral("Parse error: %1").arg(parseError.errorString())))
            .toJson(QJsonDocument::Compact);
    }
    if (document.isArray()) {
        const QJsonArray messages = document.array();
        if (messages.isEmpty()) {
            return QJsonDocument(errorResponse(QJsonValue::Null, InvalidRequest, QStringLiteral("Empty batch"))).toJson(QJsonDocument::Compact);
        }
        QJsonArray responses;
        for (const QJsonValue &message : messages) {
            const QJsonObject answer =
                message.isObject() ? handle(message.toObject()) : errorResponse(QJsonValue::Null, InvalidRequest, QStringLiteral("Invalid request"));
            if (!answer.isEmpty()) {
                responses.append(answer);
            }
        }
        return responses.isEmpty() ? QByteArray() : QJsonDocument(responses).toJson(QJsonDocument::Compact);
    }
    const QJsonObject answer =
        document.isObject() ? handle(document.object()) : errorResponse(QJsonValue::Null, InvalidRequest, QStringLiteral("Invalid request"));
    return answer.isEmpty() ? QByteArray() : QJsonDocument(answer).toJson(QJsonDocument::Compact);
}

QJsonObject McpServer::handle(const QJsonObject &message)
{
    const bool notification = !message.contains(QStringLiteral("id"));
    const QJsonValue id = message.value(QStringLiteral("id"));
    if (message.value(QStringLiteral("jsonrpc")).toString() != QLatin1String("2.0") || !message.value(QStringLiteral("method")).isString()) {
        // Responses need no answer; this server sends no requests of its own.
        if (notification || message.contains(QStringLiteral("result")) || message.contains(QStringLiteral("error"))) {
            return {};
        }
        return errorResponse(id, InvalidRequest, QStringLiteral("Invalid request"));
    }
    // Notifications such as notifications/initialized and notifications/cancelled need no answer.
    if (notification) {
        return {};
    }
    const QString method = message.value(QStringLiteral("method")).toString();
    const QJsonObject params = message.value(QStringLiteral("params")).toObject();
    if (method == QLatin1String("initialize")) {
        return resultResponse(id, initialize(params));
    }
    if (method == QLatin1String("ping")) {
        return resultResponse(id, {});
    }
    if (method == QLatin1String("tools/list")) {
        return resultResponse(id, {{QStringLiteral("tools"), tools()}});
    }
    if (method == QLatin1String("tools/call")) {
        const QString name = params.value(QStringLiteral("name")).toString();
        if (!ReadTools.contains(name) && !WriteTools.contains(name)) {
            return errorResponse(id, InvalidParams, QStringLiteral("Unknown tool: %1").arg(name));
        }
        return resultResponse(id, callTool(params));
    }
    return errorResponse(id, MethodNotFound, QStringLiteral("Method not found: %1").arg(method));
}

QJsonObject McpServer::initialize(const QJsonObject &params) const
{
    const QString requested = params.value(QStringLiteral("protocolVersion")).toString();
    const QString version = protocolVersions().contains(requested) ? requested : protocolVersions().constFirst();
    const QString folders = m_folders.join(QStringLiteral(", "));
    QString instructions;
    switch (m_options.access) {
    case Access::Off:
        instructions = QStringLiteral("The user has turned off MCP access to Plasma Opus, so no tools are available.");
        break;
    case Access::Read:
        instructions =
            QStringLiteral("Plasma Opus tools read local files and folders inside: %1. Use absolute paths. Changing files is turned off.").arg(folders);
        break;
    case Access::Write:
        instructions = QStringLiteral(
                           "Plasma Opus tools read and change local files and folders inside: %1. Use absolute paths. Nothing is deleted "
                           "permanently: move_to_trash moves items to the trash, and existing files are only replaced when overwrite is set.")
                           .arg(folders);
        break;
    }
    return {{QStringLiteral("protocolVersion"), version},
            {QStringLiteral("capabilities"), QJsonObject{{QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), false}}}}},
            {QStringLiteral("serverInfo"),
             QJsonObject{{QStringLiteral("name"), QStringLiteral("plasma-opus")},
                         {QStringLiteral("title"), QStringLiteral("Plasma Opus")},
                         {QStringLiteral("version"), m_options.version}}},
            {QStringLiteral("instructions"), instructions}};
}

QJsonArray McpServer::tools() const
{
    QJsonArray list;
    if (m_options.access == Access::Off) {
        return list;
    }
    const QJsonObject path = stringProperty(QStringLiteral("Absolute local path"));
    const QJsonObject hidden = booleanProperty(QStringLiteral("Include hidden items, whose names start with a dot; false by default"));
    list.append(tool(QStringLiteral("list_folder"),
                     QStringLiteral("List Folder"),
                     QStringLiteral("List a local folder's files and subfolders with their type, size and modification time, folders first. "
                                    "Returns up to %1 entries.")
                         .arg(MaximumEntries),
                     inputSchema({{QStringLiteral("path"), path}, {QStringLiteral("include_hidden"), hidden}}, {QStringLiteral("path")}),
                     true,
                     false));
    list.append(tool(QStringLiteral("file_info"),
                     QStringLiteral("File Information"),
                     QStringLiteral("Describe a local file, folder or symbolic link: type, size, MIME type, times, owner and permissions."),
                     inputSchema({{QStringLiteral("path"), path}}, {QStringLiteral("path")}),
                     true,
                     false));
    list.append(tool(QStringLiteral("read_text_file"),
                     QStringLiteral("Read Text File"),
                     QStringLiteral("Read a local UTF-8 text file. Binary files are refused, and longer files are cut off after max_bytes."),
                     inputSchema({{QStringLiteral("path"), path},
                                  {QStringLiteral("max_bytes"),
                                   integerProperty(QStringLiteral("The most bytes to read; %1 by default").arg(DefaultReadBytes), MaximumReadBytes)}},
                                 {QStringLiteral("path")}),
                     true,
                     false));
    list.append(tool(QStringLiteral("search_files"),
                     QStringLiteral("Search Files"),
                     QStringLiteral("Find files and folders by name inside a local folder and its subfolders. Symbolic links to folders are "
                                    "not followed."),
                     inputSchema({{QStringLiteral("folder"), stringProperty(QStringLiteral("Absolute local folder to search"))},
                                  {QStringLiteral("pattern"),
                                   stringProperty(QStringLiteral("Name pattern with * and ? wildcards, such as *.md; text without wildcards "
                                                                 "matches anywhere in the name. Case-insensitive."))},
                                  {QStringLiteral("include_hidden"), hidden},
                                  {QStringLiteral("max_results"),
                                   integerProperty(QStringLiteral("The most results to return; %1 by default").arg(DefaultSearchResults), MaximumEntries)}},
                                 {QStringLiteral("folder"), QStringLiteral("pattern")}),
                     true,
                     false));
    if (m_options.openFolder) {
        list.append(tool(QStringLiteral("open_in_plasma_opus"),
                         QStringLiteral("Open in Plasma Opus"),
                         QStringLiteral("Show a local folder, or the folder containing a file, in a Plasma Opus window for the user."),
                         inputSchema({{QStringLiteral("path"), path}}, {QStringLiteral("path")}),
                         true,
                         false));
    }
    if (m_options.access != Access::Write) {
        return list;
    }
    list.append(tool(QStringLiteral("create_folder"),
                     QStringLiteral("Create Folder"),
                     QStringLiteral("Create a new local folder. Its parent folder must exist."),
                     inputSchema({{QStringLiteral("path"), path}}, {QStringLiteral("path")}),
                     false,
                     false));
    list.append(tool(QStringLiteral("write_text_file"),
                     QStringLiteral("Write Text File"),
                     QStringLiteral("Save UTF-8 text to a local file. An existing file is replaced only when overwrite is true; symbolic links "
                                    "are refused."),
                     inputSchema({{QStringLiteral("path"), path},
                                  {QStringLiteral("content"), stringProperty(QStringLiteral("The text to save"))},
                                  {QStringLiteral("overwrite"), booleanProperty(QStringLiteral("Replace an existing file; false by default"))}},
                                 {QStringLiteral("path"), QStringLiteral("content")}),
                     false,
                     true));
    const QJsonObject transfer = inputSchema({{QStringLiteral("source"), stringProperty(QStringLiteral("Absolute local path of the item"))},
                                              {QStringLiteral("destination"),
                                               stringProperty(QStringLiteral("Absolute local path of the new item, including its name; it must "
                                                                             "not exist"))}},
                                             {QStringLiteral("source"), QStringLiteral("destination")});
    list.append(tool(QStringLiteral("copy"),
                     QStringLiteral("Copy"),
                     QStringLiteral("Copy a local file or folder to a new path. Symbolic links inside folders are copied as links."),
                     transfer,
                     false,
                     false));
    list.append(tool(QStringLiteral("move"),
                     QStringLiteral("Move or Rename"),
                     QStringLiteral("Move or rename a local file or folder to a new path on the same file system."),
                     transfer,
                     false,
                     false));
    list.append(tool(QStringLiteral("move_to_trash"),
                     QStringLiteral("Move to Trash"),
                     QStringLiteral("Move a local file or folder to the trash, where the user can restore it."),
                     inputSchema({{QStringLiteral("path"), path}}, {QStringLiteral("path")}),
                     false,
                     true));
    return list;
}

QJsonObject McpServer::callTool(const QJsonObject &params)
{
    const QString name = params.value(QStringLiteral("name")).toString();
    const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
    Result result;
    if (m_options.access == Access::Off) {
        result.error = QStringLiteral("The user has turned off MCP access to Plasma Opus.");
    } else if (WriteTools.contains(name) && m_options.access != Access::Write) {
        result.error = QStringLiteral("Changing files is turned off; this Plasma Opus server only reads them.");
    } else if (name == QLatin1String("open_in_plasma_opus") && !m_options.openFolder) {
        result.error = QStringLiteral("Plasma Opus can't be opened from this server.");
    } else {
        result = runTool(name, arguments);
    }
    audit(name, arguments, result);

    QJsonArray content;
    const auto addText = [&content](const QString &text) {
        content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}});
    };
    if (!result.error.isEmpty()) {
        addText(result.error);
        return {{QStringLiteral("content"), content}, {QStringLiteral("isError"), true}};
    }
    if (result.content.isEmpty()) {
        addText(QString::fromUtf8(QJsonDocument(result.value.toObject()).toJson(QJsonDocument::Indented)));
    } else {
        for (const QString &text : std::as_const(result.content)) {
            addText(text);
        }
    }
    QJsonObject answer{{QStringLiteral("content"), content}, {QStringLiteral("isError"), false}};
    if (result.value.isObject()) {
        answer.insert(QStringLiteral("structuredContent"), result.value);
    }
    return answer;
}

McpServer::Result McpServer::runTool(const QString &name, const QJsonObject &arguments)
{
    if (name == QLatin1String("list_folder")) {
        return listFolder(arguments);
    }
    if (name == QLatin1String("file_info")) {
        return fileInfo(arguments);
    }
    if (name == QLatin1String("read_text_file")) {
        return readTextFile(arguments);
    }
    if (name == QLatin1String("search_files")) {
        return searchFiles(arguments);
    }
    if (name == QLatin1String("open_in_plasma_opus")) {
        return openInPlasmaOpus(arguments);
    }
    if (name == QLatin1String("create_folder")) {
        return createFolder(arguments);
    }
    if (name == QLatin1String("write_text_file")) {
        return writeTextFile(arguments);
    }
    if (name == QLatin1String("copy")) {
        return copy(arguments);
    }
    if (name == QLatin1String("move")) {
        return move(arguments);
    }
    return moveToTrash(arguments);
}

McpServer::Checked McpServer::check(const QJsonValue &value, Check mode, bool allowFolderRoot) const
{
    Checked checked;
    QString path = value.toString();
    if (path.isEmpty()) {
        checked.error = QStringLiteral("An absolute local path is required.");
        return checked;
    }
    if (path.startsWith(QLatin1String("file:"))) {
        const QUrl url(path);
        path = url.isLocalFile() ? url.toLocalFile() : QString();
    }
    if (!QDir::isAbsolutePath(path)) {
        checked.error = QStringLiteral("Use an absolute local path instead of %1.").arg(value.toString());
        return checked;
    }
    path = QDir::cleanPath(path);
    const QFileInfo info(path);
    checked.exists = info.exists() || info.isSymLink();
    if (mode == Check::New && checked.exists) {
        checked.error = QStringLiteral("%1 already exists.").arg(path);
        return checked;
    }
    if (mode != Check::New && !checked.exists) {
        checked.error = QStringLiteral("%1 does not exist.").arg(path);
        return checked;
    }
    // Resolve the parent folder, so links in it can't lead outside, but keep
    // the entry itself: a symbolic link is moved or trashed, not its target.
    const QString parent = QFileInfo(info.path()).canonicalFilePath();
    if (parent.isEmpty()) {
        checked.error = QStringLiteral("The folder containing %1 does not exist.").arg(path);
        return checked;
    }
    const QString location = info.fileName().isEmpty() ? parent : QDir(parent).filePath(info.fileName());
    if (!inside(location)) {
        checked.error = QStringLiteral("%1 is outside the folders this server may use: %2.").arg(path, m_folders.join(QStringLiteral(", ")));
        return checked;
    }
    if (isProtected(location)) {
        checked.error = QStringLiteral("%1 is in a password-protected folder, which this server can't use.").arg(path);
        return checked;
    }
    if (mode == Check::Follow && info.isSymLink()) {
        const QString target = info.canonicalFilePath();
        if (target.isEmpty()) {
            checked.error = QStringLiteral("%1 is a broken symbolic link.").arg(path);
            return checked;
        }
        if (!inside(target)) {
            checked.error = QStringLiteral("%1 links to a location outside the folders this server may use.").arg(path);
            return checked;
        }
        if (isProtected(target)) {
            checked.error = QStringLiteral("%1 links to a password-protected folder, which this server can't use.").arg(path);
            return checked;
        }
    }
    if (!allowFolderRoot && isRoot(location)) {
        checked.error = QStringLiteral("%1 is one of the folders this server may use, so it can't be moved or trashed.").arg(path);
        return checked;
    }
    checked.path = location;
    return checked;
}

bool McpServer::inside(const QString &path) const
{
    return std::any_of(m_folders.cbegin(), m_folders.cend(), [&path](const QString &folder) {
        return within(folder, path);
    });
}

bool McpServer::isRoot(const QString &path) const
{
    return m_folders.contains(path);
}

bool McpServer::isProtected(const QString &path) const
{
    return std::any_of(m_protected.cbegin(), m_protected.cend(), [&path](const QString &folder) {
        return within(folder, path);
    });
}

bool McpServer::containsProtected(const QString &folder) const
{
    return std::any_of(m_protected.cbegin(), m_protected.cend(), [&folder](const QString &gate) {
        return gate != folder && within(folder, gate);
    });
}

void McpServer::audit(const QString &tool, const QJsonObject &arguments, const Result &result) const
{
    if (m_options.auditLogPath.isEmpty()) {
        return;
    }
    QDir().mkpath(QFileInfo(m_options.auditLogPath).path());
    QFile log(m_options.auditLogPath);
    if (!log.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QStringList fields{QDateTime::currentDateTimeUtc().toString(Qt::ISODate),
                       tool,
                       result.error.isEmpty() ? QStringLiteral("ok") : QStringLiteral("error: %1").arg(result.error.simplified())};
    for (const QString &key : PathArguments) {
        const QJsonValue value = arguments.value(key);
        if (value.isString()) {
            fields.append(QStringLiteral("%1=%2").arg(key, value.toString()));
        }
    }
    for (QString &field : fields) {
        field = escapeLogField(field);
    }
    log.write(fields.join(QLatin1Char('\t')).toUtf8() + '\n');
}

McpServer::Result McpServer::listFolder(const QJsonObject &arguments) const
{
    const Checked folder = check(arguments.value(QStringLiteral("path")), Check::Follow);
    if (!folder.error.isEmpty()) {
        return {.error = folder.error};
    }
    const QFileInfo info(folder.path);
    if (!info.isDir()) {
        return {.error = QStringLiteral("%1 is not a folder.").arg(folder.path)};
    }
    if (!info.isReadable() || !info.isExecutable()) {
        return {.error = QStringLiteral("%1 can't be read.").arg(folder.path)};
    }
    QDir::Filters filters = QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot;
    if (arguments.value(QStringLiteral("include_hidden")).toBool()) {
        filters |= QDir::Hidden;
    }
    const QFileInfoList entries = QDir(folder.path).entryInfoList(filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QJsonArray list;
    for (qsizetype i = 0; i < entries.size() && i < MaximumEntries; ++i) {
        list.append(entry(entries.at(i)));
    }
    return {.value = QJsonObject{{QStringLiteral("path"), folder.path},
                                 {QStringLiteral("entries"), list},
                                 {QStringLiteral("total"), entries.size()},
                                 {QStringLiteral("truncated"), entries.size() > MaximumEntries}}};
}

McpServer::Result McpServer::fileInfo(const QJsonObject &arguments) const
{
    const Checked checked = check(arguments.value(QStringLiteral("path")), Check::Entry);
    if (!checked.error.isEmpty()) {
        return {.error = checked.error};
    }
    const QFileInfo info(checked.path);
    QJsonObject object{{QStringLiteral("path"), checked.path},
                       {QStringLiteral("name"), info.fileName()},
                       {QStringLiteral("type"), typeOf(info)},
                       {QStringLiteral("hidden"), info.fileName().startsWith(QLatin1Char('.'))},
                       {QStringLiteral("modified"), timestamp(info.lastModified())},
                       {QStringLiteral("owner"), info.owner()},
                       {QStringLiteral("group"), info.group()},
                       {QStringLiteral("readable"), info.isReadable()},
                       {QStringLiteral("writable"), info.isWritable()},
                       {QStringLiteral("executable"), info.isExecutable()}};
    const QString created = timestamp(info.birthTime());
    if (!created.isEmpty()) {
        object.insert(QStringLiteral("created"), created);
    }
    if (info.isSymLink()) {
        const QString target = info.canonicalFilePath();
        if (!target.isEmpty() && inside(target)) {
            object.insert(QStringLiteral("target"), target);
            object.insert(QStringLiteral("targetType"), typeOf(QFileInfo(target)));
        } else {
            object.insert(QStringLiteral("targetOutsideFolders"), !target.isEmpty());
            object.insert(QStringLiteral("broken"), target.isEmpty());
        }
        return {.value = object};
    }
    if (info.isDir()) {
        object.insert(QStringLiteral("mimeType"), QStringLiteral("inode/directory"));
        object.insert(QStringLiteral("items"), QDir(checked.path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).size());
    } else {
        object.insert(QStringLiteral("size"), info.size());
        object.insert(QStringLiteral("mimeType"), QMimeDatabase().mimeTypeForFile(info).name());
    }
    return {.value = object};
}

McpServer::Result McpServer::readTextFile(const QJsonObject &arguments) const
{
    const Checked checked = check(arguments.value(QStringLiteral("path")), Check::Follow);
    if (!checked.error.isEmpty()) {
        return {.error = checked.error};
    }
    const QFileInfo info(checked.path);
    if (!info.isFile()) {
        return {.error = QStringLiteral("%1 is not a file.").arg(checked.path)};
    }
    const qint64 maximum = std::clamp<qint64>(arguments.value(QStringLiteral("max_bytes")).toInteger(DefaultReadBytes), 1, MaximumReadBytes);
    QFile file(checked.path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {.error = QStringLiteral("%1 can't be read: %2").arg(checked.path, file.errorString())};
    }
    QByteArray bytes = file.read(maximum);
    const bool truncated = !file.atEnd();
    if (bytes.contains('\0')) {
        return {.error = QStringLiteral("%1 looks like a binary file, not text.").arg(checked.path)};
    }
    if (truncated) {
        bytes = trimIncompleteUtf8(bytes);
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    if (decoder.hasError()) {
        return {.error = QStringLiteral("%1 is not UTF-8 text.").arg(checked.path)};
    }
    Result result{.value = QJsonObject{{QStringLiteral("path"), checked.path},
                                       {QStringLiteral("size"), info.size()},
                                       {QStringLiteral("bytesRead"), bytes.size()},
                                       {QStringLiteral("truncated"), truncated}},
                  .content = {text}};
    if (truncated) {
        result.content.append(QStringLiteral("[Cut off after %1 of %2 bytes. Pass a larger max_bytes, up to %3, to read more.]")
                                  .arg(bytes.size())
                                  .arg(info.size())
                                  .arg(MaximumReadBytes));
    }
    return result;
}

McpServer::Result McpServer::searchFiles(const QJsonObject &arguments) const
{
    const Checked folder = check(arguments.value(QStringLiteral("folder")), Check::Follow);
    if (!folder.error.isEmpty()) {
        return {.error = folder.error};
    }
    if (!QFileInfo(folder.path).isDir()) {
        return {.error = QStringLiteral("%1 is not a folder.").arg(folder.path)};
    }
    QString pattern = arguments.value(QStringLiteral("pattern")).toString().trimmed();
    if (pattern.isEmpty()) {
        return {.error = QStringLiteral("A name pattern is required, such as *.md.")};
    }
    if (!pattern.contains(QLatin1Char('*')) && !pattern.contains(QLatin1Char('?')) && !pattern.contains(QLatin1Char('['))) {
        pattern = QLatin1Char('*') + pattern + QLatin1Char('*');
    }
    const QRegularExpression expression = QRegularExpression::fromWildcard(pattern, Qt::CaseInsensitive, QRegularExpression::NonPathWildcardConversion);
    if (!expression.isValid()) {
        return {.error = QStringLiteral("The pattern %1 is not valid.").arg(pattern)};
    }
    const int maximum = std::clamp<int>(arguments.value(QStringLiteral("max_results")).toInt(DefaultSearchResults), 1, MaximumEntries);
    QDir::Filters filters = QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot;
    if (arguments.value(QStringLiteral("include_hidden")).toBool()) {
        filters |= QDir::Hidden;
    }
    QDirIterator iterator(folder.path, filters, QDirIterator::Subdirectories);
    QJsonArray results;
    int visited = 0;
    bool truncated = false;
    while (iterator.hasNext()) {
        if (++visited > SearchVisitLimit || results.size() >= maximum) {
            truncated = true;
            break;
        }
        const QFileInfo info = iterator.nextFileInfo();
        if (expression.match(info.fileName()).hasMatch() && !isProtected(info.path())) {
            results.append(QJsonObject{{QStringLiteral("path"), info.filePath()}, {QStringLiteral("type"), typeOf(info)}});
        }
    }
    return {.value = QJsonObject{{QStringLiteral("folder"), folder.path},
                                 {QStringLiteral("pattern"), pattern},
                                 {QStringLiteral("results"), results},
                                 {QStringLiteral("truncated"), truncated}}};
}

McpServer::Result McpServer::openInPlasmaOpus(const QJsonObject &arguments) const
{
    const Checked checked = check(arguments.value(QStringLiteral("path")), Check::Follow);
    if (!checked.error.isEmpty()) {
        return {.error = checked.error};
    }
    const QFileInfo info(checked.path);
    const QString folder = info.isDir() ? checked.path : info.path();
    const QString error = m_options.openFolder(folder);
    if (!error.isEmpty()) {
        return {.error = error};
    }
    return {.value = QJsonObject{{QStringLiteral("opened"), folder}}};
}

McpServer::Result McpServer::createFolder(const QJsonObject &arguments) const
{
    const Checked checked = check(arguments.value(QStringLiteral("path")), Check::New);
    if (!checked.error.isEmpty()) {
        return {.error = checked.error};
    }
    if (!QDir().mkdir(checked.path)) {
        return {.error = QStringLiteral("Could not create the folder %1.").arg(checked.path)};
    }
    return {.value = QJsonObject{{QStringLiteral("created"), checked.path}}};
}

McpServer::Result McpServer::writeTextFile(const QJsonObject &arguments) const
{
    const QJsonValue content = arguments.value(QStringLiteral("content"));
    if (!content.isString()) {
        return {.error = QStringLiteral("Text content is required.")};
    }
    const QByteArray data = content.toString().toUtf8();
    if (data.size() > MaximumWriteBytes) {
        return {.error = QStringLiteral("The text is larger than %1 bytes.").arg(MaximumWriteBytes)};
    }
    Checked target = check(arguments.value(QStringLiteral("path")), Check::Entry);
    if (!target.exists) {
        target = check(arguments.value(QStringLiteral("path")), Check::New);
    }
    if (!target.error.isEmpty()) {
        return {.error = target.error};
    }
    if (target.exists) {
        const QFileInfo info(target.path);
        if (info.isSymLink()) {
            return {.error = QStringLiteral("%1 is a symbolic link; write to the file it points to instead.").arg(target.path)};
        }
        if (!info.isFile()) {
            return {.error = QStringLiteral("%1 is not a file.").arg(target.path)};
        }
        if (!arguments.value(QStringLiteral("overwrite")).toBool()) {
            return {.error = QStringLiteral("%1 already exists. Pass overwrite: true to replace it.").arg(target.path)};
        }
    }
    QSaveFile file(target.path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        return {.error = QStringLiteral("Could not save %1: %2").arg(target.path, file.errorString())};
    }
    return {.value =
                QJsonObject{{QStringLiteral("path"), target.path}, {QStringLiteral("bytesWritten"), data.size()}, {QStringLiteral("replaced"), target.exists}}};
}

McpServer::Result McpServer::copy(const QJsonObject &arguments) const
{
    const Checked source = check(arguments.value(QStringLiteral("source")), Check::Follow);
    if (!source.error.isEmpty()) {
        return {.error = source.error};
    }
    const Checked destination = check(arguments.value(QStringLiteral("destination")), Check::New);
    if (!destination.error.isEmpty()) {
        return {.error = destination.error};
    }
    const QFileInfo info(source.path);
    if (info.isDir() && !info.isSymLink() && within(source.path, destination.path)) {
        return {.error = QStringLiteral("A folder can't be copied into itself.")};
    }
    if (containsProtected(source.path)) {
        return {.error = QStringLiteral("%1 contains a password-protected folder, so it can't be copied.").arg(source.path)};
    }
    QString error;
    if (!copyEntry(source.path, destination.path, &error)) {
        if (QFileInfo::exists(destination.path)) {
            error += QStringLiteral(" A partial copy remains at %1.").arg(destination.path);
        }
        return {.error = error};
    }
    return {.value = QJsonObject{{QStringLiteral("source"), source.path}, {QStringLiteral("destination"), destination.path}}};
}

McpServer::Result McpServer::move(const QJsonObject &arguments) const
{
    const Checked source = check(arguments.value(QStringLiteral("source")), Check::Entry, false);
    if (!source.error.isEmpty()) {
        return {.error = source.error};
    }
    const Checked destination = check(arguments.value(QStringLiteral("destination")), Check::New);
    if (!destination.error.isEmpty()) {
        return {.error = destination.error};
    }
    const QFileInfo info(source.path);
    if (info.isDir() && !info.isSymLink() && within(source.path, destination.path)) {
        return {.error = QStringLiteral("A folder can't be moved into itself.")};
    }
    if (containsProtected(source.path)) {
        return {.error = QStringLiteral("%1 contains a password-protected folder, so it can't be moved.").arg(source.path)};
    }
    if (!QDir().rename(source.path, destination.path)) {
        return {.error = QStringLiteral("Could not move %1 to %2. Moving between file systems isn't supported: copy the item, then move the "
                                        "original to the trash.")
                             .arg(source.path, destination.path)};
    }
    return {.value = QJsonObject{{QStringLiteral("source"), source.path}, {QStringLiteral("destination"), destination.path}}};
}

McpServer::Result McpServer::moveToTrash(const QJsonObject &arguments) const
{
    const Checked checked = check(arguments.value(QStringLiteral("path")), Check::Entry, false);
    if (!checked.error.isEmpty()) {
        return {.error = checked.error};
    }
    if (containsProtected(checked.path)) {
        return {.error = QStringLiteral("%1 contains a password-protected folder, so it can't be moved to the trash.").arg(checked.path)};
    }
    QString trashed;
    if (!QFile::moveToTrash(checked.path, &trashed)) {
        return {.error = QStringLiteral("Could not move %1 to the trash.").arg(checked.path)};
    }
    return {.value = QJsonObject{{QStringLiteral("path"), checked.path}, {QStringLiteral("trashPath"), trashed}}};
}
