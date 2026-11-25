// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryscan.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

#include <algorithm>

#include <sys/stat.h>

namespace
{
int depthOf(const QString &relativePath)
{
    return int(relativePath.count(QLatin1Char('/'))) + 1;
}
}

LibraryScan::Scope::Scope(const Options &options)
    : m_hidden(options.hidden)
    , m_depth(std::max(0, options.depth))
{
    for (const QString &exclusion : options.exclusions) {
        QString pattern = exclusion.trimmed();
        Pattern compiled;
        if (pattern.endsWith(QLatin1Char('/'))) {
            compiled.foldersOnly = true;
        }
        while (pattern.endsWith(QLatin1Char('/'))) {
            pattern.chop(1);
        }
        // A leading / anchors a pattern to the library root.
        if (pattern.startsWith(QLatin1Char('/'))) {
            compiled.path = true;
        }
        while (pattern.startsWith(QLatin1Char('/'))) {
            pattern.remove(0, 1);
        }
        if (pattern.isEmpty()) {
            continue;
        }
        compiled.path = compiled.path || pattern.contains(QLatin1Char('/'));
        compiled.expression = QRegularExpression(QRegularExpression::wildcardToRegularExpression(
            pattern,
            compiled.path ? QRegularExpression::DefaultWildcardConversion : QRegularExpression::NonPathWildcardConversion));
        if (compiled.expression.isValid()) {
            m_patterns.append(compiled);
        }
    }
}

