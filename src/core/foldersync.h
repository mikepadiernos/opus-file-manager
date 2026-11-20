// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QStringList>

#include <functional>

namespace FolderSync
{
struct Options {
    QStringList includePatterns;
    QStringList excludePatterns;
    bool recursive = true;
    bool compareContents = false;
    bool compareSymlinks = false;
};

enum class Kind {
    Missing,
    File,
    Directory,
    Symlink,
    Other
};
enum class Difference {
    LeftOnly,
    RightOnly,
    Type,
    Size,
    Time,
    MetadataEqual,
    ContentEqual,
    ContentDifferent,
    LinkEqual,
    LinkDifferent,
    Directory,
    Unsupported
};

struct Entry {
    Kind kind = Kind::Missing;
    quint64 device = 0;
    quint64 inode = 0;
    qint64 size = 0;
    qint64 modifiedSeconds = 0;
    qint64 modifiedNanoseconds = 0;
    qint64 changedSeconds = 0;
    qint64 changedNanoseconds = 0;
    QByteArray linkTarget;
};

struct Row {
    QString relativePath;
    Entry left;
    Entry right;
    Difference difference = Difference::Unsupported;
};

struct Comparison {
    QString leftRoot;
    QString rightRoot;
    Entry leftRootEntry;
    Entry rightRootEntry;
    QList<Row> rows;
    QString error;
};

enum class Action {
    Skip,
    CopyToLeft,
    CopyToRight,
    DeleteLeft,
    DeleteRight
};

enum class CopyPolicy {
    All,
    SkipExisting,
    NewerOnly
};

struct ApplyResult {
    QStringList completed;
    QString error;
};

QString optionsError(const Options &options);
bool copyAllowed(const Entry &source, const Entry &target, CopyPolicy policy);
Comparison compare(const QString &leftRoot, const QString &rightRoot, const Options &options, const std::function<bool()> &canceled = {});
QString planError(const Comparison &comparison, const QList<Action> &actions, bool allowOverwrite, bool allowDelete);
ApplyResult apply(const Comparison &comparison,
                  const QList<Action> &actions,
                  bool allowOverwrite,
                  bool allowDelete,
                  const std::function<bool()> &canceled = {},
                  quint64 speedLimitBytesPerSecond = 0);
}
