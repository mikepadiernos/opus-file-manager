// SPDX-License-Identifier: GPL-3.0-or-later

#include "filetypeplaces.h"
#include "searchquery.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/Query>
#endif

using FileTypePlaces::Category;

class FileTypePlacesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void describesCategories()
    {
        const QList<Category> categories = FileTypePlaces::categories();
        QCOMPARE(categories.size(), 8);
        QSet<QString> ids;
        QSet<QString> titles;
        for (const Category category : categories) {
            ids.insert(FileTypePlaces::id(category));
            titles.insert(FileTypePlaces::title(category));
            QVERIFY(!FileTypePlaces::iconName(category).isEmpty());
            QCOMPARE(FileTypePlaces::usesIndex(category), category != Category::LargeFiles);
            if (FileTypePlaces::usesIndex(category)) {
                // Each indexed category selects files by a Baloo type or by MIME types, not both.
                QVERIFY(FileTypePlaces::balooType(category).isEmpty() != FileTypePlaces::balooSearchString(category).isEmpty());
            }
        }
        QCOMPARE(ids.size(), 8);
        QCOMPARE(titles.size(), 8);
        QCOMPARE(FileTypePlaces::balooType(Category::Documents), QStringLiteral("Document"));
        QVERIFY(FileTypePlaces::balooType(Category::LargeFiles).isEmpty());
        QVERIFY(FileTypePlaces::balooSearchString(Category::LargeFiles).isEmpty());
        QVERIFY(FileTypePlaces::balooSearchString(Category::Ebooks).contains(QStringLiteral("mimetype:\"application/epub+zip\"")));
        QVERIFY(FileTypePlaces::balooSearchString(Category::Packages).contains(QStringLiteral(" OR mimetype:\"application/vnd.debian.binary-package\"")));
        QCOMPARE(FileTypePlaces::largeFileSize(), qint64(100) * 1024 * 1024);
    }

    void roundTripsPlaceUrls()
    {
        for (const Category category : FileTypePlaces::categories()) {
            const QUrl url = FileTypePlaces::placeUrl(category);
            // KDE's places model puts a URL in Search For when its scheme contains "search".
            QVERIFY(url.scheme().contains(QStringLiteral("search")));
            QVERIFY(url.scheme() != QStringLiteral("search") && url.scheme() != QStringLiteral("baloosearch"));
            QVERIFY(FileTypePlaces::isPlaceUrl(url));
            Category parsed = Category::Documents;
            QVERIFY(FileTypePlaces::fromPlaceUrl(url, &parsed));
            QCOMPARE(parsed, category);
            QVERIFY(FileTypePlaces::isPlaceUrl(QUrl(url.toString() + QLatin1Char('/'))));
        }
        QVERIFY(!FileTypePlaces::isPlaceUrl(QUrl(QStringLiteral("opus-search:/unknown"))));
        QVERIFY(!FileTypePlaces::isPlaceUrl(QUrl(QStringLiteral("search:/documents"))));
        QVERIFY(!FileTypePlaces::isPlaceUrl(QUrl::fromLocalFile(QStringLiteral("/documents"))));
    }

    void liveQueriesFindTheirFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto touch = [&dir](const QString &name, qint64 size = 0) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            if (size > 0) {
                QVERIFY(file.resize(size));
            }
        };
        touch(QStringLiteral("report.PDF"));
        touch(QStringLiteral("letter.docx"));
        touch(QStringLiteral("song.mp3"));
        touch(QStringLiteral("clip.mkv"));
        touch(QStringLiteral("backup.tar.gz"));
        touch(QStringLiteral("novel.epub"));
        touch(QStringLiteral("comic.cbz"));
        touch(QStringLiteral("tool.AppImage"));
        touch(QStringLiteral("app.flatpakref"));
        touch(QStringLiteral("notes.txt"));
        touch(QStringLiteral("huge.bin"), FileTypePlaces::largeFileSize() + 1);
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::black);
        QVERIFY(image.save(dir.filePath(QStringLiteral("photo.png")), "PNG"));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("folder.zip")));

        const auto matches = [&dir](Category category) {
            const SearchQuery::Query query = SearchQuery::Query::parse(FileTypePlaces::liveQuery(category));
            QStringList names;
            if (!query.isValid()) {
                names.append(QStringLiteral("invalid: ") + query.error());
                return names;
            }
            const QFileInfoList entries = QDir(dir.path()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
            for (const QFileInfo &entry : entries) {
                if (query.matches(entry)) {
                    names.append(entry.fileName());
                }
            }
            return names;
        };
        QCOMPARE(matches(Category::Documents), (QStringList{QStringLiteral("letter.docx"), QStringLiteral("report.PDF")}));
        QCOMPARE(matches(Category::Images), QStringList{QStringLiteral("photo.png")});
        QCOMPARE(matches(Category::Audio), QStringList{QStringLiteral("song.mp3")});
        QCOMPARE(matches(Category::Video), QStringList{QStringLiteral("clip.mkv")});
        // Archives skip folders named like archives and the zip-based ebooks and documents.
        QCOMPARE(matches(Category::Archives), QStringList{QStringLiteral("backup.tar.gz")});
        QCOMPARE(matches(Category::Ebooks), (QStringList{QStringLiteral("comic.cbz"), QStringLiteral("novel.epub")}));
        QCOMPARE(matches(Category::Packages), (QStringList{QStringLiteral("app.flatpakref"), QStringLiteral("tool.AppImage")}));
        QCOMPARE(matches(Category::LargeFiles), QStringList{QStringLiteral("huge.bin")});
    }

    void buildsBalooUrls()
    {
        QVERIFY(FileTypePlaces::balooUrl(Category::LargeFiles).isEmpty());
#ifndef OPUSFM_HAVE_BALOO
        QVERIFY(FileTypePlaces::balooUrl(Category::Images).isEmpty());
#else
        const QUrl images = FileTypePlaces::balooUrl(Category::Images);
        QCOMPARE(images.scheme(), QStringLiteral("baloosearch"));
        QCOMPARE(Baloo::Query::titleFromQueryUrl(images), FileTypePlaces::title(Category::Images));
        QCOMPARE(Baloo::Query::fromSearchUrl(images).types(), QStringList{QStringLiteral("Image")});
        const Baloo::Query ebooks = Baloo::Query::fromSearchUrl(FileTypePlaces::balooUrl(Category::Ebooks));
        QVERIFY(ebooks.types().isEmpty());
        QCOMPARE(ebooks.searchString(), FileTypePlaces::balooSearchString(Category::Ebooks));
#endif
    }
};

QTEST_GUILESS_MAIN(FileTypePlacesTest)

#include "filetypeplacestest.moc"
