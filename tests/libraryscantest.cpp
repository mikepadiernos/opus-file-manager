// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryscan.h"
#include "librarymoves.h"
#include "libraryregistry.h"
#include "taglibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class LibraryScanTest : public QObject
{
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &data = "data")
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
    }

    /// Hashes every path and file below @p root, excluding the library document.
    static QByteArray treeHash(const QString &root)
    {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        QStringList paths;
        while (it.hasNext()) {
            paths.append(it.next());
        }
        paths.sort();
        for (const QString &path : std::as_const(paths)) {
            if (TagLibrary::isLibraryFile(QFileInfo(path).fileName())) {
                continue;
            }
            hash.addData(path.toUtf8());
            QFile file(path);
            if (QFileInfo(path).isFile() && file.open(QIODevice::ReadOnly)) {
                hash.addData(file.readAll());
            }
        }
        return hash.result();
    }

    static QStringList paths(const QList<TagLibrary::Observation> &items)
    {
        QStringList result;
        for (const auto &item : items) {
            result.append(item.path);
        }
        result.sort();
        return result;
    }

    /// Returns @p library with a tag assigned to the entries at @p tagged.
    static TagLibrary::Library withTag(const TagLibrary::Library &library, const QStringList &tagged)
    {
        QJsonObject document = library.toJson();
        document[u"tags"_s] = QJsonArray{QJsonObject{{u"id"_s, u"t1"_s}, {u"name"_s, u"Tag"_s}}};
        QJsonArray entries = document.value(u"entries"_s).toArray();
        for (qsizetype i = 0; i < entries.size(); ++i) {
            QJsonObject entry = entries.at(i).toObject();
            if (tagged.contains(entry.value(u"path"_s).toString())) {
                entry[u"tags"_s] = QJsonArray{u"t1"_s};
            }
            entries[i] = entry;
        }
        document[u"entries"_s] = entries;
        return TagLibrary::Library::fromJson(document, library.root());
    }

    static void writeLibrary(const TagLibrary::Library &library)
    {
        write(library.filePath(), QJsonDocument(library.toJson()).toJson());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        for (const QString &root : LibraryRegistry::roots()) {
            LibraryRegistry::remove(root);
        }
    }

    void scansFilesOfAnyTypeWithoutChangingThem()
    {
        QTemporaryDir dir;
        write(dir.filePath(u"photo.jpg"_s), "\xff\xd8\xff");
        write(dir.filePath(u"notes/todo.txt"_s));
        write(dir.filePath(u"notes/.hidden"_s));
        write(dir.filePath(u"notes/deep/archive.tar.zst"_s));
        write(dir.filePath(u"notes/.mfm.json"_s), "{}");
        QDir().mkpath(dir.filePath(u"empty"_s));
        QVERIFY(QFile::link(dir.filePath(u"notes"_s), dir.filePath(u"linked"_s)));
        QVERIFY(QFile::link(dir.filePath(u"photo.jpg"_s), dir.filePath(u"photo-link.jpg"_s)));
        TagLibrary::Library library(dir.path());
        QVERIFY(library.save());
        const QByteArray before = treeHash(dir.path());

        int progressCalls = 0;
        const LibraryScan::Result result = LibraryScan::scan(dir.path(), {}, [&](int count, const QString &) {
            progressCalls = count;
        });
        QVERIFY(!result.canceled);
        QVERIFY(result.errors.isEmpty());
        QCOMPARE(result.skipped, 0);
        QCOMPARE(
            paths(result.items),
            (QStringList{u"empty"_s, u"notes"_s, u"notes/.hidden"_s, u"notes/deep"_s, u"notes/deep/archive.tar.zst"_s, u"notes/todo.txt"_s, u"photo.jpg"_s}));
        QVERIFY(progressCalls >= result.items.size());
        for (const auto &item : result.items) {
            if (item.path == u"photo.jpg"_s) {
                QCOMPARE(item.size, 3);
                QVERIFY(!item.folder);
                QVERIFY(item.inode != 0);
                QVERIFY(item.modified > 0);
            }
            if (item.path == u"notes"_s) {
                QVERIFY(item.folder);
            }
        }
        QCOMPARE(treeHash(dir.path()), before);
    }

    void scanCanBeCanceled()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 20; ++i) {
            write(dir.filePath(u"file%1"_s.arg(i)));
        }
        int calls = 0;
        const LibraryScan::Result result = LibraryScan::scan(dir.path(), [&calls] {
            return ++calls > 5;
        });
        QVERIFY(result.canceled);
        QVERIFY(result.items.size() < 20);
    }

    void applyingScansKeepsMetadataForMissingFiles()
    {
        QTemporaryDir dir;
        write(dir.filePath(u"a.txt"_s));
        write(dir.filePath(u"b.txt"_s));
        TagLibrary::Library library(dir.path());
        TagLibrary::ScanChanges changes = library.applyScan(LibraryScan::scan(dir.path()).items, true);
        QCOMPARE(changes.added, 2);
        const QString id = library.entry(u"a.txt"_s)->id;

        QVERIFY(QFile::remove(dir.filePath(u"a.txt"_s)));
        changes = library.applyScan(LibraryScan::scan(dir.path()).items, true);
        QCOMPARE(changes.added, 0);
        QCOMPARE(changes.missing, 1);
        QVERIFY(library.entry(u"a.txt"_s)->missing);
        QCOMPARE(library.entry(u"a.txt"_s)->id, id);

        // An incomplete scan never marks entries missing.
        write(dir.filePath(u"c.txt"_s));
        changes = library.applyScan({}, false);
        QCOMPARE(changes.missing, 0);
        QVERIFY(!library.entry(u"b.txt"_s)->missing);

        write(dir.filePath(u"a.txt"_s), "changed");
        changes = library.applyScan(LibraryScan::scan(dir.path()).items, true);
        QCOMPARE(changes.found, 1);
        QCOMPARE(changes.added, 1);
        QVERIFY(!library.entry(u"a.txt"_s)->missing);
        QCOMPARE(library.entry(u"a.txt"_s)->size, 7);
        QCOMPARE(library.entry(u"a.txt"_s)->id, id);

        QVERIFY(library.save());
        const TagLibrary::Library loaded = TagLibrary::Library::load(dir.path());
        QCOMPARE(loaded.entries().size(), 3);
        QCOMPARE(loaded.entry(u"a.txt"_s)->id, id);
    }

    void registryCreatesAndAdoptsLibraries()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath(u"Pictures"_s);
        QDir().mkpath(root + u"/Trips"_s);
        write(root + u"/.mfm.json"_s, "{\"notes\":{}}");

        QCOMPARE(LibraryRegistry::create(root), QString());
        QVERIFY(QFile::exists(QDir(root).filePath(TagLibrary::fileName())));
        QCOMPARE(LibraryRegistry::roots(), QStringList{root});
        QVERIFY(LibraryRegistry::contains(root + u"/"_s));
        QCOMPARE(LibraryRegistry::rootFor(root + u"/Trips/x.jpg"_s), root);
        QCOMPARE(LibraryRegistry::rootFor(root), root);
        QVERIFY(LibraryRegistry::rootFor(root + u"-other/x"_s).isEmpty());

        QVERIFY(!LibraryRegistry::create(root).isEmpty());
        QVERIFY(!LibraryRegistry::create(root + u"/Trips"_s).isEmpty());
        QVERIFY(!LibraryRegistry::create(dir.path()).isEmpty());
        QVERIFY(!LibraryRegistry::create(u"relative"_s).isEmpty());
        QCOMPARE(LibraryRegistry::roots(), QStringList{root});

        LibraryRegistry::remove(root);
        QVERIFY(LibraryRegistry::roots().isEmpty());
        QVERIFY(QFile::exists(QDir(root).filePath(TagLibrary::fileName())));

        // Adopting keeps the existing document.
        TagLibrary::Library library = TagLibrary::Library::load(root);
        library.applyScan(LibraryScan::scan(root).items, true);
        QVERIFY(library.save());
        QCOMPARE(LibraryRegistry::create(root), QString());
        QCOMPARE(TagLibrary::Library::load(root).entries().size(), 1);

        // An invalid document is reported rather than replaced.
        const QString broken = dir.filePath(u"Broken"_s);
        write(QDir(broken).filePath(TagLibrary::fileName()), "not json");
        QVERIFY(!LibraryRegistry::create(broken).isEmpty());
        QFile document(QDir(broken).filePath(TagLibrary::fileName()));
        QVERIFY(document.open(QIODevice::ReadOnly));
        QCOMPARE(document.readAll(), "not json");
        QCOMPARE(LibraryRegistry::roots(), QStringList{root});
    }

    void movingPathsKeepsMetadataWithEntries()
    {
        QTemporaryDir dir;
        write(dir.filePath(u"a.txt"_s));
        write(dir.filePath(u"b.txt"_s));
        write(dir.filePath(u"Trips/x.jpg"_s));
        write(dir.filePath(u"Trips/Day 1/y.jpg"_s));
        write(dir.filePath(u"Tripsy/z.jpg"_s));
        TagLibrary::Library scanned(dir.path());
        scanned.applyScan(LibraryScan::scan(dir.path()).items, true);
        TagLibrary::Library library = withTag(scanned, {u"a.txt"_s, u"Trips/Day 1/y.jpg"_s});
        const QString aId = library.entry(u"a.txt"_s)->id;
        const QString bId = library.entry(u"b.txt"_s)->id;
        const QString yId = library.entry(u"Trips/Day 1/y.jpg"_s)->id;

        // Swapped names move together.
        QCOMPARE(library.movePaths({{u"a.txt"_s, u"b.txt"_s}, {u"b.txt"_s, u"a.txt"_s}}), 2);
        QCOMPARE(library.entry(u"b.txt"_s)->id, aId);
        QCOMPARE(library.entry(u"a.txt"_s)->id, bId);

        // A folder moves with its descendants, but not with a sibling sharing its prefix.
        QCOMPARE(library.movePaths({{u"Trips"_s, u"Archive/Trips"_s}}), 4);
        QCOMPARE(library.entry(u"Archive/Trips/Day 1/y.jpg"_s)->id, yId);
        QCOMPARE(library.entry(u"Archive/Trips/Day 1/y.jpg"_s)->tags, QStringList{u"t1"_s});
        QVERIFY(library.entry(u"Tripsy/z.jpg"_s));
        QVERIFY(!library.entry(u"Trips"_s));

        // Overwriting a file replaces the entry that described it.
        QCOMPARE(library.movePaths({{u"b.txt"_s, u"a.txt"_s}}), 1);
        QCOMPARE(library.entry(u"a.txt"_s)->id, aId);
        QVERIFY(!library.entry(u"b.txt"_s));
        QVERIFY(!library.entryById(bId));

        // Invalid moves change nothing.
        QCOMPARE(library.movePaths({{u"a.txt"_s, u"../a.txt"_s}, {u"Archive"_s, u"Archive/Inner"_s}, {u"none"_s, u"other"_s}}), 0);

        QCOMPARE(library.markMissing(u"Archive/Trips"_s), 4);
        QVERIFY(library.entry(u"Archive/Trips/x.jpg"_s)->missing);
        QVERIFY(!library.entry(u"Tripsy/z.jpg"_s)->missing);
    }

    void relinkingNeedsAnExplicitChoice()
    {
        QTemporaryDir dir;
        write(dir.filePath(u"photo.jpg"_s), "12345");
        write(dir.filePath(u"Album/song.mp3"_s));
        write(dir.filePath(u"other.jpg"_s), "12345");
        TagLibrary::Library scanned(dir.path());
        scanned.applyScan(LibraryScan::scan(dir.path()).items, true);
        TagLibrary::Library library = withTag(scanned, {u"photo.jpg"_s, u"Album/song.mp3"_s, u"other.jpg"_s});
        const QString photoId = library.entry(u"photo.jpg"_s)->id;
        const QString albumId = library.entry(u"Album"_s)->id;

        // Moved outside Plasma Opus: a refresh marks the old paths missing and indexes the new ones.
        QVERIFY(QDir().mkpath(dir.filePath(u"Moved"_s)));
        QVERIFY(QFile::rename(dir.filePath(u"photo.jpg"_s), dir.filePath(u"Moved/photo.jpg"_s)));
        QVERIFY(QFile::rename(dir.filePath(u"Album"_s), dir.filePath(u"Music"_s)));
        write(dir.filePath(u"Copies/photo.jpg"_s), "12345");
        const TagLibrary::ScanChanges changes = library.applyScan(LibraryScan::scan(dir.path()).items, true);
        QCOMPARE(changes.missing, 3);
        QVERIFY(library.entryById(photoId)->missing);
        QCOMPARE(library.entryById(photoId)->tags, QStringList{u"t1"_s});

        // Candidates are suggested strongest first, never applied.
        const QStringList candidates = library.relinkCandidates(photoId);
        QCOMPARE(candidates.size(), 2);
        QCOMPARE(library.entryById(candidates.first())->path, u"Moved/photo.jpg"_s);
        QCOMPARE(library.entryById(candidates.last())->path, u"Copies/photo.jpg"_s);
        QVERIFY(library.entryById(photoId)->missing);
        QVERIFY(library.relinkCandidates(library.entry(u"other.jpg"_s)->id).isEmpty());

        // Relinking never overwrites an entry with metadata or crosses kinds.
        TagLibrary::Observation target;
        QString error;
        QVERIFY(LibraryScan::observe(dir.path(), dir.filePath(u"other.jpg"_s), &target));
        QVERIFY(!library.relink(photoId, target, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(LibraryScan::observe(dir.path(), dir.filePath(u"Music"_s), &target));
        QVERIFY(!library.relink(photoId, target, &error));

        const qsizetype before = library.entries().size();
        QVERIFY(LibraryScan::observe(dir.path(), dir.filePath(u"Moved/photo.jpg"_s), &target));
        QVERIFY(library.relink(photoId, target, &error));
        QCOMPARE(library.entry(u"Moved/photo.jpg"_s)->id, photoId);
        QVERIFY(!library.entryById(photoId)->missing);
        QCOMPARE(library.entries().size(), before - 1);

        // A relinked folder brings its missing descendants.
        QCOMPARE(library.relinkCandidates(albumId).size(), 1);
        QCOMPARE(library.entryById(library.relinkCandidates(albumId).first())->path, u"Music"_s);
        QVERIFY(LibraryScan::observe(dir.path(), dir.filePath(u"Music"_s), &target));
        QVERIFY(library.relink(albumId, target, &error));
        QCOMPARE(library.entry(u"Music/song.mp3"_s)->tags, QStringList{u"t1"_s});
        QVERIFY(!library.entry(u"Album/song.mp3"_s));
        QVERIFY(library.save(&error));
        QCOMPARE(TagLibrary::Library::load(dir.path()).entry(u"Music/song.mp3"_s)->tags, QStringList{u"t1"_s});

        QCOMPARE(library.removeEntry(library.entry(u"Copies"_s)->id), 1);
        QVERIFY(library.entry(u"Copies/photo.jpg"_s));
    }

    void appliesOnlyConfirmedMoves()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath(u"Library"_s);
        const QString other = dir.filePath(u"Other"_s);
        write(root + u"/a.txt"_s);
        write(root + u"/b.txt"_s);
        write(root + u"/c.txt"_s);
        write(other + u"/d.txt"_s);
        TagLibrary::Library scanned(root);
        scanned.applyScan(LibraryScan::scan(root).items, true);
        writeLibrary(withTag(scanned, {u"a.txt"_s, u"b.txt"_s, u"c.txt"_s}));
        const TagLibrary::Library before = TagLibrary::Library::load(root);
        QVERIFY(!before.isNull());

        QVERIFY(QFile::rename(root + u"/a.txt"_s, root + u"/renamed.txt"_s));
        QVERIFY(QFile::rename(root + u"/b.txt"_s, other + u"/b.txt"_s));
        QVERIFY(QFile::rename(other + u"/d.txt"_s, root + u"/d.txt"_s));
        const LibraryMoves::Outcome outcome = LibraryMoves::apply({{root + u"/a.txt"_s, root + u"/renamed.txt"_s},
                                                                   {root + u"/b.txt"_s, other + u"/b.txt"_s},
                                                                   {other + u"/d.txt"_s, root + u"/d.txt"_s},
                                                                   // Not confirmed: the source still exists.
                                                                   {root + u"/c.txt"_s, root + u"/elsewhere.txt"_s},
                                                                   {root, dir.filePath(u"Moved"_s)}},
                                                                  {root});
        QCOMPARE(outcome.changed, QStringList{root});
        QVERIFY(outcome.errors.isEmpty());
        const TagLibrary::Library after = TagLibrary::Library::load(root);
        QCOMPARE(after.entry(u"renamed.txt"_s)->id, before.entry(u"a.txt"_s)->id);
        QCOMPARE(after.entry(u"renamed.txt"_s)->tags, QStringList{u"t1"_s});
        QVERIFY(after.entry(u"b.txt"_s)->missing);
        QVERIFY(after.entry(u"d.txt"_s));
        QVERIFY(after.entry(u"d.txt"_s)->tags.isEmpty());
        QVERIFY(!after.entry(u"c.txt"_s)->missing);
        QVERIFY(!after.entry(u"elsewhere.txt"_s));

        // Swapped names are confirmed by identity.
        TagLibrary::Observation c;
        TagLibrary::Observation renamed;
        QVERIFY(LibraryScan::observe(root, root + u"/c.txt"_s, &c));
        QVERIFY(LibraryScan::observe(root, root + u"/renamed.txt"_s, &renamed));
        QVERIFY(QFile::rename(root + u"/c.txt"_s, root + u"/swap"_s));
        QVERIFY(QFile::rename(root + u"/renamed.txt"_s, root + u"/c.txt"_s));
        QVERIFY(QFile::rename(root + u"/swap"_s, root + u"/renamed.txt"_s));
        const QString cId = after.entry(u"c.txt"_s)->id;
        QCOMPARE(LibraryMoves::apply({{root + u"/c.txt"_s, root + u"/renamed.txt"_s, c.device, c.inode},
                                      {root + u"/renamed.txt"_s, root + u"/c.txt"_s, renamed.device, renamed.inode}},
                                     {root})
                     .changed,
                 QStringList{root});
        const TagLibrary::Library swapped = TagLibrary::Library::load(root);
        QCOMPARE(swapped.entry(u"renamed.txt"_s)->id, cId);
        QCOMPARE(swapped.entry(u"c.txt"_s)->id, after.entry(u"renamed.txt"_s)->id);
        // A wrong identity is never applied.
        QVERIFY(LibraryMoves::apply({{root + u"/c.txt"_s, root + u"/renamed.txt"_s, c.device, c.inode + 1}}, {root}).changed.isEmpty());

        // Nothing to apply leaves the document alone.
        QVERIFY(LibraryMoves::apply({{root + u"/c.txt"_s, root + u"/x.txt"_s}}, {root}).changed.isEmpty());
    }
};

QTEST_GUILESS_MAIN(LibraryScanTest)

#include "libraryscantest.moc"
