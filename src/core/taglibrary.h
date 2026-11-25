// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

/**
 * Tag libraries: opt-in metadata for the files below an existing local
 * folder. A library is stored as one open, documented JSON document, at its
 * root or privately in Plasma Opus's data folder (see docs/tagging.md),
 * instead of a sidecar per file. Libraries never read or write the
 * per-folder .mfm.json metadata.
 */
namespace TagLibrary
{
/// The name of the library document at a library root.
QString fileName();

/// Whether @p name is the library document or one of its temporary files.
bool isLibraryFile(const QString &name);

/// Where a library document is kept.
enum class Storage {
    /// fileName() in the library root.
    LibraryFolder,
    /// privateDocumentPath(), so nothing is written to the library folder.
    PrivateFolder,
};

/// The path of the library document in @p root itself.
QString folderDocumentPath(const QString &root);

/// The path of the private library document for @p root in Plasma Opus's data folder, named after a hash of the cleaned root.
QString privateDocumentPath(const QString &root);

/// The library document of @p root: the private document when it exists, otherwise the one in the folder.
QString documentPath(const QString &root);

/// Whether the library document of @p root is kept privately.
bool isPrivate(const QString &root);

/**
 * Moves the library document of @p root to @p target storage. The document
 * is checked, written atomically to the target and read back before the
 * source is deleted; no other file is touched. Refused when the target
 * already holds a document or can't be written.
 * @returns an error message, or an empty string on success or when the document is already there
 */
QString moveDocument(const QString &root, Storage target);

/// Whether @p color is a valid tag or group colour: #rrggbb, or empty for none.
bool isValidColor(const QString &color);

struct Tag {
    QString id;
    QString name;
    QStringList aliases;
    QString shorthand;
    /// A #rrggbb colour, or empty for the default.
    QString color;
    /// Identifiers of parent tags.
    QStringList parents;
};

struct Group {
    QString id;
    QString name;
    QString color;
    /// Identifiers of member tags, in display order.
    QStringList tags;
};

enum class FieldType {
    Text,
    Date,
};

struct Field {
    QString id;
    QString name;
    FieldType type = FieldType::Text;
};

struct Entry {
    QString id;
    /// The path relative to the library root, with / separators.
    QString path;
    bool folder = false;
    qint64 size = 0;
    /// Modification time in milliseconds since the epoch, UTC.
    qint64 modified = 0;
    /// Identity hints for relinking; never used to attach metadata automatically.
    quint64 device = 0;
    quint64 inode = 0;
    /// Set by a refresh when the file is no longer at path.
    bool missing = false;
    /// Identifiers of assigned tags.
    QStringList tags;
    /// Field identifiers to values; dates use YYYY-MM-DD.
    QMap<QString, QString> fields;
};

/// A file or folder found below a library root by a scan.
struct Observation {
    QString path;
    bool folder = false;
    qint64 size = 0;
    qint64 modified = 0;
    quint64 device = 0;
    quint64 inode = 0;
};

struct ScanChanges {
    int added = 0;
    int updated = 0;
    /// Entries no longer found; their metadata is kept.
    int missing = 0;
    /// Missing entries found again at their recorded path.
    int found = 0;
};

/// Describes why @p root can't hold a library, or returns an empty string.
/// @param readOnly set when the folder is supported but its library document, at documentPath(), can't be written
QString checkLocation(const QString &root, bool *readOnly = nullptr);

/// Creates a stable identifier for a new tag, group, field or entry.
QString createId();

/// Identifies the current version of the library document at @p root, so callers can tell when to reload it.
QString documentSignature(const QString &root);

/// Whether @p entry has tags or field values.
bool hasMetadata(const Entry &entry);

/**
 * Parses field templates written as "text:Name" or "date:Name", each with a
 * new identifier. The type is matched ignoring case and spaces around the
 * name are dropped. Entries without a known type or a valid name, and later
 * entries whose name repeats an earlier one ignoring case, are ignored.
 */
QList<Field> parseFieldTemplates(const QStringList &templates);

/// Writes @p field as a template that parseFieldTemplates() reads.
QString fieldTemplate(const Field &field);

class Library
{
public:
    Library() = default;
    /// An empty library for @p root at documentPath(); nothing is written until save().
    explicit Library(const QString &root);
    /// An empty library for @p root kept in @p storage; nothing is written until save().
    Library(const QString &root, Storage storage);

