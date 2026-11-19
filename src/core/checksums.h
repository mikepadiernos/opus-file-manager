// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace Checksums
{
struct Entry {
    QString name;
    QString error;
};

struct Result {
    QList<Entry> entries;
    QString error;
    bool canceled = false;
};

using Cancel = std::function<bool()>;
using Progress = std::function<void(int, int, const QString &)>;

struct DigestResult {
    QByteArray digest;
    QString error;
    bool canceled = false;
};

DigestResult digestFile(const QString &path, const Cancel &cancel = {});
Result create(const QStringList &paths, const QString &manifest, const Cancel &cancel = {}, const Progress &progress = {});
Result verify(const QString &manifest, const Cancel &cancel = {}, const Progress &progress = {});
}
