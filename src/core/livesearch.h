// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

#include <functional>

namespace LiveSearch
{
enum class Field {
    FileName,
    Contents,
};

struct Result {
    QStringList paths;
    QStringList errors;
    QStringList notices;
    int skippedFiles = 0;
    int scannedEntries = 0;
    bool canceled = false;
    bool limited = false;
};

using Cancel = std::function<bool()>;
using Progress = std::function<void(int, const QString &)>;

Result find(const QString &folder, const QString &text, Field field, const Cancel &cancel = {}, const Progress &progress = {});
}
