// SPDX-License-Identifier: GPL-3.0-or-later

#include "taglibrary.h"

#include <KLocalizedString>

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace
{
const QLatin1String formatName("plasma-opus-library");
constexpr int formatVersion = 1;
constexpr qsizetype maxNameLength = 200;
constexpr qsizetype maxValueLength = 10000;

QString signatureOf(const QString &path)
{
    struct stat data;
    if (::stat(QFile::encodeName(path).constData(), &data) != 0) {
        return QStringLiteral("absent");
    }
    return QStringLiteral("%1:%2.%3:%4").arg(qint64(data.st_size)).arg(qint64(data.st_mtim.tv_sec)).arg(qint64(data.st_mtim.tv_nsec)).arg(quint64(data.st_ino));
}

bool validColor(const QString &color)
{
    static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    return color.isEmpty() || pattern.match(color).hasMatch();
}

bool validName(const QString &name)
{
    return !name.trimmed().isEmpty() && name == name.trimmed() && name.size() <= maxNameLength;
}

bool validDate(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    return pattern.match(value).hasMatch() && QDate::fromString(value, Qt::ISODate).isValid();
}

bool validRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')) || QDir::cleanPath(path) != path || path == QLatin1String(".")) {
        return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'));
    return !parts.contains(QStringLiteral("..")) && !parts.contains(QString());
}

/// Whether @p path is @p prefix or below it.
bool isBelow(const QString &path, const QString &prefix)
{
    return path == prefix || (path.size() > prefix.size() && path.startsWith(prefix) && path.at(prefix.size()) == QLatin1Char('/'));
}

QStringList stringList(const QJsonValue &value, bool *ok)
{
    QStringList result;
    if (value.isUndefined()) {
        return result;
    }
    if (!value.isArray()) {
        *ok = false;
        return result;
    }
    for (const QJsonValue &item : value.toArray()) {
        if (!item.isString()) {
            *ok = false;
            return {};
        }
        if (!result.contains(item.toString())) {
            result.append(item.toString());
        }
    }
    return result;
}

QJsonArray toArray(const QStringList &list)
{
    return QJsonArray::fromStringList(list);
}

QString fieldTypeName(TagLibrary::FieldType type)
{
    return type == TagLibrary::FieldType::Date ? QStringLiteral("date") : QStringLiteral("text");
}

/// Removes parent links that would make a cycle, reporting each one.
void breakCycles(QList<TagLibrary::Tag> &tags, QStringList &warnings)
{
    QHash<QString, qsizetype> index;
    for (qsizetype i = 0; i < tags.size(); ++i) {
        index.insert(tags.at(i).id, i);
    }
    enum State {
        Unvisited,
        Active,
        Done
    };
    QHash<QString, State> state;
    std::function<void(const QString &)> visit = [&](const QString &id) {
        state.insert(id, Active);
        TagLibrary::Tag &tag = tags[index.value(id)];
        for (qsizetype i = 0; i < tag.parents.size();) {
            const QString parent = tag.parents.at(i);
            const State parentState = state.value(parent, Unvisited);
            if (parentState == Active) {
                warnings.append(i18nc("@info", "Removed a parent of tag “%1” that formed a cycle.", tag.name));
                tag.parents.removeAt(i);
                continue;
            }
            if (parentState == Unvisited) {
                visit(parent);
            }
            ++i;
        }
        state.insert(id, Done);
    };
    for (const TagLibrary::Tag &tag : std::as_const(tags)) {
        if (state.value(tag.id, Unvisited) == Unvisited) {
            visit(tag.id);
        }
    }
}
}

QString TagLibrary::fileName()
{
    return QStringLiteral(".opus-library.json");
}

bool TagLibrary::isLibraryFile(const QString &name)
{
    return name.startsWith(fileName());
}

QString TagLibrary::createId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString TagLibrary::documentSignature(const QString &root)
{
    return signatureOf(QDir(QDir::cleanPath(root)).filePath(fileName()));
}

bool TagLibrary::hasMetadata(const Entry &entry)
{
    return !entry.tags.isEmpty() || !entry.fields.isEmpty();
}

QString TagLibrary::checkLocation(const QString &root, bool *readOnly)
{
    if (readOnly) {
        *readOnly = false;
    }
    if (root.isEmpty() || !QDir::isAbsolutePath(root)) {
        return i18nc("@info", "Libraries must be local folders: %1", root);
    }
    const QFileInfo info(root);
    if (info.isSymLink()) {
        return i18nc("@info", "Libraries can't be rooted at a symbolic link: %1", root);
    }
    if (!info.exists()) {
        return i18nc("@info", "The folder doesn't exist: %1", root);
    }
    if (!info.isDir()) {
        return i18nc("@info", "Libraries must be folders: %1", root);
    }
    if (QDir::cleanPath(info.absoluteFilePath()) == QLatin1String("/")) {
        return i18nc("@info", "The root folder can't be a library; choose a folder inside it.");
    }
    if (!info.isReadable() || !info.isExecutable()) {
        return i18nc("@info", "The folder can't be read: %1", root);
    }
    const QByteArray encoded = QFile::encodeName(root);
    struct statvfs volume;
    const bool readOnlyVolume = ::statvfs(encoded.constData(), &volume) == 0 && (volume.f_flag & ST_RDONLY);
    const QFileInfo document(QDir(root).filePath(fileName()));
    const bool documentLocked = document.exists() && ::access(QFile::encodeName(document.filePath()).constData(), W_OK) != 0;
    if (readOnlyVolume || ::access(encoded.constData(), W_OK) != 0 || documentLocked) {
        if (readOnly) {
            *readOnly = true;
        }
    }
    return {};
}

