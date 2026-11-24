// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexedsearch.h"

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/IndexerConfig>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

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
        const QString indexer = QStandardPaths::findExecutable(QStringLiteral("baloo_file"), {QStringLiteral("/usr/lib/kf6"), QStringLiteral("/usr/libexec")});
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
        qputenv("BALOO_DB_PATH", (root + QStringLiteral("/missing-db")).toLocal8Bit());
        const auto noIndex = IndexedSearch::find(folder, QStringLiteral("opalneedle"), LiveSearch::Field::FileName);
        QVERIFY(!noIndex.errors.isEmpty());
        QVERIFY(noIndex.errors.first().contains(QStringLiteral("missing")));
#endif
    }
};

QTEST_GUILESS_MAIN(IndexedSearchTest)
#include "indexedsearchtest.moc"
