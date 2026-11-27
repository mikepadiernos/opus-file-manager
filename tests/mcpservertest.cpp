// SPDX-License-Identifier: GPL-3.0-or-later

#include "mcpserver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

class McpServerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_root;
    QString m_outside;
    QString m_log;

    QString path(const QString &relative) const
    {
        return m_root + QLatin1Char('/') + relative;
    }

    static void writeFile(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
    }

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    McpServer server(McpServer::Access access, std::function<QString(const QString &)> openFolder = {}) const
    {
        McpServer::Options options;
        options.access = access;
        options.folders = {m_root};
        options.auditLogPath = m_log;
        options.openFolder = std::move(openFolder);
        options.version = QStringLiteral("1.0");
        return McpServer(options);
    }

    static QJsonObject request(const QString &method, const QJsonObject &params = {}, int id = 1)
    {
        return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), id},
                {QStringLiteral("method"), method},
                {QStringLiteral("params"), params}};
    }

    static QJsonObject call(McpServer &server, const QString &tool, const QJsonObject &arguments)
    {
        return server.handle(request(QStringLiteral("tools/call"), {{QStringLiteral("name"), tool}, {QStringLiteral("arguments"), arguments}}))
            .value(QStringLiteral("result"))
            .toObject();
    }

    static bool isError(const QJsonObject &result)
    {
        return result.value(QStringLiteral("isError")).toBool();
    }

    static QString text(const QJsonObject &result, int index = 0)
    {
        return result.value(QStringLiteral("content")).toArray().at(index).toObject().value(QStringLiteral("text")).toString();
    }

    static QJsonObject structured(const QJsonObject &result)
    {
        return result.value(QStringLiteral("structuredContent")).toObject();
    }

    static QStringList toolNames(McpServer &server)
    {
        QStringList names;
        const QJsonArray tools =
            server.handle(request(QStringLiteral("tools/list"))).value(QStringLiteral("result")).toObject().value(QStringLiteral("tools")).toArray();
        for (const QJsonValue &tool : tools) {
            names.append(tool.toObject().value(QStringLiteral("name")).toString());
        }
        return names;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        // Under the test-mode data folder, so the trash used is the test one.
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QVERIFY(QDir().mkpath(base));
        m_dir = std::make_unique<QTemporaryDir>(base + QStringLiteral("/mcpservertest-XXXXXX"));
        QVERIFY(m_dir->isValid());
        const QString top = QDir(m_dir->path()).canonicalPath();
        m_root = top + QStringLiteral("/allowed");
        m_outside = top + QStringLiteral("/outside");
        m_log = top + QStringLiteral("/log/mcp-audit.log");
        QVERIFY(QDir().mkpath(m_root + QStringLiteral("/docs/nested")));
        QVERIFY(QDir().mkpath(m_outside));
        writeFile(path(QStringLiteral("docs/readme.md")), "# Hello\n");
        writeFile(path(QStringLiteral("docs/nested/notes.md")), "notes\n");
        writeFile(path(QStringLiteral(".hidden")), "secret\n");
        writeFile(m_outside + QStringLiteral("/private.txt"), "private\n");
    }

    void cleanup()
    {
        m_dir.reset();
    }

    void initializeNegotiatesTheProtocolVersion()
    {
        McpServer mcp = server(McpServer::Access::Read);
        QJsonObject result = mcp.handle(request(QStringLiteral("initialize"), {{QStringLiteral("protocolVersion"), QStringLiteral("2025-03-26")}}))
                                 .value(QStringLiteral("result"))
                                 .toObject();
        QCOMPARE(result.value(QStringLiteral("protocolVersion")).toString(), QStringLiteral("2025-03-26"));
        QCOMPARE(result.value(QStringLiteral("serverInfo")).toObject().value(QStringLiteral("version")).toString(), QStringLiteral("1.0"));
        QVERIFY(result.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("tools")));
        QVERIFY(result.value(QStringLiteral("instructions")).toString().contains(m_root));

        result = mcp.handle(request(QStringLiteral("initialize"), {{QStringLiteral("protocolVersion"), QStringLiteral("1999-01-01")}}))
                     .value(QStringLiteral("result"))
                     .toObject();
        QCOMPARE(result.value(QStringLiteral("protocolVersion")).toString(), McpServer::protocolVersions().constFirst());
    }

    void toolsDependOnAccess()
    {
        McpServer off = server(McpServer::Access::Off);
        QVERIFY(toolNames(off).isEmpty());
        QVERIFY(isError(call(off, QStringLiteral("list_folder"), {{QStringLiteral("path"), m_root}})));

        McpServer read = server(McpServer::Access::Read);
        QCOMPARE(toolNames(read),
                 (QStringList{QStringLiteral("list_folder"), QStringLiteral("file_info"), QStringLiteral("read_text_file"), QStringLiteral("search_files")}));

        McpServer write = server(McpServer::Access::Write, [](const QString &) {
            return QString();
        });
        const QStringList names = toolNames(write);
        QVERIFY(names.contains(QStringLiteral("open_in_plasma_opus")));
        QVERIFY(names.contains(QStringLiteral("write_text_file")));
        QVERIFY(names.contains(QStringLiteral("move_to_trash")));
        QVERIFY(!names.contains(QStringLiteral("delete")));
    }

    void handlesProtocolErrors()
    {
        McpServer mcp = server(McpServer::Access::Read);
        QJsonObject answer = QJsonDocument::fromJson(mcp.handleLine("{not json")).object();
        QCOMPARE(answer.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt(), -32700);
        QVERIFY(answer.value(QStringLiteral("id")).isNull());

        answer = mcp.handle(request(QStringLiteral("resources/list")));
        QCOMPARE(answer.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt(), -32601);

        answer = mcp.handle(request(QStringLiteral("tools/call"), {{QStringLiteral("name"), QStringLiteral("format_disk")}}));
        QCOMPARE(answer.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt(), -32602);

        // Notifications and responses get no answer.
        QVERIFY(mcp.handleLine(R"({"jsonrpc":"2.0","method":"notifications/initialized"})").isEmpty());
        QVERIFY(mcp.handleLine(R"({"jsonrpc":"2.0","id":4,"result":{}})").isEmpty());
        QVERIFY(mcp.handleLine("   ").isEmpty());

        const QJsonArray batch =
            QJsonDocument::fromJson(
                mcp.handleLine(
                    R"([{"jsonrpc":"2.0","id":1,"method":"ping"},{"jsonrpc":"2.0","method":"notifications/initialized"},{"jsonrpc":"2.0","id":2,"method":"tools/list"}])"))
                .array();
        QCOMPARE(batch.size(), 2);
        QCOMPARE(batch.at(0).toObject().value(QStringLiteral("id")).toInt(), 1);
        QCOMPARE(batch.at(1).toObject().value(QStringLiteral("id")).toInt(), 2);
    }

    void readsInsideTheFolders()
    {
        McpServer mcp = server(McpServer::Access::Read);
        QJsonObject result = call(mcp, QStringLiteral("list_folder"), {{QStringLiteral("path"), m_root}});
        QVERIFY(!isError(result));
        QJsonArray entries = structured(result).value(QStringLiteral("entries")).toArray();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("folder"));

        result = call(mcp,
                      QStringLiteral("list_folder"),
                      {{QStringLiteral("path"), QUrl::fromLocalFile(m_root).toString()}, {QStringLiteral("include_hidden"), true}});
        QCOMPARE(structured(result).value(QStringLiteral("entries")).toArray().size(), 2);

        result = call(mcp, QStringLiteral("file_info"), {{QStringLiteral("path"), path(QStringLiteral("docs/readme.md"))}});
        QCOMPARE(structured(result).value(QStringLiteral("size")).toInteger(), 8);
        QCOMPARE(structured(result).value(QStringLiteral("mimeType")).toString(), QStringLiteral("text/markdown"));

        result = call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("docs/readme.md"))}});
        QVERIFY(!isError(result));
        QCOMPARE(text(result), QStringLiteral("# Hello\n"));
        QCOMPARE(structured(result).value(QStringLiteral("truncated")).toBool(), false);

        result = call(mcp, QStringLiteral("search_files"), {{QStringLiteral("folder"), m_root}, {QStringLiteral("pattern"), QStringLiteral("*.MD")}});
        QCOMPARE(structured(result).value(QStringLiteral("results")).toArray().size(), 2);
        result = call(mcp, QStringLiteral("search_files"), {{QStringLiteral("folder"), m_root}, {QStringLiteral("pattern"), QStringLiteral("note")}});
        QCOMPARE(structured(result).value(QStringLiteral("results")).toArray().size(), 1);
    }

    void readsTextOnly()
    {
        McpServer mcp = server(McpServer::Access::Read);
        writeFile(path(QStringLiteral("binary.bin")), QByteArray("ab\0cd", 5));
        QVERIFY(isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("binary.bin"))}})));
        writeFile(path(QStringLiteral("latin1.txt")), "caf\xe9 au lait");
        QVERIFY(isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("latin1.txt"))}})));

        // A limit that cuts a character in half drops the partial character.
        writeFile(path(QStringLiteral("utf8.txt")), "a\xc3\xa9z");
        const QJsonObject result =
            call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("utf8.txt"))}, {QStringLiteral("max_bytes"), 2}});
        QVERIFY(!isError(result));
        QCOMPARE(text(result), QStringLiteral("a"));
        QVERIFY(structured(result).value(QStringLiteral("truncated")).toBool());
        QVERIFY(text(result, 1).contains(QStringLiteral("Cut off")));
    }

    void refusesPathsOutsideTheFolders()
    {
        McpServer mcp = server(McpServer::Access::Write);
        const QString outsideFile = m_outside + QStringLiteral("/private.txt");
        QVERIFY(isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), outsideFile}})));
        QVERIFY(isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("../outside/private.txt"))}})));
        QVERIFY(isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), QStringLiteral("docs/readme.md")}})));
        QVERIFY(isError(call(mcp, QStringLiteral("list_folder"), {{QStringLiteral("path"), QStringLiteral("smb://server/share")}})));

        // A link inside that points outside can't be followed or written through.
        QVERIFY(QFile::link(outsideFile, path(QStringLiteral("escape.txt"))));
        QVERIFY(QFile::link(m_outside, path(QStringLiteral("escape-folder"))));
        QJsonObject result = call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("escape.txt"))}});
        QVERIFY(isError(result));
        QVERIFY(text(result).contains(QStringLiteral("outside")));
        QVERIFY(isError(call(mcp, QStringLiteral("list_folder"), {{QStringLiteral("path"), path(QStringLiteral("escape-folder"))}})));
        QVERIFY(isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("escape-folder/private.txt"))}})));
        QVERIFY(isError(call(mcp,
                             QStringLiteral("write_text_file"),
                             {{QStringLiteral("path"), path(QStringLiteral("escape-folder/new.txt"))}, {QStringLiteral("content"), QStringLiteral("x")}})));
        QVERIFY(isError(call(mcp,
                             QStringLiteral("write_text_file"),
                             {{QStringLiteral("path"), path(QStringLiteral("escape.txt"))},
                              {QStringLiteral("content"), QStringLiteral("x")},
                              {QStringLiteral("overwrite"), true}})));
        QCOMPARE(readFile(outsideFile), QByteArray("private\n"));
        QVERIFY(
            isError(call(mcp,
                         QStringLiteral("copy"),
                         {{QStringLiteral("source"), path(QStringLiteral("escape.txt"))}, {QStringLiteral("destination"), path(QStringLiteral("copy.txt"))}})));

        // File information describes the link without revealing its target.
        result = call(mcp, QStringLiteral("file_info"), {{QStringLiteral("path"), path(QStringLiteral("escape.txt"))}});
        QVERIFY(!isError(result));
        QVERIFY(structured(result).value(QStringLiteral("targetOutsideFolders")).toBool());
        QVERIFY(!structured(result).contains(QStringLiteral("target")));

        // The folder itself can't be moved or trashed.
        QVERIFY(isError(call(mcp, QStringLiteral("move_to_trash"), {{QStringLiteral("path"), m_root}})));
        QVERIFY(isError(call(mcp,
                             QStringLiteral("move"),
                             {{QStringLiteral("source"), m_root}, {QStringLiteral("destination"), QString(m_root + QStringLiteral("-renamed"))}})));
        QVERIFY(QFileInfo::exists(m_root));
    }

    void readAccessRefusesChanges()
    {
        McpServer mcp = server(McpServer::Access::Read);
        const QJsonObject result = call(mcp, QStringLiteral("create_folder"), {{QStringLiteral("path"), path(QStringLiteral("new"))}});
        QVERIFY(isError(result));
        QVERIFY(text(result).contains(QStringLiteral("turned off")));
        QVERIFY(!QFileInfo::exists(path(QStringLiteral("new"))));
        QVERIFY(isError(call(mcp, QStringLiteral("move_to_trash"), {{QStringLiteral("path"), path(QStringLiteral("docs/readme.md"))}})));
        QVERIFY(QFileInfo::exists(path(QStringLiteral("docs/readme.md"))));
    }

    void writeToolsChangeFiles()
    {
        McpServer mcp = server(McpServer::Access::Write);
        QVERIFY(!isError(call(mcp, QStringLiteral("create_folder"), {{QStringLiteral("path"), path(QStringLiteral("drafts"))}})));
        QVERIFY(QFileInfo(path(QStringLiteral("drafts"))).isDir());
        QVERIFY(isError(call(mcp, QStringLiteral("create_folder"), {{QStringLiteral("path"), path(QStringLiteral("missing/child"))}})));

        const QString draft = path(QStringLiteral("drafts/plan.md"));
        QVERIFY(
            !isError(call(mcp, QStringLiteral("write_text_file"), {{QStringLiteral("path"), draft}, {QStringLiteral("content"), QStringLiteral("Plan é")}})));
        QCOMPARE(readFile(draft), QByteArray("Plan \xc3\xa9"));
        QJsonObject result =
            call(mcp, QStringLiteral("write_text_file"), {{QStringLiteral("path"), draft}, {QStringLiteral("content"), QStringLiteral("Replaced")}});
        QVERIFY(isError(result));
        QCOMPARE(readFile(draft), QByteArray("Plan \xc3\xa9"));
        result = call(mcp,
                      QStringLiteral("write_text_file"),
                      {{QStringLiteral("path"), draft}, {QStringLiteral("content"), QStringLiteral("Replaced")}, {QStringLiteral("overwrite"), true}});
        QVERIFY(!isError(result));
        QVERIFY(structured(result).value(QStringLiteral("replaced")).toBool());
        QCOMPARE(readFile(draft), QByteArray("Replaced"));

        QVERIFY(QFile::link(QStringLiteral("readme.md"), path(QStringLiteral("docs/link.md"))));
        QVERIFY(!isError(call(mcp,
                              QStringLiteral("copy"),
                              {{QStringLiteral("source"), path(QStringLiteral("docs"))}, {QStringLiteral("destination"), path(QStringLiteral("docs-copy"))}})));
        QCOMPARE(readFile(path(QStringLiteral("docs-copy/nested/notes.md"))), QByteArray("notes\n"));
        QVERIFY(QFileInfo(path(QStringLiteral("docs-copy/link.md"))).isSymLink());
        QVERIFY(isError(call(mcp,
                             QStringLiteral("copy"),
                             {{QStringLiteral("source"), path(QStringLiteral("docs"))}, {QStringLiteral("destination"), path(QStringLiteral("docs/inner"))}})));
        QVERIFY(isError(
            call(mcp, QStringLiteral("copy"), {{QStringLiteral("source"), draft}, {QStringLiteral("destination"), path(QStringLiteral("docs/readme.md"))}})));

        QVERIFY(
            !isError(call(mcp, QStringLiteral("move"), {{QStringLiteral("source"), draft}, {QStringLiteral("destination"), path(QStringLiteral("plan.md"))}})));
        QVERIFY(!QFileInfo::exists(draft));
        QCOMPARE(readFile(path(QStringLiteral("plan.md"))), QByteArray("Replaced"));
        QVERIFY(isError(call(
            mcp,
            QStringLiteral("move"),
            {{QStringLiteral("source"), path(QStringLiteral("plan.md"))}, {QStringLiteral("destination"), QString(m_outside + QStringLiteral("/plan.md"))}})));

        result = call(mcp, QStringLiteral("move_to_trash"), {{QStringLiteral("path"), path(QStringLiteral("plan.md"))}});
        QVERIFY2(!isError(result), qPrintable(text(result)));
        QVERIFY(!QFileInfo::exists(path(QStringLiteral("plan.md"))));
        const QString trashed = structured(result).value(QStringLiteral("trashPath")).toString();
        QVERIFY(trashed.startsWith(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)));
        QFile::remove(trashed);
    }

    void opensFoldersInPlasmaOpus()
    {
        QStringList opened;
        McpServer mcp = server(McpServer::Access::Read, [&opened](const QString &folder) {
            opened.append(folder);
            return QString();
        });
        QVERIFY(!isError(call(mcp, QStringLiteral("open_in_plasma_opus"), {{QStringLiteral("path"), path(QStringLiteral("docs/readme.md"))}})));
        QVERIFY(isError(call(mcp, QStringLiteral("open_in_plasma_opus"), {{QStringLiteral("path"), m_outside}})));
        QCOMPARE(opened, QStringList{path(QStringLiteral("docs"))});
    }

    void auditLogOmitsContents()
    {
        McpServer mcp = server(McpServer::Access::Write);
        call(mcp,
             QStringLiteral("write_text_file"),
             {{QStringLiteral("path"), path(QStringLiteral("secret.txt"))}, {QStringLiteral("content"), QStringLiteral("hunter2")}});
        call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("secret.txt"))}});
        call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), QString(m_outside + QStringLiteral("/private.txt"))}});

        const QList<QByteArray> lines = readFile(m_log).trimmed().split('\n');
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.at(0).contains(QByteArray("\twrite_text_file\tok\tpath=" + path(QStringLiteral("secret.txt")).toUtf8())));
        QVERIFY(lines.at(2).contains("\tread_text_file\terror: "));
        QVERIFY(!readFile(m_log).contains("hunter2"));
        QVERIFY(!readFile(m_log).contains("private\n"));
    }

    void protectedFoldersAreRefused()
    {
        McpServer::Options options;
        options.access = McpServer::Access::Write;
        options.folders = {m_root};
        options.protectedFolders = {path(QStringLiteral("docs/nested"))};
        McpServer mcp(options);
        QVERIFY(QFile::link(path(QStringLiteral("docs/nested")), path(QStringLiteral("shortcut"))));

        const auto refused = [&mcp](const QString &tool, const QJsonObject &arguments) {
            const QJsonObject result = call(mcp, tool, arguments);
            return isError(result) && text(result).contains(QStringLiteral("password-protected"));
        };
        QVERIFY(refused(QStringLiteral("list_folder"), {{QStringLiteral("path"), path(QStringLiteral("docs/nested"))}}));
        QVERIFY(refused(QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("docs/nested/notes.md"))}}));
        QVERIFY(refused(QStringLiteral("list_folder"), {{QStringLiteral("path"), path(QStringLiteral("shortcut"))}}));
        QVERIFY(refused(QStringLiteral("write_text_file"),
                        {{QStringLiteral("path"), path(QStringLiteral("docs/nested/new.md"))}, {QStringLiteral("content"), QStringLiteral("x")}}));
        QVERIFY(refused(QStringLiteral("copy"),
                        {{QStringLiteral("source"), path(QStringLiteral("docs"))}, {QStringLiteral("destination"), path(QStringLiteral("docs-copy"))}}));
        QVERIFY(refused(QStringLiteral("move"),
                        {{QStringLiteral("source"), path(QStringLiteral("docs"))}, {QStringLiteral("destination"), path(QStringLiteral("docs-moved"))}}));
        QVERIFY(refused(QStringLiteral("move_to_trash"), {{QStringLiteral("path"), path(QStringLiteral("docs"))}}));
        QVERIFY(QFileInfo::exists(path(QStringLiteral("docs/nested/notes.md"))));

        const QJsonObject result =
            call(mcp, QStringLiteral("search_files"), {{QStringLiteral("folder"), m_root}, {QStringLiteral("pattern"), QStringLiteral("*")}});
        QVERIFY(!isError(result));
        const QString found = QString::fromUtf8(QJsonDocument(structured(result)).toJson());
        QVERIFY(found.contains(QStringLiteral("readme.md")));
        QVERIFY(found.contains(QStringLiteral("docs/nested")));
        QVERIFY(!found.contains(QStringLiteral("notes.md")));
        QVERIFY(!isError(call(mcp, QStringLiteral("read_text_file"), {{QStringLiteral("path"), path(QStringLiteral("docs/readme.md"))}})));
    }
};

QTEST_GUILESS_MAIN(McpServerTest)
#include "mcpservertest.moc"
