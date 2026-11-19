// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicatefinder.h"

#include <KLocalizedString>

#include <QDateTime>
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
