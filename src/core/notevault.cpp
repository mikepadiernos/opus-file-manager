// SPDX-License-Identifier: GPL-3.0-or-later

#include "notevault.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>

namespace NoteVault
{
namespace
{
QString folded(const QString &text)
{
    return text.toCaseFolded();
}

QString withoutNoteExtension(const QString &path)
{
    if (!isNoteFileName(path)) {
        return path;
    }
    return path.left(path.lastIndexOf(QLatin1Char('.')));
}

QString folderOf(const QString &path)
{
    return QFileInfo(path).absolutePath();
}

/// An existing file inside the vault at @p path, such as an image, or empty.
QString attachmentAt(const Index &index, const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        return {};
    }
    const QString canonical = info.canonicalFilePath();
    return isInside(index.root, canonical) ? canonical : QString();
}

Resolution found(const QString &path)
{
    return {Status::Found, path, {}};
}
}

qsizetype Index::indexOf(const QString &path) const
{
    for (qsizetype i = 0; i < notes.size(); ++i) {
        if (notes.at(i).path == path) {
            return i;
        }
    }
    return -1;
}

bool Index::isComplete() const
{
    return !root.isEmpty() && !truncated && skipped.isEmpty();
}

bool isNoteFileName(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return suffix == QLatin1String("md") || suffix == QLatin1String("markdown") || suffix == QLatin1String("mkd");
}

bool isInside(const QString &root, const QString &path)
{
    if (root.isEmpty() || path.isEmpty()) {
        return false;
    }
    if (root == QLatin1String("/")) {
        return path.startsWith(QLatin1Char('/'));
    }
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

Index scan(const QString &root, int fileLimit, const Cancel &cancel)
{
    Index index;
    index.root = QFileInfo(root).canonicalFilePath();
    if (index.root.isEmpty() || !QFileInfo(index.root).isDir()) {
        index.root.clear();
        return index;
    }
    const QDir rootDir(index.root);
    QSet<QString> seenFolders{index.root};
    QSet<QString> seenNotes;
    QStringList pending{index.root};
    while (!pending.isEmpty() && !index.truncated) {
        if (cancel && cancel()) {
            index.truncated = true;
            break;
        }
        const QString folder = pending.takeFirst();
        // Without QDir::Hidden, hidden entries such as .obsidian and .git are skipped,
        // and without QDir::System, so are broken links.
        const QFileInfoList entries = QDir(folder).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            const QString canonical = entry.canonicalFilePath();
            if (canonical.isEmpty() || !isInside(index.root, canonical)) {
                continue;
            }
            if (entry.isDir()) {
                if (!seenFolders.contains(canonical)) {
                    seenFolders.insert(canonical);
                    pending.append(canonical);
                }
                continue;
            }
            if (!entry.isFile() || !isNoteFileName(entry.fileName()) || seenNotes.contains(canonical)) {
                continue;
            }
            if (index.notes.size() >= fileLimit) {
                index.truncated = true;
                break;
            }
            seenNotes.insert(canonical);
            if (entry.size() > MaxNoteSize) {
                index.skipped.append(canonical);
                continue;
            }
            QFile file(canonical);
            if (!file.open(QIODevice::ReadOnly)) {
                index.skipped.append(canonical);
                continue;
            }
            QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
            const QString text = decoder.decode(file.read(MaxNoteSize + 1));
            if (decoder.hasError()) {
                index.skipped.append(canonical);
                continue;
            }
            Note note;
            note.path = canonical;
            note.relativePath = rootDir.relativeFilePath(canonical);
            note.name = QFileInfo(canonical).completeBaseName();
            note.links = WikiLinks::parse(text);
            const qsizetype position = index.notes.size();
            index.byName[folded(note.name)].append(position);
            index.byRelativePath.insert(folded(withoutNoteExtension(note.relativePath)), position);
            index.notes.append(std::move(note));
        }
    }
    return index;
}

Resolution resolve(const Index &index, const QString &fromPath, const QString &target)
{
    const QString name = target.trimmed();
    if (name.isEmpty()) {
        return found(fromPath);
    }
    if (index.root.isEmpty()) {
        return {};
    }
    if (QDir::isAbsolutePath(name) || name.startsWith(QLatin1Char('~'))) {
        return {Status::Outside, {}, {}};
    }
    const QString fromFolder = folderOf(fromPath);

    const bool relativeToNote = name.startsWith(QLatin1String("./")) || name.startsWith(QLatin1String("../")) || name == QLatin1String("..");
    if (relativeToNote || name.contains(QLatin1Char('/'))) {
        QStringList bases;
        if (!relativeToNote) {
            bases.append(index.root);
        }
        bases.append(fromFolder);
        bool outside = true;
        for (const QString &base : std::as_const(bases)) {
            const QString path = QDir::cleanPath(base + QLatin1Char('/') + name);
            if (!isInside(index.root, path)) {
                continue;
            }
            outside = false;
            const QString relative = QDir(index.root).relativeFilePath(path);
            const auto note = index.byRelativePath.constFind(folded(withoutNoteExtension(relative)));
            if (note != index.byRelativePath.constEnd()) {
                return found(index.notes.at(*note).path);
            }
            const QString attachment = attachmentAt(index, path);
            if (!attachment.isEmpty()) {
                return found(attachment);
            }
        }
        return {outside ? Status::Outside : Status::Missing, {}, {}};
    }

    const QList<qsizetype> matches = index.byName.value(folded(withoutNoteExtension(name)));
    if (matches.size() == 1) {
        return found(index.notes.at(matches.constFirst()).path);
    }
    if (matches.size() > 1) {
        QStringList candidates;
        QStringList nearby;
        for (const qsizetype match : matches) {
            const QString &path = index.notes.at(match).path;
            candidates.append(path);
            if (folderOf(path) == fromFolder) {
                nearby.append(path);
            }
        }
        if (nearby.size() == 1) {
            return found(nearby.constFirst());
        }
        candidates.sort();
        return {Status::Ambiguous, {}, candidates};
    }
    for (const QString &base : {fromFolder, index.root}) {
        if (!isInside(index.root, base)) {
            continue;
        }
        const QString attachment = attachmentAt(index, QDir::cleanPath(base + QLatin1Char('/') + name));
        if (!attachment.isEmpty()) {
            return found(attachment);
        }
    }
    return {};
}

QList<Backlink> backlinks(const Index &index, const QString &notePath)
{
    QList<Backlink> result;
    for (const Note &note : index.notes) {
        if (note.path == notePath) {
            continue;
        }
        for (const WikiLinks::Link &link : note.links) {
            if (link.target.isEmpty()) {
                continue;
            }
            const Resolution resolution = resolve(index, note.path, link.target);
            if (resolution.status == Status::Found && resolution.path == notePath) {
                result.append({note.path, link});
            }
        }
    }
    return result;
}
}
