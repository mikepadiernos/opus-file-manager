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

#include <unistd.h>

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

    static QString privateFolder()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + u"/libraries"_s;
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
        QDir(privateFolder()).removeRecursively();
    }

    void cleanup()
    {
        QDir(privateFolder()).removeRecursively();
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

    void scopeMatchesNamesAndPaths()
    {
        LibraryScan::Options options;
        options.exclusions = {u"node_modules"_s, u"*.tmp"_s, u"Archive/Old"_s, u"Exports/*"_s, u"cache/"_s, u"  "_s};
        const LibraryScan::Scope scope(options);
        QVERIFY(!scope.isUnrestricted());

        // Names match at any depth, and everything below an excluded folder is excluded.
        QVERIFY(!scope.contains(u"node_modules"_s, true));
        QVERIFY(!scope.contains(u"src/node_modules"_s, true));
        QVERIFY(!scope.contains(u"src/node_modules/lib/index.js"_s, false));
        QVERIFY(scope.contains(u"src/node_modules_old"_s, true));
        QVERIFY(!scope.contains(u"a.tmp"_s, false));
        QVERIFY(!scope.contains(u"deep/down/b.tmp"_s, false));
        QVERIFY(scope.contains(u"a.TMP"_s, false));
        QVERIFY(scope.contains(u"a.tmp.txt"_s, false));

        // Paths match from the library root.
        QVERIFY(!scope.contains(u"Archive/Old"_s, true));
        QVERIFY(!scope.contains(u"Archive/Old/photo.jpg"_s, false));
        QVERIFY(scope.contains(u"Archive"_s, true));
        QVERIFY(scope.contains(u"Archive/Older"_s, true));
        QVERIFY(scope.contains(u"Other/Archive/Old"_s, true));
        QVERIFY(scope.contains(u"archive/old"_s, true));
        QVERIFY(scope.contains(u"Exports"_s, true));
        QVERIFY(!scope.contains(u"Exports/a.png"_s, false));
        QVERIFY(!scope.contains(u"Exports/sub/a.png"_s, false));

        // A trailing / matches folders only.
        QVERIFY(!scope.contains(u"cache"_s, true));
        QVERIFY(scope.contains(u"cache"_s, false));
        QVERIFY(!scope.contains(u"cache/item"_s, false));

        QVERIFY(LibraryScan::inScope(options, u"notes/todo.txt"_s, false));
        QVERIFY(!LibraryScan::inScope(options, u"notes/todo.tmp"_s, false));
        QVERIFY(LibraryScan::Scope(LibraryScan::Options()).isUnrestricted());
    }

    void scopeHandlesHiddenItemsAndDepth()
    {
        LibraryScan::Options options;
        QVERIFY(LibraryScan::inScope(options, u".git/config"_s, false));
        options.hidden = false;
        QVERIFY(!LibraryScan::inScope(options, u".git"_s, true));
        QVERIFY(!LibraryScan::inScope(options, u".git/config"_s, false));
        QVERIFY(!LibraryScan::inScope(options, u"notes/.hidden"_s, false));
        QVERIFY(LibraryScan::inScope(options, u"notes/visible.txt"_s, false));

        options = {};
        options.depth = 2;
        QVERIFY(LibraryScan::inScope(options, u"a"_s, true));
        QVERIFY(LibraryScan::inScope(options, u"a/b"_s, true));
        QVERIFY(!LibraryScan::inScope(options, u"a/b/c"_s, false));
    }

    void scanSkipsExcludedHiddenAndDeepItems()
    {
        QTemporaryDir dir;
        write(dir.filePath(u"a.txt"_s));
        write(dir.filePath(u"a.tmp"_s));
        write(dir.filePath(u"node_modules/x.js"_s));
        write(dir.filePath(u".git/config"_s));
        write(dir.filePath(u"notes/.hidden"_s));
        write(dir.filePath(u"notes/todo.txt"_s));
        write(dir.filePath(u"notes/deep/z.txt"_s));
        write(dir.filePath(u"Archive/Old/o.txt"_s));
        write(dir.filePath(u"Archive/New/n.txt"_s));

        LibraryScan::Options options;
        options.exclusions = {u"node_modules"_s, u"*.tmp"_s, u"Archive/Old"_s};
        LibraryScan::Result result = LibraryScan::scan(dir.path(), options);
        QVERIFY(result.isComplete());
        QCOMPARE(paths(result.items),
                 (QStringList{u".git"_s,
                              u".git/config"_s,
                              u"Archive"_s,
                              u"Archive/New"_s,
                              u"Archive/New/n.txt"_s,
                              u"a.txt"_s,
                              u"notes"_s,
                              u"notes/.hidden"_s,
                              u"notes/deep"_s,
                              u"notes/deep/z.txt"_s,
                              u"notes/todo.txt"_s}));

        options.hidden = false;
        result = LibraryScan::scan(dir.path(), options);
        QCOMPARE(paths(result.items),
                 (QStringList{u"Archive"_s,
                              u"Archive/New"_s,
                              u"Archive/New/n.txt"_s,
                              u"a.txt"_s,
                              u"notes"_s,
                              u"notes/deep"_s,
                              u"notes/deep/z.txt"_s,
                              u"notes/todo.txt"_s}));

        LibraryScan::Options shallow;
        shallow.depth = 1;
        result = LibraryScan::scan(dir.path(), shallow);
        QVERIFY(result.isComplete());
        QCOMPARE(paths(result.items), (QStringList{u".git"_s, u"Archive"_s, u"a.tmp"_s, u"a.txt"_s, u"node_modules"_s, u"notes"_s}));
        shallow.depth = 2;
        result = LibraryScan::scan(dir.path(), shallow);
        QVERIFY(paths(result.items).contains(u"notes/deep"_s));
        QVERIFY(!paths(result.items).contains(u"notes/deep/z.txt"_s));
    }

    void scanLimitStopsAnIncompleteRefresh()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 10; ++i) {
            write(dir.filePath(u"file%1"_s.arg(i)));
        }
        TagLibrary::Library library(dir.path());
        QVERIFY(library.save());
        QCOMPARE(LibraryScan::apply(dir.path(), LibraryScan::scan(dir.path()), {}).changes.added, 10);
        QVERIFY(QFile::remove(dir.filePath(u"file0"_s)));
        QVERIFY(QFile::remove(dir.filePath(u"file1"_s)));
        write(dir.filePath(u"new"_s));

        LibraryScan::Options options;
        options.limit = 4;
        const LibraryScan::Result limited = LibraryScan::scan(dir.path(), options);
        QVERIFY(limited.limited);
        QVERIFY(!limited.canceled);
        QVERIFY(!limited.isComplete());
        QCOMPARE(limited.items.size(), 4);
        const LibraryScan::Applied applied = LibraryScan::apply(dir.path(), limited, options);
        QVERIFY(applied.saved);
        QCOMPARE(applied.changes.missing, 0);
        QCOMPARE(applied.missing, 0);
        QVERIFY(LibraryScan::summary(limited, applied).contains(u"Stopped after indexing 4 items"_s));

        // A limit equal to the number of items doesn't stop the refresh.
        options.limit = 9;
        const LibraryScan::Result exact = LibraryScan::scan(dir.path(), options);
        QVERIFY(!exact.limited);
        QVERIFY(exact.isComplete());
        QCOMPARE(exact.items.size(), 9);
        const LibraryScan::Applied full = LibraryScan::apply(dir.path(), exact, options);
        QCOMPARE(full.changes.missing, 2);
        QCOMPARE(full.missing, 2);
        QVERIFY(!LibraryScan::summary(exact, full).contains(u"Stopped after"_s));
    }

    void completeScansKeepEntriesOutOfScope()
    {
        QTemporaryDir dir;
        write(dir.filePath(u"a.txt"_s));
        write(dir.filePath(u"b.txt"_s));
        write(dir.filePath(u"node_modules/x.js"_s));
        write(dir.filePath(u".hidden"_s));
        write(dir.filePath(u"deep/er/c.txt"_s));
        TagLibrary::Library library(dir.path());
        library.applyScan(LibraryScan::scan(dir.path()).items, true);
        writeLibrary(withTag(library, {u"node_modules/x.js"_s, u".hidden"_s, u"deep/er/c.txt"_s}));

        QVERIFY(QFile::remove(dir.filePath(u"b.txt"_s)));
        LibraryScan::Options options;
        options.exclusions = {u"node_modules"_s};
        options.hidden = false;
        options.depth = 2;
        const LibraryScan::Result result = LibraryScan::scan(dir.path(), options);
        QVERIFY(result.isComplete());
        const LibraryScan::Applied applied = LibraryScan::apply(dir.path(), result, options);
        QVERIFY(applied.saved);
        QCOMPARE(applied.changes.missing, 1);
        QCOMPARE(applied.missing, 1);

        const TagLibrary::Library loaded = TagLibrary::Library::load(dir.path());
        QVERIFY(loaded.entry(u"b.txt"_s)->missing);
        QVERIFY(!loaded.entry(u"a.txt"_s)->missing);
        for (const QString &path : {u"node_modules/x.js"_s, u".hidden"_s, u"deep/er/c.txt"_s}) {
            QVERIFY2(!loaded.entry(path)->missing, qPrintable(path));
            QCOMPARE(loaded.entry(path)->tags, QStringList{u"t1"_s});
        }
        QVERIFY(!loaded.entry(u"node_modules"_s)->missing);

        // Out-of-scope entries that are really gone aren't marked missing either; the predicate decides.
        TagLibrary::Library direct = loaded;
        const TagLibrary::ScanChanges changes = direct.applyScan({}, true, [](const QString &path, bool) {
            return path == u"a.txt"_s;
        });
        QCOMPARE(changes.missing, 1);
        QVERIFY(direct.entry(u"a.txt"_s)->missing);
        QVERIFY(!direct.entry(u"deep/er/c.txt"_s)->missing);

        // observe() isn't restricted by the options, so excluded items can still be tagged explicitly.
        TagLibrary::Observation observation;
        QVERIFY(LibraryScan::observe(dir.path(), dir.filePath(u"node_modules/x.js"_s), &observation));
        QCOMPARE(observation.path, u"node_modules/x.js"_s);
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

    void registryCreatesPrivateLibraries()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath(u"Shared"_s);
        write(root + u"/photo.jpg"_s);
        const QByteArray before = treeHash(root);
        const QStringList names = QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);

        QCOMPARE(LibraryRegistry::storageFor(root, TagLibrary::Storage::PrivateFolder), TagLibrary::Storage::PrivateFolder);
        QCOMPARE(LibraryRegistry::create(root, TagLibrary::Storage::PrivateFolder), QString());
        QVERIFY(QFile::exists(TagLibrary::privateDocumentPath(root)));
        QCOMPARE(QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), names);
        QCOMPARE(treeHash(root), before);
        QVERIFY(TagLibrary::isPrivate(root));
        QCOMPARE(LibraryRegistry::roots(), QStringList{root});

        // Scanning and saving keep writing privately.
        TagLibrary::Library library = TagLibrary::Library::load(root);
        library.applyScan(LibraryScan::scan(root).items, true);
        QVERIFY(library.save());
        QCOMPARE(QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), names);

        // An existing private document is adopted whatever the preferred storage.
        LibraryRegistry::remove(root);
        QCOMPARE(LibraryRegistry::storageFor(root, TagLibrary::Storage::LibraryFolder), TagLibrary::Storage::PrivateFolder);
        QCOMPARE(LibraryRegistry::create(root, TagLibrary::Storage::LibraryFolder), QString());
        QVERIFY(!QFile::exists(TagLibrary::folderDocumentPath(root)));
        QCOMPARE(TagLibrary::Library::load(root).entries().size(), 1);
    }

    void registryAdoptsDocumentsInTheFolder()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath(u"Pictures"_s);
        write(root + u"/photo.jpg"_s);
        TagLibrary::Library existing(root);
        existing.applyScan(LibraryScan::scan(root).items, true);
        QVERIFY(existing.save());

        // An in-folder document is used in place even when private storage is preferred.
        QCOMPARE(LibraryRegistry::storageFor(root, TagLibrary::Storage::PrivateFolder), TagLibrary::Storage::LibraryFolder);
        QCOMPARE(LibraryRegistry::create(root, TagLibrary::Storage::PrivateFolder), QString());
        QVERIFY(!QFile::exists(TagLibrary::privateDocumentPath(root)));
        QVERIFY(!TagLibrary::isPrivate(root));
        QCOMPARE(TagLibrary::Library::load(root).entries().size(), 1);
        // The in-folder document is never indexed.
        for (const auto &item : LibraryScan::scan(root).items) {
            QVERIFY(!TagLibrary::isLibraryFile(item.path));
        }

        // With documents in both places, the private one is used.
        LibraryRegistry::remove(root);
        TagLibrary::Library privateCopy(root, TagLibrary::Storage::PrivateFolder);
        QVERIFY(privateCopy.save());
        QCOMPARE(LibraryRegistry::storageFor(root, TagLibrary::Storage::LibraryFolder), TagLibrary::Storage::PrivateFolder);
        QCOMPARE(LibraryRegistry::create(root), QString());
        QVERIFY(TagLibrary::Library::load(root).entries().isEmpty());
        QVERIFY(QFile::exists(TagLibrary::folderDocumentPath(root)));
    }

    void registryAddsFieldsToNewDocumentsOnly()
    {
        QTemporaryDir dir;
        const QList<TagLibrary::Field> fields = TagLibrary::parseFieldTemplates({u"text:Title"_s, u"date:Shot on"_s})
            + QList<TagLibrary::Field>{{QString(), u"Place"_s, TagLibrary::FieldType::Text}};

        // A new document starts with the fields, in order; one without an identifier gets one.
        const QString root = dir.filePath(u"New"_s);
        QVERIFY(QDir().mkpath(root));
        QCOMPARE(LibraryRegistry::create(root, TagLibrary::Storage::LibraryFolder, fields), QString());
        TagLibrary::Library created = TagLibrary::Library::load(root);
        QCOMPARE(created.fields().size(), 3);
        QCOMPARE(created.fields().at(0).name, u"Title"_s);
        QCOMPARE(created.fields().at(1).name, u"Shot on"_s);
        QCOMPARE(created.fields().at(1).type, TagLibrary::FieldType::Date);
        QCOMPARE(created.fields().at(2).name, u"Place"_s);
        QVERIFY(!created.fields().at(2).id.isEmpty());
        QVERIFY(created.entries().isEmpty());

        // So does a new private document; fields a library refuses, such as repeated names, are skipped.
        const QString privateRoot = dir.filePath(u"Private"_s);
        QVERIFY(QDir().mkpath(privateRoot));
        QCOMPARE(LibraryRegistry::create(privateRoot,
                                         TagLibrary::Storage::PrivateFolder,
                                         fields + QList<TagLibrary::Field>{{QString(), u"TITLE"_s, TagLibrary::FieldType::Date}}),
                 QString());
        QVERIFY(TagLibrary::isPrivate(privateRoot));
        QCOMPARE(TagLibrary::Library::load(privateRoot).fields().size(), 3);

        // An adopted document keeps its fields as they are.
        const QString existingRoot = dir.filePath(u"Existing"_s);
        QVERIFY(QDir().mkpath(existingRoot));
        TagLibrary::Library existing(existingRoot);
        QVERIFY(existing.setField({u"f-own"_s, u"Own"_s, TagLibrary::FieldType::Text}));
        QVERIFY(existing.save());
        QFile document(TagLibrary::folderDocumentPath(existingRoot));
        QVERIFY(document.open(QIODevice::ReadOnly));
        const QByteArray contents = document.readAll();
        document.close();
        QCOMPARE(LibraryRegistry::create(existingRoot, TagLibrary::Storage::LibraryFolder, fields), QString());
        QVERIFY(document.open(QIODevice::ReadOnly));
        QCOMPARE(document.readAll(), contents);
        const TagLibrary::Library adopted = TagLibrary::Library::load(existingRoot);
        QCOMPARE(adopted.fields().size(), 1);
        QCOMPARE(adopted.fields().constFirst().name, u"Own"_s);

        // A refused registration writes nothing.
        QVERIFY(QDir().mkpath(root + u"/Inner"_s));
        QVERIFY(!LibraryRegistry::create(root + u"/Inner"_s, TagLibrary::Storage::LibraryFolder, fields).isEmpty());
        QVERIFY(!QFile::exists(TagLibrary::folderDocumentPath(root + u"/Inner"_s)));
    }

    void registryCreatesPrivateLibrariesOfReadOnlyFolders()
    {
        if (::geteuid() == 0) {
            QSKIP("Permission checks don't apply to root.");
        }
        QTemporaryDir dir;
        const QString root = dir.filePath(u"ReadOnly"_s);
        write(root + u"/photo.jpg"_s);
        QVERIFY(QFile::setPermissions(root, QFile::ReadOwner | QFile::ExeOwner));
        const QString error = LibraryRegistry::create(root);
        QVERIFY(!error.isEmpty());
        QVERIFY(LibraryRegistry::roots().isEmpty());
        QCOMPARE(LibraryRegistry::create(root, TagLibrary::Storage::PrivateFolder), QString());
        QCOMPARE(LibraryRegistry::roots(), QStringList{root});
        TagLibrary::Library library = TagLibrary::Library::load(root);
        QVERIFY(!library.isReadOnly());
        library.applyScan(LibraryScan::scan(root).items, true);
        QVERIFY(library.save());
        QVERIFY(!QFile::exists(TagLibrary::folderDocumentPath(root)));
        QVERIFY(QFile::setPermissions(root, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
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
