// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

#include <functional>

namespace FileSplitter
{
struct Result {
    QStringList outputs;
    QString error;
};

using CancelCheck = std::function<bool()>;

Result split(const QString &source, const QString &outputPrefix, qint64 partSize, const CancelCheck &canceled = {});
Result combine(const QStringList &parts, const QString &output, const CancelCheck &canceled = {});
}
