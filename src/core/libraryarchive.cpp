// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryarchive.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int formatVersion = 1;
const QLatin1String libraryFormatName("plasma-opus-library");

QString fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return message;
}

/// Reads a JSON object from @p path, or describes why it can't be read.
QJsonObject readObject(const QString &path, QString *problem)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *problem = i18nc("@info", "Could not read %1: %2", path, file.errorString());
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *problem = i18nc("@info", "%1 is not valid JSON: %2", path, parseError.errorString());
        return {};
    }
    return document.object();
}

bool isExport(const QJsonObject &document)
{
    return document.value(QStringLiteral("format")).toString() == LibraryArchive::formatName();
}

/// Marks the entries of @p library whose file or folder isn't below @p root missing; returns how many aren't there.
int markAbsentEntries(QJsonObject &library, const QString &root)
{
    int absent = 0;
    QJsonArray entries = library.value(QStringLiteral("entries")).toArray();
    for (qsizetype i = 0; i < entries.size(); ++i) {
        QJsonObject entry = entries.at(i).toObject();
        const QString path = entry.value(QStringLiteral("path")).toString();
        if (path.isEmpty()) {
            continue;
        }
        const QFileInfo info(QDir(root).filePath(path));
        const bool folder = entry.value(QStringLiteral("kind")).toString() == QLatin1String("folder");
        if (info.exists() && !info.isSymLink() && info.isDir() == folder) {
            continue;
        }
        ++absent;
        entry.insert(QStringLiteral("missing"), true);
        entries.replace(i, entry);
    }
    if (absent > 0) {
        library.insert(QStringLiteral("entries"), entries);
    }
    return absent;
}

/// The item's tags and field values as text, so libraries with different identifiers compare by what users see.
QString describeMetadata(const TagLibrary::Library &library, const TagLibrary::Entry &entry)
{
    QStringList tags;
    for (const QString &id : entry.tags) {
        tags.append(library.displayName(id));
    }
    std::sort(tags.begin(), tags.end(), [](const QString &a, const QString &b) {
        return a.localeAwareCompare(b) < 0;
    });
    QStringList parts;
    if (!tags.isEmpty()) {
        parts.append(tags.join(i18nc("@info list separator", ", ")));
    }
    for (const TagLibrary::Field &field : library.fields()) {
        if (const QString value = entry.fields.value(field.id); !value.isEmpty()) {
            parts.append(i18nc("@info field name and value", "%1: %2", field.name, value));
        }
    }
    return parts.join(i18nc("@info separator between tags and field values", "; "));
}

QStringList parentNames(const TagLibrary::Library &library, const TagLibrary::Tag &tag)
{
    QStringList names;
    for (const QString &id : tag.parents) {
        names.append(library.displayName(id));
    }
    return names;
}

QString changedProperties(const QStringList &properties)
{
    return i18nc("@info %1 is a list of changed properties", "Changes %1", properties.join(i18nc("@info list separator", ", ")));
}

void compareTags(const TagLibrary::Library &current, const TagLibrary::Library &restored, QList<LibraryArchive::Change> &changes)
{
    using LibraryArchive::Action;
    using LibraryArchive::Kind;
    for (const TagLibrary::Tag &tag : restored.tags()) {
        const TagLibrary::Tag *old = current.tag(tag.id);
        if (!old) {
            const QStringList parents = parentNames(restored, tag);
            changes.append({Kind::Tag,
                            Action::Add,
                            restored.displayName(tag.id),
                            parents.isEmpty() ? QString() : i18nc("@info", "Inherits %1", parents.join(i18nc("@info list separator", ", ")))});
            continue;
        }
        QStringList properties;
        if (old->name != tag.name) {
            properties.append(i18nc("@info changed tag property", "name, was “%1”", old->name));
        }
        if (old->aliases != tag.aliases) {
            properties.append(i18nc("@info changed tag property", "aliases"));
        }
        if (old->shorthand != tag.shorthand) {
            properties.append(i18nc("@info changed tag property", "shorthand"));
        }
        if (old->color.compare(tag.color, Qt::CaseInsensitive) != 0) {
            properties.append(i18nc("@info changed tag property", "colour"));
        }
        if (old->parents != tag.parents) {
            properties.append(i18nc("@info changed tag property", "parents"));
        }
        if (!properties.isEmpty()) {
            changes.append({Kind::Tag, Action::Change, restored.displayName(tag.id), changedProperties(properties)});
        }
    }
    for (const TagLibrary::Tag &tag : current.tags()) {
        if (!restored.tag(tag.id)) {
            const int used = current.usageCount(tag.id);
            changes.append({Kind::Tag,
                            Action::Remove,
                            current.displayName(tag.id),
                            used > 0 ? i18ncp("@info", "Assigned to one item", "Assigned to %1 items", used) : QString()});
        }
    }
}

