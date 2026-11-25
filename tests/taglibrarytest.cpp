// SPDX-License-Identifier: GPL-3.0-or-later

#include "taglibrary.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

private Q_SLOTS:
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
};

QTEST_GUILESS_MAIN(TagLibraryTest)

#include "taglibrarytest.moc"
