// SPDX-License-Identifier: GPL-3.0-or-later

#include "mcpsettings.h"
#include "appidentity.h"
#include "foldergates.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class McpSettingsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_root;

    QString makeFolder(const QString &relative)
    {
        const QString path = m_root + u'/' + relative;
        QDir().mkpath(path);
        return path;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_root = QFileInfo(m_dir.path()).canonicalFilePath();
    }

    void auditLogIsInPlasmaOpusStateFolder()
    {
        const QString path = McpSettings::auditLogPath();
        QCOMPARE(QFileInfo(path).fileName(), QStringLiteral("mcp-audit.log"));
        QCOMPARE(QFileInfo(QFileInfo(path).path()).fileName(), AppIdentity::componentName());
        QVERIFY(path.startsWith(QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u'/'));
    }

    void emptyFoldersMeanHome()
    {
        const McpSettings::Folders folders = McpSettings::resolveFolders({}, {});
        QCOMPARE(folders.used, QStringList{QFileInfo(QDir::homePath()).canonicalFilePath()});
        QVERIFY(folders.skipped.isEmpty());
        QVERIFY(folders.gated.isEmpty());
    }

    void reportsSkippedAndGatedFolders()
    {
        const QString work = makeFolder(QStringLiteral("work"));
        const QString secret = makeFolder(QStringLiteral("secret"));
        const QString inner = makeFolder(QStringLiteral("secret/inner"));
        const QString secretive = makeFolder(QStringLiteral("secretive"));
        const QString missing = m_root + QStringLiteral("/missing");
        QFile file(m_root + QStringLiteral("/file.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const McpSettings::Folders folders =
            McpSettings::resolveFolders({work, work + u'/', missing, file.fileName(), QStringLiteral("relative"), secret, inner, secretive}, {secret});
        QCOMPARE(folders.used, (QStringList{work, secret, inner, secretive}));
        QCOMPARE(folders.skipped, (QStringList{missing, file.fileName(), QStringLiteral("relative")}));
        QCOMPARE(folders.gated, (QStringList{secret, inner}));
    }

    void clientConfigurationIsJson()
    {
        const QJsonObject server = QJsonDocument::fromJson(McpSettings::clientConfiguration().toUtf8())
                                       .object()
                                       .value(QStringLiteral("mcpServers"))
                                       .toObject()
                                       .value(QStringLiteral("plasma-opus"))
                                       .toObject();
        QCOMPARE(server.value(QStringLiteral("command")).toString(), QStringLiteral("opus-file-manager-mcp"));
    }

    // The server reads the settings again for each message, so Preferences apply to a running server.
    void runningServerFollowsSettings()
    {
        const QString home = makeFolder(QStringLiteral("server/home"));
        const QString config = makeFolder(QStringLiteral("server/config"));
        const QString state = makeFolder(QStringLiteral("server/state"));
        const QString work = makeFolder(QStringLiteral("server/home/work"));
        const QString rcPath = config + QStringLiteral("/opus-file-managerrc");
        const auto write = [&rcPath](const QString &key, const QString &value) {
            const KSharedConfigPtr rc = KSharedConfig::openConfig(rcPath, KConfig::SimpleConfig);
            rc->reparseConfiguration();
            rc->group(QStringLiteral("Mcp")).writeEntry(key, value);
            QVERIFY(rc->sync());
        };
        write(QStringLiteral("McpAccess"), QStringLiteral("Off"));

        QProcess server;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), home);
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        environment.insert(QStringLiteral("XDG_STATE_HOME"), state);
        server.setProcessEnvironment(environment);
        server.start(QStringLiteral(OPUSFM_MCP_EXECUTABLE), {});
        QVERIFY(server.waitForStarted());
        int id = 0;
        const auto call = [&server, &id](const QString &method, const QJsonObject &params = {}) {
            QJsonObject message{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), ++id}, {QStringLiteral("method"), method}};
            if (!params.isEmpty()) {
                message.insert(QStringLiteral("params"), params);
            }
            server.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
            while (!server.canReadLine()) {
                if (!server.waitForReadyRead(10000)) {
                    return QJsonObject();
                }
            }
            return QJsonDocument::fromJson(server.readLine()).object().value(QStringLiteral("result")).toObject();
        };
        const auto toolNames = [&call] {
            QStringList names;
            const QJsonArray tools = call(QStringLiteral("tools/list")).value(QStringLiteral("tools")).toArray();
            for (const QJsonValue &tool : tools) {
                names.append(tool.toObject().value(QStringLiteral("name")).toString());
            }
            return names;
        };
        const auto listFolder = [&call](const QString &path) {
            return call(QStringLiteral("tools/call"),
                        {{QStringLiteral("name"), QStringLiteral("list_folder")}, {QStringLiteral("arguments"), QJsonObject{{QStringLiteral("path"), path}}}});
        };

        QVERIFY(toolNames().isEmpty());

        write(QStringLiteral("McpAccess"), QStringLiteral("Write"));
        write(QStringLiteral("McpFolders"), work);
        QVERIFY(toolNames().contains(QStringLiteral("write_text_file")));
        QVERIFY(!listFolder(work).value(QStringLiteral("isError")).toBool());
        QVERIFY(listFolder(home).value(QStringLiteral("isError")).toBool());
        const QString log = state + u'/' + AppIdentity::componentName() + QStringLiteral("/mcp-audit.log");
        QVERIFY(QFileInfo::exists(log));

        write(QStringLiteral("McpAccess"), QStringLiteral("Read"));
        QVERIFY(toolNames().contains(QStringLiteral("list_folder")));
        QVERIFY(!toolNames().contains(QStringLiteral("write_text_file")));

        write(QStringLiteral("McpAuditLog"), QStringLiteral("false"));
        const qint64 logSize = QFileInfo(log).size();
        QVERIFY(!listFolder(work).value(QStringLiteral("isError")).toBool());
        QCOMPARE(QFileInfo(log).size(), logSize);

        {
            FolderGates gates(KSharedConfig::openConfig(rcPath, KConfig::SimpleConfig));
            gates.setIterations(10);
            QCOMPARE(gates.protect(work, QStringLiteral("secret")), QString());
        }
        QVERIFY(listFolder(work).value(QStringLiteral("isError")).toBool());

        server.closeWriteChannel();
        QVERIFY(server.waitForFinished());
    }
};

QTEST_GUILESS_MAIN(McpSettingsTest)
#include "mcpsettingstest.moc"
