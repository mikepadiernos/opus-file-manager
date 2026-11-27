// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexedsearch.h"
#include "embeddedmetadata.h"
#include "filetypeplaces.h"

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/IndexerConfig>
#include <Baloo/Query>
#include <Baloo/ResultIterator>
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <utility>

class IndexedSearchTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void rejectsUnsupportedInputAndCancellation()
    {
        QVERIFY(!IndexedSearch::find({}, QStringLiteral(" "), LiveSearch::Field::FileName).errors.isEmpty());
        QVERIFY(!IndexedSearch::find({}, QStringLiteral("a\"b"), LiveSearch::Field::FileName).errors.isEmpty());
        PathFilter filter;
        filter.maxDepth = 0;
        QVERIFY(!IndexedSearch::find({}, QStringLiteral("needle"), LiveSearch::Field::FileName, {}, {}, filter).errors.isEmpty());
        QVERIFY(IndexedSearch::find({}, QStringLiteral("needle"), LiveSearch::Field::FileName, [] {
                    return true;
                }).canceled);
        const auto broad = IndexedSearch::find({}, QStringLiteral("size>1M OR type:pdf"), LiveSearch::Field::Query);
        QCOMPARE(broad.errors.size(), 1);
        QVERIFY(broad.errors.first().contains(QStringLiteral("name, content or meta term")));
        QVERIFY(!IndexedSearch::find({}, QStringLiteral("report AND"), LiveSearch::Field::Query).errors.isEmpty());
        const auto quoted = IndexedSearch::find({}, QStringLiteral("\"two words\" type:pdf"), LiveSearch::Field::Query, [] {
            return true;
        });
        QVERIFY(quoted.errors.isEmpty());
        QVERIFY(quoted.canceled);
    }

    void buildsBalooSearchStrings()
    {
        SearchQuery::Term term;
        term.text = QStringLiteral("rep\"ort");
        QCOMPARE(IndexedSearch::balooSearchString(term, {}), QStringLiteral("filename:\"report\""));
        term.field = SearchQuery::Field::Content;
        QCOMPARE(IndexedSearch::balooSearchString(term, {}), QStringLiteral("content:\"report\""));
        term.field = SearchQuery::Field::Metadata;
        term.text = QStringLiteral("Canon");
        QCOMPARE(IndexedSearch::balooSearchString(term, {}), QStringLiteral("(\"Canon\")"));
        QVERIFY(EmbeddedMetadata::isText(QStringLiteral("manufacturer")));
        QVERIFY(!EmbeddedMetadata::isText(QStringLiteral("width")));
        QVERIFY(!EmbeddedMetadata::isText(QStringLiteral("photoDateTimeOriginal")));
        QVERIFY(!EmbeddedMetadata::isText(QStringLiteral("no-such-property")));
        QCOMPARE(IndexedSearch::balooSearchString(term, {QStringLiteral("manufacturer"), QStringLiteral("width"), QStringLiteral("title")}),
                 QStringLiteral("(\"Canon\" OR manufacturer:\"Canon\" OR title:\"Canon\")"));
    }

    void plansBalooQueries()
    {
        const auto planFor = [](const QString &text, bool scopes = true) {
            const auto query = SearchQuery::Query::parse(text, QDateTime(QDate(2026, 11, 26), QTime(12, 0)));
            return IndexedSearch::plan(&query, text, LiveSearch::Field::Query, {QStringLiteral("title")}, scopes);
        };
        QCOMPARE(IndexedSearch::plan(nullptr, QStringLiteral("report"), LiveSearch::Field::FileName, {}, true).searchString,
                 QStringLiteral("filename:\"report\""));
        const auto contents = IndexedSearch::plan(nullptr, QStringLiteral("invoice"), LiveSearch::Field::Contents, {}, true);
        QCOMPARE(contents.searchString, QStringLiteral("content:\"invoice\""));

        // A required content term selects candidates, as before; the name is checked in Plasma Opus.
        auto plan = planFor(QStringLiteral("report content:invoice"));
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.searchString, QStringLiteral("content:\"invoice\""));
        QCOMPARE(plan.decidedContent, QStringList{QStringLiteral("invoice")});
        QVERIFY(plan.contentLookups.isEmpty());

        // Content alternatives, types and dates narrow the name search inside Baloo.
        plan = planFor(QStringLiteral("report (content:a OR content:b) type:image/* modified>=2024-05"));
        QCOMPARE(plan.searchString, QStringLiteral("filename:\"report\" AND (content:\"a\" OR content:\"b\") AND modified>=2024-05-01"));
        QCOMPARE(plan.types, QStringList{QStringLiteral("Image")});
        QCOMPARE(plan.decidedContent, (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
        QVERIFY(plan.contentLookups.isEmpty());

        // Baloo has no NOT, so negated content is looked up separately.
        plan = planFor(QStringLiteral("content:a -content:b"));
        QCOMPARE(plan.searchString, QStringLiteral("content:\"a\""));
        QCOMPARE(plan.decidedContent, QStringList{QStringLiteral("a")});
        QCOMPARE(plan.contentLookups, QStringList{QStringLiteral("b")});

        // A term also used outside a passed alternative still needs its lookup.
        plan = planFor(QStringLiteral("(content:a OR content:b) (name:x OR content:a)"));
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.searchString, QStringLiteral("(content:\"a\" OR content:\"b\")"));
        QCOMPARE(plan.decidedContent, QStringList{QStringLiteral("b")});
        QCOMPARE(plan.contentLookups, QStringList{QStringLiteral("a")});

        // Content alternatives alone are now enough for an indexed query.
        plan = planFor(QStringLiteral("content:a OR content:b"));
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.searchString, QStringLiteral("(content:\"a\" OR content:\"b\")"));

        QCOMPARE(planFor(QStringLiteral("report modified:2024")).searchString,
                 QStringLiteral("filename:\"report\" AND modified>=2024-01-01 AND modified<2025-01-01"));
        QCOMPARE(planFor(QStringLiteral("report modified<2024-05-02")).searchString, QStringLiteral("filename:\"report\" AND modified<2024-05-02"));
        QCOMPARE(planFor(QStringLiteral("report modified<=2024-05")).searchString, QStringLiteral("filename:\"report\" AND modified<2024-06-01"));
        QCOMPARE(planFor(QStringLiteral("report modified>2024")).searchString, QStringLiteral("filename:\"report\" AND modified>=2025-01-01"));
        QCOMPARE(planFor(QStringLiteral("report modified:today")).searchString,
                 QStringLiteral("filename:\"report\" AND modified>=2026-11-26 AND modified<2026-11-27"));
        // Dates and types under OR or NOT are checked in Plasma Opus.
        QCOMPARE(planFor(QStringLiteral("report (modified:2024 OR type:folder)")).searchString, QStringLiteral("filename:\"report\""));
        QVERIFY(planFor(QStringLiteral("report (modified:2024 OR type:folder)")).types.isEmpty());

        // Extensions and plain files have no Baloo type.
        QCOMPARE(planFor(QStringLiteral("report type:folder type:audio/mpeg type:pdf type:file type:video/*")).types,
                 (QStringList{QStringLiteral("Folder"), QStringLiteral("Audio"), QStringLiteral("Video")}));

        plan = planFor(QStringLiteral("meta:Canon"));
        QCOMPARE(plan.searchString, QStringLiteral("(\"Canon\" OR title:\"Canon\")"));
        QVERIFY(plan.metadataSeed);
        QVERIFY(plan.metadataScope);
        QVERIFY(!planFor(QStringLiteral("meta:Canon"), false).metadataScope);
        QVERIFY(!planFor(QStringLiteral("report meta:Canon")).metadataSeed);
        QVERIFY(planFor(QStringLiteral("size>1M OR type:pdf")).error.contains(QStringLiteral("name, content or meta term")));
        QVERIFY(planFor(QStringLiteral("meta:2024")).error.contains(QStringLiteral("numbers or dates")));
    }

    void rejectsUnsearchableMetadataSeeds()
    {
        const auto canceled = [] {
            return true;
        };
        const auto numeric = IndexedSearch::find({}, QStringLiteral("meta:2024:05"), LiveSearch::Field::Query, canceled);
        QCOMPARE(numeric.errors.size(), 1);
        QVERIFY(numeric.errors.first().contains(QStringLiteral("numbers or dates")));
        const auto noKeys = IndexedSearch::find({}, QStringLiteral("meta:Canon"), LiveSearch::Field::Query, canceled, {}, {}, nullptr, nullptr, {}, {});
        QCOMPARE(noKeys.errors.size(), 1);
        QVERIFY(noKeys.errors.first().contains(QStringLiteral("No text metadata")));
        // Name terms are preferred, so numeric metadata can still narrow a name search.
        const auto named = IndexedSearch::find({}, QStringLiteral("report meta:2024"), LiveSearch::Field::Query, canceled);
        QVERIFY(named.errors.isEmpty());
        QVERIFY(named.canceled);
        const auto meta = IndexedSearch::find({}, QStringLiteral("meta:Canon type:jpg"), LiveSearch::Field::Query, canceled);
        QVERIFY(meta.errors.isEmpty());
        QVERIFY(meta.canceled);
    }

    void isolatedBaloo()
    {
#ifndef OPUSFM_HAVE_BALOO
        QVERIFY(!IndexedSearch::find({}, QStringLiteral("needle"), LiveSearch::Field::FileName).errors.isEmpty());
#else
        if (qEnvironmentVariableIsSet("OPUSFM_INDEX_FIXTURE")) {
            QSKIP("The fixture is run separately.");
        }
        const QString dbus = QStandardPaths::findExecutable(QStringLiteral("dbus-run-session"));
        const QString indexer =
            QStandardPaths::findExecutable(QStringLiteral("baloo_file"),
                                           {QStringLiteral("/usr/lib/kf6"), QStringLiteral("/usr/libexec/kf6"), QStringLiteral("/usr/libexec")});
        if (dbus.isEmpty() || indexer.isEmpty()) {
            QSKIP("Isolated Baloo integration requires dbus-run-session and baloo_file.");
        }
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("HOME"), temp.path());
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), temp.filePath(QStringLiteral("config")));
        env.insert(QStringLiteral("XDG_DATA_HOME"), temp.filePath(QStringLiteral("data")));
        env.insert(QStringLiteral("XDG_CACHE_HOME"), temp.filePath(QStringLiteral("cache")));
        env.insert(QStringLiteral("XDG_STATE_HOME"), temp.filePath(QStringLiteral("state")));
        env.insert(QStringLiteral("BALOO_DB_PATH"), temp.filePath(QStringLiteral("db")));
        env.insert(QStringLiteral("OPUSFM_INDEX_FIXTURE"), temp.path());
        env.insert(QStringLiteral("OPUSFM_BALOO_FILE"), indexer);
        QProcess child;
        child.setProcessEnvironment(env);
        child.start(dbus, {QCoreApplication::applicationFilePath(), QStringLiteral("balooFixture")});
        QVERIFY(child.waitForStarted());
        QVERIFY2(child.waitForFinished(45000), qPrintable(child.errorString()));
        QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0, child.readAllStandardOutput().constData());