    /// Reads the library document at @p root.
    static Library load(const QString &root, QString *error = nullptr);
    /// Parses a library document; @p root is the folder it describes.
    static Library fromJson(const QJsonObject &document, const QString &root, QString *error = nullptr);

    /**
     * Atomically writes the document. Fails without writing when the
     * document can't be written or changed on disk since it was loaded. A
     * private document also records the library root.
     */
    bool save(QString *error = nullptr);
    QJsonObject toJson() const;
    /**
     * Replaces the tags, groups, fields, entries and unknown top-level keys
     * with those of @p other, keeping this library's root and the state of
     * its document on disk, so save() writes them in place.
     */
    void replaceContents(const Library &other);

    bool isNull() const;
    QString root() const;
    QString filePath() const;
    /// Whether the document is kept in Plasma Opus's data folder rather than the library folder.
    bool isPrivate() const;
    /// Set when the document, or the folder that holds it, can't be written.
    bool isReadOnly() const;
    /// Problems repaired while loading, such as references to unknown tags.
    QStringList warnings() const;
    /// Whether the document on disk changed since this library was loaded or saved.
    bool changedOnDisk() const;

    const QList<Tag> &tags() const;
    const QList<Group> &groups() const;
    const QList<Field> &fields() const;
    const QList<Entry> &entries() const;

    const Tag *tag(const QString &id) const;
    const Group *group(const QString &id) const;
    const Field *field(const QString &id) const;
    /// The entry for a path relative to the root.
    const Entry *entry(const QString &relativePath) const;
    const Entry *entryById(const QString &id) const;
    /// The relative path of @p absolutePath, or an empty string outside the library.
    QString relativePath(const QString &absolutePath) const;

    /**
     * Records scanned files and folders: new paths become entries and known
     * ones are updated. Only a @p complete scan marks unseen entries missing;
     * missing entries keep their tags and fields.
     * @param inScope when set, a complete scan only marks the unseen entries
     *        it accepts missing, given their path relative to the root and
     *        whether they're folders; the others are left unchanged
     */
    ScanChanges applyScan(const QList<Observation> &observed, bool complete, const std::function<bool(const QString &relativePath, bool folder)> &inScope = {});

    /**
     * Records that Plasma Opus moved or renamed paths, as pairs of paths
     * relative to the root. The moves are applied together, so swapped names
     * are handled, and entries below a moved folder move with it. An entry
     * already at a destination is replaced because the file it described was
     * overwritten. When a folder is merged into an existing folder, the
     * existing folder's entry is kept if it has tags or fields, and the
     * moved folder's entry is marked missing for review if it has them too.
     * @returns the number of entries moved
     */
    int movePaths(const QList<QPair<QString, QString>> &moves);

    /// Marks the entry at @p relativePath and the entries below it missing; returns how many changed.
    int markMissing(const QString &relativePath);

    /**
     * Suggestions for relinking the missing entry @p entryId: indexed entries
     * of the same kind without tags or fields that have the same device and
     * inode, or the same name and, for files, the same size. The strongest
     * suggestions come first. Suggestions are never applied automatically.
     */
    QStringList relinkCandidates(const QString &entryId) const;

    /**
     * Attaches the missing entry @p entryId to @p target after the user
     * chose it. An entry at the target without tags or fields is merged into
     * it; one with tags or fields is never overwritten. Missing entries below
     * a relinked folder move with it unless their destination has tags or
     * fields.
     */
    bool relink(const QString &entryId, const Observation &target, QString *error = nullptr);

    /// Forgets an entry and, for a folder, the missing entries below it; returns how many were removed.
    int removeEntry(const QString &entryId);

    /// Identifiers of the tags that @p id inherits from, nearest first, without repeats.
    QStringList ancestors(const QString &id) const;
    /// Identifiers of the tags that inherit from @p id, nearest first, without repeats.
    QStringList descendants(const QString &id) const;

    /**
     * The name to show for tag @p id. When another tag has the same name,
     * ignoring case, the name is followed by the tag's parents, its shorthand
     * or, failing those, the start of its identifier, so the tags can be told
     * apart.
     */
    QString displayName(const QString &id) const;

    /// Tags whose name, an alias or shorthand equals @p text, ignoring case; name matches come first.
    QStringList findTags(const QString &text) const;