TagLibrary::Library::Library(const QString &root)
    : m_root(QDir::cleanPath(root))
    , m_signature(signatureOf(QDir(m_root).filePath(fileName())))
{
    checkLocation(m_root, &m_readOnly);
}

TagLibrary::Library TagLibrary::Library::load(const QString &root, QString *error)
{
    const QString clean = QDir::cleanPath(root);
    if (const QString problem = checkLocation(clean); !problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return {};
    }
    const QString path = QDir(clean).filePath(fileName());
    const QString signature = signatureOf(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.exists() ? i18nc("@info", "Could not read the library %1: %2", path, file.errorString())
                                   : i18nc("@info", "There is no library at %1.", clean);
        }
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = i18nc("@info", "The library %1 is not valid JSON: %2", path, parseError.errorString());
        }
        return {};
    }
    Library library = fromJson(document.object(), clean, error);
    if (!library.isNull()) {
        library.m_signature = signature;
    }
    return library;
}

TagLibrary::Library TagLibrary::Library::fromJson(const QJsonObject &document, const QString &root, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return Library();
    };
    if (document.value(QStringLiteral("format")).toString() != formatName) {
        return fail(i18nc("@info", "The file is not a Plasma Opus library."));
    }
    const QJsonValue version = document.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != std::floor(version.toDouble()) || version.toInt() < 1) {
        return fail(i18nc("@info", "The library has an invalid version."));
    }
    if (version.toInt() > formatVersion) {
        return fail(i18nc("@info", "The library was created by a newer version of Plasma Opus (format %1).", version.toInt()));
    }

    Library library(root);
    const QSet<QString> known{QStringLiteral("format"),
                              QStringLiteral("version"),
                              QStringLiteral("tags"),
                              QStringLiteral("groups"),
                              QStringLiteral("fields"),
                              QStringLiteral("entries")};
    for (auto it = document.begin(); it != document.end(); ++it) {
        if (!known.contains(it.key())) {
            library.m_extra.insert(it.key(), it.value());
        }
    }
    const auto array = [&document](const char *key, bool *ok) {
        const QJsonValue value = document.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isArray()) {
            *ok = false;
        }
        return value.toArray();
    };
    bool ok = true;
    const QJsonArray tags = array("tags", &ok);
    const QJsonArray groups = array("groups", &ok);
    const QJsonArray fields = array("fields", &ok);
    const QJsonArray entries = array("entries", &ok);
    if (!ok) {
        return fail(i18nc("@info", "The library's tags, groups, fields and entries must be lists."));
    }

    QSet<QString> tagIds;
    for (const QJsonValue &value : tags) {
        const QJsonObject object = value.toObject();
        Tag tag;
        tag.id = object.value(QStringLiteral("id")).toString();
        tag.name = object.value(QStringLiteral("name")).toString();
        tag.aliases = stringList(object.value(QStringLiteral("aliases")), &ok);
        tag.shorthand = object.value(QStringLiteral("shorthand")).toString();
        tag.color = object.value(QStringLiteral("color")).toString();
        tag.parents = stringList(object.value(QStringLiteral("parents")), &ok);
        if (!ok || !value.isObject() || tag.id.isEmpty() || tagIds.contains(tag.id) || !validName(tag.name)) {
            return fail(i18nc("@info", "The library contains an invalid or duplicate tag."));
        }
        if (!validColor(tag.color)) {
            library.m_warnings.append(i18nc("@info", "Ignored the invalid colour of tag “%1”.", tag.name));
            tag.color.clear();
        }
        tagIds.insert(tag.id);
        library.m_tags.append(tag);
    }
    for (Tag &tag : library.m_tags) {
        const qsizetype before = tag.parents.size();
        tag.parents.removeIf([&tagIds, &tag](const QString &parent) {
            return parent == tag.id || !tagIds.contains(parent);
        });
        if (tag.parents.size() != before) {
            library.m_warnings.append(i18nc("@info", "Removed unknown parents of tag “%1”.", tag.name));
        }
    }
    breakCycles(library.m_tags, library.m_warnings);

    QSet<QString> groupIds;
    for (const QJsonValue &value : groups) {
        const QJsonObject object = value.toObject();
        Group group;
        group.id = object.value(QStringLiteral("id")).toString();
        group.name = object.value(QStringLiteral("name")).toString();
        group.color = object.value(QStringLiteral("color")).toString();
        group.tags = stringList(object.value(QStringLiteral("tags")), &ok);
        if (!ok || !value.isObject() || group.id.isEmpty() || groupIds.contains(group.id) || !validName(group.name)) {
            return fail(i18nc("@info", "The library contains an invalid or duplicate tag group."));
        }
        if (!validColor(group.color)) {
            group.color.clear();
        }
        group.tags.removeDuplicates();
        const qsizetype before = group.tags.size();
        group.tags.removeIf([&tagIds](const QString &id) {
            return !tagIds.contains(id);
        });
        if (group.tags.size() != before) {
            library.m_warnings.append(i18nc("@info", "Removed unknown tags from group “%1”.", group.name));
        }
        groupIds.insert(group.id);
        library.m_groups.append(group);
    }

    QHash<QString, FieldType> fieldTypes;
    for (const QJsonValue &value : fields) {
        const QJsonObject object = value.toObject();
        Field field;
        field.id = object.value(QStringLiteral("id")).toString();
        field.name = object.value(QStringLiteral("name")).toString();
        const QString type = object.value(QStringLiteral("type")).toString();
        if (!value.isObject() || field.id.isEmpty() || fieldTypes.contains(field.id) || !validName(field.name)
            || (type != QLatin1String("text") && type != QLatin1String("date"))) {
            return fail(i18nc("@info", "The library contains an invalid or duplicate field."));
        }
        field.type = type == QLatin1String("date") ? FieldType::Date : FieldType::Text;
        fieldTypes.insert(field.id, field.type);
        library.m_fields.append(field);
    }

    QSet<QString> entryIds;
    QSet<QString> paths;
    qsizetype droppedTags = 0;
    qsizetype droppedValues = 0;
    for (const QJsonValue &value : entries) {
        const QJsonObject object = value.toObject();
        Entry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.path = object.value(QStringLiteral("path")).toString();
        const QString kind = object.value(QStringLiteral("kind")).toString();
        entry.folder = kind == QLatin1String("folder");
        entry.size = qint64(object.value(QStringLiteral("size")).toDouble());
        entry.modified = qint64(object.value(QStringLiteral("modified")).toDouble());
        entry.device = object.value(QStringLiteral("device")).toString().toULongLong();
        entry.inode = object.value(QStringLiteral("inode")).toString().toULongLong();
        entry.missing = object.value(QStringLiteral("missing")).toBool();
        entry.tags = stringList(object.value(QStringLiteral("tags")), &ok);
        const QJsonValue fieldValues = object.value(QStringLiteral("fields"));
        if (!ok || !value.isObject() || entry.id.isEmpty() || entryIds.contains(entry.id) || !validRelativePath(entry.path) || paths.contains(entry.path)
            || (kind != QLatin1String("file") && kind != QLatin1String("folder")) || (!fieldValues.isUndefined() && !fieldValues.isObject())) {
            return fail(i18nc("@info", "The library contains an invalid or duplicate entry."));
        }
        droppedTags += entry.tags.removeIf([&tagIds](const QString &id) {
            return !tagIds.contains(id);
        });
        const QJsonObject values = fieldValues.toObject();
        for (auto it = values.begin(); it != values.end(); ++it) {
            const QString text = it.value().toString();
            if (!fieldTypes.contains(it.key()) || !it.value().isString() || text.isEmpty()
                || (fieldTypes.value(it.key()) == FieldType::Date && !validDate(text))) {
                ++droppedValues;
                continue;
            }
            entry.fields.insert(it.key(), text);
        }
        entryIds.insert(entry.id);
        paths.insert(entry.path);
        library.m_entries.append(entry);
    }
    if (droppedTags) {
        library.m_warnings.append(i18ncp("@info", "Removed one assignment of an unknown tag.", "Removed %1 assignments of unknown tags.", droppedTags));
    }
    if (droppedValues) {
        library.m_warnings.append(i18ncp("@info", "Removed one invalid field value.", "Removed %1 invalid field values.", droppedValues));
    }
    library.reindex();
    return library;
}