void compareGroups(const TagLibrary::Library &current, const TagLibrary::Library &restored, QList<LibraryArchive::Change> &changes)
{
    using LibraryArchive::Action;
    using LibraryArchive::Kind;
    for (const TagLibrary::Group &group : restored.groups()) {
        const TagLibrary::Group *old = current.group(group.id);
        if (!old) {
            changes.append({Kind::Group, Action::Add, group.name, i18ncp("@info", "One tag", "%1 tags", group.tags.size())});
            continue;
        }
        QStringList properties;
        if (old->name != group.name) {
            properties.append(i18nc("@info changed group property", "name, was “%1”", old->name));
        }
        if (old->color.compare(group.color, Qt::CaseInsensitive) != 0) {
            properties.append(i18nc("@info changed group property", "colour"));
        }
        if (old->tags != group.tags) {
            properties.append(i18nc("@info changed group property", "tags"));
        }
        if (!properties.isEmpty()) {
            changes.append({Kind::Group, Action::Change, group.name, changedProperties(properties)});
        }
    }
    for (const TagLibrary::Group &group : current.groups()) {
        if (!restored.group(group.id)) {
            changes.append({Kind::Group, Action::Remove, group.name, i18ncp("@info", "One tag", "%1 tags", group.tags.size())});
        }
    }
}

QString fieldTypeName(TagLibrary::FieldType type)
{
    return type == TagLibrary::FieldType::Date ? i18nc("@info field type", "Date") : i18nc("@info field type", "Text");
}

void compareFields(const TagLibrary::Library &current, const TagLibrary::Library &restored, QList<LibraryArchive::Change> &changes)
{
    using LibraryArchive::Action;
    using LibraryArchive::Kind;
    for (const TagLibrary::Field &field : restored.fields()) {
        const TagLibrary::Field *old = current.field(field.id);
        if (!old) {
            changes.append({Kind::Field, Action::Add, field.name, fieldTypeName(field.type)});
            continue;
        }
        QStringList properties;
        if (old->name != field.name) {
            properties.append(i18nc("@info changed field property", "name, was “%1”", old->name));
        }
        if (old->type != field.type) {
            properties.append(i18nc("@info changed field property", "type, was %1", fieldTypeName(old->type)));
        }
        if (!properties.isEmpty()) {
            changes.append({Kind::Field, Action::Change, field.name, changedProperties(properties)});
        }
    }
    for (const TagLibrary::Field &field : current.fields()) {
        if (!restored.field(field.id)) {
            const int used = current.fieldUsage(field.id);
            changes.append({Kind::Field,
                            Action::Remove,
                            field.name,
                            used > 0 ? i18ncp("@info", "One item has a value", "%1 items have a value", used) : fieldTypeName(field.type)});
        }
    }
}

void compareItems(const TagLibrary::Library &current, const TagLibrary::Library &restored, QList<LibraryArchive::Change> &changes)
{
    using LibraryArchive::Action;
    using LibraryArchive::Kind;
    for (const TagLibrary::Entry &entry : restored.entries()) {
        const TagLibrary::Entry *old = current.entry(entry.path);
        const QString after = TagLibrary::hasMetadata(entry) ? describeMetadata(restored, entry) : QString();
        const QString before = old && TagLibrary::hasMetadata(*old) ? describeMetadata(current, *old) : QString();
        if (after == before) {
            continue;
        }
        if (before.isEmpty()) {
            changes.append({Kind::Item, Action::Add, entry.path, after});
        } else if (after.isEmpty()) {
            changes.append({Kind::Item, Action::Remove, entry.path, before});
        } else {
            changes.append({Kind::Item, Action::Change, entry.path, after});
        }
    }
    for (const TagLibrary::Entry &entry : current.entries()) {
        if (TagLibrary::hasMetadata(entry) && !restored.entry(entry.path)) {
            changes.append({Kind::Item, Action::Remove, entry.path, describeMetadata(current, entry)});
        }
    }
}
}

QString LibraryArchive::formatName()
{
    return QStringLiteral("plasma-opus-library-export");
}

QString LibraryArchive::suggestedFileName(const QString &root, const QDate &date)
{
    return i18nc("@info file name of a library export; %1 is the library folder's name and %2 a date",
                 "%1 library %2.json",
                 QFileInfo(QDir::cleanPath(root)).fileName(),
                 date.toString(Qt::ISODate));
}

QString LibraryArchive::backupDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("library-backups"));
}

LibraryArchive::Summary LibraryArchive::summarize(const TagLibrary::Library &library)
{
    Summary summary;
    summary.tags = library.tags().size();
    summary.groups = library.groups().size();
    summary.fields = library.fields().size();
    summary.entries = library.entries().size();
    for (const TagLibrary::Entry &entry : library.entries()) {
        summary.itemsWithMetadata += TagLibrary::hasMetadata(entry) ? 1 : 0;
        summary.missing += entry.missing ? 1 : 0;
    }
    return summary;
}

