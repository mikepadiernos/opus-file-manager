// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileMetaData/Properties>

#include <QStringList>

#include <functional>

namespace BatchRename
{
struct Input {
    QString path;
    KFileMetaData::PropertyMultiMap metadata;
};

struct Row {
    QString source;
    QString destination;
    QString error;
    quint64 device = 0;
    quint64 inode = 0;
};

struct Plan {
    QList<Row> rows;
    QString error;

    bool valid() const;
};

struct Result {
    QStringList renamed;
    QString error;
};

Plan plan(const QList<Input> &inputs, const QString &pattern, int start, int padding);
Result apply(const Plan &plan, const std::function<bool()> &canceled = {});
}