QJsonObject TagLibrary::Library::toJson() const
{
    QJsonObject document = m_extra;
    document.insert(QStringLiteral("format"), formatName);
    document.insert(QStringLiteral("version"), formatVersion);
    QJsonArray tags;
    for (const Tag &tag : m_tags) {
        QJsonObject object{{QStringLiteral("id"), tag.id}, {QStringLiteral("name"), tag.name}};
        if (!tag.aliases.isEmpty()) {
            object.insert(QStringLiteral("aliases"), toArray(tag.aliases));
        }
        if (!tag.shorthand.isEmpty()) {
            object.insert(QStringLiteral("shorthand"), tag.shorthand);
        }
        if (!tag.color.isEmpty()) {
            object.insert(QStringLiteral("color"), tag.color);
        }
        if (!tag.parents.isEmpty()) {
            object.insert(QStringLiteral("parents"), toArray(tag.parents));
        }
        tags.append(object);
    }
    QJsonArray groups;
    for (const Group &group : m_groups) {
        QJsonObject object{{QStringLiteral("id"), group.id}, {QStringLiteral("name"), group.name}, {QStringLiteral("tags"), toArray(group.tags)}};
        if (!group.color.isEmpty()) {
            object.insert(QStringLiteral("color"), group.color);
        }
        groups.append(object);
    }
    QJsonArray fields;
    for (const Field &field : m_fields) {
        fields.append(QJsonObject{{QStringLiteral("id"), field.id}, {QStringLiteral("name"), field.name}, {QStringLiteral("type"), fieldTypeName(field.type)}});
    }
    QJsonArray entries;
    for (const Entry &entry : m_entries) {
        QJsonObject object{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("path"), entry.path},
            {QStringLiteral("kind"), entry.folder ? QStringLiteral("folder") : QStringLiteral("file")},
            {QStringLiteral("size"), double(entry.size)},
            {QStringLiteral("modified"), double(entry.modified)},
            {QStringLiteral("device"), QString::number(entry.device)},
            {QStringLiteral("inode"), QString::number(entry.inode)},
        };
        if (entry.missing) {
            object.insert(QStringLiteral("missing"), true);
        }
        if (!entry.tags.isEmpty()) {
            object.insert(QStringLiteral("tags"), toArray(entry.tags));
        }
        if (!entry.fields.isEmpty()) {
            QJsonObject values;
            for (auto it = entry.fields.cbegin(); it != entry.fields.cend(); ++it) {
                values.insert(it.key(), it.value());
            }
            object.insert(QStringLiteral("fields"), values);
        }
        entries.append(object);
    }
    document.insert(QStringLiteral("tags"), tags);
    document.insert(QStringLiteral("groups"), groups);
    document.insert(QStringLiteral("fields"), fields);
    document.insert(QStringLiteral("entries"), entries);
    return document;
}

