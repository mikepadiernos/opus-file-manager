// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsbackup.h"
#include "workspacecatalog.h"

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KTar>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SettingsBackupTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void backupPreviewExcludesCredentialsAndRestoreWaitsForStartup()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString configPath = root.filePath(QStringLiteral("opus-file-managerrc"));
        const QString statePath = root.filePath(QStringLiteral("opus-file-managerstaterc"));
        const auto config = KSharedConfig::openConfig(configPath, KConfig::SimpleConfig);
        const auto state = KSharedConfig::openConfig(statePath, KConfig::SimpleConfig);
        KConfigGroup preferences = config->group(QStringLiteral("Preferences"));
        preferences.writeEntry("Theme", QStringLiteral("dark"));
        preferences.writeEntry("RecentValues", QStringList{QStringLiteral("one"), QStringLiteral("two,three")});
        preferences.writeEntry("AccessToken", QStringLiteral("secret-token"));
        preferences.writeEntry("RemoteUrl", QStringLiteral("sftp://user:password@example.invalid/folder"));
        config->group(QStringLiteral("Credentials")).writeEntry("Password", QStringLiteral("secret"));
        config->group(QStringLiteral("CustomCommands")).writeEntry("Command", QStringLiteral("tool --password secret"));
        KConfigGroup sizes = state->group(QStringLiteral("SplitterSizes")).group(QStringLiteral("SideBySide"));
        sizes.writeEntry("Main", 320);
        state->group(QStringLiteral("Session")).writeEntry("RemoteTab", QStringLiteral("sftp://user:password@example.invalid/folder"));
        state->group(QStringLiteral("Flat")).writeEntry("Locations", QStringList{QStringLiteral("sftp://user:password@example.invalid/folder")});
        state->group(QStringLiteral("Panels")).writeEntry("FoldersExpanded", QStringList{QStringLiteral("/home/current-folder")});
        QVERIFY(config->sync());
        QVERIFY(state->sync());
        QCOMPARE(preferences.readEntry("AccessToken", QString()), QStringLiteral("secret-token"));

        const QString archivePath = root.filePath(QStringLiteral("settings.tar.gz"));
        QVERIFY2(SettingsBackup::create(archivePath, config, state).isEmpty(), "backup creation failed");
        SettingsBackup::Preview preview;
        QVERIFY2(SettingsBackup::inspect(archivePath, &preview).isEmpty(), "backup preview failed");
        QCOMPARE(preview.included.size(), 2);
        QCOMPARE(preview.excluded.size(), 3);

        KTar archive(archivePath, QStringLiteral("application/gzip"));
        QVERIFY(archive.open(QIODevice::ReadOnly));
        const KArchiveDirectory *directory = archive.directory();
        QVERIFY(directory);
        const auto *settingsFile = static_cast<const KArchiveFile *>(directory->entry(QStringLiteral("settings.ini")));
        const auto *layoutFile = static_cast<const KArchiveFile *>(directory->entry(QStringLiteral("layout.ini")));
        QVERIFY(settingsFile && layoutFile);
        const QByteArray settingsBytes = settingsFile->data();
        const QByteArray layoutBytes = layoutFile->data();
        QVERIFY2(settingsBytes.contains("Theme=dark"), settingsBytes.constData());
        QVERIFY(!settingsBytes.contains("AccessToken"));
        QVERIFY(!settingsBytes.contains("password@example.invalid"));
        QVERIFY(!settingsBytes.contains("CustomCommands"));
        QVERIFY(!settingsBytes.contains("Credentials"));
        QVERIFY(layoutBytes.contains("Main=320"));
        QVERIFY(!layoutBytes.contains("Session"));
        QVERIFY(!layoutBytes.contains("Flat"));
        QVERIFY(!layoutBytes.contains("current-folder"));
        QVERIFY(!layoutBytes.contains("password@example.invalid"));
        QVERIFY(archive.close());

        preferences.writeEntry("Theme", QStringLiteral("blue"));
        preferences.writeEntry("NewOnly", QStringLiteral("discard after restore"));
        sizes.writeEntry("Main", 640);
        state->group(QStringLiteral("Session")).writeEntry("RemoteTab", QStringLiteral("/current/session"));
        state->group(QStringLiteral("Flat")).writeEntry("Locations", QStringList{QStringLiteral("/current/flat")});
        state->group(QStringLiteral("Panels")).writeEntry("FoldersExpanded", QStringList{QStringLiteral("/current/expanded")});
        QVERIFY(config->sync());
        QVERIFY(state->sync());
        const QString dataDirectory = root.filePath(QStringLiteral("data"));
        QVERIFY(SettingsBackup::stageRestore(archivePath, dataDirectory).isEmpty());
        QCOMPARE(preferences.readEntry("Theme", QString()), QStringLiteral("blue"));
        KConfig diskConfig(configPath, KConfig::SimpleConfig);
        QCOMPARE(diskConfig.group(QStringLiteral("Preferences")).readEntry("AccessToken", QString()), QStringLiteral("secret-token"));

        QVERIFY2(SettingsBackup::applyPendingRestore(dataDirectory, config, state).isEmpty(), "restore application failed");
        QFile restoredConfigFile(configPath);
        QVERIFY(restoredConfigFile.open(QIODevice::ReadOnly));
        const QByteArray restoredConfigBytes = restoredConfigFile.readAll();
        QVERIFY2(restoredConfigBytes.contains("AccessToken=secret-token"), restoredConfigBytes.constData());
        QCOMPARE(preferences.readEntry("Theme", QString()), QStringLiteral("dark"));
        QCOMPARE(preferences.readEntry("RecentValues", QStringList()), QStringList({QStringLiteral("one"), QStringLiteral("two,three")}));
        QVERIFY(!preferences.hasKey(QStringLiteral("NewOnly")));
        QCOMPARE(preferences.readEntry("AccessToken", QString()), QStringLiteral("secret-token"));
        QCOMPARE(config->group(QStringLiteral("Credentials")).readEntry("Password", QString()), QStringLiteral("secret"));
        QCOMPARE(config->group(QStringLiteral("CustomCommands")).readEntry("Command", QString()), QStringLiteral("tool --password secret"));
        QCOMPARE(state->group(QStringLiteral("SplitterSizes")).group(QStringLiteral("SideBySide")).readEntry("Main", 0), 320);
        QCOMPARE(state->group(QStringLiteral("Session")).readEntry("RemoteTab", QString()), QStringLiteral("/current/session"));
        QCOMPARE(state->group(QStringLiteral("Flat")).readEntry("Locations", QStringList()), QStringList{QStringLiteral("/current/flat")});
        QCOMPARE(state->group(QStringLiteral("Panels")).readEntry("FoldersExpanded", QStringList()), QStringList{QStringLiteral("/current/expanded")});
        QVERIFY(!QFileInfo::exists(SettingsBackup::pendingRestorePath(dataDirectory)));
    }

    void rejectsUnsupportedArchiveWithoutStaging()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("not-a-backup.tar.gz"));
        QFile invalid(path);
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        QCOMPARE(invalid.write("not an archive"), qint64(14));
        invalid.close();
        SettingsBackup::Preview preview;
        QVERIFY(!SettingsBackup::inspect(path, &preview).isEmpty());
        QVERIFY(!SettingsBackup::stageRestore(path, root.filePath(QStringLiteral("data"))).isEmpty());
        QVERIFY(!QFileInfo::exists(SettingsBackup::pendingRestorePath(root.filePath(QStringLiteral("data")))));

        const QString credentialArchivePath = root.filePath(QStringLiteral("credential-backup.tar.gz"));
        KTar credentialArchive(credentialArchivePath, QStringLiteral("application/gzip"));
        QVERIFY(credentialArchive.open(QIODevice::WriteOnly));
        QVERIFY(credentialArchive.writeFile(QStringLiteral("settings.ini"), QByteArray("[Preferences]\nAccessToken=secret\n")));
        QVERIFY(credentialArchive.writeFile(QStringLiteral("layout.ini"), QByteArray("[WindowLayout]\nStacked=true\n")));
        QVERIFY(credentialArchive.writeFile(QStringLiteral("manifest.txt"), QByteArray("opus-settings-backup\n1\n")));
        QVERIFY(credentialArchive.close());
        QVERIFY(!SettingsBackup::inspect(credentialArchivePath, &preview).isEmpty());
        QVERIFY(!SettingsBackup::stageRestore(credentialArchivePath, root.filePath(QStringLiteral("data"))).isEmpty());
    }

    void exportsAndImportsSelectedWorkspaceProfilesWithoutCredentials()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto sourceConfig = KSharedConfig::openConfig(root.filePath(QStringLiteral("source")));
        const auto sourceState = KSharedConfig::openConfig(root.filePath(QStringLiteral("source-state")));
        sourceState->group(QStringLiteral("WindowLayout")).writeEntry("Stacked", true);
        WorkspaceCatalog::Catalog source;
        QString error;
        QVERIFY(WorkspaceCatalog::create(source, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(source, QStringLiteral("Other"), QStringLiteral("O"), error));
        const QString selectedId = source.workspaces.first().id;
        const QString otherId = source.workspaces.last().id;
        WindowSession session;
        session.left.tabs = {QUrl(QStringLiteral("sftp://user:password@example.invalid/folder?token=secret"))};
        session.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/tmp"))};
        const WorkspaceCatalog::Layout layout{QByteArray("window"), {200, 300}, {500, 100}, true, false, std::nullopt, session};
        QVERIFY(WorkspaceCatalog::setLayout(source, selectedId, layout, error));
        QVERIFY(WorkspaceCatalog::save(sourceConfig->group(QStringLiteral("Workspaces")), source, error));
        QVERIFY(sourceConfig->sync());

        const QString archivePath = root.filePath(QStringLiteral("workspaces.tar.gz"));
        const QString exportError = SettingsBackup::createWorkspaceProfiles(archivePath, sourceConfig, sourceState, {selectedId});
        QVERIFY2(exportError.isEmpty(), qPrintable(exportError));
        KTar archive(archivePath, QStringLiteral("application/gzip"));
        QVERIFY(archive.open(QIODevice::ReadOnly));
        const auto *settingsFile = static_cast<const KArchiveFile *>(archive.directory()->entry(QStringLiteral("settings.ini")));
        QVERIFY(settingsFile);
        const QByteArray settings = settingsFile->data();
        QVERIFY(settings.contains("Work"));
        QVERIFY(!settings.contains("Other"));
        QVERIFY(!settings.contains("password"));
        QVERIFY(!settings.contains("token=secret"));
        QVERIFY(archive.close());

        const auto targetConfig = KSharedConfig::openConfig(root.filePath(QStringLiteral("target")));
        WorkspaceCatalog::Catalog target;
        QVERIFY(WorkspaceCatalog::create(target, QStringLiteral("Work"), QStringLiteral("Existing"), error));
        const QString currentId = target.workspaces.first().id;
        QVERIFY(WorkspaceCatalog::activate(target, currentId, error));
        QVERIFY(WorkspaceCatalog::save(targetConfig->group(QStringLiteral("Workspaces")), target, error));
        QVERIFY(targetConfig->sync());
        targetConfig->group(QStringLiteral("Preferences")).writeEntry("Keep", true);
        QVERIFY(targetConfig->sync());

        SettingsBackup::WorkspaceProfilesPreview preview;
        QVERIFY2(SettingsBackup::inspectWorkspaceProfiles(archivePath, targetConfig, &preview).isEmpty(), "profile preview failed");
        QCOMPARE(preview.profiles.size(), 1);
        QCOMPARE(preview.conflicts.size(), 1);
        QStringList imported;
        QVERIFY2(SettingsBackup::importWorkspaceProfiles(archivePath, targetConfig, false, &imported).isEmpty(), "skip import failed");
        QVERIFY(imported.isEmpty());
        WorkspaceCatalog::Catalog unchanged;
        QVERIFY(WorkspaceCatalog::load(targetConfig->group(QStringLiteral("Workspaces")), unchanged, error));
        QCOMPARE(unchanged.workspaces.size(), 1);
        QCOMPARE(unchanged.activeId, currentId);

        QVERIFY2(SettingsBackup::importWorkspaceProfiles(archivePath, targetConfig, true, &imported).isEmpty(), "replace import failed");
        QCOMPARE(imported, QStringList{QStringLiteral("Work")});
        WorkspaceCatalog::Catalog restored;
        QVERIFY(WorkspaceCatalog::load(targetConfig->group(QStringLiteral("Workspaces")), restored, error));
        QCOMPARE(restored.workspaces.size(), 1);
        QCOMPARE(restored.activeId, currentId);
        QCOMPARE(restored.workspaces.first().id, currentId);
        QCOMPARE(restored.workspaces.first().layout->session->left.tabs.first().toString(), QStringLiteral("sftp://example.invalid/folder"));
        QVERIFY(targetConfig->group(QStringLiteral("Preferences")).readEntry("Keep", false));
        QVERIFY(!restored.workspaces.first().id.isEmpty());
        QVERIFY(otherId != selectedId);
    }
};

QTEST_MAIN(SettingsBackupTest)
#include "settingsbackuptest.moc"
