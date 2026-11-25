// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryscan.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>

#include <sys/stat.h>

namespace
{
bool indexable(const QString &name)
{
    return !TagLibrary::isLibraryFile(name) && name != QLatin1String(".mfm.json");
}
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
    Result result;
    const QString cleanRoot = QDir::cleanPath(root);
    if (const QString problem = TagLibrary::checkLocation(cleanRoot); !problem.isEmpty()) {
        result.errors.append(problem);
        return result;
    }
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
            if (observation.folder) {
                subfolders.append(path);
            }
            result.items.append(observation);
        }
        pending = subfolders + pending;
    }
    return result;
}