bool TagLibrary::Library::save(QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    if (isNull()) {
        return fail(i18nc("@info", "No library is open."));
    }
    checkLocation(m_root, &m_readOnly);
    if (m_readOnly) {
        return fail(i18nc("@info", "The library folder is read-only: %1", m_root));
    }
    if (changedOnDisk()) {
        return fail(i18nc("@info", "The library %1 was changed by another window or program. Reload it and try again.", filePath()));
    }
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(i18nc("@info", "Could not write the library %1: %2", filePath(), file.errorString()));
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return fail(i18nc("@info", "Could not write the library %1: %2", filePath(), file.errorString()));
    }
    m_signature = signatureOf(filePath());
    return true;
}

bool TagLibrary::Library::isNull() const
{
    return m_root.isEmpty();
}

QString TagLibrary::Library::root() const
{
    return m_root;
}

QString TagLibrary::Library::filePath() const
{
    return isNull() ? QString() : QDir(m_root).filePath(fileName());
}

bool TagLibrary::Library::isReadOnly() const
{
    return m_readOnly;
}

QStringList TagLibrary::Library::warnings() const
{
    return m_warnings;
}

bool TagLibrary::Library::changedOnDisk() const
{
    return !isNull() && signatureOf(filePath()) != m_signature;
}

const QList<TagLibrary::Tag> &TagLibrary::Library::tags() const
{
    return m_tags;
}

const QList<TagLibrary::Group> &TagLibrary::Library::groups() const
{
    return m_groups;
}

const QList<TagLibrary::Field> &TagLibrary::Library::fields() const
{
    return m_fields;
}

const QList<TagLibrary::Entry> &TagLibrary::Library::entries() const
{
    return m_entries;
}

const TagLibrary::Tag *TagLibrary::Library::tag(const QString &id) const
{
    for (const Tag &tag : m_tags) {
        if (tag.id == id) {
            return &tag;
        }
    }
    return nullptr;
}

const TagLibrary::Group *TagLibrary::Library::group(const QString &id) const
{
    for (const Group &group : m_groups) {
        if (group.id == id) {
            return &group;
        }
    }
    return nullptr;
}

const TagLibrary::Field *TagLibrary::Library::field(const QString &id) const
{
    for (const Field &field : m_fields) {
        if (field.id == id) {
            return &field;
        }
    }
    return nullptr;
}

const TagLibrary::Entry *TagLibrary::Library::entry(const QString &relativePath) const
{
    const auto it = m_entryIndex.constFind(relativePath);
    return it == m_entryIndex.cend() ? nullptr : &m_entries.at(*it);
}

const TagLibrary::Entry *TagLibrary::Library::entryById(const QString &id) const
{
    for (const Entry &entry : m_entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

QString TagLibrary::Library::relativePath(const QString &absolutePath) const
{
    if (isNull()) {
        return {};
    }
    const QString clean = QDir::cleanPath(absolutePath);
    if (!clean.startsWith(m_root + QLatin1Char('/'))) {
        return {};
    }
    return clean.mid(m_root.size() + 1);
}

TagLibrary::ScanChanges TagLibrary::Library::applyScan(const QList<Observation> &observed, bool complete)
{
    ScanChanges changes;
    QSet<QString> seen;
    for (const Observation &item : observed) {
        if (!validRelativePath(item.path) || seen.contains(item.path)) {
            continue;
        }
        seen.insert(item.path);
        const auto it = m_entryIndex.constFind(item.path);
        if (it == m_entryIndex.cend()) {
            Entry entry;
            entry.id = createId();
            entry.path = item.path;
            entry.folder = item.folder;
            entry.size = item.size;
            entry.modified = item.modified;
            entry.device = item.device;
            entry.inode = item.inode;
            m_entryIndex.insert(entry.path, m_entries.size());
            m_entries.append(entry);
            ++changes.added;
            continue;
        }
        Entry &entry = m_entries[*it];
        const bool changed = entry.folder != item.folder || entry.size != item.size || entry.modified != item.modified || entry.device != item.device
            || entry.inode != item.inode;
        entry.folder = item.folder;
        entry.size = item.size;
        entry.modified = item.modified;
        entry.device = item.device;
        entry.inode = item.inode;
        if (entry.missing) {
            entry.missing = false;
            ++changes.found;
        } else if (changed) {
            ++changes.updated;
        }
    }
    if (complete) {
        for (Entry &entry : m_entries) {
            if (!entry.missing && !seen.contains(entry.path)) {
                entry.missing = true;
                ++changes.missing;
            }
        }
    }
    return changes;
}

void TagLibrary::Library::reindex()
{
    m_entryIndex.clear();
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        m_entryIndex.insert(m_entries.at(i).path, i);
    }
}

qsizetype TagLibrary::Library::indexOfId(const QString &id) const
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void TagLibrary::Library::eraseEntries(QSet<qsizetype> indices)
{
    if (indices.isEmpty()) {
        return;
    }
    QList<Entry> kept;
    kept.reserve(m_entries.size() - indices.size());
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (!indices.contains(i)) {
            kept.append(m_entries.at(i));
        }
    }
    m_entries = kept;
}