QString LibraryArchive::describe(const Summary &summary)
{
    QString text = i18nc("@info %1 tags, %2 groups, %3 fields",
                         "%1, %2 and %3",
                         i18ncp("@info", "1 tag", "%1 tags", summary.tags),
                         i18ncp("@info", "1 group", "%1 groups", summary.groups),
                         i18ncp("@info", "1 field", "%1 fields", summary.fields));
    text += i18ncp("@info appended to the tag, group and field counts",
                   "; %2 of 1 indexed item has tags or fields",
                   "; %2 of %1 indexed items have tags or fields",
                   summary.entries,
                   summary.itemsWithMetadata);
    if (summary.missing > 0) {
        text += i18ncp("@info appended to the item counts", ", 1 missing", ", %1 missing", summary.missing);
    }
    return text + QLatin1Char('.');
}

QJsonObject LibraryArchive::toJson(const TagLibrary::Library &library, const QDateTime &exported)
{
    return {
        {QStringLiteral("format"), formatName()},
        {QStringLiteral("version"), formatVersion},
        {QStringLiteral("exported"), exported.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("root"), library.root()},
        {QStringLiteral("library"), library.toJson()},
    };
}

QString LibraryArchive::checkDestination(const QString &destination, bool *replaces)
{
    if (replaces) {
        *replaces = false;
    }
    if (destination.isEmpty() || !QDir::isAbsolutePath(destination)) {
        return i18nc("@info", "Choose a local file to export to.");
    }
    const QFileInfo info(QDir::cleanPath(destination));
    if (TagLibrary::isLibraryFile(info.fileName())) {
        return i18nc("@info", "Exports can't be named %1, because that would create a library document.", TagLibrary::fileName());
    }
    if (info.isSymLink()) {
        return i18nc("@info", "%1 is a symbolic link; choose another name.", info.filePath());
    }
    if (info.exists()) {
        if (!info.isFile()) {
            return i18nc("@info", "%1 already exists and isn't a file; choose another name.", info.filePath());
        }
        QString problem;
        if (!isExport(readObject(info.filePath(), &problem))) {
            return i18nc("@info", "%1 already exists and isn't a library export, so it won't be replaced; choose another name.", info.filePath());
        }
        if (!info.isWritable()) {
            return i18nc("@info", "%1 can't be replaced because it is read-only.", info.filePath());
        }
        if (replaces) {
            *replaces = true;
        }
    }
    const QFileInfo folder(info.absolutePath());
    if (!folder.isDir()) {
        return i18nc("@info", "The folder %1 doesn't exist.", folder.filePath());
    }
    if (!folder.isWritable()) {
        return i18nc("@info", "The folder %1 can't be written.", folder.filePath());
    }
    return {};
}

