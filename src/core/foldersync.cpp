// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersync.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
using namespace FolderSync;
constexpr qsizetype MaxEntries = 100000;

QString systemError(const QString &path)
{
    return i18nc("@info", "Could not inspect %1: %2", path, QString::fromLocal8Bit(std::strerror(errno)));
}

bool snapshot(const QString &path, Entry &entry, QString &error)
{
    struct stat data;
    if (::lstat(QFile::encodeName(path).constData(), &data) != 0) {
        error = systemError(path);
        return false;
    }
    entry.kind = S_ISREG(data.st_mode) ? Kind::File : S_ISDIR(data.st_mode) ? Kind::Directory : S_ISLNK(data.st_mode) ? Kind::Symlink : Kind::Other;
    entry.device = data.st_dev;
    entry.inode = data.st_ino;
    entry.size = data.st_size;
    entry.modifiedSeconds = data.st_mtim.tv_sec;
    entry.modifiedNanoseconds = data.st_mtim.tv_nsec;
    entry.changedSeconds = data.st_ctim.tv_sec;
    entry.changedNanoseconds = data.st_ctim.tv_nsec;
    if (entry.kind == Kind::Symlink) {
        QByteArray target(256, Qt::Uninitialized);
        ssize_t length = 0;
        while (true) {
            length = ::readlink(QFile::encodeName(path).constData(), target.data(), target.size());
            if (length < 0) {
                error = systemError(path);
                return false;
            }
            if (length < target.size()) {
                break;
            }
            if (target.size() >= 1024 * 1024) {
                error = i18nc("@info", "The symbolic link target is too long: %1", path);
                return false;
            }
            target.resize(target.size() * 2);
        }
        target.resize(length);
        entry.linkTarget = target;
        struct stat after;
        if (::lstat(QFile::encodeName(path).constData(), &after) != 0 || data.st_dev != after.st_dev || data.st_ino != after.st_ino
            || data.st_ctim.tv_sec != after.st_ctim.tv_sec || data.st_ctim.tv_nsec != after.st_ctim.tv_nsec) {
            error = i18nc("@info", "A symbolic link changed during comparison: %1", path);
            return false;
        }
    }
    return true;
}

bool sameSnapshot(const Entry &left, const Entry &right)
{
    return left.kind == right.kind && left.device == right.device && left.inode == right.inode && left.size == right.size
        && left.modifiedSeconds == right.modifiedSeconds && left.modifiedNanoseconds == right.modifiedNanoseconds && left.changedSeconds == right.changedSeconds
        && left.changedNanoseconds == right.changedNanoseconds && left.linkTarget == right.linkTarget;
}

bool sameFile(int fd, const Entry &entry)
{
    struct stat data;
    return ::fstat(fd, &data) == 0 && S_ISREG(data.st_mode) && quint64(data.st_dev) == entry.device && quint64(data.st_ino) == entry.inode
        && data.st_size == entry.size && data.st_mtim.tv_sec == entry.modifiedSeconds && data.st_mtim.tv_nsec == entry.modifiedNanoseconds
        && data.st_ctim.tv_sec == entry.changedSeconds && data.st_ctim.tv_nsec == entry.changedNanoseconds;
}

bool skipped(const QSet<QString> &links, QString path)
{
    while (!path.isEmpty()) {
        if (links.contains(path)) {
            return true;
        }
        const qsizetype separator = path.lastIndexOf(QLatin1Char('/'));
        path = separator < 0 ? QString() : path.left(separator);
    }
    return false;
}

bool matches(const QList<QRegularExpression> &patterns, const QString &path)
{
    for (const auto &pattern : patterns) {
        if (pattern.match(path).hasMatch()) {
            return true;
        }
    }
    return false;
}

bool compile(const QStringList &patterns, QList<QRegularExpression> &result, QString &error)
{
    for (const QString &pattern : patterns) {
        if (pattern.isEmpty() || pattern.contains(QChar::Null) || pattern.contains(QLatin1Char('\n')) || pattern.contains(QLatin1Char('\r'))) {
            error = i18nc("@info", "Filter patterns cannot be empty or contain line breaks.");
            return false;
        }
        QRegularExpression expression(QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::NonPathWildcardConversion));
        if (!expression.isValid()) {
            error = i18nc("@info", "Invalid filter pattern: %1", pattern);
            return false;
        }
        result.append(expression);
    }
    return true;
}