int TagLibrary::Library::movePaths(const QList<QPair<QString, QString>> &moves)
{
    QHash<qsizetype, QString> destinations;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        const QString &path = m_entries.at(i).path;
        qsizetype best = -1;
        for (qsizetype m = 0; m < moves.size(); ++m) {
            const auto &[from, to] = moves.at(m);
            if (!validRelativePath(from) || !validRelativePath(to) || from == to || isBelow(to, from) || !isBelow(path, from)) {
                continue;
            }
            if (best < 0 || from.size() > moves.at(best).first.size()) {
                best = m;
            }
        }
        if (best >= 0) {
            const auto &[from, to] = moves.at(best);
            destinations.insert(i, to + path.mid(from.size()));
        }
    }
    QSet<qsizetype> erase;
    QSet<qsizetype> stay;
    for (auto it = destinations.cbegin(); it != destinations.cend(); ++it) {
        const auto existing = m_entryIndex.constFind(it.value());
        if (existing == m_entryIndex.cend() || destinations.contains(*existing)) {
            continue;
        }
        Entry &moving = m_entries[it.key()];
        const Entry &other = m_entries.at(*existing);
        if (moving.folder && other.folder && hasMetadata(other)) {
            // Folders were merged; neither folder's metadata is silently replaced.
            if (hasMetadata(moving)) {
                moving.missing = true;
            } else {
                erase.insert(it.key());
            }
            stay.insert(it.key());
            continue;
        }
        erase.insert(*existing);
    }
    int moved = 0;
    for (auto it = destinations.cbegin(); it != destinations.cend(); ++it) {
        if (!stay.contains(it.key())) {
            m_entries[it.key()].path = it.value();
            ++moved;
        }
    }
    eraseEntries(erase);
    reindex();
    return moved;
}

int TagLibrary::Library::markMissing(const QString &relativePath)
{
    int changed = 0;
    for (Entry &entry : m_entries) {
        if (!entry.missing && isBelow(entry.path, relativePath)) {
            entry.missing = true;
            ++changed;
        }
    }
    return changed;
}

QStringList TagLibrary::Library::relinkCandidates(const QString &entryId) const
{
    const Entry *missing = entryById(entryId);
    if (!missing || !missing->missing) {
        return {};
    }
    const QString name = missing->path.section(QLatin1Char('/'), -1);
    QList<QPair<int, const Entry *>> scored;
    for (const Entry &entry : m_entries) {
        if (entry.missing || entry.folder != missing->folder || hasMetadata(entry)) {
            continue;
        }
        const bool identity = missing->inode != 0 && entry.inode == missing->inode && entry.device == missing->device;
        const bool sameName = entry.path.section(QLatin1Char('/'), -1) == name;
        const bool sameSize = !missing->folder && entry.size == missing->size;
        if (!identity && !(sameName && (missing->folder || sameSize))) {
            continue;
        }
        scored.append({(identity ? 4 : 0) + (sameName ? 2 : 0) + (sameSize ? 1 : 0), &entry});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first > b.first : a.second->path < b.second->path;
    });
    QStringList ids;
    for (const auto &[score, entry] : std::as_const(scored)) {
        ids.append(entry->id);
    }
    return ids;
}

