// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicatefinder.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSet>

DuplicateFinder::Result DuplicateFinder::find(const QStringList &paths, const Checksums::Cancel &cancel, const Checksums::Progress &progress)
{
    Result result;
    QMap<qint64, QStringList> sizes;
    QSet<QString> seen;
    for (const QString &path : paths) {
        if (cancel && cancel()) {
            result.canceled = true;
            return result;
        }

        const QFileInfo info(path);
        if (!info.isAbsolute() || info.isSymLink() || !info.isFile()) {
            result.errors.append(i18nc("@info", "Not an existing regular file: %1", path));
        } else if (!seen.contains(info.absoluteFilePath())) {
            seen.insert(info.absoluteFilePath());
            sizes[info.size()].append(info.absoluteFilePath());
        }
    }
    int total = 0;
    for (const QStringList &files : std::as_const(sizes)) {
        if (files.size() > 1) {
            total += files.size();
        }
    }
    int done = 0;
    for (auto size = sizes.cbegin(); size != sizes.cend(); ++size) {
        if (size.value().size() < 2) {
            continue;
        }
        QMap<QByteArray, QStringList> hashes;
        for (const QString &path : size.value()) {
            if (progress) {
                progress(done, total, path);
            }
            const QFileInfo before(path);
            const QDateTime modified = before.lastModified();
            if (!before.isFile() || before.isSymLink() || before.size() != size.key() || !modified.isValid()) {
                result.errors.append(i18nc("@info", "File changed or is unavailable: %1", path));
                ++done;
                continue;
            }
            const auto digest = Checksums::digestFile(path, cancel);
            if (digest.canceled) {
                result.canceled = true;
                result.groups.clear();
                return result;
            }

            const QFileInfo after(path);
            if (!digest.error.isEmpty()) {
                result.errors.append(i18nc("@info", "Could not read %1: %2", path, digest.error));
            } else if (!after.isFile() || after.isSymLink() || after.size() != size.key() || after.lastModified() != modified) {
                result.errors.append(i18nc("@info", "File changed while reading: %1", path));
            } else {
                hashes[digest.digest].append(path);
            }
            ++done;
        }
        for (auto hash = hashes.cbegin(); hash != hashes.cend(); ++hash) {
            if (hash.value().size() > 1) {
                result.groups.append({size.key(), hash.key(), hash.value()});
            }
        }
    }
    if (cancel && cancel()) {
        result.canceled = true;
        result.groups.clear();
    } else if (progress) {
        progress(total, total, QString());
    }
    return result;
}

DuplicateFinder::SelectionResult DuplicateFinder::select(const QList<Group> &groups, SelectionPolicy policy, const QString &folder)
{
    SelectionResult result;
    const QFileInfo folderInfo(folder);
    if (policy == SelectionPolicy::InFolder && (!folderInfo.isAbsolute() || !folderInfo.isDir())) {
        result.errors.append(i18nc("@info", "Choose an existing absolute local folder."));
        return result;
    }
    for (const Group &group : groups) {
        QStringList paths = group.paths;
        paths.sort();
        if (paths.size() < 2) {
            result.errors.append(i18nc("@info", "A duplicate group has fewer than two paths."));
            continue;
        }
        bool valid = true;
        QString keep;
        QDateTime keepTime;
        for (const QString &path : std::as_const(paths)) {
            const QFileInfo info(path);
            const QDateTime time = info.lastModified();
            if (!info.isAbsolute() || !info.isFile() || info.isSymLink() || (policy != SelectionPolicy::InFolder && !time.isValid())) {
                result.errors.append(i18nc("@info", "Could not select candidates from this group because a file is unavailable: %1", path));
                valid = false;
                break;
            }
            if (keep.isEmpty() || (policy == SelectionPolicy::KeepNewest && time > keepTime) || (policy == SelectionPolicy::KeepOldest && time < keepTime)) {
                keep = path;
                keepTime = time;
            }
        }
        if (!valid) {
            continue;
        }
        if (policy == SelectionPolicy::InFolder) {
            QStringList candidates;
            const QDir root(folderInfo.absoluteFilePath());
            for (const QString &path : std::as_const(paths)) {
                const QString relative = root.relativeFilePath(path);
                if (!QDir::isAbsolutePath(relative) && relative != QLatin1String("..") && !relative.startsWith(QLatin1String("../"))) {
                    candidates.append(path);
                }
            }
            if (candidates.size() == paths.size()) {
                candidates.removeFirst();
            }
            result.paths.append(candidates);
            continue;
        }
        paths.removeAll(keep);
        result.paths.append(paths);
    }
    return result;
}
