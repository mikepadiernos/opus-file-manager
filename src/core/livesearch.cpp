// SPDX-License-Identifier: GPL-3.0-or-later

#include "livesearch.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>

namespace
{
constexpr qint64 MaxTextSize = 2 * 1024 * 1024;
constexpr int MaxResults = 5000;
constexpr int MaxEntries = 200000;
}

LiveSearch::Result LiveSearch::find(const QString &folder, const QString &text, Field field, const Cancel &cancel, const Progress &progress)
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
            if (result.scannedEntries >= MaxEntries) {
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
            if (entry.isDir()) {
                pending.append(entry.absoluteFilePath());
            }

            bool matches = false;
            if (field == Field::FileName) {
                matches = entry.fileName().contains(text, Qt::CaseInsensitive);
            } else if (entry.isFile()) {
                if (entry.size() > MaxTextSize) {
                    ++result.skippedFiles;
                    continue;
                }
                QFile file(entry.absoluteFilePath());
                if (!file.open(QIODevice::ReadOnly)) {
                    result.errors.append(i18nc("@info", "Could not read %1: %2", entry.absoluteFilePath(), file.errorString()));
                    continue;
                }
                const QByteArray bytes = file.read(MaxTextSize + 1);
                if (cancel && cancel()) {
                    result.canceled = true;
                    return result;
                }
                if (file.error() != QFileDevice::NoError) {
                    result.errors.append(i18nc("@info", "Could not read %1: %2", entry.absoluteFilePath(), file.errorString()));
                    continue;
                }
                if (bytes.size() > MaxTextSize || bytes.contains('\0')) {
                    ++result.skippedFiles;
                    continue;
                }
                QStringDecoder decoder(QStringDecoder::Utf8);
                const QString contents = decoder.decode(bytes);
                if (decoder.hasError()) {
                    ++result.skippedFiles;
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