bool TagLibrary::Library::relink(const QString &entryId, const Observation &target, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    const qsizetype index = indexOfId(entryId);
    if (index < 0) {
        return fail(i18nc("@info", "The library entry no longer exists."));
    }
    const Entry original = m_entries.at(index);
    if (!original.missing) {
        return fail(i18nc("@info", "“%1” isn't missing.", original.path));
    }
    if (!validRelativePath(target.path)) {
        return fail(i18nc("@info", "The chosen item isn't inside the library."));
    }
    if (target.folder != original.folder) {
        return fail(original.folder ? i18nc("@info", "A missing folder can only be relinked to a folder.")
                                    : i18nc("@info", "A missing file can only be relinked to a file."));
    }
    if (target.path != original.path && original.folder && isBelow(target.path, original.path)) {
        return fail(i18nc("@info", "A folder can't be relinked to a folder inside itself."));
    }
    QSet<qsizetype> erase;
    if (const Entry *existing = entry(target.path); existing && existing->id != entryId) {
        if (hasMetadata(*existing)) {
            return fail(i18nc("@info", "“%1” already has tags or fields in this library.", target.path));
        }
        erase.insert(m_entryIndex.value(target.path));
    }
    Entry &relinked = m_entries[index];
    relinked.path = target.path;
    relinked.size = target.size;
    relinked.modified = target.modified;
    relinked.device = target.device;
    relinked.inode = target.inode;
    relinked.missing = false;
    if (original.folder && target.path != original.path) {
        for (qsizetype i = 0; i < m_entries.size(); ++i) {
            Entry &child = m_entries[i];
            if (i == index || !child.missing || !isBelow(child.path, original.path)) {
                continue;
            }
            const QString destination = target.path + child.path.mid(original.path.size());
            if (const auto existing = m_entryIndex.constFind(destination); existing != m_entryIndex.cend() && *existing != i) {
                if (hasMetadata(m_entries.at(*existing))) {
                    continue;
                }
                erase.insert(*existing);
            }
            child.path = destination;
        }
    }
    eraseEntries(erase);
    reindex();
    return true;
}

int TagLibrary::Library::removeEntry(const QString &entryId)
{
    const qsizetype index = indexOfId(entryId);
    if (index < 0) {
        return 0;
    }
    const Entry removed = m_entries.at(index);
    QSet<qsizetype> erase{index};
    if (removed.folder) {
        for (qsizetype i = 0; i < m_entries.size(); ++i) {
            if (m_entries.at(i).missing && isBelow(m_entries.at(i).path, removed.path)) {
                erase.insert(i);
            }
        }
    }
    eraseEntries(erase);
    reindex();
    return int(erase.size());
}

QStringList TagLibrary::Library::ancestors(const QString &id) const
{
    QStringList result;
    QStringList queue;
    if (const Tag *start = tag(id)) {
        queue = start->parents;
    }
    for (qsizetype i = 0; i < queue.size(); ++i) {
        const QString current = queue.at(i);
        if (current == id || result.contains(current)) {
            continue;
        }
        result.append(current);
        if (const Tag *parent = tag(current)) {
            queue.append(parent->parents);
        }
    }
    return result;
}

QStringList TagLibrary::Library::descendants(const QString &id) const
{
    QHash<QString, QStringList> children;
    for (const Tag &tag : m_tags) {
        for (const QString &parent : tag.parents) {
            children[parent].append(tag.id);
        }
    }
    QStringList result;
    QStringList queue = children.value(id);
    for (qsizetype i = 0; i < queue.size(); ++i) {
        const QString current = queue.at(i);
        if (current == id || result.contains(current)) {
            continue;
        }
        result.append(current);
        queue.append(children.value(current));
    }
    return result;
}

QString TagLibrary::Library::displayName(const QString &id) const
{
    const Tag *named = tag(id);
    if (!named) {
        return {};
    }
    QList<const Tag *> namesakes;
    for (const Tag &other : m_tags) {
        if (other.id != id && other.name.compare(named->name, Qt::CaseInsensitive) == 0) {
            namesakes.append(&other);
        }
    }
    if (namesakes.isEmpty()) {
        return named->name;
    }
    const auto parentNames = [this](const Tag &of) {
        QStringList names;
        for (const QString &parent : of.parents) {
            if (const Tag *found = tag(parent)) {
                names.append(found->name);
            }
        }
        return names.join(QStringLiteral(", "));
    };
    const auto distinct = [&namesakes](const QString &qualifier, const std::function<QString(const Tag &)> &of) {
        return !qualifier.isEmpty() && std::none_of(namesakes.cbegin(), namesakes.cend(), [&](const Tag *other) {
            return of(*other).compare(qualifier, Qt::CaseInsensitive) == 0;
        });
    };
    const QString parents = parentNames(*named);
    if (distinct(parents, parentNames)) {
        return i18nc("@item tag name followed by its parent tags", "%1 (%2)", named->name, parents);
    }
    const auto shorthandOf = [](const Tag &of) {
        return of.shorthand;
    };
    if (distinct(named->shorthand, shorthandOf)) {
        return i18nc("@item tag name followed by its shorthand", "%1 (%2)", named->name, named->shorthand);
    }
    return i18nc("@item tag name followed by the start of its identifier", "%1 (%2)", named->name, id.left(8));
}

QStringList TagLibrary::Library::findTags(const QString &text) const
{
    const QString wanted = text.trimmed();
    QStringList byName;
    QStringList byOther;
    if (wanted.isEmpty()) {
        return {};
    }
    for (const Tag &tag : m_tags) {
        if (tag.name.compare(wanted, Qt::CaseInsensitive) == 0) {
            byName.append(tag.id);
        } else if (tag.shorthand.compare(wanted, Qt::CaseInsensitive) == 0 || tag.aliases.contains(wanted, Qt::CaseInsensitive)) {
            byOther.append(tag.id);
        }
    }
    return byName + byOther;
}

