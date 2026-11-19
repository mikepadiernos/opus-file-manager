// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercomparison.h"

#include <KIO/UDSEntry>
#include <KLocalizedString>

#include <QDateTime>
#include <QMap>

namespace
{
using Status = FolderComparison::Status;

Status classify(const KFileItem &left, const KFileItem &right)
{
    if (left.isNull()) {
        return Status::RightOnly;
    }
    if (right.isNull()) {
        return Status::LeftOnly;
    }
    if (left.isLink() || right.isLink()) {
        if (left.isLink() != right.isLink()) {
            return Status::TypeDifferent;
        }
        if (left.linkDest().isEmpty() || right.linkDest().isEmpty()) {
            return Status::Unknown;
        }
        return left.linkDest() == right.linkDest() ? Status::LinkMatches : Status::LinkDifferent;
    }
    if (left.isDir() != right.isDir() || left.isFile() != right.isFile()) {
        return Status::TypeDifferent;
    }
    if (left.isDir()) {
        return Status::BothFolders;
    }
    if (!left.isFile() || !left.entry().contains(KIO::UDSEntry::UDS_SIZE) || !right.entry().contains(KIO::UDSEntry::UDS_SIZE)) {
        return Status::Unknown;
    }
    if (left.size() != right.size()) {
        return Status::SizeDifferent;
    }
    const QDateTime leftTime = left.time(KFileItem::ModificationTime);
    const QDateTime rightTime = right.time(KFileItem::ModificationTime);
    if (!leftTime.isValid() || !rightTime.isValid()) {
        return Status::Unknown;
    }
    return leftTime == rightTime ? Status::MetadataMatches : Status::TimeDifferent;
}
}

FolderComparison::Result FolderComparison::compare(const KFileItemList &left, const KFileItemList &right)
{
    Result result;
    QMap<QString, QPair<KFileItem, KFileItem>> items;
    for (const KFileItem &item : left) {
        if (item.name() == QLatin1String(".") || item.name() == QLatin1String("..")) {
            continue;
        }
        if (item.isNull() || item.name().isEmpty() || !items[item.name()].first.isNull()) {
            result.error = i18nc("@info", "The first folder returned an invalid or duplicate item name.");
            return result;
        }
        items[item.name()].first = item;
    }
    for (const KFileItem &item : right) {
        if (item.name() == QLatin1String(".") || item.name() == QLatin1String("..")) {
            continue;
        }
        if (item.isNull() || item.name().isEmpty() || !items[item.name()].second.isNull()) {
            result.error = i18nc("@info", "The second folder returned an invalid or duplicate item name.");
            return result;
        }
        items[item.name()].second = item;
    }
    for (auto it = items.cbegin(); it != items.cend(); ++it) {
        result.rows.append({it.key(), it.value().first, it.value().second, classify(it.value().first, it.value().second)});
    }
    return result;
}

bool FolderComparison::isDifference(Status status)
{
    return status != Status::MetadataMatches && status != Status::BothFolders && status != Status::LinkMatches;
}
