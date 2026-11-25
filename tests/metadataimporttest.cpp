// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataimport.h"
#include "embeddedmetadata.h"
#include "libraryregistry.h"
#include "tagassignment.h"
#include "taglibrary.h"

#include <KPluginMetaData>

#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

namespace
{
bool hasExtractor(const QString &name)
{
    const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfilemetadata"));
    return std::any_of(plugins.cbegin(), plugins.cend(), [&name](const KPluginMetaData &plugin) {
        return plugin.pluginId().contains(name);
    });
}

bool run(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    return process.waitForStarted() && process.waitForFinished(15000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void touch(const QString &path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
}

EmbeddedMetadata::Value value(const QString &key, const QStringList &items, const QDate &date = {})
{
    EmbeddedMetadata::Value result;
    result.key = key;
    result.name = key;
    result.items = items;
    result.text = items.join(QStringLiteral("; "));
    result.date = date;
    return result;
}

EmbeddedMetadata::Reading reading(const QString &path, const QList<EmbeddedMetadata::Value> &values)
{
    EmbeddedMetadata::Reading result;
    result.path = path;
    result.supported = true;
    result.values = values;
    return result;
}

const EmbeddedMetadata::Value *find(const EmbeddedMetadata::Reading &reading, const QString &key)
{
    const auto it = std::find_if(reading.values.cbegin(), reading.values.cend(), [&key](const EmbeddedMetadata::Value &value) {
        return value.key == key;
    });
    return it == reading.values.cend() ? nullptr : &*it;
}
}

class MetadataImportTest : public QObject
{
    Q_OBJECT

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
        TagAssignment::invalidate();
    }

    void readsImageAndAudioMetadata()
    {
        const QString exiftool = QStandardPaths::findExecutable(QStringLiteral("exiftool"));
        const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (!hasExtractor(QStringLiteral("exiv2")) || !hasExtractor(QStringLiteral("taglib")) || exiftool.isEmpty() || ffmpeg.isEmpty()) {
            QSKIP("The EXIF and audio extractors, ExifTool and ffmpeg are required for these fixtures.");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString photo = dir.filePath(QStringLiteral("photo.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(photo));
        QVERIFY(run(exiftool,
                    {QStringLiteral("-q"),
                     QStringLiteral("-overwrite_original"),
                     QStringLiteral("-Make=Acme"),
                     QStringLiteral("-Model=Acme One"),
                     QStringLiteral("-DateTimeOriginal=2026:02:28 10:30:00"),
                     photo}));
        const QString song = dir.filePath(QStringLiteral("song.mp3"));
        if (!run(ffmpeg,
                 {QStringLiteral("-v"),
                  QStringLiteral("error"),
                  QStringLiteral("-f"),
                  QStringLiteral("lavfi"),
                  QStringLiteral("-i"),
                  QStringLiteral("anullsrc=r=44100:cl=mono"),
                  QStringLiteral("-t"),
                  QStringLiteral("0.1"),
                  QStringLiteral("-metadata"),
                  QStringLiteral("title=Blue Train"),
                  QStringLiteral("-metadata"),
                  QStringLiteral("genre=Jazz"),
                  QStringLiteral("-c:a"),
                  QStringLiteral("libmp3lame"),
                  QStringLiteral("-y"),
                  song})) {
            QSKIP("The available ffmpeg cannot create the audio fixture.");
        }

        const EmbeddedMetadata::Reading image_ = EmbeddedMetadata::read(photo);
        QVERIFY2(image_.supported, qPrintable(image_.problem));
        QCOMPARE(image_.mimeType, QStringLiteral("image/jpeg"));
        QVERIFY(find(image_, QStringLiteral("manufacturer")));
        QCOMPARE(find(image_, QStringLiteral("manufacturer"))->text, QStringLiteral("Acme"));
        QCOMPARE(find(image_, QStringLiteral("model"))->text, QStringLiteral("Acme One"));
        const EmbeddedMetadata::Value *taken = find(image_, QStringLiteral("photoDateTimeOriginal"));
        QVERIFY(taken);
        QCOMPARE(taken->date, QDate(2026, 2, 28));
        QVERIFY(EmbeddedMetadata::isDate(taken->key));
        QVERIFY(!EmbeddedMetadata::isDate(QStringLiteral("model")));
        QVERIFY(EmbeddedMetadata::searchTexts(image_).contains(QStringLiteral("2026-02-28")));
        QVERIFY(EmbeddedMetadata::lazySearchTexts(photo)().contains(QStringLiteral("Acme One")));

        const EmbeddedMetadata::Reading audio = EmbeddedMetadata::read(song);
        QVERIFY2(audio.supported, qPrintable(audio.problem));
        QVERIFY(find(audio, QStringLiteral("genre")));
        QCOMPARE(find(audio, QStringLiteral("genre"))->items, QStringList{QStringLiteral("Jazz")});
        QCOMPARE(find(audio, QStringLiteral("title"))->text, QStringLiteral("Blue Train"));
        QVERIFY(!find(audio, QStringLiteral("manufacturer")));
    }

    void reportsUnsupportedFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString notes = dir.filePath(QStringLiteral("notes.txt"));
        touch(notes);
        EmbeddedMetadata::Reading result = EmbeddedMetadata::read(notes);
        QVERIFY(!result.supported);
        QVERIFY(!result.problem.isEmpty());
        QVERIFY(result.values.isEmpty());
        result = EmbeddedMetadata::read(dir.path());
        QVERIFY(!result.supported);
        QVERIFY(!result.problem.isEmpty());
        const QString link = dir.filePath(QStringLiteral("link.jpg"));
        QVERIFY(QFile::link(notes, link));
        QVERIFY(!EmbeddedMetadata::read(link).supported);
        QVERIFY(EmbeddedMetadata::lazySearchTexts(notes)().isEmpty());
        QVERIFY(EmbeddedMetadata::keys().contains(QStringLiteral("genre")));
        QVERIFY(!EmbeddedMetadata::displayName(QStringLiteral("genre")).isEmpty());
    }

    void previewsAndAppliesImport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        QCOMPARE(LibraryRegistry::create(root), QString());
        const QString photo = dir.filePath(QStringLiteral("photo.jpg"));
        const QString other = dir.filePath(QStringLiteral("other.jpg"));
        const QString notes = dir.filePath(QStringLiteral("notes.txt"));
        touch(photo);
        touch(other);
        touch(notes);

        QString error;
        QVERIFY2(TagLibrary::update(
                     root,
                     [](TagLibrary::Library &library, QString *problem) {
                         TagLibrary::Tag acme;
                         acme.id = QStringLiteral("acme");
                         acme.name = QStringLiteral("Acme Cameras");
                         acme.aliases = {QStringLiteral("acme")};
                         return library.setTag(acme, problem)
                             && library.setField({QStringLiteral("camera"), QStringLiteral("Camera"), TagLibrary::FieldType::Text}, problem)
                             && library.setField({QStringLiteral("taken"), QStringLiteral("Taken"), TagLibrary::FieldType::Date}, problem);
                     },
                     &error),
                 qPrintable(error));
        QVERIFY2(TagAssignment::applyFields(root, {other}, {{QStringLiteral("camera"), QStringLiteral("Old")}}, nullptr, &error), qPrintable(error));

        const QList<EmbeddedMetadata::Reading> readings{
            reading(photo,
                    {value(QStringLiteral("manufacturer"), {QStringLiteral("ACME")}),
                     value(QStringLiteral("model"), {QStringLiteral("One")}),
                     value(QStringLiteral("keywords"), {QStringLiteral("Beach"), QStringLiteral("beach"), QString(51 * 4, QLatin1Char('x'))}),
                     value(QStringLiteral("photoDateTimeOriginal"), {QStringLiteral("2026-02-28T10:30:00")}, QDate(2026, 2, 28)),
                     value(QStringLiteral("artist"), {QStringLiteral("Someone")})}),
            reading(other, {value(QStringLiteral("model"), {QStringLiteral("Two")}), value(QStringLiteral("artist"), {QStringLiteral("Nobody")})}),
            EmbeddedMetadata::Reading{notes, QStringLiteral("text/plain"), false, {}, QStringLiteral("Not supported")},
        };
        const QMap<QString, QString> mappings{
            {QStringLiteral("manufacturer"), MetadataImport::tagsTarget()},
            {QStringLiteral("keywords"), MetadataImport::tagsTarget()},
            {QStringLiteral("model"), QStringLiteral("camera")},
            {QStringLiteral("photoDateTimeOriginal"), QStringLiteral("taken")},
            // A text value can't fill a date field, and a second property can't fill the same field.
            {QStringLiteral("artist"), QStringLiteral("taken")},
        };

        TagLibrary::Library library = TagLibrary::Library::load(root);
        MetadataImport::Plan plan = MetadataImport::preview(library, readings, mappings, MetadataImport::Policy::Keep);
        QCOMPARE(plan.root, root);
        QCOMPARE(plan.count(MetadataImport::Action::AssignTag), 1);
        QCOMPARE(plan.count(MetadataImport::Action::CreateTag), 1);
        QCOMPARE(plan.count(MetadataImport::Action::SetField), 2);
        QCOMPARE(plan.count(MetadataImport::Action::Conflict), 2);
        QCOMPARE(plan.count(MetadataImport::Action::Invalid), 2);
        QCOMPARE(plan.count(MetadataImport::Action::Unsupported), 1);
        QVERIFY(plan.hasChanges());
        const auto changeFor = [&plan](const QString &path, const QString &key) {
            return *std::find_if(plan.changes.cbegin(), plan.changes.cend(), [&](const MetadataImport::Change &change) {
                return change.path == path && change.key == key;
            });
        };
        QCOMPARE(changeFor(photo, QStringLiteral("manufacturer")).target, QStringLiteral("acme"));
        QCOMPARE(changeFor(photo, QStringLiteral("photoDateTimeOriginal")).value, QStringLiteral("2026-02-28"));
        QCOMPARE(changeFor(photo, QStringLiteral("artist")).action, MetadataImport::Action::Conflict);
        QCOMPARE(changeFor(other, QStringLiteral("model")).action, MetadataImport::Action::Conflict);
        QCOMPARE(changeFor(other, QStringLiteral("model")).current, QStringLiteral("Old"));
        QCOMPARE(changeFor(other, QStringLiteral("artist")).action, MetadataImport::Action::Invalid);

        const MetadataImport::Plan replace = MetadataImport::preview(library, readings, mappings, MetadataImport::Policy::Replace);
        QCOMPARE(replace.count(MetadataImport::Action::SetField), 3);
        QCOMPARE(replace.count(MetadataImport::Action::Conflict), 1);

        // Created tags get the plan's colour.
        plan.tagColor = QStringLiteral("#336699");
        MetadataImport::Result result;
        QVERIFY2(MetadataImport::apply(plan, &result, &error), qPrintable(error));
        QCOMPARE(result.tagsCreated, 1);
        QCOMPARE(result.itemsTagged, 1);
        QCOMPARE(result.valuesSet, 2);
        QCOMPARE(result.changedSincePreview, 0);
        library = TagLibrary::Library::load(root);
        const TagLibrary::Entry *entry = library.entry(QStringLiteral("photo.jpg"));
        QVERIFY(entry);
        QCOMPARE(entry->tags.size(), 2);
        QVERIFY(entry->tags.contains(QStringLiteral("acme")));
        QCOMPARE(library.findTags(QStringLiteral("beach")).size(), 1);
        QCOMPARE(library.tag(library.findTags(QStringLiteral("beach")).constFirst())->color, QStringLiteral("#336699"));
        QCOMPARE(entry->fields.value(QStringLiteral("camera")), QStringLiteral("One"));
        QCOMPARE(entry->fields.value(QStringLiteral("taken")), QStringLiteral("2026-02-28"));
        QCOMPARE(library.entry(QStringLiteral("other.jpg"))->fields.value(QStringLiteral("camera")), QStringLiteral("Old"));

        // Previewing again finds nothing left to change.
        const MetadataImport::Plan again = MetadataImport::preview(library, readings, mappings, MetadataImport::Policy::Keep);
        QVERIFY(!again.hasChanges());
        QCOMPARE(again.count(MetadataImport::Action::Unchanged), 4);

        // Values changed after the preview are kept, and the tag created by
        // another window in the meantime is reused.
        const MetadataImport::Plan stale = MetadataImport::preview(library, readings, mappings, MetadataImport::Policy::Replace);
        QVERIFY2(TagAssignment::applyFields(root, {other}, {{QStringLiteral("camera"), QStringLiteral("Newer")}}, nullptr, &error), qPrintable(error));
        QVERIFY2(MetadataImport::apply(stale, &result, &error), qPrintable(error));
        QCOMPARE(result.valuesSet, 0);
        QCOMPARE(result.changedSincePreview, 1);
        QCOMPARE(TagLibrary::Library::load(root).entry(QStringLiteral("other.jpg"))->fields.value(QStringLiteral("camera")), QStringLiteral("Newer"));

        // A plan without changes succeeds without touching the library.
        QVERIFY(MetadataImport::apply(again, &result, &error));
        QCOMPARE(result.valuesSet + result.itemsTagged + result.tagsCreated, 0);
    }

    void reusesTagsCreatedAfterThePreview()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        QCOMPARE(LibraryRegistry::create(root), QString());
        const QString song = dir.filePath(QStringLiteral("song.mp3"));
        touch(song);
        const QList<EmbeddedMetadata::Reading> readings{reading(song, {value(QStringLiteral("genre"), {QStringLiteral("Jazz")})})};
        const MetadataImport::Plan plan = MetadataImport::preview(TagLibrary::Library::load(root),
                                                                  readings,
                                                                  {{QStringLiteral("genre"), MetadataImport::tagsTarget()}},
                                                                  MetadataImport::Policy::Keep);
        QCOMPARE(plan.count(MetadataImport::Action::CreateTag), 1);

        QString error;
        QVERIFY(TagLibrary::update(
            root,
            [](TagLibrary::Library &library, QString *problem) {
                TagLibrary::Tag jazz;
                jazz.id = QStringLiteral("jazz");
                jazz.name = QStringLiteral("jazz");
                return library.setTag(jazz, problem);
            },
            &error));
        MetadataImport::Result result;
        QVERIFY2(MetadataImport::apply(plan, &result, &error), qPrintable(error));
        QCOMPARE(result.tagsCreated, 0);
        const TagLibrary::Library library = TagLibrary::Library::load(root);
        QCOMPARE(library.tags().size(), 1);
        QCOMPARE(library.entry(QStringLiteral("song.mp3"))->tags, QStringList{QStringLiteral("jazz")});
    }
};

QTEST_GUILESS_MAIN(MetadataImportTest)
#include "metadataimporttest.moc"
