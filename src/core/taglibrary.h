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
 * folder. A library is stored as one open, documented JSON document at its
 * root (see docs/tagging.md) instead of a sidecar per file. Libraries never
 * read or write the per-folder .mfm.json metadata.
 */
namespace TagLibrary
{
/// The name of the library document at a library root.
QString fileName();

/// Whether @p name is the library document or one of its temporary files.
bool isLibraryFile(const QString &name);

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
/// @param readOnly set when the folder is supported but can't be written
QString checkLocation(const QString &root, bool *readOnly = nullptr);

/// Creates a stable identifier for a new tag, group, field or entry.
QString createId();

/// Whether @p entry has tags or field values.
bool hasMetadata(const Entry &entry);

class Library
{
public:
    Library() = default;
    /// An empty library for @p root; nothing is written until save().
    explicit Library(const QString &root);

    /// Reads the library document at @p root.
    static Library load(const QString &root, QString *error = nullptr);
    /// Parses a library document; @p root is the folder it describes.
    static Library fromJson(const QJsonObject &document, const QString &root, QString *error = nullptr);

    /**
     * Atomically writes the document. Fails without writing when the folder
     * is read-only or the document changed on disk since it was loaded.
     */
    bool save(QString *error = nullptr);
    QJsonObject toJson() const;

    bool isNull() const;
    QString root() const;
    QString filePath() const;
    /// Set when the folder or document can't be written.
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
     */
    ScanChanges applyScan(const QList<Observation> &observed, bool complete);

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

private:
    QString m_root;
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