bool LibraryArchive::write(const TagLibrary::Library &library, const QString &destination, QString *error)
{
    if (library.isNull()) {
        fail(error, i18nc("@info", "No library is open."));
        return false;
    }
    if (const QString problem = checkDestination(destination); !problem.isEmpty()) {
        fail(error, problem);
        return false;
    }
    const QString path = QDir::cleanPath(destination);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        fail(error, i18nc("@info", "Could not write %1: %2", path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(toJson(library, QDateTime::currentDateTimeUtc())).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        fail(error, i18nc("@info", "Could not write %1: %2", path, file.errorString()));
        return false;
    }
    return true;
}

bool LibraryArchive::Plan::isNull() const
{
    return restored.isNull();
}

int LibraryArchive::Plan::count(Kind kind, Action action) const
{
    return static_cast<int>(std::count_if(changes.cbegin(), changes.cend(), [kind, action](const Change &change) {
        return change.kind == kind && change.action == action;
    }));
}

bool LibraryArchive::Plan::hasChanges() const
{
    return !isNull() && changesDocument;
}

LibraryArchive::Plan LibraryArchive::preview(const QString &root, const QString &source, QString *error)
{
    const QString clean = QDir::cleanPath(root);
    if (const QString problem = TagLibrary::checkLocation(clean); !problem.isEmpty()) {
        fail(error, problem);
        return {};
    }
    const QFileInfo info(source);
    if (!info.isFile()) {
        fail(error, i18nc("@info", "%1 isn't a file.", source));
        return {};
    }
    QString problem;
    const QJsonObject document = readObject(info.absoluteFilePath(), &problem);
    if (!problem.isEmpty()) {
        fail(error, problem);
        return {};
    }

    Plan plan;
    plan.root = clean;
    plan.source = info.absoluteFilePath();
    QJsonObject library;
    if (isExport(document)) {
        const QJsonValue version = document.value(QStringLiteral("version"));
        if (!version.isDouble() || version.toDouble() != std::floor(version.toDouble()) || version.toInt() < 1) {
            fail(error, i18nc("@info", "The export has an invalid version."));
            return {};
        }
        if (version.toInt() > formatVersion) {
            fail(error, i18nc("@info", "The export was made by a newer version of Plasma Opus and can't be restored safely."));
            return {};
        }
        if (!document.value(QStringLiteral("library")).isObject()) {
            fail(error, i18nc("@info", "The export doesn't contain a library."));
            return {};
        }
        library = document.value(QStringLiteral("library")).toObject();
        plan.exportedRoot = document.value(QStringLiteral("root")).toString();
        plan.exported = QDateTime::fromString(document.value(QStringLiteral("exported")).toString(), Qt::ISODate);
    } else if (document.value(QStringLiteral("format")).toString() == libraryFormatName) {
        // A copy of a library document, such as one kept before an earlier restore.
        library = document;
        plan.exported = info.lastModified();
    } else {
        fail(error, i18nc("@info", "%1 isn't a library export or library document.", source));
        return {};
    }
    plan.notFound = markAbsentEntries(library, clean);
    QString restoreError;
    plan.restored = TagLibrary::Library::fromJson(library, clean, &restoreError);
    if (plan.restored.isNull()) {
        fail(error, i18nc("@info", "The export can't be restored: %1", restoreError));
        return {};
    }
    plan.warnings = plan.restored.warnings();
    plan.after = summarize(plan.restored);

    plan.signature = TagLibrary::documentSignature(clean);
    QString loadError;
    const TagLibrary::Library current = TagLibrary::Library::load(clean, &loadError);
    if (current.isNull()) {
        plan.currentProblem = loadError;
    } else {
        plan.current = current.toJson();
        plan.before = summarize(current);
    }
    const TagLibrary::Library &compared = current.isNull() ? TagLibrary::Library(clean) : current;
    compareTags(compared, plan.restored, plan.changes);
    compareGroups(compared, plan.restored, plan.changes);
    compareFields(compared, plan.restored, plan.changes);
    compareItems(compared, plan.restored, plan.changes);
    plan.changesDocument = current.isNull() || plan.current != plan.restored.toJson();
    return plan;
}

bool LibraryArchive::restore(const Plan &plan, QString *backup, QString *error)
{
    if (backup) {
        backup->clear();
    }
    if (plan.isNull()) {
        fail(error, i18nc("@info", "Preview the restore first."));
        return false;
    }
    bool readOnly = false;
    if (const QString problem = TagLibrary::checkLocation(plan.root, &readOnly); !problem.isEmpty()) {
        fail(error, problem);
        return false;
    }
    if (readOnly) {
        fail(error, i18nc("@info", "The library folder is read-only: %1", plan.root));
        return false;
    }
    const QString changed = i18nc("@info", "The library changed since the preview. Preview the restore again.");
    if (TagLibrary::documentSignature(plan.root) != plan.signature) {
        fail(error, changed);
        return false;
    }
    TagLibrary::Library library;
    if (plan.currentProblem.isEmpty()) {
        QString loadError;
        library = TagLibrary::Library::load(plan.root, &loadError);
        if (library.isNull()) {
            fail(error, loadError);
            return false;
        }
        if (library.toJson() != plan.current) {
            fail(error, changed);
            return false;
        }
    } else {
        // The document is missing or unreadable, so it is replaced as a whole; its signature guards against concurrent writes.
        library = TagLibrary::Library(plan.root);
    }

    const QString document = QDir(plan.root).filePath(TagLibrary::fileName());
    QString copy;
    if (QFileInfo::exists(document)) {
        const QDir folder(backupDirectory());
        if (!folder.mkpath(QStringLiteral("."))) {
            fail(error, i18nc("@info", "Could not create the backup folder %1.", folder.path()));
            return false;
        }
        const QString base =
            QStringLiteral("%1 %2").arg(QFileInfo(plan.root).fileName(), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HHmmss")));
        copy = folder.filePath(base + QStringLiteral(".json"));
        for (int n = 2; QFileInfo::exists(copy); ++n) {
            copy = folder.filePath(QStringLiteral("%1 (%2).json").arg(base).arg(n));
        }
        if (!QFile::copy(document, copy)) {
            fail(error, i18nc("@info", "Could not keep a copy of the current library in %1, so nothing was restored.", folder.path()));
            return false;
        }
    }
    library.replaceContents(plan.restored);
    QString saveError;
    if (!library.save(&saveError)) {
        fail(error, saveError);
        if (!copy.isEmpty()) {
            QFile::remove(copy);
        }
        return false;
    }
    if (backup) {
        *backup = copy;
    }
    return true;
}
