// SPDX-License-Identifier: GPL-3.0-or-later

#include "taglibrary.h"
#include "libraryscan.h"
#include "searchquery.h"
#include "tagsearch.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Qt::StringLiterals;

class TagLibraryTest : public QObject
{
    Q_OBJECT

private:
    static QJsonObject sampleDocument()
    {
        return QJsonDocument::fromJson(
                   "{"
                   "\"format\": \"plasma-opus-library\","
                   "\"version\": 1,"
                   "\"futureKey\": {\"kept\": true},"
                   "\"tags\": ["
                   "{\"id\": \"t-animal\", \"name\": \"Animal\", \"color\": \"#336699\"},"
                   "{\"id\": \"t-cat\", \"name\": \"Cat\", \"aliases\": [\"Kitty\"], \"shorthand\": \"c\", \"parents\": [\"t-animal\"]}"
                   "],"
                   "\"groups\": [{\"id\": \"g-1\", \"name\": \"Subjects\", \"tags\": [\"t-animal\", \"t-cat\"]}],"
                   "\"fields\": ["
                   "{\"id\": \"f-title\", \"name\": \"Title\", \"type\": \"text\"},"
                   "{\"id\": \"f-shot\", \"name\": \"Shot\", \"type\": \"date\"}"
                   "],"
                   "\"entries\": ["
                   "{\"id\": \"e-1\", \"path\": \"photos/cat.jpg\", \"kind\": \"file\", \"size\": 12, \"modified\": 1700000000000,"
                   "\"device\": \"18446744073709551615\", \"inode\": \"42\", \"tags\": [\"t-cat\"],"
                   "\"fields\": {\"f-title\": \"Whiskers\", \"f-shot\": \"2026-02-28\"}},"
                   "{\"id\": \"e-2\", \"path\": \"photos\", \"kind\": \"folder\", \"size\": 0, \"modified\": 0, \"device\": \"1\", \"inode\": \"2\", "
                   "\"missing\": true}"
                   "]"
                   "}"_ba)
            .object();
    }

