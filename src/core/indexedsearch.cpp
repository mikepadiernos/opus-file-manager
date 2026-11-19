// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexedsearch.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/IndexerConfig>
#include <Baloo/Query>
#include <Baloo/ResultIterator>

namespace
{
bool folderConfigured(const QString &folder, const Baloo::IndexerConfig &config)
{
    if (folder.isEmpty() || !QDir::isAbsolutePath(folder)) {
        return false;
    }
    const QString path = QDir::cleanPath(folder);
    int closest = -1;
    bool included = false;
    const auto consider = [&](const QStringList &folders, bool value) {
        for (const QString &entry : folders) {
            const QString base = QDir::cleanPath(entry);
            const QString relative = QDir(base).relativeFilePath(path);
            if (!QDir::isAbsolutePath(relative) && relative != QLatin1String("..") && !relative.startsWith(QLatin1String("../")) && base.size() > closest) {
                closest = base.size();
                included = value;
            }
        }
    };
    consider(config.includeFolders(), true);
    consider(config.excludeFolders(), false);
    return included;
}
}
#endif

LiveSearch::Result IndexedSearch::find(const QString &currentFolder,
                                       const QString &text,
                                       LiveSearch::Field field,
                                       const LiveSearch::Cancel &cancel,
                                       const LiveSearch::Progress &progress)
{
    LiveSearch::Result result;
    if (text.trimmed().isEmpty()) {
        result.errors.append(i18nc("@info", "Enter text to search for."));
        return result;
    }
    if (text.contains(QLatin1Char('"'))) {
        result.errors.append(i18nc("@info", "Indexed search does not support double quotes in the search text."));
        return result;
    }
    if (cancel && cancel()) {
        result.canceled = true;
        return result;
    }
#ifdef OPUSFM_HAVE_BALOO
    const Baloo::IndexerConfig config;
    if (!config.fileIndexingEnabled()) {
        result.errors.append(i18nc("@info", "Baloo file indexing is disabled. Enable it in System Settings or use current-folder search."));
        return result;
    }
    if (field == LiveSearch::Field::Contents && config.onlyBasicIndexing()) {
        result.errors.append(i18nc("@info", "Baloo content indexing is disabled. Enable it in System Settings or use current-folder search."));
        return result;
    }
    if (!folderConfigured(currentFolder, config)) {
        result.notices.append(
            i18nc("@info", "The current location is not configured for Baloo indexing; some descendants or other locations may still be indexed."));
    } else {
        result.notices.append(
            i18nc("@info", "The current location may only be partly indexed. Baloo configuration does not guarantee that files are indexed."));
    }
    QString dbFolder = qEnvironmentVariable("BALOO_DB_PATH");
    if (dbFolder.isEmpty()) {
        dbFolder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/baloo");
    }
    const QFileInfo database(dbFolder + QStringLiteral("/index"));
    if (!database.isFile() || !database.isReadable()) {
        result.errors.append(i18nc("@info", "The Baloo index is missing or unreadable. Check file indexing in System Settings."));
        return result;
    }

    Baloo::Query query;
    query.setSearchString((field == LiveSearch::Field::FileName ? QStringLiteral("filename:\"") : QStringLiteral("content:\"")) + text + QLatin1Char('"'));
    query.setSortingOption(Baloo::Query::SortNone);
    query.setLimit(5001);
    auto iterator = query.exec();
    while (iterator.next()) {
        if (cancel && cancel()) {
            result.canceled = true;
            return result;
        }
        if (result.paths.size() == 5000) {
            result.limited = true;
            break;
        }
        result.paths.append(iterator.filePath());
        ++result.scannedEntries;
        if (progress) {
            progress(result.scannedEntries, iterator.filePath());
        }
    }
    if (cancel && cancel()) {
        result.canceled = true;
    }
    result.notices.append(i18nc("@info", "Baloo results may be incomplete or stale; Baloo does not report index read errors here."));
#else
    Q_UNUSED(currentFolder)
    Q_UNUSED(field)
    Q_UNUSED(progress)
    result.errors.append(i18nc("@info", "Indexed search is unavailable because Opus was built without Baloo."));
#endif
    return result;
}
