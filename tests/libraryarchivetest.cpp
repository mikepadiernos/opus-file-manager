// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryarchive.h"
#include "libraryscan.h"
#include "taglibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace
{
QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeAll(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QJsonObject readObject(const QString &path)
{
    return QJsonDocument::fromJson(readAll(path)).object();
}

const LibraryArchive::Change *findChange(const LibraryArchive::Plan &plan, LibraryArchive::Kind kind, LibraryArchive::Action action, const QString &name)
{
    for (const LibraryArchive::Change &change : plan.changes) {
        if (change.kind == kind && change.action == action && change.name == name) {
            return &change;
        }
    }
    return nullptr;
}
}

class LibraryArchiveTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void exportsTheLibraryDocument();
    void checksDestinations();
    void previewsAndRestores();
    void marksEntriesMissingFromTheFolder();
    void refusesWhenChangedSincePreview();
    void replacesAnUnreadableLibrary();
    void restoresCopiesOfLibraryDocuments();
    void refusesInvalidExports_data();
    void refusesInvalidExports();
    void restoresPrivateLibraries();
    void prunesOnlyOlderBackupsOfTheLibrary();

private:
    QString path(const QString &relative) const;
    /// The user files in the library with their contents, to check that nothing touches them.
    QHash<QString, QByteArray> userFiles() const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QTemporaryDir> m_exports;
    QString m_root;
};

void LibraryArchiveTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

QString LibraryArchiveTest::path(const QString &relative) const
{
    return QDir(m_root).filePath(relative);
}

QHash<QString, QByteArray> LibraryArchiveTest::userFiles() const
{
    QHash<QString, QByteArray> files;
    for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("photos/cat.jpg"), QStringLiteral("photos/dog.jpg")}) {
        files.insert(name, readAll(path(name)));
    }
    return files;
}

void LibraryArchiveTest::init()
{
    QDir(LibraryArchive::backupDirectory()).removeRecursively();
    QDir(QFileInfo(TagLibrary::privateDocumentPath(QStringLiteral("/"))).absolutePath()).removeRecursively();
    m_dir = std::make_unique<QTemporaryDir>();
    m_exports = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid() && m_exports->isValid());
    m_root = QDir::cleanPath(m_dir->path());
    QVERIFY(QDir(m_root).mkpath(QStringLiteral("photos")));
    QVERIFY(writeAll(path(QStringLiteral("a.txt")), "alpha"));
    QVERIFY(writeAll(path(QStringLiteral("photos/cat.jpg")), "cat"));
    QVERIFY(writeAll(path(QStringLiteral("photos/dog.jpg")), "dog"));

    TagLibrary::Library library(m_root);
    QList<TagLibrary::Observation> items;
    for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("photos"), QStringLiteral("photos/cat.jpg"), QStringLiteral("photos/dog.jpg")}) {
        TagLibrary::Observation observation;
        QVERIFY(LibraryScan::observe(m_root, path(name), &observation));
        items.append(observation);
    }
    QString error;
    library.applyScan(items, true);
    QVERIFY(library.setTag({QStringLiteral("animal"), QStringLiteral("Animal"), {}, {}, QStringLiteral("#336699"), {}}, &error));
    QVERIFY(
        library.setTag({QStringLiteral("cat"), QStringLiteral("Cat"), {QStringLiteral("Kitty")}, QStringLiteral("c"), {}, {QStringLiteral("animal")}}, &error));
    QVERIFY(library.setTag({QStringLiteral("dog"), QStringLiteral("Dog"), {}, {}, {}, {QStringLiteral("animal")}}, &error));
    QVERIFY(library.setGroup({QStringLiteral("pets"), QStringLiteral("Pets"), {}, {QStringLiteral("cat"), QStringLiteral("dog")}}, &error));
    QVERIFY(library.setField({QStringLiteral("title"), QStringLiteral("Title"), TagLibrary::FieldType::Text}, &error));
    QVERIFY(library.assignTags({items.at(2)}, {QStringLiteral("cat")}, {}, nullptr, &error));
    QVERIFY(library.assignTags({items.at(3)}, {QStringLiteral("dog")}, {}, nullptr, &error));
    QVERIFY(library.setFieldValues({items.at(2)}, {{QStringLiteral("title"), QStringLiteral("Tom")}}, nullptr, &error));
    QVERIFY2(library.save(&error), qPrintable(error));
}

