// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersyncprofiles.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class FolderSyncProfilesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void roundTripsAndRejectsBadData()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("profilesrc")), KConfig::SimpleConfig);
        auto group = config->group(QStringLiteral("FolderSyncProfiles"));
        FolderSyncProfiles::Profile profile;
        profile.name = QStringLiteral("Photos");
        profile.options.includePatterns = {QStringLiteral("*.jpg"), QStringLiteral("*.png")};
        profile.options.excludePatterns = {QStringLiteral("cache")};
        profile.options.recursive = false;
        profile.options.compareContents = true;
        profile.options.compareSymlinks = true;
        QString error;
        QVERIFY(FolderSyncProfiles::save(group, {profile}, &error));
        config->reparseConfiguration();
        const auto loaded = FolderSyncProfiles::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().name, profile.name);
        QCOMPARE(loaded.first().options.includePatterns, profile.options.includePatterns);
        QCOMPARE(loaded.first().options.excludePatterns, profile.options.excludePatterns);
        QVERIFY(!loaded.first().options.recursive);
        QVERIFY(loaded.first().options.compareContents);
        QVERIFY(loaded.first().options.compareSymlinks);
        profile.name = QStringLiteral(" photos ");
        QVERIFY(!FolderSyncProfiles::save(group, {loaded.first(), profile}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(FolderSyncProfiles::load(group, &error).size(), 1);
        group.writeEntry("Profiles", QByteArray("[{\"name\":\"bad\",\"include\":[3],\"exclude\":[],\"recursive\":true,\"contents\":false,\"links\":false}]"));
        QVERIFY(FolderSyncProfiles::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FolderSyncProfilesTest)
#include "foldersyncprofilestest.moc"
