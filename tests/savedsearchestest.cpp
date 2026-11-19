// SPDX-License-Identifier: GPL-3.0-or-later

#include "savedsearches.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SavedSearchesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void persistsCriteriaAndRejectsDuplicateOrInvalidSearches()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        const KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        QString error;
        QList<SavedSearches::Search> searches{
            {QStringLiteral("Local"), QStringLiteral("some text"), QUrl::fromLocalFile(dir.path()), LiveSearch::Field::Contents, false},
            {QStringLiteral("Index"), QStringLiteral("birds"), {}, LiveSearch::Field::FileName, true},
        };
        searches[0].filter = {QStringLiteral("path"), QStringLiteral("parent"), {dir.filePath(QStringLiteral("exclude"))}, 2};
        QVERIFY(SavedSearches::save(group, searches, &error));
        QVERIFY(error.isEmpty());
        config->reparseConfiguration();
        const auto loaded = SavedSearches::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.first().name, searches.first().name);
        QCOMPARE(loaded.first().text, searches.first().text);
        QCOMPARE(loaded.first().folder, searches.first().folder);
        QCOMPARE(loaded.first().field, LiveSearch::Field::Contents);
        QVERIFY(!loaded.first().indexed);
        QCOMPARE(loaded.first().filter.pathText, searches.first().filter.pathText);
        QCOMPARE(loaded.first().filter.parentText, searches.first().filter.parentText);
        QCOMPARE(loaded.first().filter.excludedFolders, searches.first().filter.excludedFolders);
        QCOMPARE(loaded.first().filter.maxDepth, 2);
        QVERIFY(loaded.last().indexed);
        auto duplicate = searches;
        duplicate.append({QStringLiteral(" local "), QStringLiteral("other"), {}, LiveSearch::Field::FileName, true});
        QVERIFY(!SavedSearches::save(group, duplicate, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(SavedSearches::load(group, &error).size(), 2);
        QVERIFY(!SavedSearches::save(group, {{QStringLiteral("bad"), QStringLiteral("term"), {}, LiveSearch::Field::FileName, false}}, &error));
    }

    void reportsCorruptConfiguration()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        group.writeEntry("Searches", QByteArray("{broken"));
        QString error;
        QVERIFY(SavedSearches::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void loadsOlderCriteriaAndRejectsMalformedFilters()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        group.writeEntry("Searches", QByteArray(R"([{"name":"old","text":"x","folder":"","field":"name","indexed":true}])"));
        QString error;
        const auto old = SavedSearches::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(old.size(), 1);
        QVERIFY(!old.first().filter.isActive());
        group.writeEntry(
            "Searches",
            QByteArray(R"([{"name":"bad","text":"x","folder":"","field":"name","indexed":true,"filter":{"path":"","parent":"","excluded":[],"depth":1.5}}])"));
        QVERIFY(SavedSearches::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SavedSearchesTest)
#include "savedsearchestest.moc"