void LibraryArchiveTest::cleanup()
{
    QDir(LibraryArchive::backupDirectory()).removeRecursively();
    QDir(QFileInfo(TagLibrary::privateDocumentPath(QStringLiteral("/"))).absolutePath()).removeRecursively();
}

void LibraryArchiveTest::exportsTheLibraryDocument()
{
    const auto files = userFiles();
    const TagLibrary::Library library = TagLibrary::Library::load(m_root);
    QVERIFY(!library.isNull());
    const QByteArray document = readAll(library.filePath());

    const QString destination = QDir(m_exports->path()).filePath(LibraryArchive::suggestedFileName(m_root, QDate(2026, 11, 25)));
    QVERIFY(destination.endsWith(QStringLiteral(" library 2026-11-25.json")));
    QString error;
    QVERIFY2(LibraryArchive::write(library, destination, &error), qPrintable(error));

    const QJsonObject exported = readObject(destination);
    QCOMPARE(exported.value(QStringLiteral("format")).toString(), LibraryArchive::formatName());
    QCOMPARE(exported.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(exported.value(QStringLiteral("root")).toString(), m_root);
    QVERIFY(QDateTime::fromString(exported.value(QStringLiteral("exported")).toString(), Qt::ISODate).isValid());
    QCOMPARE(exported.value(QStringLiteral("library")).toObject(), library.toJson());

    const LibraryArchive::Summary summary = LibraryArchive::summarize(library);
    QCOMPARE(summary.tags, 3);
    QCOMPARE(summary.groups, 1);
    QCOMPARE(summary.fields, 1);
    QCOMPARE(summary.entries, 4);
    QCOMPARE(summary.itemsWithMetadata, 2);
    QCOMPARE(summary.missing, 0);

    // Exporting reads the library only.
    QCOMPARE(readAll(library.filePath()), document);
    QCOMPARE(userFiles(), files);
}

void LibraryArchiveTest::checksDestinations()
{
    const QString folder = m_exports->path();
    bool replaces = true;
    QVERIFY(LibraryArchive::checkDestination(QDir(folder).filePath(QStringLiteral("new.json")), &replaces).isEmpty());
    QVERIFY(!replaces);
    QVERIFY(!LibraryArchive::checkDestination(QStringLiteral("relative.json")).isEmpty());
    QVERIFY(!LibraryArchive::checkDestination(QString()).isEmpty());
    QVERIFY(!LibraryArchive::checkDestination(QDir(folder).filePath(QStringLiteral("missing/new.json"))).isEmpty());
    QVERIFY(!LibraryArchive::checkDestination(QDir(folder).filePath(TagLibrary::fileName())).isEmpty());
    QVERIFY(!LibraryArchive::checkDestination(path(TagLibrary::fileName())).isEmpty());
    QVERIFY(!LibraryArchive::checkDestination(folder).isEmpty());

    // Other files are never replaced, not even other JSON documents.
    const QString other = QDir(folder).filePath(QStringLiteral("notes.json"));
    QVERIFY(writeAll(other, "{\"keep\": true}"));
    QVERIFY(!LibraryArchive::checkDestination(other).isEmpty());
    const TagLibrary::Library library = TagLibrary::Library::load(m_root);
    QVERIFY(!LibraryArchive::write(library, other));
    QCOMPARE(readAll(other), QByteArray("{\"keep\": true}"));

    // Earlier exports may be replaced.
    const QString earlier = QDir(folder).filePath(QStringLiteral("earlier.json"));
    QVERIFY(LibraryArchive::write(library, earlier));
    QVERIFY(LibraryArchive::checkDestination(earlier, &replaces).isEmpty());
    QVERIFY(replaces);
    QVERIFY(LibraryArchive::write(library, earlier));

    const QString link = QDir(folder).filePath(QStringLiteral("link.json"));
    QVERIFY(QFile::link(earlier, link));
    QVERIFY(!LibraryArchive::checkDestination(link).isEmpty());
}

void LibraryArchiveTest::previewsAndRestores()
{
    const QString exported = QDir(m_exports->path()).filePath(QStringLiteral("export.json"));
    TagLibrary::Library original = TagLibrary::Library::load(m_root);
    QVERIFY(LibraryArchive::write(original, exported));
    const QJsonObject originalJson = original.toJson();

    // Change the library after the export.
    QString error;
    QVERIFY2(TagLibrary::update(
                 m_root,
                 [](TagLibrary::Library &library, QString *problem) {
                     TagLibrary::Tag cat = *library.tag(QStringLiteral("cat"));
                     cat.name = QStringLiteral("Kitten");
                     library.removeTag(QStringLiteral("dog"));
                     library.removeGroup(QStringLiteral("pets"));
                     library.removeField(QStringLiteral("title"));
                     return library.setTag(cat, problem) && library.setTag({QStringLiteral("bird"), QStringLiteral("Bird"), {}, {}, {}, {}}, problem)
                         && library.setField({QStringLiteral("place"), QStringLiteral("Place"), TagLibrary::FieldType::Text}, problem);
                 },
                 &error),
             qPrintable(error));
    const QByteArray changed = readAll(path(TagLibrary::fileName()));
    const auto files = userFiles();

    LibraryArchive::Plan plan = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY2(!plan.isNull(), qPrintable(error));
    QVERIFY(plan.hasChanges());
    QVERIFY(plan.currentProblem.isEmpty());
    QCOMPARE(plan.exportedRoot, m_root);
    QVERIFY(plan.exported.isValid());
    QCOMPARE(plan.notFound, 0);
    QCOMPARE(plan.before.tags, 3);
    QCOMPARE(plan.after.tags, 3);
    QCOMPARE(plan.before.groups, 0);
    QCOMPARE(plan.after.groups, 1);

    using LibraryArchive::Action;
    using LibraryArchive::Kind;
    const LibraryArchive::Change *renamed = findChange(plan, Kind::Tag, Action::Change, QStringLiteral("Cat"));
    QVERIFY(renamed);
    QVERIFY(renamed->detail.contains(QStringLiteral("Kitten")));
    QVERIFY(findChange(plan, Kind::Tag, Action::Add, QStringLiteral("Dog")));
    QVERIFY(findChange(plan, Kind::Tag, Action::Remove, QStringLiteral("Bird")));
    QVERIFY(findChange(plan, Kind::Group, Action::Add, QStringLiteral("Pets")));
    QVERIFY(findChange(plan, Kind::Field, Action::Add, QStringLiteral("Title")));
    QVERIFY(findChange(plan, Kind::Field, Action::Remove, QStringLiteral("Place")));
    const LibraryArchive::Change *cat = findChange(plan, Kind::Item, Action::Change, QStringLiteral("photos/cat.jpg"));
    QVERIFY(cat);
    QVERIFY(cat->detail.contains(QStringLiteral("Title: Tom")));
    QVERIFY(findChange(plan, Kind::Item, Action::Add, QStringLiteral("photos/dog.jpg")));
    QCOMPARE(plan.count(Kind::Item, Action::Remove), 0);

    // A preview writes nothing.
    QCOMPARE(readAll(path(TagLibrary::fileName())), changed);
    QVERIFY(!QFileInfo::exists(LibraryArchive::backupDirectory()));

    QString backup;
    QVERIFY2(LibraryArchive::restore(plan, &backup, &error), qPrintable(error));
    QVERIFY(!backup.isEmpty());
    QVERIFY(backup.startsWith(LibraryArchive::backupDirectory()));
    QCOMPARE(readAll(backup), changed);
    QCOMPARE(TagLibrary::Library::load(m_root).toJson(), originalJson);
    QCOMPARE(userFiles(), files);

    // Restoring again changes nothing.
    plan = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY(!plan.isNull());
    QVERIFY(!plan.hasChanges());
    QVERIFY(plan.changes.isEmpty());
}

void LibraryArchiveTest::marksEntriesMissingFromTheFolder()
{
    const QString exported = QDir(m_exports->path()).filePath(QStringLiteral("export.json"));
    QVERIFY(LibraryArchive::write(TagLibrary::Library::load(m_root), exported));
    QVERIFY(QFile::remove(path(QStringLiteral("photos/dog.jpg"))));
    // A folder where the file was isn't the same item.
    QVERIFY(QDir(m_root).mkdir(QStringLiteral("photos/dog.jpg")));
    QVERIFY(QFile::rename(path(QStringLiteral("a.txt")), path(QStringLiteral("b.txt"))));

    QString error;
    const LibraryArchive::Plan plan = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY2(!plan.isNull(), qPrintable(error));
    QCOMPARE(plan.notFound, 2);
    QCOMPARE(plan.after.missing, 2);
    QVERIFY(plan.restored.entry(QStringLiteral("photos/dog.jpg"))->missing);
    QVERIFY(plan.restored.entry(QStringLiteral("a.txt"))->missing);
    QVERIFY(!plan.restored.entry(QStringLiteral("photos/cat.jpg"))->missing);
    QVERIFY(plan.hasChanges());
    QVERIFY(LibraryArchive::restore(plan, nullptr, &error));
    const TagLibrary::Library restored = TagLibrary::Library::load(m_root);
    QVERIFY(restored.entry(QStringLiteral("photos/dog.jpg"))->missing);
    QCOMPARE(restored.entry(QStringLiteral("photos/dog.jpg"))->tags, QStringList{QStringLiteral("dog")});
}

void LibraryArchiveTest::refusesWhenChangedSincePreview()
{
    const QString exported = QDir(m_exports->path()).filePath(QStringLiteral("export.json"));
    QVERIFY(LibraryArchive::write(TagLibrary::Library::load(m_root), exported));
    QVERIFY(TagLibrary::update(m_root, [](TagLibrary::Library &library, QString *) {
        library.removeTag(QStringLiteral("dog"));
        return true;
    }));
    QString error;
    const LibraryArchive::Plan plan = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY(plan.hasChanges());

    QVERIFY(TagLibrary::update(m_root, [](TagLibrary::Library &library, QString *) {
        library.removeTag(QStringLiteral("cat"));
        return true;
    }));
    const QByteArray current = readAll(path(TagLibrary::fileName()));
    QVERIFY(!LibraryArchive::restore(plan, nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("Preview the restore again")));
    QCOMPARE(readAll(path(TagLibrary::fileName())), current);
    QVERIFY(QDir(LibraryArchive::backupDirectory()).isEmpty());

    QVERIFY(!LibraryArchive::restore({}, nullptr, &error));
}

void LibraryArchiveTest::replacesAnUnreadableLibrary()
{
    const QString exported = QDir(m_exports->path()).filePath(QStringLiteral("export.json"));
    const TagLibrary::Library original = TagLibrary::Library::load(m_root);
    QVERIFY(LibraryArchive::write(original, exported));
    QVERIFY(writeAll(path(TagLibrary::fileName()), "{ broken"));

    QString error;
    const LibraryArchive::Plan plan = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY2(!plan.isNull(), qPrintable(error));
    QVERIFY(!plan.currentProblem.isEmpty());
    QVERIFY(plan.hasChanges());
    QCOMPARE(plan.count(LibraryArchive::Kind::Tag, LibraryArchive::Action::Add), 3);

    QString backup;
    QVERIFY2(LibraryArchive::restore(plan, &backup, &error), qPrintable(error));
    QCOMPARE(readAll(backup), QByteArray("{ broken"));
    QCOMPARE(TagLibrary::Library::load(m_root).toJson(), original.toJson());

    // A library whose document was deleted is recreated without a backup.
    QVERIFY(QFile::remove(path(TagLibrary::fileName())));
    const LibraryArchive::Plan recreate = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY(recreate.hasChanges());
    QVERIFY(LibraryArchive::restore(recreate, &backup, &error));
    QVERIFY(backup.isEmpty());
    QCOMPARE(TagLibrary::Library::load(m_root).toJson(), original.toJson());
}

void LibraryArchiveTest::restoresCopiesOfLibraryDocuments()
{
    const QString copy = QDir(m_exports->path()).filePath(QStringLiteral("copy.json"));
    QVERIFY(QFile::copy(path(TagLibrary::fileName()), copy));
    const QJsonObject original = TagLibrary::Library::load(m_root).toJson();
    QVERIFY(TagLibrary::update(m_root, [](TagLibrary::Library &library, QString *) {
        library.removeTag(QStringLiteral("cat"));
        return true;
    }));

    QString error;
    const LibraryArchive::Plan plan = LibraryArchive::preview(m_root, copy, &error);
    QVERIFY2(!plan.isNull(), qPrintable(error));
    QVERIFY(plan.exportedRoot.isEmpty());
    QVERIFY(plan.exported.isValid());
    QVERIFY(findChange(plan, LibraryArchive::Kind::Tag, LibraryArchive::Action::Add, QStringLiteral("Cat")));
    QVERIFY(LibraryArchive::restore(plan, nullptr, &error));
    QCOMPARE(TagLibrary::Library::load(m_root).toJson(), original);
}

void LibraryArchiveTest::refusesInvalidExports_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::addRow("not JSON") << QByteArray("not json");
    QTest::addRow("array") << QByteArray("[]");
    QTest::addRow("other format") << QByteArray("{\"format\": \"something-else\", \"version\": 1}");
    QTest::addRow("newer version") << QByteArray("{\"format\": \"plasma-opus-library-export\", \"version\": 2, \"library\": {}}");
    QTest::addRow("fractional version") << QByteArray("{\"format\": \"plasma-opus-library-export\", \"version\": 1.5, \"library\": {}}");
    QTest::addRow("no library") << QByteArray("{\"format\": \"plasma-opus-library-export\", \"version\": 1}");
    QTest::addRow("invalid library") << QByteArray(
        "{\"format\": \"plasma-opus-library-export\", \"version\": 1, \"library\": {\"format\": \"plasma-opus-library\", \"version\": 1, \"entries\": "
        "[{\"id\": "
        "\"x\", \"path\": \"../outside\", \"kind\": \"file\"}]}}");
}