bool LibraryScan::Scope::containsItem(const QString &relativePath, bool folder) const
{
    if (relativePath.isEmpty()) {
        return true;
    }
    if (m_depth > 0 && depthOf(relativePath) > m_depth) {
        return false;
    }
    const QString name = relativePath.mid(relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    if (!m_hidden && name.startsWith(QLatin1Char('.'))) {
        return false;
    }
    for (const Pattern &pattern : m_patterns) {
        if (pattern.foldersOnly && !folder) {
            continue;
        }
        if (pattern.expression.match(pattern.path ? relativePath : name).hasMatch()) {
            return false;
        }
    }
    return true;
}

bool LibraryScan::Scope::contains(const QString &relativePath, bool folder) const
{
    if (isUnrestricted()) {
        return true;
    }
    qsizetype from = 0;
    while (true) {
        const qsizetype slash = relativePath.indexOf(QLatin1Char('/'), from);
        if (slash < 0) {
            return containsItem(relativePath, folder);
        }
        if (!containsItem(relativePath.left(slash), true)) {
            return false;
        }
        from = slash + 1;
    }
}

bool LibraryScan::Scope::isUnrestricted() const
{
    return m_patterns.isEmpty() && m_hidden && m_depth == 0;
}

bool LibraryScan::inScope(const Options &options, const QString &relativePath, bool folder)
{
    return Scope(options).contains(relativePath, folder);
}

bool LibraryScan::Result::isComplete() const
{
    return !canceled && !limited && errors.isEmpty();
}

bool LibraryScan::indexable(const QString &name)
{
    return !TagLibrary::isLibraryFile(name) && name != QLatin1String(".mfm.json");
}

bool LibraryScan::observe(const QString &root, const QString &absolutePath, TagLibrary::Observation *observation)
{
    const QString cleanRoot = QDir::cleanPath(root);
    const QString path = QDir::cleanPath(absolutePath);
    if (!path.startsWith(cleanRoot + QLatin1Char('/'))) {
        return false;
    }
    const QFileInfo info(path);
    if (!indexable(info.fileName())) {
        return false;
    }
    struct stat data;
    if (::lstat(QFile::encodeName(path).constData(), &data) != 0 || S_ISLNK(data.st_mode)) {
        return false;
    }
    observation->path = path.mid(cleanRoot.size() + 1);
    observation->folder = S_ISDIR(data.st_mode);
    observation->size = observation->folder ? 0 : qint64(data.st_size);
    observation->modified = qint64(data.st_mtim.tv_sec) * 1000 + data.st_mtim.tv_nsec / 1000000;
    observation->device = quint64(data.st_dev);
    observation->inode = quint64(data.st_ino);
    return true;
}

LibraryScan::Result LibraryScan::scan(const QString &root, const Cancel &cancel, const Progress &progress)
{
    return scan(root, Options(), cancel, progress);
}

LibraryScan::Result LibraryScan::scan(const QString &root, const Options &options, const Cancel &cancel, const Progress &progress)
{
    Result result;
    const QString cleanRoot = QDir::cleanPath(root);
    if (const QString problem = TagLibrary::checkLocation(cleanRoot); !problem.isEmpty()) {
        result.errors.append(problem);
        return result;
    }
    const Scope scope(options);
    int scanned = 0;
    QStringList pending{cleanRoot};
    while (!pending.isEmpty()) {
        if (cancel && cancel()) {
            result.canceled = true;
            return result;
        }
        const QString folder = pending.takeFirst();
        const QDir directory(folder);
        if (!directory.isReadable()) {
            result.errors.append(i18nc("@info", "Could not read folder: %1", folder));
            continue;
        }
        const QStringList names = directory.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
        QStringList subfolders;
        for (const QString &name : names) {
            if (cancel && cancel()) {
                result.canceled = true;
                return result;
            }
            const QString path = directory.filePath(name);
            if (progress) {
                progress(++scanned, path);
            }
            if (!indexable(name)) {
                continue;
            }
            TagLibrary::Observation observation;
            if (!observe(cleanRoot, path, &observation)) {
                // A name that doesn't survive decoding can't be found again by path.
                if (!QFileInfo(path).isSymLink()) {
                    ++result.skipped;
                }
                continue;
            }
            if (!scope.containsItem(observation.path, observation.folder)) {
                continue;
            }
            if (options.limit > 0 && result.items.size() >= options.limit) {
                result.limited = true;
                return result;
            }
            if (observation.folder && (options.depth <= 0 || depthOf(observation.path) < options.depth)) {
                subfolders.append(path);
            }
            result.items.append(observation);
        }
        pending = subfolders + pending;
    }
    return result;
}

LibraryScan::Applied LibraryScan::apply(const QString &root, const Result &result, const Options &options)
{
    Applied applied;
    const bool complete = result.isComplete();
    if (result.items.isEmpty() && !complete) {
        applied.saved = true;
        return applied;
    }
    const Scope scope(options);
    std::function<bool(const QString &, bool)> inScope;
    if (!scope.isUnrestricted()) {
        inScope = [&scope](const QString &path, bool folder) {
            return scope.contains(path, folder);
        };
    }
    applied.saved = TagLibrary::update(
        root,
        [&](TagLibrary::Library &library, QString *) {
            applied.changes = library.applyScan(result.items, complete, inScope);
            applied.missing = int(std::count_if(library.entries().cbegin(), library.entries().cend(), [](const TagLibrary::Entry &entry) {
                return entry.missing;
            }));
            return true;
        },
        &applied.error);
    if (!applied.saved) {
        applied.changes = {};
        applied.missing = 0;
    }
    return applied;
}

QString LibraryScan::summary(const Result &result, const Applied &applied)
{
    QStringList lines;
    const TagLibrary::ScanChanges &changes = applied.changes;
    if (!applied.saved) {
        lines.append(applied.error);
    } else {
        lines.append(i18ncp("@info", "Added one entry.", "Added %1 entries.", changes.added));
        if (changes.updated) {
            lines.append(i18ncp("@info", "Updated one entry.", "Updated %1 entries.", changes.updated));
        }
        if (changes.found) {
            lines.append(i18ncp("@info", "Found one missing entry again.", "Found %1 missing entries again.", changes.found));
        }
        if (changes.missing) {
            lines.append(i18ncp("@info",
                                "One entry is missing; its tags and fields are kept until you review it.",
                                "%1 entries are missing; their tags and fields are kept until you review them.",
                                changes.missing));
        }
    }
    if (result.canceled) {
        lines.prepend(i18nc("@info", "Refresh canceled. Items found so far were recorded; no entries were marked missing."));
    } else {
        if (!result.errors.isEmpty()) {
            lines.prepend(i18nc("@info", "Some folders couldn't be read, so no entries were marked missing."));
            lines.append(result.errors.mid(0, 5));
        }
        if (result.limited) {
            lines.prepend(i18ncp("@info",
                                 "Stopped after indexing one item, the limit set in Preferences › Tagging › Libraries. No entries were marked missing.",
                                 "Stopped after indexing %1 items, the limit set in Preferences › Tagging › Libraries. No entries were marked missing.",
                                 int(result.items.size())));
        }
    }
    if (result.skipped) {
        lines.append(i18ncp("@info",
                            "Skipped one item whose name isn't valid in the system encoding.",
                            "Skipped %1 items whose names aren't valid in the system encoding.",
                            result.skipped));
    }
    return lines.join(QLatin1Char('\n'));
}
