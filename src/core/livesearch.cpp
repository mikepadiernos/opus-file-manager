// SPDX-License-Identifier: GPL-3.0-or-later

#include "livesearch.h"

#include "libraryregistry.h"
#include "searchquery.h"
#include "tagsearch.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>

#include <optional>

namespace
{
constexpr qint64 MaxTextSize = 2 * 1024 * 1024;
constexpr int MaxResults = 5000;

enum class TextRead {
    Text,
    Skipped,
    Failed,
};

/// Reads a small UTF-8 text file; binary, oversized and invalid files are skipped.
TextRead readText(const QFileInfo &entry, QString &contents, QString &error)
{
    if (entry.size() > MaxTextSize) {
        return TextRead::Skipped;
    }
    QFile file(entry.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        error = i18nc("@info", "Could not read %1: %2", entry.absoluteFilePath(), file.errorString());
        return TextRead::Failed;
    }
    const QByteArray bytes = file.read(MaxTextSize + 1);
    if (file.error() != QFileDevice::NoError) {
        error = i18nc("@info", "Could not read %1: %2", entry.absoluteFilePath(), file.errorString());
        return TextRead::Failed;
    }
    if (bytes.size() > MaxTextSize || bytes.contains('\0')) {
        return TextRead::Skipped;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    contents = decoder.decode(bytes);
    return decoder.hasError() ? TextRead::Skipped : TextRead::Text;
}
}

LiveSearch::Result LiveSearch::find(const QString &folder,
                                    const QString &text,
                                    Field field,
                                    const Cancel &cancel,
                                    const Progress &progress,
                                    const PathFilter &filter,
                                    int entryLimit,
                                    const QStringList *tagLibraries)
{
    Result result;
    const QFileInfo root(folder);
    if (!root.isAbsolute() || root.isSymLink() || !root.isDir()) {
        result.errors.append(i18nc("@info", "Choose an existing local folder that is not a symbolic link: %1", folder));
        return result;
    }
    if (text.trimmed().isEmpty()) {
        result.errors.append(i18nc("@info", "Enter text to search for."));
        return result;
    }
    const QString filterError = filter.error();
    if (!filterError.isEmpty()) {
        result.errors.append(filterError);
        return result;
    }
    std::optional<SearchQuery::Query> query;
    std::optional<TagSearch::Context> tags;
    if (field == Field::Query) {
        query = SearchQuery::Query::parse(text);
        if (!query->isValid()) {
            result.errors.append(query->error());
            return result;
        }
        if (query->usesTags()) {
            tags.emplace(tagLibraries ? *tagLibraries : LibraryRegistry::roots());
        }
    }
    if (filter.excludes(root.absoluteFilePath())) {
        return result;
    }

    QStringList pending{root.absoluteFilePath()};
    while (!pending.isEmpty()) {
        if (cancel && cancel()) {
            result.canceled = true;
            return result;
        }
        const QString path = pending.takeLast();
        const QDir directory(path);
        if (!directory.isReadable()) {
            result.errors.append(i18nc("@info", "Could not read folder: %1", path));
            continue;
        }
        const auto entries = directory.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (cancel && cancel()) {
                result.canceled = true;
                return result;
            }
            if (result.scannedEntries >= entryLimit) {
                result.limited = true;
                return result;
            }
            ++result.scannedEntries;
            if (progress) {
                progress(result.scannedEntries, entry.absoluteFilePath());
            }
            if (cancel && cancel()) {
                result.canceled = true;
                return result;
            }
            if (entry.isSymLink()) {
                continue;
            }
            if (filter.excludes(entry.absoluteFilePath())) {
                continue;
            }
            const int depth = QDir(root.absoluteFilePath()).relativeFilePath(entry.absoluteFilePath()).count(QLatin1Char('/'));
            if (entry.isDir() && (filter.maxDepth < 0 || depth < filter.maxDepth)) {
                pending.append(entry.absoluteFilePath());
            }
            if (!filter.matches(root.absoluteFilePath(), entry.absoluteFilePath())) {
                continue;
            }

            bool matches = false;
            if (field == Field::FileName) {
                matches = entry.fileName().contains(text, Qt::CaseInsensitive);
            } else if (field == Field::Query) {
                std::optional<TextRead> read;
                QString contents;
                const qsizetype tagErrors = tags ? tags->errors().size() : 0;
                matches = query->matches(
                    entry,
                    [&](const QString &needle) {
                        if (!read) {
                            QString error;
                            read = readText(entry, contents, error);
                            if (*read == TextRead::Skipped) {
                                ++result.skippedFiles;
                            } else if (*read == TextRead::Failed) {
                                result.errors.append(error);
                            }
                        }
                        return *read == TextRead::Text && contents.contains(needle, Qt::CaseInsensitive);
                    },
                    tags ? tags->matcher(entry.absoluteFilePath()) : SearchQuery::TagMatcher());
                if (tags && tags->errors().size() > tagErrors) {
                    result.errors.append(tags->errors().mid(tagErrors));
                }
                if (read && cancel && cancel()) {
                    result.canceled = true;
                    return result;
                }
            } else if (entry.isFile()) {
                QString contents;
                QString error;
                const TextRead read = readText(entry, contents, error);
                if (cancel && cancel()) {
                    result.canceled = true;
                    return result;
                }
                if (read == TextRead::Skipped) {
                    ++result.skippedFiles;
                    continue;
                }
                if (read == TextRead::Failed) {
                    result.errors.append(error);
                    continue;
                }
                matches = contents.contains(text, Qt::CaseInsensitive);
            }
            if (matches) {
                result.paths.append(entry.absoluteFilePath());
                if (result.paths.size() >= MaxResults) {
                    result.limited = true;
                    return result;
                }
            }
        }
    }
    if (cancel && cancel()) {
        result.canceled = true;
    }
    return result;
}