int TagLibrary::Library::usageCount(const QString &id) const
{
    return int(std::count_if(m_entries.cbegin(), m_entries.cend(), [&id](const Entry &entry) {
        return entry.tags.contains(id);
    }));
}

QString TagLibrary::Library::checkTag(const Tag &tag) const
{
    if (tag.id.isEmpty()) {
        return i18nc("@info", "The tag has no identifier.");
    }
    if (!validName(tag.name)) {
        return tag.name.trimmed().isEmpty() ? i18nc("@info", "Enter a name for the tag.")
                                            : i18nc("@info", "Tag names can't start or end with spaces or be longer than %1 characters.", maxNameLength);
    }
    for (const QString &alias : tag.aliases) {
        if (alias.trimmed().size() > maxNameLength) {
            return i18nc("@info", "Aliases can't be longer than %1 characters.", maxNameLength);
        }
    }
    if (tag.shorthand.size() > maxNameLength || std::any_of(tag.shorthand.cbegin(), tag.shorthand.cend(), [](QChar c) {
            return c.isSpace();
        })) {
        return i18nc("@info", "The shorthand can't contain spaces or be longer than %1 characters.", maxNameLength);
    }
    if (!validColor(tag.color)) {
        return i18nc("@info", "The colour must be written as #rrggbb.");
    }
    const QStringList below = descendants(tag.id);
    for (const QString &parent : tag.parents) {
        if (parent == tag.id) {
            return i18nc("@info", "A tag can't be its own parent.");
        }
        const Tag *found = this->tag(parent);
        if (!found) {
            return i18nc("@info", "A parent tag no longer exists.");
        }
        if (below.contains(parent)) {
            return i18nc("@info", "“%1” can't be a parent because it is already below this tag.", displayName(parent));
        }
    }
    return {};
}

bool TagLibrary::Library::setTag(const Tag &tag, QString *error)
{
    if (const QString problem = checkTag(tag); !problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return false;
    }
    Tag saved = tag;
    saved.aliases.clear();
    for (const QString &alias : tag.aliases) {
        const QString trimmed = alias.trimmed();
        if (!trimmed.isEmpty() && trimmed.compare(tag.name, Qt::CaseInsensitive) != 0 && !saved.aliases.contains(trimmed, Qt::CaseInsensitive)) {
            saved.aliases.append(trimmed);
        }
    }
    saved.parents.removeDuplicates();
    for (Tag &existing : m_tags) {
        if (existing.id == saved.id) {
            existing = saved;
            return true;
        }
    }
    m_tags.append(saved);
    return true;
}

int TagLibrary::Library::removeTag(const QString &id)
{
    const qsizetype before = m_tags.size();
    m_tags.removeIf([&id](const Tag &tag) {
        return tag.id == id;
    });
    if (m_tags.size() == before) {
        return 0;
    }
    for (Tag &tag : m_tags) {
        tag.parents.removeAll(id);
    }
    for (Group &group : m_groups) {
        group.tags.removeAll(id);
    }
    int removed = 0;
    for (Entry &entry : m_entries) {
        removed += entry.tags.removeAll(id) > 0 ? 1 : 0;
    }
    return removed;
}

QStringList TagLibrary::Library::groupsOf(const QString &tagId) const
{
    QStringList result;
    for (const Group &group : m_groups) {
        if (group.tags.contains(tagId)) {
            result.append(group.id);
        }
    }
    return result;
}

QString TagLibrary::Library::checkGroup(const Group &group) const
{
    if (group.id.isEmpty()) {
        return i18nc("@info", "The group has no identifier.");
    }
    if (!validName(group.name)) {
        return group.name.trimmed().isEmpty() ? i18nc("@info", "Enter a name for the group.")
                                              : i18nc("@info", "Group names can't start or end with spaces or be longer than %1 characters.", maxNameLength);
    }
    if (!validColor(group.color)) {
        return i18nc("@info", "The colour must be written as #rrggbb.");
    }
    return {};
}

bool TagLibrary::Library::setGroup(const Group &group, QString *error)
{
    if (const QString problem = checkGroup(group); !problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return false;
    }
    Group saved = group;
    saved.tags.clear();
    for (const QString &id : group.tags) {
        if (tag(id) && !saved.tags.contains(id)) {
            saved.tags.append(id);
        }
    }
    for (Group &existing : m_groups) {
        if (existing.id == saved.id) {
            existing = saved;
            return true;
        }
    }
    m_groups.append(saved);
    return true;
}

bool TagLibrary::Library::removeGroup(const QString &id)
{
    return m_groups.removeIf([&id](const Group &group) {
        return group.id == id;
    }) > 0;
}

bool TagLibrary::Library::moveGroup(const QString &id, int index)
{
    const auto found = std::find_if(m_groups.cbegin(), m_groups.cend(), [&id](const Group &group) {
        return group.id == id;
    });
    if (found == m_groups.cend()) {
        return false;
    }
    const qsizetype from = found - m_groups.cbegin();
    const qsizetype to = std::clamp<qsizetype>(index, 0, m_groups.size() - 1);
    m_groups.move(from, to);
    return true;
}