#endif
    }

    void balooFixture()
    {
#ifndef OPUSFM_HAVE_BALOO
        QSKIP("Baloo is not available.");
#else
        const QString root = qEnvironmentVariable("OPUSFM_INDEX_FIXTURE");
        if (root.isEmpty()) {
            QSKIP("Only the isolated subprocess runs this fixture.");
        }
        const QString folder = root + QStringLiteral("/files");
        QVERIFY(QDir().mkpath(folder));
        const QString excluded = folder + QStringLiteral("/excluded");
        QVERIFY(QDir().mkpath(excluded));
        QVERIFY(QDir().mkpath(folder + QStringLiteral("/nested/deep")));
        QVERIFY(QDir().mkpath(qEnvironmentVariable("BALOO_DB_PATH")));
        QFile named(folder + QStringLiteral("/opalneedle.txt"));
        QVERIFY(named.open(QIODevice::WriteOnly));
        QCOMPARE(named.write("ordinary text"), 13);
        named.close();
        QFile nested(folder + QStringLiteral("/nested/opalneedle-nested.txt"));
        QVERIFY(nested.open(QIODevice::WriteOnly));
        nested.close();
        QFile deep(folder + QStringLiteral("/nested/deep/opalneedle-deep.txt"));
        QVERIFY(deep.open(QIODevice::WriteOnly));
        deep.close();
        QFile contents(folder + QStringLiteral("/other.txt"));
        QVERIFY(contents.open(QIODevice::WriteOnly));
        const QByteArray text("This is an ordinary UTF-8 text file.\nuniquekestrel is a searchable word.\n");
        QCOMPARE(contents.write(text), text.size());
        contents.close();
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::white);
        image.setText(QStringLiteral("Title"), QStringLiteral("zephyrquill sketch"));
        const QString picture = folder + QStringLiteral("/picture.png");
        QVERIFY(image.save(picture, "PNG"));
        const QString ebook = folder + QStringLiteral("/novel.epub");
        const QString package = folder + QStringLiteral("/tool.deb");
        // The indexer detects types by content, so give each file its format's signature.
        const QList<std::pair<QString, QByteArray>> signatures{
            {ebook, QByteArray("PK\x03\x04", 4) + QByteArray(26, '\0') + QByteArray("mimetypeapplication/epub+zip")},
            {package, QByteArray("!<arch>\ndebian-binary   ")},
        };
        for (const auto &[path, data] : signatures) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(data), data.size());
        }
        {
            Baloo::IndexerConfig config;
            config.setIncludeFolders({folder});
            config.setExcludeFolders({excluded});
            config.setFileIndexingEnabled(true);
            config.setOnlyBasicIndexing(false);
        }
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
        QProcess indexer;
        indexer.setProcessEnvironment(env);
        indexer.start(qEnvironmentVariable("OPUSFM_BALOO_FILE"), {});
        QVERIFY(indexer.waitForStarted());
        LiveSearch::Result names;
        QTRY_VERIFY_WITH_TIMEOUT(
            (names = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName)).paths.contains(named.fileName()),
            8000);
        QVERIFY(names.errors.isEmpty());
        QVERIFY(!names.paths.contains(contents.fileName()));
        QTRY_VERIFY_WITH_TIMEOUT(
            (names = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName)).paths.contains(deep.fileName()),
            8000);
        PathFilter filter;
        filter.parentText = QStringLiteral("nested");
        filter.excludedFolders = {folder + QStringLiteral("/nested/deep")};
        const auto filtered = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName, {}, {}, filter);
        QCOMPARE(filtered.paths, QStringList{nested.fileName()});
        filter.parentText.clear();
        filter.excludedFolders.clear();
        filter.maxDepth = 0;
        const auto shallow = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName, {}, {}, filter);
        QCOMPARE(shallow.paths, QStringList{named.fileName()});
        filter.maxDepth = 1;
        filter.pathText = QStringLiteral("nested");
        const auto limitedDepth = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName, {}, {}, filter);
        QCOMPARE(limitedDepth.paths, QStringList{nested.fileName()});
        filter.pathText.clear();
        filter.maxDepth = -1;
        const QStringList workspaceRoots{folder + QStringLiteral("/nested")};
        const auto scoped = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName, {}, {}, filter, &workspaceRoots);
        QVERIFY(!scoped.paths.contains(named.fileName()));
        QVERIFY(scoped.paths.contains(nested.fileName()));
        QVERIFY(scoped.paths.contains(deep.fileName()));
        filter.maxDepth = 0;
        const auto scopedShallow = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName, {}, {}, filter, &workspaceRoots);
        QCOMPARE(scopedShallow.paths, QStringList{nested.fileName()});
        LiveSearch::Result content;
        QTRY_VERIFY_WITH_TIMEOUT(
            (content = IndexedSearch::find(folder, QStringLiteral("uniquekestrel"), LiveSearch::Field::Contents)).paths.contains(contents.fileName()),
            8000);
        QVERIFY(content.errors.isEmpty());
        QVERIFY(!content.paths.contains(named.fileName()));
        const auto query = IndexedSearch::find(folder, QStringLiteral("opalneedle type:txt -path:deep -name:nested"), LiveSearch::Field::Query);
        QVERIFY(query.errors.isEmpty());
        QCOMPARE(query.paths, QStringList{named.fileName()});
        const auto contentQuery = IndexedSearch::find(folder, QStringLiteral("content:uniquekestrel name:other"), LiveSearch::Field::Query);
        QCOMPARE(contentQuery.paths, QStringList{contents.fileName()});
        QVERIFY(IndexedSearch::find(folder, QStringLiteral("opalneedle content:uniquekestrel"), LiveSearch::Field::Query).paths.isEmpty());
        const auto either = IndexedSearch::find(folder, QStringLiteral("txt (content:uniquekestrel OR name:nested)"), LiveSearch::Field::Query);
        QVERIFY2(either.paths.contains(contents.fileName()), qPrintable(either.paths.join(QLatin1Char(',')) + either.errors.join(QLatin1Char(','))));
        QVERIFY(either.paths.contains(nested.fileName()));
        QVERIFY(!either.paths.contains(named.fileName()));
        // Baloo applies content alternatives, dates and types itself.
        const auto alternatives = IndexedSearch::find(folder, QStringLiteral("content:uniquekestrel OR content:nosuchwordzz"), LiveSearch::Field::Query);
        QVERIFY2(alternatives.errors.isEmpty(), qPrintable(alternatives.errors.join(QLatin1Char(','))));
        QCOMPARE(alternatives.paths, QStringList{contents.fileName()});
        const auto recent = IndexedSearch::find(folder, QStringLiteral("opalneedle modified>=2000 -path:nested"), LiveSearch::Field::Query);
        QCOMPARE(recent.paths, QStringList{named.fileName()});
        QVERIFY(IndexedSearch::find(folder, QStringLiteral("opalneedle modified<2000"), LiveSearch::Field::Query).paths.isEmpty());
        const auto nestedFolders = IndexedSearch::find(folder, QStringLiteral("nested type:folder"), LiveSearch::Field::Query);
        QCOMPARE(nestedFolders.paths, QStringList{folder + QStringLiteral("/nested")});
        // Baloo finds nothing in an unindexed location, so the whole index is searched once, without duplicates.
        const QStringList mixedRoots{folder + QStringLiteral("/nested"), excluded};
        const auto mixed = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName, {}, {}, {}, &mixedRoots);
        QCOMPARE(QSet<QString>(mixed.paths.cbegin(), mixed.paths.cend()).size(), mixed.paths.size());
        QVERIFY(mixed.paths.contains(nested.fileName()));
        QVERIFY(mixed.paths.contains(deep.fileName()));
        QVERIFY(!mixed.paths.contains(named.fileName()));
        LiveSearch::Result metadata;
        QTRY_VERIFY_WITH_TIMEOUT((metadata = IndexedSearch::find(folder, QStringLiteral("meta:zephyrquill"), LiveSearch::Field::Query)).paths.contains(picture),
                                 8000);
        QVERIFY2(metadata.errors.isEmpty(), qPrintable(metadata.errors.join(QLatin1Char(','))));
        QCOMPARE(metadata.paths, QStringList{picture});
        QVERIFY(IndexedSearch::find(folder, QStringLiteral("meta:zephyrquill type:txt"), LiveSearch::Field::Query).paths.isEmpty());
        // Without the title among the enabled properties, the candidate is rejected.
        QVERIFY(IndexedSearch::
                    find(folder, QStringLiteral("meta:zephyrquill"), LiveSearch::Field::Query, {}, {}, {}, nullptr, nullptr, {}, {QStringLiteral("artist")})
                        .paths.isEmpty());
        // The file-type places' Baloo searches find their categories.
        const auto placeResults = [](FileTypePlaces::Category category) {
            Baloo::Query query = Baloo::Query::fromSearchUrl(FileTypePlaces::balooUrl(category));
            Baloo::ResultIterator it = query.exec();
            QStringList paths;
            while (it.next()) {
                paths.append(it.filePath());
            }
            return paths;
        };
        QTRY_VERIFY_WITH_TIMEOUT(placeResults(FileTypePlaces::Category::Ebooks).contains(ebook), 8000);
        QVERIFY(!placeResults(FileTypePlaces::Category::Ebooks).contains(package));
        QTRY_VERIFY_WITH_TIMEOUT(placeResults(FileTypePlaces::Category::Packages).contains(package), 8000);
        QVERIFY(!placeResults(FileTypePlaces::Category::Packages).contains(ebook));
        QTRY_VERIFY_WITH_TIMEOUT(placeResults(FileTypePlaces::Category::Images).contains(picture), 8000);
        QVERIFY(!placeResults(FileTypePlaces::Category::Images).contains(named.fileName()));
        const auto elsewhere = IndexedSearch::find(root + QStringLiteral("/missing"), QStringLiteral("opalneedle"), LiveSearch::Field::FileName);
        QVERIFY(!elsewhere.notices.isEmpty());
        QVERIFY(elsewhere.notices.first().contains(QStringLiteral("not configured")));
        const auto excludedResult = IndexedSearch::find(excluded, QStringLiteral("opalneedle"), LiveSearch::Field::FileName);
        QVERIFY(!excludedResult.notices.isEmpty());
        QVERIFY(excludedResult.notices.first().contains(QStringLiteral("not configured")));
        indexer.terminate();
        QVERIFY(indexer.waitForFinished(5000));
        {
            Baloo::IndexerConfig config;
            config.setFileIndexingEnabled(false);
        }
        const auto disabled = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName);
        QVERIFY(!disabled.errors.isEmpty());
        QVERIFY(disabled.errors.first().contains(QStringLiteral("disabled")));
        {
            Baloo::IndexerConfig config;
            config.setFileIndexingEnabled(true);
            config.setOnlyBasicIndexing(true);
        }
        const auto noContent = IndexedSearch::find(folder, QStringLiteral("uniquekestrel"), LiveSearch::Field::Contents);
        QVERIFY(!noContent.errors.isEmpty());
        QVERIFY(noContent.errors.first().contains(QStringLiteral("disabled")));
        const auto noContentQuery = IndexedSearch::find(folder, QStringLiteral("opalneedle content:x"), LiveSearch::Field::Query);
        QVERIFY(!noContentQuery.errors.isEmpty());
        QVERIFY(noContentQuery.errors.first().contains(QStringLiteral("disabled")));
        const auto noMetadata = IndexedSearch::find(folder, QStringLiteral("meta:Canon"), LiveSearch::Field::Query);
        QVERIFY(!noMetadata.errors.isEmpty());
        QVERIFY(noMetadata.errors.first().contains(QStringLiteral("file names only")));
        qputenv("BALOO_DB_PATH", (root + QStringLiteral("/missing-db")).toLocal8Bit());
        const auto noIndex = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName);
        QVERIFY(!noIndex.errors.isEmpty());
        QVERIFY(noIndex.errors.first().contains(QStringLiteral("missing")));
#endif
    }
};

QTEST_GUILESS_MAIN(IndexedSearchTest)
#include "indexedsearchtest.moc"
