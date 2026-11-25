// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchlocations.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SearchLocationsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void normalizesFolders()
    {
        QCOMPARE(
            SearchLocations::normalized({QStringLiteral(" /a/b/ "), QString(), QStringLiteral("relative"), QStringLiteral("/a/b"), QStringLiteral("/a/./c")}),
            QStringList({QStringLiteral("/a/b"), QStringLiteral("/a/c")}));
    }

    void matchesExcludedFoldersAndTheirContents()
    {
        const QStringList exclusions{QStringLiteral("/data/cache/"), QStringLiteral("relative")};
        QVERIFY(SearchLocations::isExcluded(QStringLiteral("/data/cache"), exclusions));
        QVERIFY(SearchLocations::isExcluded(QStringLiteral("/data/cache/a/b.txt"), exclusions));
        QVERIFY(!SearchLocations::isExcluded(QStringLiteral("/data/cached"), exclusions));
        QVERIFY(!SearchLocations::isExcluded(QStringLiteral("/data"), exclusions));
        QVERIFY(!SearchLocations::isExcluded(QStringLiteral("relative"), exclusions));
        QVERIFY(SearchLocations::isExcluded(QStringLiteral("/anything"), {QStringLiteral("/")}));
    }

    void choosesSearchableRoots()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QDir root(dir.path());
        QVERIFY(root.mkpath(QStringLiteral("docs/nested")));
        QVERIFY(root.mkpath(QStringLiteral("music")));
        QVERIFY(root.mkpath(QStringLiteral("cache")));
        QVERIFY(QFile::link(root.filePath(QStringLiteral("music")), root.filePath(QStringLiteral("link"))));
        QFile file(root.filePath(QStringLiteral("file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        QStringList errors;
        const QStringList roots = SearchLocations::searchRoots({root.filePath(QStringLiteral("docs/nested")),
                                                                root.filePath(QStringLiteral("docs")),
                                                                root.filePath(QStringLiteral("music")),
                                                                root.filePath(QStringLiteral("cache")),
                                                                root.filePath(QStringLiteral("link")),
                                                                root.filePath(QStringLiteral("file.txt")),
                                                                root.filePath(QStringLiteral("missing")),
                                                                QStringLiteral("relative")},
                                                               {root.filePath(QStringLiteral("cache"))},
                                                               &errors);
        QCOMPARE(roots, QStringList({root.filePath(QStringLiteral("docs")), root.filePath(QStringLiteral("music"))}));
        // Relative, cache (excluded), link, file and missing are each explained.
        QCOMPARE(errors.size(), 5);
        QVERIFY(SearchLocations::searchRoots({}, {}).isEmpty());
    }

    void addsExclusionsToFilters()
    {
        PathFilter filter;
        filter.excludedFolders = {QStringLiteral("/a")};
        const PathFilter combined = SearchLocations::withExclusions(filter, {QStringLiteral("/a/"), QStringLiteral("/b"), QStringLiteral("rel")});
        QCOMPARE(combined.excludedFolders, QStringList({QStringLiteral("/a"), QStringLiteral("/b")}));
        QVERIFY(combined.error().isEmpty());
        QVERIFY(combined.excludes(QStringLiteral("/b/c")));
        QVERIFY(!SearchLocations::withExclusions({}, {}).isActive());
    }
};

QTEST_GUILESS_MAIN(SearchLocationsTest)

#include "searchlocationstest.moc"