    static bool writeDocument(const QString &root, const QJsonObject &document)
    {
        QFile file(QDir(root).filePath(TagLibrary::fileName()));
        return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(document).toJson()) > 0;
    }

    static QString privateFolder()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + u"/libraries"_s;
    }

    static QByteArray readAll(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(privateFolder()).removeRecursively();
    }

    void cleanup()
    {
        QDir(privateFolder()).removeRecursively();
    }

    void documentRoundTrips()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        QString error;
        TagLibrary::Library library = TagLibrary::Library::load(dir.path(), &error);
        QVERIFY2(!library.isNull(), qPrintable(error));
        QVERIFY(library.warnings().isEmpty());
        QCOMPARE(library.tags().size(), 2);
        QCOMPARE(library.tag(u"t-cat"_s)->aliases, QStringList{u"Kitty"_s});
        QCOMPARE(library.tag(u"t-cat"_s)->parents, QStringList{u"t-animal"_s});
        QCOMPARE(library.groups().first().tags.size(), 2);
        QCOMPARE(library.field(u"f-shot"_s)->type, TagLibrary::FieldType::Date);
        const TagLibrary::Entry *cat = library.entry(u"photos/cat.jpg"_s);
        QVERIFY(cat);
        QCOMPARE(cat->device, std::numeric_limits<quint64>::max());
        QCOMPARE(cat->modified, 1700000000000);
        QCOMPARE(cat->fields.value(u"f-shot"_s), u"2026-02-28"_s);
        QVERIFY(library.entry(u"photos"_s)->folder);
        QVERIFY(library.entry(u"photos"_s)->missing);
        QCOMPARE(library.relativePath(dir.filePath(u"photos/cat.jpg"_s)), u"photos/cat.jpg"_s);
        QVERIFY(library.relativePath(dir.path()).isEmpty());
        QVERIFY(library.relativePath(dir.path() + u"-other/x"_s).isEmpty());

        QVERIFY2(library.save(&error), qPrintable(error));
        const TagLibrary::Library again = TagLibrary::Library::load(dir.path(), &error);
        QCOMPARE(again.toJson(), library.toJson());
        QCOMPARE(again.toJson().value(u"futureKey"_s).toObject().value(u"kept"_s).toBool(), true);
    }

    void parsesFieldTemplates()
    {
        const QList<TagLibrary::Field> fields = TagLibrary::parseFieldTemplates({u"text:Title"_s,
                                                                                 u"DATE: Shot on "_s,
                                                                                 u"text:title"_s,
                                                                                 u"date:TITLE"_s,
                                                                                 u"number:Count"_s,
                                                                                 u"Plain"_s,
                                                                                 u"text:"_s,
                                                                                 u"text:   "_s,
                                                                                 u"text:"_s + QString(201, u'x'),
                                                                                 u" Text :Place: Town"_s,
                                                                                 QString()});
        QCOMPARE(fields.size(), 3);
        QCOMPARE(fields.at(0).name, u"Title"_s);
        QCOMPARE(fields.at(0).type, TagLibrary::FieldType::Text);
        QCOMPARE(fields.at(1).name, u"Shot on"_s);
        QCOMPARE(fields.at(1).type, TagLibrary::FieldType::Date);
        QCOMPARE(fields.at(2).name, u"Place: Town"_s);
        QCOMPARE(fields.at(2).type, TagLibrary::FieldType::Text);
        QStringList ids;
        for (const TagLibrary::Field &field : fields) {
            QVERIFY(!field.id.isEmpty());
            ids.append(field.id);
        }
        ids.removeDuplicates();
        QCOMPARE(ids.size(), 3);
        QVERIFY(TagLibrary::parseFieldTemplates({}).isEmpty());

        // Templates round-trip.
        QStringList written;
        for (const TagLibrary::Field &field : fields) {
            written.append(TagLibrary::fieldTemplate(field));
        }
        QCOMPARE(written, (QStringList{u"text:Title"_s, u"date:Shot on"_s, u"text:Place: Town"_s}));
        QCOMPARE(TagLibrary::parseFieldTemplates(written).size(), 3);
    }

    void rejectsInvalidDocuments_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("format") << u"format"_s << QJsonValue(u"other"_s);
        QTest::newRow("newer") << u"version"_s << QJsonValue(2);
        QTest::newRow("fraction") << u"version"_s << QJsonValue(1.5);
        QTest::newRow("tags not list") << u"tags"_s << QJsonValue(QJsonObject());
        QTest::newRow("duplicate tag") << u"tags"_s
                                       << QJsonValue(QJsonArray{QJsonObject{{u"id"_s, u"a"_s}, {u"name"_s, u"A"_s}},
                                                                QJsonObject{{u"id"_s, u"a"_s}, {u"name"_s, u"B"_s}}});
        QTest::newRow("blank name") << u"tags"_s << QJsonValue(QJsonArray{QJsonObject{{u"id"_s, u"a"_s}, {u"name"_s, u" "_s}}});
        QTest::newRow("bad field type") << u"fields"_s << QJsonValue(QJsonArray{QJsonObject{{u"id"_s, u"f"_s}, {u"name"_s, u"F"_s}, {u"type"_s, u"number"_s}}});
        for (const QString &path : {u"../escape"_s, u"/absolute"_s, u"a//b"_s, u"a/./b"_s, u"."_s, QString()}) {
            QTest::newRow(qPrintable(u"path "_s + path))
                << u"entries"_s << QJsonValue(QJsonArray{QJsonObject{{u"id"_s, u"e"_s}, {u"path"_s, path}, {u"kind"_s, u"file"_s}}});
        }
        QTest::newRow("duplicate path") << u"entries"_s
                                        << QJsonValue(QJsonArray{QJsonObject{{u"id"_s, u"e"_s}, {u"path"_s, u"a"_s}, {u"kind"_s, u"file"_s}},
                                                                 QJsonObject{{u"id"_s, u"f"_s}, {u"path"_s, u"a"_s}, {u"kind"_s, u"file"_s}}});
        QTest::newRow("bad kind") << u"entries"_s << QJsonValue(QJsonArray{QJsonObject{{u"id"_s, u"e"_s}, {u"path"_s, u"a"_s}, {u"kind"_s, u"link"_s}}});
    }

    void rejectsInvalidDocuments()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        QJsonObject document = sampleDocument();
        document.insert(key, value);
        QTemporaryDir dir;
        QString error;
        QVERIFY(TagLibrary::Library::fromJson(document, dir.path(), &error).isNull());
        QVERIFY(!error.isEmpty());
    }

    void repairsDanglingReferencesWithWarnings()
    {
        QJsonObject document = sampleDocument();
        QJsonArray tags = document.value(u"tags"_s).toArray();
        QJsonObject animal = tags.at(0).toObject();
        animal.insert(u"parents"_s, QJsonArray{u"t-cat"_s, u"t-unknown"_s});
        animal.insert(u"color"_s, u"red"_s);
        tags.replace(0, animal);
        document.insert(u"tags"_s, tags);
        QJsonArray entries = document.value(u"entries"_s).toArray();
        QJsonObject cat = entries.at(0).toObject();
        cat.insert(u"tags"_s, QJsonArray{u"t-cat"_s, u"t-gone"_s});
        cat.insert(u"fields"_s, QJsonObject{{u"f-shot"_s, u"2026-02-30"_s}, {u"f-none"_s, u"x"_s}, {u"f-title"_s, u"Kept"_s}});
        entries.replace(0, cat);
        document.insert(u"entries"_s, entries);

        QTemporaryDir dir;
        QString error;
        const TagLibrary::Library library = TagLibrary::Library::fromJson(document, dir.path(), &error);
        QVERIFY2(!library.isNull(), qPrintable(error));
        QVERIFY(library.warnings().size() >= 4);
        QVERIFY(library.tag(u"t-animal"_s)->color.isEmpty());
        // Animal → Cat → Animal is a cycle; one of the two links is removed.
        QCOMPARE(library.tag(u"t-animal"_s)->parents.size() + library.tag(u"t-cat"_s)->parents.size(), 1);
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->tags, QStringList{u"t-cat"_s});
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->fields, (QMap<QString, QString>{{u"f-title"_s, u"Kept"_s}}));
    }

    void savingLeavesFolderMetadataAlone()
    {
        QTemporaryDir dir;
        const QByteArray folderMetadata = "{\"labels\":{\"a.txt\":\"red\"},\"notes\":{\"a.txt\":\"keep\"}}";
        QFile mfm(dir.filePath(u".mfm.json"_s));
        QVERIFY(mfm.open(QIODevice::WriteOnly));
        mfm.write(folderMetadata);
        mfm.close();

        TagLibrary::Library library(dir.path());
        QString error;
        QVERIFY2(library.save(&error), qPrintable(error));
        QVERIFY(QFile::exists(dir.filePath(TagLibrary::fileName())));
        QVERIFY(mfm.open(QIODevice::ReadOnly));
        QCOMPARE(mfm.readAll(), folderMetadata);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 2);
        QVERIFY(TagLibrary::isLibraryFile(TagLibrary::fileName()));
        QVERIFY(TagLibrary::isLibraryFile(TagLibrary::fileName() + u".XyZ123"_s));
        QVERIFY(!TagLibrary::isLibraryFile(u".mfm.json"_s));
    }

    void resolvesPrivateDocuments()
    {
        QTemporaryDir dir;
        const QString root = QDir::cleanPath(dir.path());
        const QString inFolder = QDir(root).filePath(TagLibrary::fileName());
        const QString hash = QString::fromLatin1(QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha1).toHex());
        const QString privatePath = privateFolder() + u"/"_s + hash + u".json"_s;
        QCOMPARE(TagLibrary::privateDocumentPath(root), privatePath);
        QCOMPARE(TagLibrary::privateDocumentPath(root + u"/"_s), privatePath);
        QCOMPARE(TagLibrary::folderDocumentPath(root), inFolder);
        // Without a private document, the library is kept in its folder.
        QCOMPARE(TagLibrary::documentPath(root), inFolder);
        QVERIFY(!TagLibrary::isPrivate(root));

        TagLibrary::Library library(root, TagLibrary::Storage::PrivateFolder);
        QVERIFY(library.isPrivate());
        QCOMPARE(library.filePath(), privatePath);
        QString error;
        QVERIFY2(library.save(&error), qPrintable(error));
        // Nothing is written to the library folder, and the private folder is the user's alone.
        QVERIFY(QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
        QVERIFY(QFile::exists(privatePath));
        QCOMPARE(QFileInfo(privateFolder()).permissions() & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther),
                 QFile::Permissions());
        QCOMPARE(TagLibrary::documentPath(root), privatePath);
        QVERIFY(TagLibrary::isPrivate(root));
        QCOMPARE(QJsonDocument::fromJson(readAll(privatePath)).object().value(u"root"_s).toString(), root);

        // The recorded root isn't carried into other documents, such as exports.
        TagLibrary::Library loaded = TagLibrary::Library::load(root, &error);
        QVERIFY2(!loaded.isNull(), qPrintable(error));
        QVERIFY(loaded.isPrivate());
        QVERIFY(!loaded.isReadOnly());
        QVERIFY(!loaded.toJson().contains(u"root"_s));
        QVERIFY(!loaded.changedOnDisk());
        const QString signature = TagLibrary::documentSignature(root);
        QVERIFY(!signature.isEmpty());

        // A private document takes precedence over one in the folder, which doesn't affect its signature.
        QVERIFY(writeDocument(root, sampleDocument()));
        QCOMPARE(TagLibrary::documentPath(root), privatePath);
        QCOMPARE(TagLibrary::documentSignature(root), signature);
        QVERIFY(TagLibrary::Library::load(root).tags().isEmpty());

        // The in-folder document is still never indexed.
        QVERIFY(!LibraryScan::indexable(TagLibrary::fileName()));
    }

    void privateLibrariesOfReadOnlyFoldersAreWritable()
    {
        if (::geteuid() == 0) {
            QSKIP("Permission checks don't apply to root.");
        }
        QTemporaryDir dir;
        const QString root = QDir::cleanPath(dir.path());
        QVERIFY(QFile::setPermissions(root, QFile::ReadOwner | QFile::ExeOwner));
        bool readOnly = false;
        QVERIFY(TagLibrary::checkLocation(root, &readOnly).isEmpty());
        QVERIFY(readOnly);
        QVERIFY(TagLibrary::Library(root).isReadOnly());

        TagLibrary::Library library(root, TagLibrary::Storage::PrivateFolder);
        QVERIFY(!library.isReadOnly());
        QString error;
        QVERIFY2(library.save(&error), qPrintable(error));
        QVERIFY(TagLibrary::checkLocation(root, &readOnly).isEmpty());
        QVERIFY(!readOnly);
        QVERIFY(TagLibrary::update(
            root,
            [](TagLibrary::Library &library, QString *error) {
                return library.setTag({u"t"_s, u"Tag"_s, {}, {}, {}, {}}, error);
            },
            &error));
        QCOMPARE(TagLibrary::Library::load(root).tags().size(), 1);
        // Moving the document into the read-only folder is refused and keeps it private.
        QVERIFY(!TagLibrary::moveDocument(root, TagLibrary::Storage::LibraryFolder).isEmpty());
        QVERIFY(TagLibrary::isPrivate(root));
        QVERIFY(QFile::setPermissions(root, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }

    void movesDocumentsBetweenLocations()
    {
        QTemporaryDir dir;
        const QString root = QDir::cleanPath(dir.path());
        const QString inFolder = TagLibrary::folderDocumentPath(root);
        const QString privatePath = TagLibrary::privateDocumentPath(root);
        QFile other(QDir(root).filePath(u"keep.txt"_s));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("keep");
        other.close();
        QVERIFY(writeDocument(root, sampleDocument()));
        const QByteArray original = readAll(inFolder);

        // Already there: nothing changes.
        QCOMPARE(TagLibrary::moveDocument(root, TagLibrary::Storage::LibraryFolder), QString());
        QCOMPARE(readAll(inFolder), original);

        QCOMPARE(TagLibrary::moveDocument(root, TagLibrary::Storage::PrivateFolder), QString());
        QVERIFY(!QFile::exists(inFolder));
        QCOMPARE(readAll(privatePath), original);
        QVERIFY(TagLibrary::isPrivate(root));
        QCOMPARE(TagLibrary::Library::load(root).toJson(), TagLibrary::Library::fromJson(sampleDocument(), root).toJson());
        QCOMPARE(readAll(QDir(root).filePath(u"keep.txt"_s)), QByteArray("keep"));

        // A document already at the target is never replaced, and the source is kept.
        QVERIFY(writeDocument(root, QJsonObject{{u"format"_s, u"plasma-opus-library"_s}, {u"version"_s, 1}}));
        const QByteArray blocking = readAll(inFolder);
        QVERIFY(!TagLibrary::moveDocument(root, TagLibrary::Storage::LibraryFolder).isEmpty());
        QCOMPARE(readAll(inFolder), blocking);
        QCOMPARE(readAll(privatePath), original);
        QVERIFY(QFile::remove(inFolder));

        QCOMPARE(TagLibrary::moveDocument(root, TagLibrary::Storage::LibraryFolder), QString());
        QVERIFY(!QFile::exists(privatePath));
        QCOMPARE(readAll(inFolder), original);
        QVERIFY(!TagLibrary::isPrivate(root));
        QCOMPARE(QDir(root).entryList(QDir::Files | QDir::Hidden).size(), 2);

        // An invalid document isn't moved.
        QFile broken(inFolder);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not json");
        broken.close();
        QVERIFY(!TagLibrary::moveDocument(root, TagLibrary::Storage::PrivateFolder).isEmpty());
        QCOMPARE(readAll(inFolder), QByteArray("not json"));
        QVERIFY(!QFile::exists(privatePath));

        // Without a document there is nothing to move.
        QVERIFY(QFile::remove(inFolder));
        QVERIFY(!TagLibrary::moveDocument(root, TagLibrary::Storage::PrivateFolder).isEmpty());
        QVERIFY(!QFile::exists(privatePath));
    }

    void reportsUnsupportedLocations()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(u"file"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(QFile::link(dir.path(), dir.filePath(u"link"_s)));
        bool readOnly = true;
        QVERIFY(TagLibrary::checkLocation(dir.path(), &readOnly).isEmpty());
        QVERIFY(!readOnly);
        QVERIFY(!TagLibrary::checkLocation(u"relative/folder"_s).isEmpty());
        QVERIFY(!TagLibrary::checkLocation(dir.filePath(u"missing"_s)).isEmpty());
        QVERIFY(!TagLibrary::checkLocation(dir.filePath(u"file"_s)).isEmpty());
        QVERIFY(!TagLibrary::checkLocation(dir.filePath(u"link"_s)).isEmpty());
        QVERIFY(!TagLibrary::checkLocation(u"/"_s).isEmpty());
        QString error;
        QVERIFY(TagLibrary::Library::load(dir.path(), &error).isNull());
        QVERIFY(error.contains(dir.path()));
    }

    void reportsReadOnlyFolders()
    {
        if (::geteuid() == 0) {
            QSKIP("Permission checks don't apply to root.");
        }
        QTemporaryDir dir;
        TagLibrary::Library created(dir.path());
        QVERIFY(created.save());
        QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner));
        bool readOnly = false;
        QVERIFY(TagLibrary::checkLocation(dir.path(), &readOnly).isEmpty());
        QVERIFY(readOnly);
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QVERIFY(!library.isNull());
        QVERIFY(library.isReadOnly());
        QString error;
        QVERIFY(!library.save(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }

    void detectsConcurrentChanges()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library first = TagLibrary::Library::load(dir.path());
        TagLibrary::Library second = TagLibrary::Library::load(dir.path());
        QVERIFY(first.save());
        QVERIFY(second.changedOnDisk());
        QString error;
        QVERIFY(!second.save(&error));
        QVERIFY(!error.isEmpty());

        int calls = 0;
        QVERIFY(TagLibrary::update(
            dir.path(),
            [&](TagLibrary::Library &library, QString *) {
                if (++calls == 1) {
                    // Another writer saves between this load and save.
                    TagLibrary::Library other = TagLibrary::Library::load(library.root());
                    QTest::qWait(10);
                    other.save();
                }
                return true;
            },
            &error));
        QCOMPARE(calls, 2);

        QTemporaryDir empty;
        QVERIFY(!TagLibrary::update(
            empty.path(),
            [](TagLibrary::Library &, QString *) {
                return true;
            },
            &error));
        QVERIFY(error.contains(empty.path()));
    }

    void managesTagsWithoutCycles()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());

        TagLibrary::Tag kitten{u"t-kitten"_s, u"Kitten"_s, {u" Kit "_s, u"kit"_s, u"kitten"_s, u""_s}, u"k"_s, u"#aabbcc"_s, {u"t-cat"_s}};
        QString error;
        QVERIFY2(library.setTag(kitten, &error), qPrintable(error));
        QCOMPARE(library.tag(u"t-kitten"_s)->aliases, QStringList{u"Kit"_s});
        QCOMPARE(library.ancestors(u"t-kitten"_s), (QStringList{u"t-cat"_s, u"t-animal"_s}));
        QCOMPARE(library.descendants(u"t-animal"_s), (QStringList{u"t-cat"_s, u"t-kitten"_s}));

        // A parent below the tag, or the tag itself, would form a cycle.
        TagLibrary::Tag animal = *library.tag(u"t-animal"_s);
        animal.parents = {u"t-kitten"_s};
        QVERIFY(!library.setTag(animal, &error));
        QVERIFY(error.contains(u"Kitten"_s));
        animal.parents = {u"t-animal"_s};
        QVERIFY(!library.setTag(animal, &error));
        animal.parents = {u"t-unknown"_s};
        QVERIFY(!library.setTag(animal, &error));
        QVERIFY(library.tag(u"t-animal"_s)->parents.isEmpty());

        // Invalid names, shorthands and colours are refused.
        TagLibrary::Tag invalid{u"t-new"_s, u" padded"_s, {}, {}, {}, {}};
        QVERIFY(!library.setTag(invalid, &error));
        invalid.name = u"Valid"_s;
        invalid.shorthand = u"two words"_s;
        QVERIFY(!library.setTag(invalid, &error));
        invalid.shorthand.clear();
        invalid.color = u"red"_s;
        QVERIFY(!library.setTag(invalid, &error));
        QVERIFY(!library.tag(u"t-new"_s));

        // Lookups match names, aliases and shorthand, ignoring case.
        QCOMPARE(library.findTags(u"KITTY"_s), QStringList{u"t-cat"_s});
        QCOMPARE(library.findTags(u"k"_s), QStringList{u"t-kitten"_s});
        QVERIFY(library.findTags(u"Dog"_s).isEmpty());

        QVERIFY(library.save(&error));
        const TagLibrary::Library reloaded = TagLibrary::Library::load(dir.path());
        QCOMPARE(reloaded.tag(u"t-kitten"_s)->color, u"#aabbcc"_s);
        QCOMPARE(reloaded.tag(u"t-kitten"_s)->parents, QStringList{u"t-cat"_s});
    }

    void disambiguatesDuplicateNames()
    {
        TagLibrary::Library library(QDir::tempPath());
        QVERIFY(library.setTag({u"t-animal"_s, u"Animal"_s, {}, {}, {}, {}}));
        QVERIFY(library.setTag({u"t-band"_s, u"Band"_s, {}, {}, {}, {}}));
        QVERIFY(library.setTag({u"t-jaguar-animal"_s, u"Jaguar"_s, {}, {}, {}, {u"t-animal"_s}}));
        QVERIFY(library.setTag({u"t-jaguar-car"_s, u"jaguar"_s, {}, u"car"_s, {}, {}}));
        QVERIFY(library.setTag({u"t-jaguar-other"_s, u"Jaguar"_s, {}, {}, {}, {}}));
        QCOMPARE(library.displayName(u"t-animal"_s), u"Animal"_s);
        QCOMPARE(library.displayName(u"t-jaguar-animal"_s), u"Jaguar (Animal)"_s);
        QCOMPARE(library.displayName(u"t-jaguar-car"_s), u"jaguar (car)"_s);
        QCOMPARE(library.displayName(u"t-jaguar-other"_s), u"Jaguar (t-jaguar)"_s);
        QCOMPARE(library.findTags(u"jaguar"_s).size(), 3);
    }

    void removingTagsClearsReferences()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QVERIFY(library.setTag({u"t-pet"_s, u"Pet"_s, {}, {}, {}, {}}));
        TagLibrary::Tag cat = *library.tag(u"t-cat"_s);
        cat.parents.append(u"t-pet"_s);
        QVERIFY(library.setTag(cat));
        QCOMPARE(library.usageCount(u"t-cat"_s), 1);

        QCOMPARE(library.removeTag(u"t-animal"_s), 0);
        QVERIFY(!library.tag(u"t-animal"_s));
        QCOMPARE(library.tag(u"t-cat"_s)->parents, QStringList{u"t-pet"_s});
        QCOMPARE(library.groups().first().tags, QStringList{u"t-cat"_s});

        QCOMPARE(library.removeTag(u"t-cat"_s), 1);
        QVERIFY(library.entry(u"photos/cat.jpg"_s)->tags.isEmpty());
        QVERIFY(library.groups().first().tags.isEmpty());
        QCOMPARE(library.removeTag(u"t-cat"_s), 0);
    }

    void managesGroups()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.groupsOf(u"t-cat"_s), QStringList{u"g-1"_s});

        // Invalid groups are refused without changing anything.
        QString error;
        QVERIFY(!library.setGroup({{}, u"Places"_s, {}, {}}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!library.setGroup({u"g-2"_s, u" Places"_s, {}, {}}));
        QVERIFY(!library.setGroup({u"g-2"_s, u"Places"_s, u"red"_s, {}}));
        QCOMPARE(library.groups().size(), 1);

        // Unknown and repeated tags are dropped; a tag can be in several groups and names may repeat.
        QVERIFY(library.setGroup({u"g-2"_s, u"Subjects"_s, u"#aa0000"_s, {u"t-cat"_s, u"t-gone"_s, u"t-cat"_s}}));
        QCOMPARE(library.group(u"g-2"_s)->tags, QStringList{u"t-cat"_s});
        QCOMPARE(library.groupsOf(u"t-cat"_s), (QStringList{u"g-1"_s, u"g-2"_s}));
        QVERIFY(library.setGroup({u"g-3"_s, u"Empty"_s, {}, {}}));

        // Replacing keeps the position; moving clamps the index.
        QVERIFY(library.setGroup({u"g-1"_s, u"Topics"_s, {}, {u"t-animal"_s}}));
        QCOMPARE(library.groups().first().name, u"Topics"_s);
        QCOMPARE(library.groupsOf(u"t-cat"_s), QStringList{u"g-2"_s});
        QVERIFY(library.moveGroup(u"g-1"_s, 10));
        QVERIFY(library.moveGroup(u"g-3"_s, -1));
        QStringList order;
        for (const TagLibrary::Group &group : library.groups()) {
            order.append(group.id);
        }
        QCOMPARE(order, (QStringList{u"g-3"_s, u"g-2"_s, u"g-1"_s}));
        QVERIFY(!library.moveGroup(u"g-gone"_s, 0));

        // Removing a group keeps its tags and their assignments.
        QVERIFY(library.removeGroup(u"g-2"_s));
        QVERIFY(!library.removeGroup(u"g-2"_s));
        QVERIFY(library.tag(u"t-cat"_s));
        QCOMPARE(library.usageCount(u"t-cat"_s), 1);
        QVERIFY(library.groupsOf(u"t-cat"_s).isEmpty());

        QVERIFY(library.save(&error));
        const TagLibrary::Library loaded = TagLibrary::Library::load(dir.path());
        QCOMPARE(loaded.groups().size(), 2);
        QCOMPARE(loaded.groups().first().id, u"g-3"_s);
        QCOMPARE(loaded.group(u"g-1"_s)->tags, QStringList{u"t-animal"_s});
    }

    void managesFieldsAndValues()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.fieldUsage(u"f-shot"_s), 1);

        // Types can't change while values exist, because values aren't converted.
        QString error;
        QVERIFY(!library.setField({u"f-shot"_s, u"Shot"_s, TagLibrary::FieldType::Text}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!library.setField({u"f-note"_s, u" Note"_s, TagLibrary::FieldType::Text}));
        QVERIFY(!library.setField({{}, u"Note"_s, TagLibrary::FieldType::Text}));
        QVERIFY(!library.setField({u"f-note"_s, u"title"_s, TagLibrary::FieldType::Text}));
        QVERIFY(library.setField({u"f-note"_s, u"Note"_s, TagLibrary::FieldType::Text}));
        QVERIFY(library.setField({u"f-note"_s, u"Due"_s, TagLibrary::FieldType::Date}));
        QVERIFY(library.setField({u"f-shot"_s, u"Taken"_s, TagLibrary::FieldType::Date}));
        QCOMPARE(library.fields().size(), 3);
        QCOMPARE(library.field(u"f-note"_s)->type, TagLibrary::FieldType::Date);

        QVERIFY(library.checkFieldValue(u"f-shot"_s, u"2026-03-01"_s).isEmpty());
        QVERIFY(library.checkFieldValue(u"f-shot"_s, QString()).isEmpty());
        QVERIFY(!library.checkFieldValue(u"f-shot"_s, u"2026-02-30"_s).isEmpty());
        QVERIFY(!library.checkFieldValue(u"f-shot"_s, u"1 March 2026"_s).isEmpty());
        QVERIFY(!library.checkFieldValue(u"f-gone"_s, u"x"_s).isEmpty());
        QVERIFY(!library.checkFieldValue(u"f-title"_s, QString(10001, u'x')).isEmpty());

        // An invalid value changes nothing.
        TagLibrary::Observation cat{u"photos/cat.jpg"_s, false, 12, 1700000000000, 0, 0};
        TagLibrary::Observation dog{u"photos/dog.jpg"_s, false, 3, 1700000000000, 0, 0};
        int changed = -1;
        QVERIFY(!library.setFieldValues({cat, dog}, {{u"f-title"_s, u"Rex"_s}, {u"f-shot"_s, u"soon"_s}}, &changed, &error));
        QVERIFY(!library.entry(u"photos/dog.jpg"_s));
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->fields.value(u"f-title"_s), u"Whiskers"_s);

        // Setting values indexes new items, clears empty values and keeps other fields.
        QVERIFY(library.setFieldValues({cat, dog, cat}, {{u"f-title"_s, u"Rex"_s}, {u"f-shot"_s, u" "_s}}, &changed, &error));
        QCOMPARE(changed, 2);
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->fields, (QMap<QString, QString>{{u"f-title"_s, u"Rex"_s}}));
        QCOMPARE(library.entry(u"photos/dog.jpg"_s)->fields.value(u"f-title"_s), u"Rex"_s);
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->tags, QStringList{u"t-cat"_s});
        QVERIFY(library.setFieldValues({cat}, {{u"f-title"_s, u"Rex"_s}}, &changed));
        QCOMPARE(changed, 0);
        QVERIFY(library.setFieldValues({dog}, {{u"f-note"_s, u"2026-12-01"_s}}, &changed));
        QVERIFY(library.save(&error));

        TagLibrary::Library loaded = TagLibrary::Library::load(dir.path());
        QCOMPARE(loaded.entry(u"photos/dog.jpg"_s)->fields.value(u"f-note"_s), u"2026-12-01"_s);
        QCOMPARE(loaded.field(u"f-shot"_s)->name, u"Taken"_s);

        // Removing a field removes its values only.
        QCOMPARE(loaded.removeField(u"f-title"_s), 2);
        QCOMPARE(loaded.removeField(u"f-title"_s), 0);
        QVERIFY(loaded.entry(u"photos/cat.jpg"_s)->fields.isEmpty());
        QCOMPARE(loaded.entry(u"photos/dog.jpg"_s)->fields.size(), 1);
        QCOMPARE(loaded.entry(u"photos/cat.jpg"_s)->tags, QStringList{u"t-cat"_s});
    }

    void assignsAndCountsTags()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        TagLibrary::Observation cat{u"photos/cat.jpg"_s, false, 12, 1700000000000, 1, 42};
        TagLibrary::Observation dog{u"photos/dog.jpg"_s, false, 5, 1700000000000, 1, 43};
        TagLibrary::Observation photos{u"photos"_s, true, 0, 0, 1, 2};

        // An unknown tag changes nothing.
        int changed = -1;
        QString error;
        QVERIFY(!library.assignTags({dog}, {u"t-missing"_s}, {}, &changed, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!library.entry(u"photos/dog.jpg"_s));

        // Unindexed items are indexed, and a missing entry found at its path is restored.
        QVERIFY(library.assignTags({cat, dog, photos, dog}, {u"t-animal"_s}, {}, &changed));
        QCOMPARE(changed, 3);
        QVERIFY(library.entry(u"photos/dog.jpg"_s));
        QVERIFY(!library.entry(u"photos"_s)->missing);
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->tags, (QStringList{u"t-cat"_s, u"t-animal"_s}));
        QVERIFY(library.assignTags({cat}, {u"t-animal"_s}, {}, &changed));
        QCOMPARE(changed, 0);

        const QStringList paths{u"photos/cat.jpg"_s, u"photos/dog.jpg"_s, u"photos/cat.jpg"_s, u"unknown"_s};
        QHash<QString, int> counts = library.tagCounts(paths);
        QCOMPARE(counts.value(u"t-animal"_s), 2);
        QCOMPARE(counts.value(u"t-cat"_s), 1);

        // Removing and adding at once; inherited tags aren't counted.
        QVERIFY(library.assignTags({cat, dog}, {u"t-cat"_s}, {u"t-animal"_s}, &changed));
        QCOMPARE(changed, 2);
        counts = library.tagCounts(paths);
        QCOMPARE(counts.value(u"t-cat"_s), 2);
        QVERIFY(!counts.contains(u"t-animal"_s));
        QCOMPARE(library.entry(u"photos/cat.jpg"_s)->tags, QStringList{u"t-cat"_s});
    }

    void searchesByTags()
    {
        QTemporaryDir dir;
        QVERIFY(writeDocument(dir.path(), sampleDocument()));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QVERIFY(library.setTag({u"t-dog"_s, u"Dog"_s, {}, {}, {}, {}}));
        const TagLibrary::Observation dog{u"photos/dog.jpg"_s, false, 5, 0, 1, 43};
        const TagLibrary::Observation plain{u"photos/plain.txt"_s, false, 5, 0, 1, 44};
        const TagLibrary::Observation gone{u"photos/gone.jpg"_s, false, 5, 0, 1, 45};
        QVERIFY(library.assignTags({dog}, {u"t-dog"_s}, {}));
        QVERIFY(library.assignTags({plain, gone}, {u"t-cat"_s}, {}));
        QVERIFY(library.assignTags({plain}, {}, {u"t-cat"_s}));
        QCOMPARE(library.markMissing(u"photos/gone.jpg"_s), 1);
        QVERIFY(library.save());

        TagSearch::Context context({dir.path()});
        const auto matches = [&context, &dir](const QString &text, const QString &relativePath) {
            const SearchQuery::Query query = SearchQuery::Query::parse(text);
            const QString path = dir.path() + u'/' + relativePath;
            return query.isValid() && query.matches(QFileInfo(path), {}, context.matcher(path));
        };
        const QString cat = u"photos/cat.jpg"_s;
        // Parent tags match items tagged with their children, unless the match must be exact.
        QVERIFY(matches(u"tag:Animal"_s, cat));
        QVERIFY(!matches(u"tag=Animal"_s, cat));
        QVERIFY(matches(u"tag=cat"_s, cat));
        QVERIFY(!matches(u"tag:Animal"_s, u"photos/dog.jpg"_s));
        // Aliases, shorthand and wildcards.
        QVERIFY(matches(u"tag:KITTY"_s, cat));
        QVERIFY(matches(u"tag:c"_s, cat));
        QVERIFY(matches(u"tag:kit*"_s, cat));
        QVERIFY(matches(u"tag:ani*"_s, cat));
        QVERIFY(!matches(u"tag=ani*"_s, cat));
        QVERIFY(!matches(u"tag:\"kit*\""_s, cat));
        QVERIFY(!matches(u"tag:zebra"_s, cat));
        QVERIFY(matches(u"tag:dog OR tag:cat"_s, u"photos/dog.jpg"_s));
        QVERIFY(matches(u"tag:cat NOT tag:dog"_s, cat));
        // Untagged items: indexed or not, and entries whose file is missing, but only inside the library.
        QVERIFY(matches(u"tagged:yes"_s, cat));
        QVERIFY(!matches(u"tagged:no"_s, cat));
        QVERIFY(matches(u"tagged:no"_s, u"photos/plain.txt"_s));
        QVERIFY(matches(u"tagged:no"_s, u"photos/new.txt"_s));
        QVERIFY(matches(u"tagged:no"_s, u"photos/gone.jpg"_s));
        QVERIFY(!matches(u"tag:cat"_s, u"photos/gone.jpg"_s));
        QVERIFY(!matches(u"tagged:no"_s, u"../outside.txt"_s));
        QVERIFY(!matches(u"tagged:no"_s, QString()));
        QVERIFY(!matches(u"tagged:no"_s, TagLibrary::fileName()));
        QVERIFY(context.errors().isEmpty());

        // Without child tags, tag: selects only the tags it names, like tag=.
        TagSearch::Context direct({dir.path()}, {false});
        const auto matchesDirectly = [&direct, &dir](const QString &text, const QString &relativePath) {
            const SearchQuery::Query query = SearchQuery::Query::parse(text);
            const QString path = dir.path() + u'/' + relativePath;
            return query.isValid() && query.matches(QFileInfo(path), {}, direct.matcher(path));
        };
        QVERIFY(!matchesDirectly(u"tag:Animal"_s, cat));
        QVERIFY(!matchesDirectly(u"tag:ani*"_s, cat));
        QVERIFY(matchesDirectly(u"tag:cat"_s, cat));
        QVERIFY(matchesDirectly(u"tag:KITTY"_s, cat));
        QVERIFY(matchesDirectly(u"tag:kit*"_s, cat));
        QVERIFY(matchesDirectly(u"tag=cat"_s, cat));
        QVERIFY(matchesDirectly(u"tagged:yes"_s, cat));
        QVERIFY(!matchesDirectly(u"tag:Animal"_s, u"photos/dog.jpg"_s));
        QVERIFY(direct.errors().isEmpty());
        SearchQuery::Term animal;
        animal.field = SearchQuery::Field::Tag;
        animal.text = u"Animal"_s;
        QCOMPARE(TagSearch::selectedTags(library, animal), (QSet<QString>{u"t-animal"_s, u"t-cat"_s}));
        QCOMPARE(TagSearch::selectedTags(library, animal, {false}), QSet<QString>{u"t-animal"_s});

        const auto matchesAt = [](TagSearch::Context &in, const QString &text, const QString &path) {
            const SearchQuery::Query query = SearchQuery::Query::parse(text);
            return query.isValid() && query.matches(QFileInfo(path), {}, in.matcher(path));
        };
        const QString catPath = dir.filePath(cat);

        // Without aliases, tag: and tag= match tag names only, also with wildcards; child tags still match.
        TagSearch::Context namesOnly({dir.path()}, {.aliases = false});
        QVERIFY(!matchesAt(namesOnly, u"tag:KITTY"_s, catPath));
        QVERIFY(!matchesAt(namesOnly, u"tag=kitty"_s, catPath));
        QVERIFY(!matchesAt(namesOnly, u"tag:c"_s, catPath));
        QVERIFY(!matchesAt(namesOnly, u"tag:kit*"_s, catPath));
        QVERIFY(matchesAt(namesOnly, u"tag:CAT"_s, catPath));
        QVERIFY(matchesAt(namesOnly, u"tag=cat"_s, catPath));
        QVERIFY(matchesAt(namesOnly, u"tag:ca*"_s, catPath));
        QVERIFY(matchesAt(namesOnly, u"tag:Animal"_s, catPath));
        QVERIFY(matchesAt(namesOnly, u"tag:ani*"_s, catPath));
        QVERIFY(namesOnly.errors().isEmpty());
        SearchQuery::Term kitty;
        kitty.field = SearchQuery::Field::Tag;
        kitty.text = u" Kitty "_s;
        QCOMPARE(TagSearch::selectedTags(library, kitty), QSet<QString>{u"t-cat"_s});
        QVERIFY(TagSearch::selectedTags(library, kitty, {.aliases = false}).isEmpty());

        // tagged:no can also match indexable items outside every library; no other tag term does.
        QTemporaryDir outside;
        const QString loose = outside.filePath(u"loose.txt"_s);
        TagSearch::Context wide({dir.path()}, {.untaggedOutsideLibraries = true});
        QVERIFY(!matchesAt(context, u"tagged:no"_s, loose));
        QVERIFY(matchesAt(wide, u"tagged:no"_s, loose));
        QVERIFY(matchesAt(wide, u"NOT tagged:yes"_s, loose));
        QVERIFY(!matchesAt(wide, u"tagged:yes"_s, loose));
        QVERIFY(!matchesAt(wide, u"tag:cat OR tag=cat OR tag:c*"_s, loose));
        QVERIFY(!matchesAt(wide, u"tagged:no"_s, outside.filePath(TagLibrary::fileName())));
        // Inside the library nothing changes, and the library folder itself still doesn't match.
        QVERIFY(matchesAt(wide, u"tagged:no"_s, dir.filePath(u"photos/plain.txt"_s)));
        QVERIFY(!matchesAt(wide, u"tagged:no"_s, catPath));
        QVERIFY(!matchesAt(wide, u"tagged:no"_s, dir.path()));
        QVERIFY(!matchesAt(wide, u"tagged:no"_s, dir.path() + u'/'));
        QVERIFY(!matchesAt(wide, u"tagged:no"_s, dir.filePath(TagLibrary::fileName())));
        QVERIFY(wide.errors().isEmpty());

        // A library that can't be read is reported once and matches nothing.
        QTemporaryDir empty;
        TagSearch::Context broken({empty.path()});
        const QString path = empty.filePath(u"a.txt"_s);
        const SearchQuery::Query untagged = SearchQuery::Query::parse(u"tagged:no"_s);
        QVERIFY(!untagged.matches(QFileInfo(path), {}, broken.matcher(path)));
        QVERIFY(!untagged.matches(QFileInfo(path), {}, broken.matcher(path)));
        QCOMPARE(broken.errors().size(), 1);
        // An unreadable library still holds its items, so tagged:no doesn't match them as outside.
        TagSearch::Context brokenWide({empty.path()}, {.untaggedOutsideLibraries = true});
        QVERIFY(!untagged.matches(QFileInfo(path), {}, brokenWide.matcher(path)));
    }

    void choosesTagSearchRoots()
    {
        using TagSearch::Scope;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &folder : {u"library/nested/inner"_s, u"library2"_s, u"other"_s}) {
            QVERIFY(QDir(dir.path()).mkpath(folder));
        }
        const QString library = dir.filePath(u"library"_s);
        const QString nested = dir.filePath(u"library/nested"_s);
        const QString inner = dir.filePath(u"library/nested/inner"_s);
        const QStringList roots{library, nested, dir.filePath(u"missing"_s), u"relative"_s};
        const SearchQuery::Query tagged = SearchQuery::Query::parse(u"report tagged:no"_s);
        QVERIFY(tagged.isValid());

        // The current folder by default, and for queries without tag criteria.
        QCOMPARE(TagSearch::searchRoot(inner, tagged, roots, Scope::CurrentFolder), inner);
        QCOMPARE(TagSearch::searchRoot(inner, SearchQuery::Query::parse(u"report OR type:file"_s), roots, Scope::LibraryRoot), inner);
        QCOMPARE(TagSearch::searchRoot(inner, SearchQuery::Query::parse(u"\"tag:x\""_s), roots, Scope::LibraryRoot), inner);
        QCOMPARE(TagSearch::searchRoot(inner, SearchQuery::Query::parse(u"tag:"_s), roots, Scope::LibraryRoot), inner);
        QCOMPARE(TagSearch::searchRoot(inner, tagged, {}, Scope::LibraryRoot), inner);

        // Otherwise the deepest library that contains or is the folder, with any tag criterion.
        QCOMPARE(TagSearch::searchRoot(inner, tagged, roots, Scope::LibraryRoot), nested);
        QCOMPARE(TagSearch::searchRoot(inner + u'/', tagged, roots, Scope::LibraryRoot), nested);
        QCOMPARE(TagSearch::searchRoot(inner, SearchQuery::Query::parse(u"NOT tag=x"_s), roots, Scope::LibraryRoot), nested);
        QCOMPARE(TagSearch::searchRoot(inner, SearchQuery::Query::parse(u"name:a OR tags:b*"_s), roots, Scope::LibraryRoot), nested);
        QCOMPARE(TagSearch::searchRoot(nested, tagged, roots, Scope::LibraryRoot), nested);
        QCOMPARE(TagSearch::searchRoot(dir.filePath(u"library/x"_s), tagged, roots, Scope::LibraryRoot), library);
        QCOMPARE(TagSearch::searchRoot(library, tagged, roots, Scope::LibraryRoot), library);

        // Folders outside every library, including a sibling whose name starts like one, stay as they are.
        QCOMPARE(TagSearch::searchRoot(dir.filePath(u"library2"_s), tagged, roots, Scope::LibraryRoot), dir.filePath(u"library2"_s));
        QCOMPARE(TagSearch::searchRoot(dir.filePath(u"other"_s), tagged, roots, Scope::LibraryRoot), dir.filePath(u"other"_s));
        QCOMPARE(TagSearch::searchRoot(dir.path(), tagged, roots, Scope::LibraryRoot), dir.path());
        QCOMPARE(TagSearch::searchRoot(u"relative/sub"_s, tagged, roots, Scope::LibraryRoot), u"relative/sub"_s);

        // Library folders that are missing or symbolic links can't be searched from.
        QCOMPARE(TagSearch::searchRoot(dir.filePath(u"missing/sub"_s), tagged, roots, Scope::LibraryRoot), dir.filePath(u"missing/sub"_s));
        const QString link = dir.filePath(u"link"_s);
        QVERIFY(QFile::link(library, link));
        QCOMPARE(TagSearch::searchRoot(link + u"/nested"_s, tagged, {link}, Scope::LibraryRoot), link + u"/nested"_s);
    }
};

QTEST_GUILESS_MAIN(TagLibraryTest)

#include "taglibrarytest.moc"
