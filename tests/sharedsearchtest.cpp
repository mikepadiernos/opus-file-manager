// SPDX-License-Identifier: GPL-3.0-or-later

#include "sharedsearch.h"

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/Query>
#endif

class SharedSearchTest : public QObject
{
    Q_OBJECT

private:
    static SavedSearches::Search search(const QString &text, LiveSearch::Field field = LiveSearch::Field::Query)
    {
        SavedSearches::Search result;
        result.name = QStringLiteral("Shared");
        result.text = text;
        result.field = field;
        result.folder = QUrl::fromLocalFile(QStringLiteral("/home/user/Documents"));
        return result;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void translatesSupportedCriteria()
    {
        const auto translation =
            SharedSearch::translate(search(QStringLiteral("report content:invoice type:image/* modified>=2024-01-01 size>1M NOT tag:x")), {}, {}, {});
        QVERIFY(translation.error.isEmpty());
        QCOMPARE(translation.searchString, QStringLiteral("filename:\"report\" AND content:\"invoice\" AND (modified>=2024-01-01)"));
        QCOMPARE(translation.types, QStringList{QStringLiteral("Image")});
        QCOMPARE(translation.folder, QStringLiteral("/home/user/Documents"));
        QCOMPARE(translation.omitted.size(), 2);
        QVERIFY(translation.omitted.at(0).startsWith(QStringLiteral("size>")));
        QVERIFY(translation.omitted.at(1).startsWith(QStringLiteral("NOT tag:")));
    }

    void translatesAlternativesOnlyWhenComplete()
    {
        auto translation = SharedSearch::translate(search(QStringLiteral("content:a OR (name:b AND modified:2024) OR type:text/plain")), {}, {}, {});
        QCOMPARE(translation.searchString,
                 QStringLiteral("(content:\"a\" OR (filename:\"b\" AND (modified>=2024-01-01 AND modified<2025-01-01)) OR mimetype:\"text/plain\")"));
        QVERIFY(translation.omitted.isEmpty());

        translation = SharedSearch::translate(search(QStringLiteral("content:a (name:b OR size>1M)")), {}, {}, {});
        QCOMPARE(translation.searchString, QStringLiteral("content:\"a\""));
        QCOMPARE(translation.omitted.size(), 1);

        translation = SharedSearch::translate(search(QStringLiteral("content:a modified:today type:folder")), {}, {}, {});
        QCOMPARE(translation.searchString, QStringLiteral("content:\"a\""));
        QCOMPARE(translation.types, QStringList{QStringLiteral("Folder")});
        QCOMPARE(translation.omitted, QStringList{QStringLiteral("modified=\"today\"")});
    }

    void refusesSearchesBalooCantExpress()
    {
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("size>1M")), {}, {}, {}).error.isEmpty());
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("*.pdf")), {}, {}, {}).error.isEmpty());
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("name=report.pdf")), {}, {}, {}).error.isEmpty());
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("meta:2024")), {}, {}, {}).error.isEmpty());
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("re*rt"), LiveSearch::Field::FileName), {}, {}, {}).error.isEmpty());
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("  ")), {}, {}, {}).error.isEmpty());
        QVERIFY(!SharedSearch::translate(search(QStringLiteral("(")), {}, {}, {}).error.isEmpty());

        auto remote = search(QStringLiteral("report"));
        remote.folder = QUrl(QStringLiteral("sftp://example.invalid/home"));
        QVERIFY(!SharedSearch::translate(remote, {}, {}, {}).error.isEmpty());
        // An unlimited indexed search covers the whole index, so the folder doesn't matter.
        remote.indexed = true;
        const auto indexed = SharedSearch::translate(remote, {}, {}, {});
        QVERIFY(indexed.error.isEmpty());
        QVERIFY(indexed.folder.isEmpty());
    }

    void translatesPlainFields()
    {
        auto plain = search(QStringLiteral("report"), LiveSearch::Field::FileName);
        QCOMPARE(SharedSearch::translate(plain, {}, {}, {}).searchString, QStringLiteral("filename:\"report\""));
        plain.field = LiveSearch::Field::Contents;
        QCOMPARE(SharedSearch::translate(plain, {}, {}, {}).searchString, QStringLiteral("content:\"report\""));
        auto meta = search(QStringLiteral("meta:Canon"));
        QCOMPARE(SharedSearch::translate(meta, {}, {}, {QStringLiteral("title")}).searchString, QStringLiteral("(\"Canon\" OR title:\"Canon\")"));
    }

    void reportsLocationsAndFilters()
    {
        auto folders = search(QStringLiteral("report"));
        folders.searchFolders = true;
        QVERIFY(!SharedSearch::translate(folders, {}, {}, {}).error.isEmpty());
        QCOMPARE(SharedSearch::translate(folders, {QStringLiteral("/srv/work/")}, {}, {}).folder, QStringLiteral("/srv/work"));
        const auto several = SharedSearch::translate(folders, {QStringLiteral("/srv/a"), QStringLiteral("/srv/b")}, {}, {});
        QVERIFY(several.folder.isEmpty());
        QCOMPARE(several.omitted.size(), 1);
        QVERIFY(several.omitted.first().contains(QStringLiteral("/srv/a, /srv/b")));

        auto filtered = search(QStringLiteral("report"));
        filtered.indexed = true;
        filtered.filter.maxDepth = 2;
        filtered.filter.pathText = QStringLiteral("src");
        filtered.filter.parentText = QStringLiteral("lib");
        filtered.filter.excludedFolders = {QStringLiteral("/home/user/Documents/old")};
        const auto translation = SharedSearch::translate(filtered, {}, {QStringLiteral("/home/user/Documents/old/"), QStringLiteral("/tmp")}, {});
        QCOMPARE(translation.folder, QStringLiteral("/home/user/Documents"));
        QCOMPARE(translation.omitted.size(), 4);
        QVERIFY(translation.omitted.at(0).contains(QLatin1Char('2')));
        QVERIFY(translation.omitted.at(1).contains(QStringLiteral("src")));
        QVERIFY(translation.omitted.at(2).contains(QStringLiteral("lib")));
        QVERIFY(translation.omitted.at(3).endsWith(QStringLiteral("/home/user/Documents/old, /tmp")));
    }

    void buildsSearchUrls()
    {
        SharedSearch::Translation failed;
        failed.error = QStringLiteral("no");
        QVERIFY(SharedSearch::url(failed, QStringLiteral("Title")).isEmpty());
        const auto translation = SharedSearch::translate(search(QStringLiteral("content:invoice type:audio/*")), {}, {}, {});
        const QUrl url = SharedSearch::url(translation, QStringLiteral("Invoices"));
#ifndef OPUSFM_HAVE_BALOO
        QVERIFY(url.isEmpty());
#else
        QCOMPARE(url.scheme(), QStringLiteral("baloosearch"));
        QCOMPARE(Baloo::Query::titleFromQueryUrl(url), QStringLiteral("Invoices"));
        const Baloo::Query query = Baloo::Query::fromSearchUrl(url);
        QCOMPARE(query.searchString(), QStringLiteral("content:\"invoice\""));
        QCOMPARE(query.types(), QStringList{QStringLiteral("Audio")});
        QCOMPARE(query.includeFolder(), QStringLiteral("/home/user/Documents"));
        QVERIFY(SharedSearch::sameSearch(url, QUrl(url.toString(QUrl::FullyEncoded).replace(QStringLiteral(":%5C"), QStringLiteral("%3A%5C")))));
        QVERIFY(!SharedSearch::sameSearch(url, SharedSearch::url(translation, QStringLiteral("Other"))));
        QVERIFY(!SharedSearch::sameSearch(url, QUrl::fromLocalFile(QStringLiteral("/tmp"))));
#endif
    }
};

QTEST_GUILESS_MAIN(SharedSearchTest)

#include "sharedsearchtest.moc"
