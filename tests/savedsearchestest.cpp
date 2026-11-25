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
        KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        QString error;
        QList<SavedSearches::Search> searches{
            {QStringLiteral("Local"), QStringLiteral("some text"), QUrl::fromLocalFile(dir.path()), LiveSearch::Field::Contents, false, {}, {}},
            {QStringLiteral("Index"), QStringLiteral("birds"), {}, LiveSearch::Field::FileName, true, {}, {}},
        };
        searches[0].filter = {QStringLiteral("path"), QStringLiteral("parent"), {dir.filePath(QStringLiteral("exclude"))}, 2};
        searches[1].filter.pathText = QStringLiteral("birds");
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
        QCOMPARE(loaded.last().filter.pathText, QStringLiteral("birds"));
        auto duplicate = searches;
        duplicate.append({QStringLiteral(" local "), QStringLiteral("other"), {}, LiveSearch::Field::FileName, true, {}, {}});
        QVERIFY(!SavedSearches::save(group, duplicate, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(SavedSearches::load(group, &error).size(), 2);
        QVERIFY(!SavedSearches::save(group, {{QStringLiteral("bad"), QStringLiteral("term"), {}, LiveSearch::Field::FileName, false, {}, {}}}, &error));
    }

    void persistsAdvancedQueries()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        QString error;
        const QList<SavedSearches::Search> searches{
            {QStringLiteral("Query"), QStringLiteral("report type:pdf OR content:\"two words\""), {}, LiveSearch::Field::Query, true, {}, {}},
        };
        QVERIFY(SavedSearches::save(group, searches, &error));
        QVERIFY(group.readEntry("Searches", QByteArray()).contains("\"field\":\"query\""));
        config->reparseConfiguration();
        const auto loaded = SavedSearches::load(group, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().field, LiveSearch::Field::Query);
        QCOMPARE(loaded.first().text, searches.first().text);
        group.writeEntry("Searches", QByteArray(R"([{"name":"x","text":"y","folder":"","field":"other","indexed":true}])"));
        QVERIFY(SavedSearches::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
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

    void assignsAndPersistsStableIdsForLegacySearches()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        group.writeEntry(
            "Searches",
            QByteArray(
                R"([{"name":"old","text":"x","folder":"","field":"name","indexed":true},{"name":"other","text":"y","folder":"","field":"name","indexed":true}])"));
        QString error;
        auto searches = SavedSearches::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(searches.size(), 2);
        QVERIFY(!searches.first().id.isEmpty());
        QVERIFY(searches.first().id != searches.last().id);
        const QString stableId = searches.first().id;
        searches.first().name = QStringLiteral("renamed");
        QVERIFY(SavedSearches::save(group, searches, &error));
        config->reparseConfiguration();
        searches = SavedSearches::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(searches.first().name, QStringLiteral("renamed"));
        QCOMPARE(searches.first().id, stableId);
        QVERIFY(SavedSearches::save(group, {}, &error));
        QVERIFY(SavedSearches::save(group, {{QStringLiteral("renamed"), QStringLiteral("x"), {}, LiveSearch::Field::FileName, true, {}, {}}}, &error));
        config->reparseConfiguration();
        searches = SavedSearches::load(group, &error);
        QVERIFY(error.isEmpty());
        QVERIFY(searches.first().id != stableId);
    }

    void persistsSearchFoldersScope()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        KConfigGroup group = config->group(QStringLiteral("SavedSearches"));
        SavedSearches::Search search{QStringLiteral("Folders"), QStringLiteral("notes"), {}, LiveSearch::Field::FileName, false, {}, {}, true};
        search.filter.maxDepth = 1;
        QString error;
        QVERIFY(SavedSearches::save(group, {search}, &error));
        QVERIFY(group.readEntry("Searches", QByteArray()).contains("\"searchFolders\":true"));
        config->reparseConfiguration();
        const auto loaded = SavedSearches::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 1);
        QVERIFY(loaded.first().searchFolders);
        QVERIFY(!loaded.first().indexed);
        QCOMPARE(loaded.first().filter.maxDepth, 1);

        search.indexed = true;
        QVERIFY(!SavedSearches::save(group, {search}, &error));
        group.writeEntry("Searches", QByteArray(R"([{"name":"bad","text":"x","folder":"","field":"name","indexed":false,"searchFolders":"yes"}])"));
        QVERIFY(SavedSearches::load(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SavedSearchesTest)
#include "savedsearchestest.moc"
