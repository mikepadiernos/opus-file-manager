// SPDX-License-Identifier: GPL-3.0-or-later

#include "vaultsettings.h"
#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using VaultSettings::Dialect;
using VaultSettings::Options;

class VaultSettingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::self()->save();
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("EditorVault"));
        KSharedConfig::openStateConfig()->sync();
    }

    void readsTheGeneralOptions()
    {
        QCOMPARE(VaultSettings::general(), Options{});
        Settings::setEditorMarkdownMode(Settings::EnumEditorMarkdownMode::Visual);
        Settings::setEditorMarkdownPreviewOnOpen(true);
        Settings::setEditorMarkdownDialect(Settings::EnumEditorMarkdownDialect::CommonMark);
        Settings::setEditorWikiLinks(false);
        Settings::setEditorVaultBacklinks(false);
        Settings::setEditorVaultLinks(false);
        Settings::setEditorVaultTags(false);
        const Options options = VaultSettings::general();
        QVERIFY(options.visual);
        QVERIFY(options.preview);
        QCOMPARE(options.dialect, Dialect::CommonMark);
        QVERIFY(!options.wikiLinks);
        QVERIFY(!options.backlinks);
        QVERIFY(!options.links);
        QVERIFY(!options.tags);
    }

    void storesTheVaultFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(VaultSettings::vaultFolder(), QString());
        VaultSettings::setVaultFolder(dir.path() + QStringLiteral("/notes/../"));
        QCOMPARE(VaultSettings::vaultFolder(), QDir::cleanPath(dir.path()));
        QCOMPARE(Settings::editorVaultFolder(), QUrl::fromLocalFile(QDir::cleanPath(dir.path())));
        // It is saved, not only set.
        Settings::self()->load();
        QCOMPARE(VaultSettings::vaultFolder(), QDir::cleanPath(dir.path()));
        VaultSettings::setVaultFolder({});
        QCOMPARE(VaultSettings::vaultFolder(), QString());
        QVERIFY(Settings::editorVaultFolder().isEmpty());
    }

    void migratesTheRememberedVaultOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        KConfigGroup legacy = KSharedConfig::openStateConfig()->group(QStringLiteral("EditorVault"));
        legacy.writeEntry("Root", dir.path());
        QCOMPARE(VaultSettings::vaultFolder(), QDir::cleanPath(dir.path()));
        QVERIFY(!legacy.hasKey("Root"));
        // Closing the vault afterwards doesn't bring the old folder back.
        VaultSettings::setVaultFolder({});
        QCOMPARE(VaultSettings::vaultFolder(), QString());

        // A folder already in the setting wins over the remembered one.
        QTemporaryDir other;
        VaultSettings::setVaultFolder(other.path());
        legacy.writeEntry("Root", dir.path());
        QCOMPARE(VaultSettings::vaultFolder(), QDir::cleanPath(other.path()));
        QVERIFY(!legacy.hasKey("Root"));
    }

    void reportsUnavailableFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(VaultSettings::unavailableReason({}).isEmpty());
        QVERIFY(VaultSettings::unavailableReason(dir.path()).isEmpty());
        QVERIFY(VaultSettings::unavailableReason(dir.filePath(QStringLiteral("missing"))).contains(QStringLiteral("doesn't exist")));
        QFile file(dir.filePath(QStringLiteral("note.md")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(VaultSettings::unavailableReason(file.fileName()).contains(QStringLiteral("isn't a folder")));
    }

    void roundTripsOptionsAsJson()
    {
        Options options;
        options.visual = true;
        options.preview = true;
        options.dialect = Dialect::CommonMark;
        options.wikiLinks = false;
        options.backlinks = false;
        options.links = true;
        options.tags = false;
        QCOMPARE(VaultSettings::fromJson(VaultSettings::toJson(options)), options);
        // Missing and invalid values keep the fallback's.
        const QJsonObject partial{{QStringLiteral("tags"), false}, {QStringLiteral("links"), QStringLiteral("yes")}, {QStringLiteral("dialect"), 3}};
        const Options read = VaultSettings::fromJson(partial, options);
        QCOMPARE(read.dialect, Dialect::CommonMark);
        QVERIFY(read.links);
        QVERIFY(!read.tags);
        QVERIFY(read.visual);
        QCOMPARE(VaultSettings::fromJson({}), Options{});
    }
};

QTEST_GUILESS_MAIN(VaultSettingsTest)

#include "vaultsettingstest.moc"