void LibraryArchiveTest::refusesInvalidExports()
{
    QFETCH(QByteArray, content);
    const QString source = QDir(m_exports->path()).filePath(QStringLiteral("invalid.json"));
    QVERIFY(writeAll(source, content));
    const QByteArray document = readAll(path(TagLibrary::fileName()));
    QString error;
    QVERIFY(LibraryArchive::preview(m_root, source, &error).isNull());
    QVERIFY(!error.isEmpty());
    QCOMPARE(readAll(path(TagLibrary::fileName())), document);

    QVERIFY(LibraryArchive::preview(m_root, QDir(m_exports->path()).filePath(QStringLiteral("absent.json")), &error).isNull());
}

void LibraryArchiveTest::restoresPrivateLibraries()
{
    QCOMPARE(TagLibrary::moveDocument(m_root, TagLibrary::Storage::PrivateFolder), QString());
    QVERIFY(!QFileInfo::exists(path(TagLibrary::fileName())));
    const QString exported = QDir(m_exports->path()).filePath(QStringLiteral("export.json"));
    const TagLibrary::Library original = TagLibrary::Library::load(m_root);
    QVERIFY(LibraryArchive::write(original, exported));
    QVERIFY(!readObject(exported).value(QStringLiteral("library")).toObject().contains(QStringLiteral("root")));
    QVERIFY(TagLibrary::update(m_root, [](TagLibrary::Library &library, QString *) {
        library.removeTag(QStringLiteral("cat"));
        return true;
    }));
    const QByteArray changed = readAll(TagLibrary::privateDocumentPath(m_root));
    QCOMPARE(readObject(TagLibrary::privateDocumentPath(m_root)).value(QStringLiteral("root")).toString(), m_root);

    // A configured backup folder is used instead of the default one.
    const QString folder = QDir(m_exports->path()).filePath(QStringLiteral("backups"));
    QCOMPARE(LibraryArchive::backupDirectory(folder + QStringLiteral("/")), folder);
    QCOMPARE(LibraryArchive::backupDirectory(QStringLiteral("relative")), LibraryArchive::backupDirectory());
    QVERIFY(LibraryArchive::backupDirectory().endsWith(QStringLiteral("/library-backups")));

    QString error;
    const LibraryArchive::Plan plan = LibraryArchive::preview(m_root, exported, &error);
    QVERIFY2(!plan.isNull(), qPrintable(error));
    QVERIFY(plan.hasChanges());
    QString backup;
    QVERIFY2(LibraryArchive::restore(plan, {folder, 20}, &backup, &error), qPrintable(error));
    QVERIFY(backup.startsWith(folder + QStringLiteral("/")));
    QCOMPARE(readAll(backup), changed);
    QVERIFY(!QFileInfo::exists(LibraryArchive::backupDirectory()));
    // The private document is restored in place; nothing is written to the library folder.
    QVERIFY(TagLibrary::isPrivate(m_root));
    QVERIFY(!QFileInfo::exists(path(TagLibrary::fileName())));
    QCOMPARE(TagLibrary::Library::load(m_root).toJson(), original.toJson());
}

