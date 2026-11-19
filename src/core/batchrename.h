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

struct Rule {
    QString pattern;
    QString replacement;
    bool regularExpression = false;
    int start = 1;
    int padding = 2;
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
Plan plan(const QList<Input> &inputs, const Rule &rule);
QString ruleError(const Rule &rule);
Result apply(const Plan &plan, const std::function<bool()> &canceled = {});
}
