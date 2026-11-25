// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * Exports a tag library's metadata to a standalone JSON file and restores it
 * later. Both operations only read or write the export and the library
 * document; the files in the library are never changed, moved or deleted.
 * A restore is always previewed first and replaces the library document as a
 * whole, after keeping a copy of the previous document.
 */
namespace LibraryArchive
{
/// The format name written to export files.
QString formatName();

/// A file name for an export of @p root, such as "Photos library 2026-11-25.json".
QString suggestedFileName(const QString &root, const QDate &date);

/**
 * The folder that keeps copies of library documents replaced by a restore:
 * @p folder when it's an absolute path, otherwise library-backups in Plasma
 * Opus's data folder.
 */
QString backupDirectory(const QString &folder = {});

/// Where restore() keeps copies of replaced library documents.
struct BackupOptions {
    /// The backup folder; see backupDirectory().
    QString folder;
    /// The most copies to keep per library, counting the new one; 0 keeps all.
    int limit = 0;
};

/**
 * Deletes the oldest backups of the library at @p root in @p folder so at
 * most @p limit remain, never deleting @p keep. Only regular files named like
 * the backups restore() makes for that library are considered, and copies
 * that record a different library root are left alone.
 * @returns the number of backups deleted
 */
int pruneBackups(const QString &folder, const QString &root, int limit, const QString &keep = {});

struct Summary {
    int tags = 0;
    int groups = 0;
    int fields = 0;
    /// Indexed files and folders.
    int entries = 0;
    /// Entries with tags or field values.
    int itemsWithMetadata = 0;
    /// Entries marked missing.
    int missing = 0;
};

Summary summarize(const TagLibrary::Library &library);

/// Describes @p summary in a sentence, such as "3 tags, 1 group and 2 fields; 5 of 12 indexed items have tags or fields."
QString describe(const Summary &summary);

/// The export document for @p library, recording when and where it was exported.
QJsonObject toJson(const TagLibrary::Library &library, const QDateTime &exported);

/**
 * Describes why an export can't be written to @p destination, or returns an
 * empty string. Existing files are only replaced when they are earlier
 * exports, so an export never overwrites a library document or other file.
 * @param replaces set when an earlier export would be replaced
 */
QString checkDestination(const QString &destination, bool *replaces = nullptr);

/// Atomically writes an export of @p library to @p destination after checkDestination() accepts it.
bool write(const TagLibrary::Library &library, const QString &destination, QString *error = nullptr);

enum class Kind {
    Tag,
    Group,
    Field,
    Item,
};

enum class Action {
    Add,
    Remove,
    Change,
};

struct Change {
    Kind kind = Kind::Tag;
    Action action = Action::Change;
    /// The name of the tag, group or field, or the item's relative path.
    QString name;
    /// What the restored metadata is, or for removals what is lost.
    QString detail;
};

struct Plan {
    QString root;
    /// The export file.
    QString source;
    /// The library root recorded in the export, which may differ from root.
    QString exportedRoot;
    QDateTime exported;
    /// The library document's signature when the preview was made.
    QString signature;
    /// Why the current library document can't be read; it is replaced as a whole.
    QString currentProblem;
    /// The current library document, when it could be read.
    QJsonObject current;
    /// The library to write, with entries missing from the folder marked missing.
    TagLibrary::Library restored;
    QList<Change> changes;
    Summary before;
    Summary after;
    /// Entries restored as missing because their file or folder isn't in the library folder.
    int notFound = 0;
    /// Problems repaired while reading the export.
    QStringList warnings;
    /// Whether the restored document differs from the current one, including in identifiers only.
    bool changesDocument = false;

    bool isNull() const;
    int count(Kind kind, Action action) const;
    /// Whether restoring changes the library document.
    bool hasChanges() const;
};

/**
 * Reads the export at @p source and compares it with the library at @p root.
 * The current library document may be missing or unreadable; it is then
 * replaced as a whole. Returns a null plan when the export can't be read or
 * the folder can't hold a library.
 */
Plan preview(const QString &root, const QString &source, QString *error = nullptr);

/**
 * Replaces the library document with the restored metadata of @p plan. Fails
 * without writing when the document can't be written or changed since the
 * preview. The previous document is first copied to backupDirectory(), and
 * older copies beyond the backup limit are then deleted.
 * @param backup receives the path of the copy, or an empty string when there was no document
 */
bool restore(const Plan &plan, const BackupOptions &options, QString *backup = nullptr, QString *error = nullptr);

/// Restores @p plan with the default backup folder, keeping all backups.
bool restore(const Plan &plan, QString *backup = nullptr, QString *error = nullptr);
}
