// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "checksums.h"

#include <QByteArray>
#include <QList>
#include <QStringList>

namespace DuplicateFinder
{
struct Group {
    qint64 size = 0;
    QByteArray digest;
    QStringList paths;
};

struct Result {
    QList<Group> groups;
    QStringList errors;
    bool canceled = false;
};

enum class SelectionPolicy {
    KeepNewest,
    KeepOldest,
    InFolder,
};

struct SelectionResult {
    QStringList paths;
    QStringList errors;
};

Result find(const QStringList &paths, const Checksums::Cancel &cancel = {}, const Checksums::Progress &progress = {});
SelectionResult select(const QList<Group> &groups, SelectionPolicy policy, const QString &folder = {});
}