int TagLibrary::Library::fieldUsage(const QString &id) const
{
    return int(std::count_if(m_entries.cbegin(), m_entries.cend(), [&id](const Entry &entry) {
        return entry.fields.contains(id);
    }));
}

QString TagLibrary::Library::checkField(const Field &field) const
{
    if (field.id.isEmpty()) {
        return i18nc("@info", "The field has no identifier.");
    }
    if (!validName(field.name)) {
        return field.name.trimmed().isEmpty() ? i18nc("@info", "Enter a name for the field.")
                                              : i18nc("@info", "Field names can't start or end with spaces or be longer than %1 characters.", maxNameLength);
    }
    const Field *existing = this->field(field.id);
    if (existing && existing->type != field.type && fieldUsage(field.id) > 0) {
        return i18nc("@info", "The type can't be changed while items have a value for this field, because values aren't converted.");
    }
    return {};
}

bool TagLibrary::Library::setField(const Field &field, QString *error)
{
    if (const QString problem = checkField(field); !problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return false;
    }
    for (Field &existing : m_fields) {
        if (existing.id == field.id) {
            existing = field;
            return true;
        }
    }
    m_fields.append(field);
    return true;
}

int TagLibrary::Library::removeField(const QString &id)
{
    const qsizetype before = m_fields.size();
    m_fields.removeIf([&id](const Field &field) {
        return field.id == id;
    });
    if (m_fields.size() == before) {
        return 0;
    }
    int removed = 0;
    for (Entry &entry : m_entries) {
        removed += entry.fields.remove(id) > 0 ? 1 : 0;
    }
    return removed;
}

QString TagLibrary::Library::checkFieldValue(const QString &fieldId, const QString &value) const
{
    const Field *found = field(fieldId);
    if (!found) {
        return i18nc("@info", "A field no longer exists.");
    }
    if (value.trimmed().isEmpty()) {
        return {};
    }
    if (found->type == FieldType::Date && !validDate(value)) {
        return i18nc("@info", "“%1” must be a date written as YYYY-MM-DD.", found->name);
    }
    if (value.size() > maxValueLength) {
        return i18nc("@info", "“%1” can't be longer than %2 characters.", found->name, maxValueLength);
    }
    return {};
}

bool TagLibrary::Library::setFieldValues(const QList<Observation> &items, const QMap<QString, QString> &values, int *changed, QString *error)
{
    if (changed) {
        *changed = 0;
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (const QString problem = checkFieldValue(it.key(), it.value()); !problem.isEmpty()) {
            if (error) {
                *error = problem;
            }
            return false;
        }
    }
    applyScan(items, false);
    QSet<QString> done;
    for (const Observation &item : items) {
        const auto found = m_entryIndex.constFind(item.path);
        if (found == m_entryIndex.cend() || done.contains(item.path)) {
            continue;
        }
        done.insert(item.path);
        Entry &entry = m_entries[*found];
        const QMap<QString, QString> before = entry.fields;
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            if (it.value().trimmed().isEmpty()) {
                entry.fields.remove(it.key());
            } else {
                entry.fields.insert(it.key(), it.value());
            }
        }
        if (changed && entry.fields != before) {
            ++*changed;
        }
    }
    return true;
}

QHash<QString, int> TagLibrary::Library::tagCounts(const QStringList &relativePaths) const
{
    QHash<QString, int> counts;
    QSet<QString> seen;
    for (const QString &path : relativePaths) {
        if (seen.contains(path)) {
            continue;
        }
        seen.insert(path);
        if (const Entry *found = entry(path)) {
            for (const QString &id : found->tags) {
                ++counts[id];
            }
        }
    }
    return counts;
}

bool TagLibrary::Library::assignTags(const QList<Observation> &items, const QStringList &add, const QStringList &remove, int *changed, QString *error)
{
    if (changed) {
        *changed = 0;
    }
    for (const QString &id : add) {
        if (!tag(id)) {
            if (error) {
                *error = i18nc("@info", "A tag to assign no longer exists.");
            }
            return false;
        }
    }
    applyScan(items, false);
    QSet<QString> done;
    for (const Observation &item : items) {
        const auto it = m_entryIndex.constFind(item.path);
        if (it == m_entryIndex.cend() || done.contains(item.path)) {
            continue;
        }
        done.insert(item.path);
        Entry &entry = m_entries[*it];
        const QStringList before = entry.tags;
        for (const QString &id : remove) {
            entry.tags.removeAll(id);
        }
        for (const QString &id : add) {
            if (!entry.tags.contains(id)) {
                entry.tags.append(id);
            }
        }
        if (changed && entry.tags != before) {
            ++*changed;
        }
    }
    return true;
}

bool TagLibrary::update(const QString &root, const std::function<bool(Library &library, QString *error)> &change, QString *error)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        Library library = Library::load(root, error);
        if (library.isNull()) {
            return false;
        }
        if (library.isReadOnly()) {
            if (error) {
                *error = i18nc("@info", "The library folder is read-only: %1", library.root());
            }
            return false;
        }
        if (!change(library, error)) {
            return false;
        }
        if (library.save(error)) {
            return true;
        }
        if (!library.changedOnDisk()) {
            return false;
        }
    }
    return false;
}
