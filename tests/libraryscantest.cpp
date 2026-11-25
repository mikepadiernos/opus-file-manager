// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryscan.h"
#include "libraryregistry.h"
#include "taglibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
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
};

QTEST_GUILESS_MAIN(LibraryScanTest)

#include "libraryscantest.moc"