    /// The number of entries that have tag @p id assigned directly.
    int usageCount(const QString &id) const;

    /**
     * Describes why @p tag can't be saved, such as an invalid name or colour,
     * an unknown parent or a parent that would form a cycle, or returns an
     * empty string. Names may repeat.
     */
    QString checkTag(const Tag &tag) const;

    /**
     * Adds @p tag, or replaces the tag with the same identifier, if
     * checkTag() accepts it. Aliases are trimmed, and empty, repeated and
     * name-matching aliases are dropped.
     */
    bool setTag(const Tag &tag, QString *error = nullptr);

    /**
     * Removes tag @p id and every reference to it from other tags' parents,
     * groups and entries. Its child tags keep their other parents.
     * @returns the number of entries it was removed from
     */
    int removeTag(const QString &id);

    /// Identifiers of the groups that contain tag @p id, in group order.
    QStringList groupsOf(const QString &tagId) const;

    /**
     * Describes why @p group can't be saved, such as a missing identifier or
     * an invalid name or colour, or returns an empty string. Names may repeat.
     */
    QString checkGroup(const Group &group) const;

    /**
     * Adds @p group after the other groups, or replaces the group with the
     * same identifier in place, if checkGroup() accepts it. Unknown and
     * repeated tags are dropped, so a group stays valid when another window
     * deleted one of its tags.
     */
    bool setGroup(const Group &group, QString *error = nullptr);

    /// Removes group @p id; its tags and their assignments are kept.
    bool removeGroup(const QString &id);

    /// Moves group @p id to position @p index, clamped to the list of groups.
    bool moveGroup(const QString &id, int index);

    /// The number of entries that have a value for field @p id.
    int fieldUsage(const QString &id) const;

    /**
     * Describes why @p field can't be saved, such as a missing identifier, an
     * invalid name or a type change while entries have values, or returns an
     * empty string. Values are never converted between types.
     */
    QString checkField(const Field &field) const;

    /// Adds @p field after the other fields, or replaces the field with the same identifier, if checkField() accepts it.
    bool setField(const Field &field, QString *error = nullptr);

    /// Removes field @p id and its values; returns the number of entries that had a value.
    int removeField(const QString &id);

    /**
     * Describes why @p value can't be stored in field @p fieldId, or returns
     * an empty string. Dates are written as YYYY-MM-DD; an empty value clears
     * the field.
     */
    QString checkFieldValue(const QString &fieldId, const QString &value) const;

    /**
     * Sets the field values in @p values on the entries for @p items; an
     * empty value clears the field and fields not in @p values are kept.
     * Items that aren't indexed yet are indexed first.
     * @param changed receives the number of entries whose values changed
     * @returns false, changing nothing, when a field doesn't exist or a value is invalid
     */
    bool setFieldValues(const QList<Observation> &items, const QMap<QString, QString> &values, int *changed = nullptr, QString *error = nullptr);

    /// For each tag assigned directly to an entry at @p relativePaths, the number of those entries that have it.
    QHash<QString, int> tagCounts(const QStringList &relativePaths) const;

    /**
     * Assigns the tags @p add and removes the tags @p remove on the entries
     * for @p items. Items that aren't indexed yet are indexed first, and
     * missing entries found at their path are no longer missing.
     * @param changed receives the number of entries whose tags changed
     * @returns false, changing nothing, when a tag to add doesn't exist
     */
    bool assignTags(const QList<Observation> &items, const QStringList &add, const QStringList &remove, int *changed = nullptr, QString *error = nullptr);

private:
    QString m_root;
    QString m_path;
    QList<Tag> m_tags;
    QList<Group> m_groups;
    QList<Field> m_fields;
    QList<Entry> m_entries;
    QHash<QString, qsizetype> m_entryIndex;
    QJsonObject m_extra;
    QStringList m_warnings;
    QString m_signature;
    bool m_readOnly = false;

    void reindex();
    qsizetype indexOfId(const QString &id) const;
    void eraseEntries(QSet<qsizetype> indices);
};

/**
 * Loads the library at @p root, applies @p change and saves it. When the
 * document changed on disk in between, it is loaded again and the change is
 * reapplied, so concurrent edits from other windows aren't lost.
 * @param change returns false to abandon the edit without saving
 */
bool update(const QString &root, const std::function<bool(Library &library, QString *error)> &change, QString *error = nullptr);
}