bool scan(const QString &root,
          const Options &options,
          const QList<QRegularExpression> &included,
          const QList<QRegularExpression> &excluded,
          QMap<QString, Entry> &entries,
          QSet<QString> &skippedLinks,
          QString &error,
          const std::function<bool()> &canceled)
{
    QStringList folders{QString()};
    qsizetype visited = 0;
    while (!folders.isEmpty()) {
        if (canceled && canceled()) {
            error = i18nc("@info", "Folder comparison was canceled.");
            return false;
        }
        const QString relativeFolder = folders.takeLast();
        const QString folder = QDir(root).filePath(relativeFolder);
        const int folderFd = ::open(QFile::encodeName(folder).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        if (folderFd < 0) {
            error = systemError(folder);
            return false;
        }
        DIR *directory = ::fdopendir(folderFd);
        if (!directory) {
            error = systemError(folder);
            ::close(folderFd);
            return false;
        }
        bool success = true;
        while (true) {
            errno = 0;
            const dirent *item = ::readdir(directory);
            if (!item) {
                if (errno) {
                    error = systemError(folder);
                    success = false;
                }
                break;
            }
            const QByteArray nativeName(item->d_name);
            if (nativeName == "." || nativeName == "..") {
                continue;
            }
            if (canceled && canceled()) {
                error = i18nc("@info", "Folder comparison was canceled.");
                success = false;
                break;
            }
            if (++visited > MaxEntries) {
                error = i18nc("@info", "Folder comparison exceeds the %1-item limit.", MaxEntries);
                success = false;
                break;
            }
            const QString name = QFile::decodeName(nativeName);
            if (QFile::encodeName(name) != nativeName) {
                error = i18nc("@info", "A filename in %1 cannot be represented safely.", folder);
                success = false;
                break;
            }
            const QString relative = relativeFolder.isEmpty() ? name : relativeFolder + QLatin1Char('/') + name;
            Entry entry;
            if (!snapshot(QDir(root).filePath(relative), entry, error)) {
                success = false;
                break;
            }
            if (matches(excluded, relative)) {
                continue;
            }
            if (entry.kind == Kind::Symlink && !options.compareSymlinks) {
                skippedLinks.insert(relative);
                continue;
            }
            if (entry.kind == Kind::Directory && options.recursive) {
                folders.append(relative);
            }
            if (entry.kind == Kind::Directory || included.isEmpty() || matches(included, relative)) {
                entries.insert(relative, entry);
            }
        }
        if (::closedir(directory) != 0 && success) {
            error = systemError(folder);
            success = false;
        }
        if (!success) {
            return false;
        }
    }
    return true;
}

bool equalContents(const QString &left,
                   const QString &right,
                   const Entry &leftEntry,
                   const Entry &rightEntry,
                   bool &equal,
                   QString &error,
                   const std::function<bool()> &canceled)
{
    QFile first(left);
    QFile second(right);
    const int firstFd = ::open(QFile::encodeName(left).constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (firstFd < 0) {
        error = systemError(left);
        return false;
    }
    if (!first.open(firstFd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(firstFd);
        error = i18nc("@info", "Could not read %1: %2", left, first.errorString());
        return false;
    }
    const int secondFd = ::open(QFile::encodeName(right).constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (secondFd < 0) {
        error = systemError(right);
        return false;
    }
    if (!second.open(secondFd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(secondFd);
        error = i18nc("@info", "Could not read %1: %2", right, second.errorString());
        return false;
    }
    if (!sameFile(first.handle(), leftEntry) || !sameFile(second.handle(), rightEntry)) {
        error = i18nc("@info", "A file changed before content comparison.");
        return false;
    }
    equal = true;
    while (true) {
        if (canceled && canceled()) {
            error = i18nc("@info", "Folder comparison was canceled.");
            return false;
        }
        const QByteArray a = first.read(128 * 1024);
        const QByteArray b = second.read(128 * 1024);
        if (first.error() != QFileDevice::NoError || second.error() != QFileDevice::NoError) {
            error = i18nc("@info", "Could not read files while comparing contents.");
            return false;
        }
        if (a != b) {
            equal = false;
            break;
        }
        if (a.isEmpty()) {
            break;
        }
    }
    Entry leftAfter;
    Entry rightAfter;
    if (!snapshot(left, leftAfter, error) || !snapshot(right, rightAfter, error)) {
        return false;
    }
    if (!sameSnapshot(leftEntry, leftAfter) || !sameSnapshot(rightEntry, rightAfter)) {
        error = i18nc("@info", "A file changed during content comparison.");
        return false;
    }
    return true;
}
}

FolderSync::Comparison FolderSync::compare(const QString &leftRoot, const QString &rightRoot, const Options &options, const std::function<bool()> &canceled)
{
    Comparison result;
    Entry leftRootEntry;
    Entry rightRootEntry;
    if (!QDir::isAbsolutePath(leftRoot) || !QDir::isAbsolutePath(rightRoot) || !snapshot(leftRoot, leftRootEntry, result.error)
        || !snapshot(rightRoot, rightRootEntry, result.error) || leftRootEntry.kind != Kind::Directory || rightRootEntry.kind != Kind::Directory) {
        if (result.error.isEmpty()) {
            result.error = i18nc("@info", "Choose two existing local folders that are not symbolic links.");
        }
        return result;
    }
    result.leftRoot = QFileInfo(leftRoot).canonicalFilePath();
    result.rightRoot = QFileInfo(rightRoot).canonicalFilePath();
    if (result.leftRoot.isEmpty() || result.rightRoot.isEmpty() || result.leftRoot == result.rightRoot || result.leftRoot == QLatin1String("/")
        || result.rightRoot == QLatin1String("/") || result.leftRoot.startsWith(result.rightRoot + QLatin1Char('/'))
        || result.rightRoot.startsWith(result.leftRoot + QLatin1Char('/'))) {
        result.error = i18nc("@info", "Choose two separate folders that do not contain one another.");
        return result;
    }
    QList<QRegularExpression> included;
    QList<QRegularExpression> excluded;
    if (!compile(options.includePatterns, included, result.error) || !compile(options.excludePatterns, excluded, result.error)) {
        return result;
    }
    QMap<QString, Entry> left;
    QMap<QString, Entry> right;
    QSet<QString> skippedLinks;
    if (!scan(result.leftRoot, options, included, excluded, left, skippedLinks, result.error, canceled)
        || !scan(result.rightRoot, options, included, excluded, right, skippedLinks, result.error, canceled)) {
        return result;
    }
    for (auto it = right.cbegin(); it != right.cend(); ++it) {
        if (!left.contains(it.key())) {
            left.insert(it.key(), {});
        }
    }
    for (auto it = left.cbegin(); it != left.cend(); ++it) {
        if (canceled && canceled()) {
            result.error = i18nc("@info", "Folder comparison was canceled.");
            result.rows.clear();
            return result;
        }
        if (skipped(skippedLinks, it.key())) {
            continue;
        }
        const Entry a = it.value();
        const Entry b = right.value(it.key());
        Difference difference = Difference::Unsupported;
        if (a.kind == Kind::Missing) {
            difference = Difference::RightOnly;
        } else if (b.kind == Kind::Missing) {
            difference = Difference::LeftOnly;
        } else if (a.kind != b.kind) {
            difference = Difference::Type;
        } else if (a.kind == Kind::Directory) {
            difference = Difference::Directory;
        } else if (a.kind == Kind::Symlink) {
            difference = a.linkTarget == b.linkTarget ? Difference::LinkEqual : Difference::LinkDifferent;
        } else if (a.kind == Kind::File) {
            if (a.size != b.size) {
                difference = Difference::Size;
            } else if (options.compareContents) {
                bool equal = false;
                if (!equalContents(QDir(result.leftRoot).filePath(it.key()), QDir(result.rightRoot).filePath(it.key()), a, b, equal, result.error, canceled)) {
                    result.rows.clear();
                    return result;
                }
                difference = equal ? Difference::ContentEqual : Difference::ContentDifferent;
            } else {
                difference =
                    a.modifiedSeconds == b.modifiedSeconds && a.modifiedNanoseconds == b.modifiedNanoseconds ? Difference::MetadataEqual : Difference::Time;
            }
        }
        result.rows.append({it.key(), a, b, difference});
    }
    return result;
}