void LibraryArchiveTest::prunesOnlyOlderBackupsOfTheLibrary()
{
    const QString folder = QDir(m_exports->path()).filePath(QStringLiteral("backups"));
    QVERIFY(QDir().mkpath(folder));
    const QString name = QFileInfo(m_root).fileName();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(m_root.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
    const auto backupPath = [&](const QString &stamp, const QString &tag) {
        return QDir(folder).filePath(QStringLiteral("%1 %2 %3.json").arg(name, stamp, tag));
    };
    // Files that aren't backups of this library are never deleted.
    const QStringList unrelated{QDir(folder).filePath(QStringLiteral("notes.txt")),
                                backupPath(QStringLiteral("2020-01-01 000000"), QStringLiteral("0badc0de")),
                                QDir(folder).filePath(name + QStringLiteral(" 2020-01-01 000000.json")),
                                backupPath(QStringLiteral("2020-01-01 000000"), hash)};
    for (const QString &file : unrelated.mid(0, 3)) {
        QVERIFY(writeAll(file, "{}"));
    }
    // A copy named like this library's backups but recording another root.
    QVERIFY(writeAll(unrelated.at(3), "{\"format\": \"plasma-opus-library\", \"version\": 1, \"root\": \"/elsewhere\"}"));
    const QString link = backupPath(QStringLiteral("2019-01-01 000000"), hash);
    QVERIFY(QFile::link(unrelated.at(0), link));
    const QString oldest = backupPath(QStringLiteral("2020-01-01 000001"), hash);
    QVERIFY(writeAll(oldest, "{}"));

    QString error;
    const QString exported = QDir(m_exports->path()).filePath(QStringLiteral("export.json"));
    QVERIFY(LibraryArchive::write(TagLibrary::Library::load(m_root), exported));
    QStringList backups;
    for (int i = 0; i < 4; ++i) {
        QVERIFY(TagLibrary::update(m_root, [i](TagLibrary::Library &library, QString *error) {
            return library.setTag({QStringLiteral("t%1").arg(i), QStringLiteral("T%1").arg(i), {}, {}, {}, {}}, error);
        }));
        const LibraryArchive::Plan plan = LibraryArchive::preview(m_root, exported, &error);
        QVERIFY2(!plan.isNull(), qPrintable(error));
        QString backup;
        QVERIFY2(LibraryArchive::restore(plan, {folder, 3}, &backup, &error), qPrintable(error));
        QVERIFY(QFileInfo(backup).fileName().contains(hash));
        backups.append(backup);
    }

    const QRegularExpression ours(QStringLiteral(" %1( \\(\\d+\\))?\\.json$").arg(hash));
    QStringList kept;
    for (const QFileInfo &info : QDir(folder).entryInfoList(QDir::Files | QDir::NoSymLinks)) {
        if (ours.match(info.fileName()).hasMatch() && info.absoluteFilePath() != unrelated.at(3)) {
            kept.append(info.absoluteFilePath());
        }
    }
    kept.sort();
    QStringList newest = backups.mid(1);
    newest.sort();
    QCOMPARE(kept, newest);
    QVERIFY(!QFileInfo::exists(oldest));
    for (const QString &file : unrelated) {
        QVERIFY2(QFileInfo::exists(file), qPrintable(file));
    }
    QVERIFY(QFileInfo(link).isSymLink());

    // A limit of 0 keeps every backup.
    QCOMPARE(LibraryArchive::pruneBackups(folder, m_root, 0), 0);
    QCOMPARE(LibraryArchive::pruneBackups(folder, m_root, 1, backups.last()), 2);
    QVERIFY(QFileInfo::exists(backups.last()));
    for (const QString &file : unrelated) {
        QVERIFY(QFileInfo::exists(file));
    }
}

QTEST_GUILESS_MAIN(LibraryArchiveTest)

#include "libraryarchivetest.moc"
