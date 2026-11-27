// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "wikilinks.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

/**
 * An index of the Markdown notes in a vault folder, so wiki links can be
 * resolved and backlinks listed. Links resolve by note name anywhere in the
 * vault, by a vault-relative path such as `Projects/Plan`, or by a path
 * relative to the linking note that starts with `./` or `../`. Nothing
 * outside the vault folder is indexed or resolved.
 */
namespace NoteVault
{
/// Notes larger than this aren't indexed.
constexpr qint64 MaxNoteSize = 4 * 1024 * 1024;
constexpr int DefaultFileLimit = 10000;

struct Note {
    /// The note's canonical absolute path.
    QString path;
    /// The path relative to the vault folder.
    QString relativePath;
    /// The file name without its Markdown extension, which links name.
    QString name;
    QList<WikiLinks::Link> links;
    /// The note's tags, as WikiLinks::tags() returns them.
    QStringList tags;
};

struct Index {
    /// The vault folder's canonical path, or empty when it doesn't exist.
    QString root;
    QList<Note> notes;
    /// Indexes into notes by case-folded name.
    QHash<QString, QList<qsizetype>> byName;
    /// Indexes into notes by case-folded relative path without its extension.
    QHash<QString, qsizetype> byRelativePath;
    /// Indexes into notes by case-folded tag, in note order.
    QHash<QString, QList<qsizetype>> byTag;
    /// Notes that are too large, unreadable or not valid UTF-8.
    QStringList skipped;
    /// Set when the scan stopped at its file limit or was canceled.
    bool truncated = false;

    /// The note at @p path, or -1.
    qsizetype indexOf(const QString &path) const;
    bool isComplete() const;
};

using Cancel = std::function<bool()>;

/// Whether @p fileName has a Markdown extension: .md, .markdown or .mkd.
bool isNoteFileName(const QString &fileName);

/**
 * Indexes the notes below @p root. Hidden files and folders are skipped, and
 * symbolic links are followed only when they point inside @p root.
 */
Index scan(const QString &root, int fileLimit = DefaultFileLimit, const Cancel &cancel = {});

enum class Status {
    Found,
    Missing,
    /// More than one note has the name, and none is in the linking note's folder.
    Ambiguous,
    /// The link points outside the vault folder.
    Outside,
};

struct Resolution {
    Status status = Status::Missing;
    /// The note or attachment found.
    QString path;
    /// The notes an ambiguous link could mean.
    QStringList candidates;
};

/**
 * Resolves the link @p target written in the note at @p fromPath. An empty
 * target, as in `[[#Heading]]`, is the linking note itself. Targets that
 * aren't notes resolve to existing files inside the vault, such as images.
 */
Resolution resolve(const Index &index, const QString &fromPath, const QString &target);

struct Backlink {
    QString path;
    WikiLinks::Link link;
};

/// The links in other notes that resolve to the note at @p notePath, in note order.
QList<Backlink> backlinks(const Index &index, const QString &notePath);

/**
 * The paths of the notes tagged @p tag or a tag nested below it, such as
 * `area/topic` for `area`, sorted. Tags match case-insensitively.
 */
QStringList tagged(const Index &index, const QString &tag);

/// Whether @p path is @p root or inside it; both must be clean absolute paths.
bool isInside(const QString &root, const QString &path);
}
