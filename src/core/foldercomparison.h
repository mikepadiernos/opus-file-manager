// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

namespace FolderComparison
{
enum class Status {
    LeftOnly,
    RightOnly,
    TypeDifferent,
    SizeDifferent,
    TimeDifferent,
    MetadataMatches,
    BothFolders,
    LinkDifferent,
    LinkMatches,
    Unknown,
};

struct Row {
    QString name;
    KFileItem left;
    KFileItem right;
    Status status;
};

struct Result {
    QList<Row> rows;
    QString error;
};

Result compare(const KFileItemList &left, const KFileItemList &right);
bool isDifference(Status status);
}
