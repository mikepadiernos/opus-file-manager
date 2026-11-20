// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersync.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
using namespace FolderSync;
constexpr qsizetype MaxEntries = 100000;

class FileDescriptor
{
public:
    explicit FileDescriptor(int fd = -1)
        : m_fd(fd)
    {
    }
    ~FileDescriptor()
    {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    int get() const
    {
        return m_fd;
    }

private:
    int m_fd;
};

QString systemError(const QString &path)
{
    return i18nc("@info", "Could not inspect %1: %2", path, QString::fromLocal8Bit(std::strerror(errno)));
}

bool snapshotAt(int parent, const QByteArray &name, const QString &path, Entry &entry, QString &error)
{
    struct stat data;
    if (::fstatat(parent, name.constData(), &data, AT_SYMLINK_NOFOLLOW) != 0) {
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
            length = ::readlinkat(parent, name.constData(), target.data(), target.size());
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
        if (::fstatat(parent, name.constData(), &after, AT_SYMLINK_NOFOLLOW) != 0 || data.st_dev != after.st_dev || data.st_ino != after.st_ino
            || data.st_ctim.tv_sec != after.st_ctim.tv_sec || data.st_ctim.tv_nsec != after.st_ctim.tv_nsec) {
            error = i18nc("@info", "A symbolic link changed during comparison: %1", path);
            return false;
        }
    }
    return true;
}

bool snapshot(const QString &path, Entry &entry, QString &error)
{
    return snapshotAt(AT_FDCWD, QFile::encodeName(path), path, entry, error);
}

int openDirectory(int root, const QString &relative)
{
    int current = ::fcntl(root, F_DUPFD_CLOEXEC, 0);
    if (current < 0 || relative.isEmpty()) {
        return current;
    }
    const auto parts = relative.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String("..") || part.contains(QChar::Null)) {
            ::close(current);
            errno = EINVAL;
            return -1;
        }
        const int next = ::openat(current, QFile::encodeName(part).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        const int savedError = errno;
        ::close(current);
        if (next < 0) {
            errno = savedError;
            return -1;
        }
        current = next;
    }
    return current;
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

bool sameDirectory(int fd, const Entry &entry)
{
    struct stat data;
    return ::fstat(fd, &data) == 0 && S_ISDIR(data.st_mode) && quint64(data.st_dev) == entry.device && quint64(data.st_ino) == entry.inode;
}

bool unchangedDirectory(int fd, const Entry &entry)
{
    struct stat data;
    return ::fstat(fd, &data) == 0 && S_ISDIR(data.st_mode) && quint64(data.st_dev) == entry.device && quint64(data.st_ino) == entry.inode
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

bool scan(int rootFd,
          const QString &root,
          const Entry &rootEntry,
          const Options &options,
          const QList<QRegularExpression> &included,
          const QList<QRegularExpression> &excluded,
          QMap<QString, Entry> &entries,
          QSet<QString> &skippedLinks,
          QString &error,
          const std::function<bool()> &canceled)
{
    QList<QPair<QString, Entry>> folders{{QString(), rootEntry}};
    qsizetype visited = 0;
    while (!folders.isEmpty()) {
        if (canceled && canceled()) {
            error = i18nc("@info", "Folder comparison was canceled.");
            return false;
        }
        const auto task = folders.takeLast();
        const QString relativeFolder = task.first;
        const QString folder = QDir(root).filePath(relativeFolder);
        const int folderFd = openDirectory(rootFd, relativeFolder);
        if (folderFd < 0) {
            error = systemError(folder);
            return false;
        }
        if (!unchangedDirectory(folderFd, task.second)) {
            ::close(folderFd);
            error = i18nc("@info", "A folder changed during comparison: %1", folder);
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
            if (!snapshotAt(folderFd, nativeName, QDir(root).filePath(relative), entry, error)) {
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
                folders.append({relative, entry});
            }
            if (entry.kind == Kind::Directory || included.isEmpty() || matches(included, relative)) {
                entries.insert(relative, entry);
            }
        }
        if (success && !unchangedDirectory(folderFd, task.second)) {
            error = i18nc("@info", "A folder changed during comparison: %1", folder);
            success = false;
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

bool equalContents(int leftRoot,
                   int rightRoot,
                   const QString &relative,
                   const QString &left,
                   const QString &right,
                   const Entry &leftEntry,
                   const Entry &rightEntry,
                   bool &equal,
                   QString &error,
                   const std::function<bool()> &canceled)
{
    QFile first(left);
    QFile second(right);
    const qsizetype separator = relative.lastIndexOf(QLatin1Char('/'));
    const QString parent = separator < 0 ? QString() : relative.left(separator);
    const QByteArray name = QFile::encodeName(separator < 0 ? relative : relative.mid(separator + 1));
    FileDescriptor leftParent(openDirectory(leftRoot, parent));
    FileDescriptor rightParent(openDirectory(rightRoot, parent));
    if (leftParent.get() < 0 || rightParent.get() < 0) {
        error = i18nc("@info", "A parent folder changed during comparison.");
        return false;
    }
    const int firstFd = ::openat(leftParent.get(), name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (firstFd < 0) {
        error = systemError(left);
        return false;
    }
    if (!first.open(firstFd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(firstFd);
        error = i18nc("@info", "Could not read %1: %2", left, first.errorString());
        return false;
    }
    const int secondFd = ::openat(rightParent.get(), name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
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
    FileDescriptor leftParentAfter(openDirectory(leftRoot, parent));
    FileDescriptor rightParentAfter(openDirectory(rightRoot, parent));
    if (leftParentAfter.get() < 0 || rightParentAfter.get() < 0) {
        error = i18nc("@info", "A parent folder changed during comparison.");
        return false;
    }
    if (!snapshotAt(leftParentAfter.get(), name, left, leftAfter, error) || !snapshotAt(rightParentAfter.get(), name, right, rightAfter, error)) {
        return false;
    }
    if (!sameSnapshot(leftEntry, leftAfter) || !sameSnapshot(rightEntry, rightAfter)) {
        error = i18nc("@info", "A file changed during content comparison.");
        return false;
    }
    return true;
}
}

QString FolderSync::optionsError(const Options &options)
{
    QList<QRegularExpression> included;
    QList<QRegularExpression> excluded;
    QString error;
    if (compile(options.includePatterns, included, error)) {
        compile(options.excludePatterns, excluded, error);
    }
    return error;
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

    result.leftRootEntry = leftRootEntry;
    result.rightRootEntry = rightRootEntry;
    result.leftRoot = QFileInfo(leftRoot).canonicalFilePath();
    result.rightRoot = QFileInfo(rightRoot).canonicalFilePath();
    if (result.leftRoot.isEmpty() || result.rightRoot.isEmpty() || result.leftRoot == result.rightRoot
        || (leftRootEntry.device == rightRootEntry.device && leftRootEntry.inode == rightRootEntry.inode) || result.leftRoot == QLatin1String("/")
        || result.rightRoot == QLatin1String("/") || result.leftRoot.startsWith(result.rightRoot + QLatin1Char('/'))
        || result.rightRoot.startsWith(result.leftRoot + QLatin1Char('/'))) {
        result.error = i18nc("@info", "Choose two separate folders that do not contain one another.");
        return result;
    }
    FileDescriptor leftFd(::open(QFile::encodeName(result.leftRoot).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    FileDescriptor rightFd(::open(QFile::encodeName(result.rightRoot).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    if (!sameDirectory(leftFd.get(), leftRootEntry) || !sameDirectory(rightFd.get(), rightRootEntry)) {
        result.error = i18nc("@info", "A root folder changed before comparison.");
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
    if (!scan(leftFd.get(), result.leftRoot, leftRootEntry, options, included, excluded, left, skippedLinks, result.error, canceled)
        || !scan(rightFd.get(), result.rightRoot, rightRootEntry, options, included, excluded, right, skippedLinks, result.error, canceled)) {
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
                if (!equalContents(leftFd.get(),
                                   rightFd.get(),
                                   it.key(),
                                   QDir(result.leftRoot).filePath(it.key()),
                                   QDir(result.rightRoot).filePath(it.key()),
                                   a,
                                   b,
                                   equal,
                                   result.error,
                                   canceled)) {
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

namespace
{
QString parentOf(const QString &path)
{
    const qsizetype separator = path.lastIndexOf(QLatin1Char('/'));
    return separator < 0 ? QString() : path.left(separator);
}

QByteArray nameOf(const QString &path)
{
    const qsizetype separator = path.lastIndexOf(QLatin1Char('/'));
    return QFile::encodeName(separator < 0 ? path : path.mid(separator + 1));
}

bool validRelative(const QString &path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains(QChar::Null)) {
        return false;
    }
    const auto parts = path.split(QLatin1Char('/'));
    return std::all_of(parts.cbegin(), parts.cend(), [](const QString &part) {
        return !part.isEmpty() && part != QLatin1String(".") && part != QLatin1String("..") && QFile::decodeName(QFile::encodeName(part)) == part;
    });
}

QByteArray temporaryName()
{
    return QByteArrayLiteral(".opus-sync-") + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
}

bool movedSnapshot(const Entry &actual, const Entry &expected)
{
    return actual.kind == expected.kind && actual.device == expected.device && actual.inode == expected.inode && actual.size == expected.size
        && actual.modifiedSeconds == expected.modifiedSeconds && actual.modifiedNanoseconds == expected.modifiedNanoseconds;
}

bool currentEntry(int directory, const QByteArray &name, const QString &display, const Entry &expected, QString &error)
{
    if (expected.kind == Kind::Missing) {
        struct stat data;
        if (::fstatat(directory, name.constData(), &data, AT_SYMLINK_NOFOLLOW) == -1 && errno == ENOENT) {
            return true;
        }
        error = i18nc("@info", "The destination changed since comparison: %1", display);
        return false;
    }
    Entry current;
    if (!snapshotAt(directory, name, display, current, error)) {
        return false;
    }
    if (!sameSnapshot(current, expected)) {
        error = i18nc("@info", "The item changed since comparison: %1", display);
        return false;
    }
    return true;
}

int checkedParent(int root, const QString &relative, const QMap<QString, Entry> &directories, QString &error)
{
    const auto it = directories.constFind(relative);
    if (it == directories.cend()) {
        error = i18nc("@info", "A parent folder was not included in the comparison: %1", relative);
        return -1;
    }
    const int fd = openDirectory(root, relative);
    if (fd < 0 || !sameDirectory(fd, it.value())) {
        if (fd >= 0) {
            ::close(fd);
        }
        error = i18nc("@info", "A parent folder changed since comparison: %1", relative);
        return -1;
    }
    return fd;
}

bool sameDescriptorAt(int directory, const QByteArray &name, int fd)
{
    struct stat expected;
    struct stat actual;
    return ::fstat(fd, &expected) == 0 && ::fstatat(directory, name.constData(), &actual, AT_SYMLINK_NOFOLLOW) == 0 && expected.st_dev == actual.st_dev
        && expected.st_ino == actual.st_ino && S_ISREG(actual.st_mode);
}

class StagingDirectory
{
public:
    StagingDirectory(int parent, const QString &path)
        : m_parent(parent)
        , m_parentPath(QFileInfo(path).dir().path())
    {
    }

    ~StagingDirectory()
    {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    bool create(QString &error)
    {
        for (int attempt = 0; attempt < 16; ++attempt) {
            m_name = temporaryName();
            if (::mkdirat(m_parent, m_name.constData(), 0700) == 0) {
                m_path = QDir(m_parentPath).filePath(QFile::decodeName(m_name));
                m_fd = ::openat(m_parent, m_name.constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
                struct stat data;
                if (m_fd >= 0 && ::fstat(m_fd, &data) == 0 && data.st_uid == ::geteuid() && (data.st_mode & 0077) == 0) {
                    return true;
                }
                error = i18nc("@info", "Could not safely open the temporary folder; inspect %1.", m_path);
                return false;
            }
            if (errno != EEXIST) {
                error = systemError(m_parentPath);
                return false;
            }
        }
        error = i18nc("@info", "Could not create a temporary folder in %1.", m_parentPath);
        return false;
    }

    int fd() const
    {
        return m_fd;
    }

    QString path(const QByteArray &name) const
    {
        return QDir(m_path).filePath(QFile::decodeName(name));
    }

    void remove(QString &error)
    {
        struct stat expected;
        struct stat actual;
        if (::fstat(m_fd, &expected) != 0 || ::fstatat(m_parent, m_name.constData(), &actual, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISDIR(actual.st_mode)
            || expected.st_dev != actual.st_dev || expected.st_ino != actual.st_ino || ::unlinkat(m_parent, m_name.constData(), AT_REMOVEDIR) != 0) {
            error += QLatin1Char(' ') + i18nc("@info", "Could not remove the temporary folder; inspect %1.", m_path);
        }
    }

private:
    int m_parent;
    int m_fd = -1;
    QByteArray m_name;
    QString m_parentPath;
    QString m_path;
};

void removeTemporary(int directory, const QByteArray &name, int fd, const QString &display, QString &error)
{
    if (!sameDescriptorAt(directory, name, fd)) {
        error += QLatin1Char(' ') + i18nc("@info", "Temporary file changed or could not be inspected: %1.", display);
    } else if (::unlinkat(directory, name.constData(), 0) != 0) {
        error += QLatin1Char(' ') + i18nc("@info", "Could not remove temporary file %1.", display);
    }
}

QString restoreBackup(int directory, int staging, const QByteArray &backup, const QByteArray &name, const QString &path, const QString &backupPath)
{
    if (::renameat2(staging, backup.constData(), directory, name.constData(), RENAME_NOREPLACE) == 0) {
        return {};
    }
    return i18nc("@info", "Could not restore %1; inspect %2.", path, backupPath);
}

bool moveToBackup(int directory, int staging, const QByteArray &name, const QString &path, const QString &backupPath, const Entry &expected, QString &error)
{
    const QByteArray backup = QByteArrayLiteral("original");
    if (!currentEntry(directory, name, path, expected, error)) {
        return false;
    }
    if (::renameat2(directory, name.constData(), staging, backup.constData(), RENAME_NOREPLACE) != 0) {
        error = i18nc("@info", "Could not move %1 to a temporary backup: %2", path, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    Entry moved;
    if (!snapshotAt(staging, backup, backupPath, moved, error) || !movedSnapshot(moved, expected)) {
        const QString restore = restoreBackup(directory, staging, backup, name, path, backupPath);
        error = i18nc("@info", "The item changed before it could be replaced: %1", path);
        if (!restore.isEmpty()) {
            error += QLatin1Char(' ') + restore;
        }
        return false;
    }
    return true;
}
}

QString FolderSync::planError(const Comparison &comparison, const QList<Action> &actions, bool allowOverwrite, bool allowDelete)
{
    if (!comparison.error.isEmpty() || !QDir::isAbsolutePath(comparison.leftRoot) || !QDir::isAbsolutePath(comparison.rightRoot)
        || comparison.leftRootEntry.kind != Kind::Directory || comparison.rightRootEntry.kind != Kind::Directory || comparison.rows.size() != actions.size()) {
        return i18nc("@info", "The synchronization preview is invalid.");
    }
    if (comparison.leftRoot == comparison.rightRoot || comparison.leftRoot == QLatin1String("/") || comparison.rightRoot == QLatin1String("/")
        || comparison.leftRoot.startsWith(comparison.rightRoot + QLatin1Char('/')) || comparison.rightRoot.startsWith(comparison.leftRoot + QLatin1Char('/'))
        || (comparison.leftRootEntry.device == comparison.rightRootEntry.device && comparison.leftRootEntry.inode == comparison.rightRootEntry.inode)) {
        return i18nc("@info", "Choose two separate folders that do not contain one another.");
    }
    QSet<QString> paths;
    QSet<QString> leftDirectories{QString()};
    QSet<QString> rightDirectories{QString()};
    QSet<QString> newLeftDirectories;
    QSet<QString> newRightDirectories;
    bool any = false;
    for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
        const Row &row = comparison.rows.at(i);
        if (!validRelative(row.relativePath) || paths.contains(row.relativePath)) {
            return i18nc("@info", "The synchronization preview contains an invalid or duplicate path.");
        }
        paths.insert(row.relativePath);
        if (row.left.kind == Kind::Directory) {
            leftDirectories.insert(row.relativePath);
        }
        if (row.right.kind == Kind::Directory) {
            rightDirectories.insert(row.relativePath);
        }
        const Action action = actions.at(i);
        if (action == Action::Skip) {
            continue;
        }
        any = true;
        if (action == Action::DeleteLeft || action == Action::DeleteRight) {
            const Entry &target = action == Action::DeleteLeft ? row.left : row.right;
            if (!allowDelete || target.kind != Kind::File) {
                return i18nc("@info", "Deletion must be confirmed and is supported for regular files only: %1", row.relativePath);
            }
        } else if (action == Action::CopyToLeft || action == Action::CopyToRight) {
            const Entry &source = action == Action::CopyToLeft ? row.right : row.left;
            const Entry &target = action == Action::CopyToLeft ? row.left : row.right;
            if (source.kind == Kind::Directory && target.kind == Kind::Missing) {
                (action == Action::CopyToLeft ? newLeftDirectories : newRightDirectories).insert(row.relativePath);
            } else if (source.kind != Kind::File || (target.kind != Kind::Missing && target.kind != Kind::File)) {
                return i18nc("@info", "This item cannot be copied by synchronization: %1", row.relativePath);
            } else if (target.kind == Kind::File && !allowOverwrite) {
                return i18nc("@info", "Overwrites require confirmation: %1", row.relativePath);
            } else if (target.kind == Kind::File && source.device == target.device && source.inode == target.inode) {
                return i18nc("@info", "The source and destination are the same file: %1", row.relativePath);
            }
        } else {
            return i18nc("@info", "The synchronization preview contains an invalid action.");
        }
    }
    if (!any) {
        return i18nc("@info", "Choose at least one operation.");
    }
    for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
        const auto action = actions.at(i);
        if (action == Action::Skip) {
            continue;
        }
        const QString parent = parentOf(comparison.rows.at(i).relativePath);
        const bool leftExists = leftDirectories.contains(parent) || newLeftDirectories.contains(parent);
        const bool rightExists = rightDirectories.contains(parent) || newRightDirectories.contains(parent);
        if (((action == Action::CopyToLeft || action == Action::CopyToRight) && (!leftExists || !rightExists)) || (action == Action::DeleteLeft && !leftExists)
            || (action == Action::DeleteRight && !rightExists)) {
            return i18nc("@info", "A required parent folder is missing from the plan: %1", comparison.rows.at(i).relativePath);
        }
    }
    return {};
}

namespace
{
QString copyFile(int sourceDirectory,
                 int destinationDirectory,
                 const QByteArray &name,
                 const QString &sourcePath,
                 const QString &destinationPath,
                 const Entry &source,
                 const Entry &destination,
                 const std::function<QString()> &validateParents,
                 const std::function<bool()> &canceled)
{
    QString error;
    if (!currentEntry(sourceDirectory, name, sourcePath, source, error) || !currentEntry(destinationDirectory, name, destinationPath, destination, error)) {
        return error;
    }
    FileDescriptor input(::openat(sourceDirectory, name.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    struct stat inputStat;
    if (input.get() < 0 || !sameFile(input.get(), source) || ::fstat(input.get(), &inputStat) != 0) {
        return i18nc("@info", "Could not safely open the source file: %1", sourcePath);
    }
    StagingDirectory staging(destinationDirectory, destinationPath);
    if (!staging.create(error)) {
        return error;
    }
    const QByteArray temporary = QByteArrayLiteral("copy");
    const int tempFd = ::openat(staging.fd(), temporary.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (tempFd < 0) {
        error = systemError(destinationPath);
        staging.remove(error);
        return error;
    }
    FileDescriptor output(tempFd);
    const QString tempPath = staging.path(temporary);
    QByteArray buffer(128 * 1024, Qt::Uninitialized);
    while (true) {
        if (canceled && canceled()) {
            error = i18nc("@info", "Synchronization canceled.");
            break;
        }
        ssize_t size;
        do {
            size = ::read(input.get(), buffer.data(), buffer.size());
        } while (size < 0 && errno == EINTR);
        if (size < 0) {
            error = systemError(sourcePath);
            break;
        }
        if (size == 0) {
            break;
        }
        ssize_t offset = 0;
        while (offset < size) {
            ssize_t written;
            do {
                written = ::write(output.get(), buffer.constData() + offset, size - offset);
            } while (written < 0 && errno == EINTR);
            if (written <= 0) {
                error = i18nc("@info", "Could not write temporary file for %1: %2", destinationPath, QString::fromLocal8Bit(std::strerror(errno)));
                break;
            }
            offset += written;
        }
        if (!error.isEmpty()) {
            break;
        }
    }
    if (error.isEmpty() && (!sameFile(input.get(), source) || !currentEntry(sourceDirectory, name, sourcePath, source, error))) {
        if (error.isEmpty()) {
            error = i18nc("@info", "The source changed during copying: %1", sourcePath);
        }
    }
    const timespec times[2] = {inputStat.st_atim, inputStat.st_mtim};
    if (error.isEmpty() && (::fchmod(output.get(), inputStat.st_mode & 0777) != 0 || ::futimens(output.get(), times) != 0 || ::fsync(output.get()) != 0)) {
        error = i18nc("@info", "Could not finish the temporary file for %1: %2", destinationPath, QString::fromLocal8Bit(std::strerror(errno)));
    }
    if (error.isEmpty() && !sameDescriptorAt(staging.fd(), temporary, output.get())) {
        error = i18nc("@info", "The temporary file changed before installation: %1", tempPath);
    }
    if (error.isEmpty() && canceled && canceled()) {
        error = i18nc("@info", "Synchronization canceled.");
    }
    if (error.isEmpty()) {
        error = validateParents();
    }
    if (!error.isEmpty()) {
        removeTemporary(staging.fd(), temporary, output.get(), tempPath, error);
        staging.remove(error);
        return error;
    }
    const QByteArray backup = QByteArrayLiteral("original");
    const bool hasBackup = destination.kind != Kind::Missing;
    const QString backupPath = staging.path(backup);
    if (hasBackup && !moveToBackup(destinationDirectory, staging.fd(), name, destinationPath, backupPath, destination, error)) {
        removeTemporary(staging.fd(), temporary, output.get(), tempPath, error);
        staging.remove(error);
        return error;
    }
    const bool temporaryUnchanged = sameDescriptorAt(staging.fd(), temporary, output.get());
    if (!temporaryUnchanged || ::renameat2(staging.fd(), temporary.constData(), destinationDirectory, name.constData(), RENAME_NOREPLACE) != 0) {
        error = temporaryUnchanged ? i18nc("@info", "Could not install %1: %2", destinationPath, QString::fromLocal8Bit(std::strerror(errno)))
                                   : i18nc("@info", "The temporary file changed before installation: %1", tempPath);
        if (hasBackup) {
            const QString restore = restoreBackup(destinationDirectory, staging.fd(), backup, name, destinationPath, backupPath);
            if (!restore.isEmpty()) {
                error += QLatin1Char(' ') + restore;
            }
        }
        removeTemporary(staging.fd(), temporary, output.get(), tempPath, error);
        staging.remove(error);
        return error;
    }
    if (!sameDescriptorAt(destinationDirectory, name, output.get())) {
        error = hasBackup ? i18nc("@info", "The installed file changed unexpectedly; inspect the backup at %1.", backupPath)
                          : i18nc("@info", "The installed file changed unexpectedly: %1", destinationPath);
        staging.remove(error);
        return error;
    }
    if (hasBackup) {
        Entry displaced;
        if (!snapshotAt(staging.fd(), backup, backupPath, displaced, error) || !movedSnapshot(displaced, destination)
            || ::unlinkat(staging.fd(), backup.constData(), 0) != 0) {
            error = i18nc("@info", "The new file was installed, but the original or another item remains at %1.", backupPath);
        }
    }
    staging.remove(error);
    return error;
}

QString deleteFile(int directory, const QByteArray &name, const QString &path, const Entry &expected)
{
    QString error;
    StagingDirectory staging(directory, path);
    if (!staging.create(error)) {
        return error;
    }
    const QByteArray backup = QByteArrayLiteral("original");
    const QString backupPath = staging.path(backup);
    if (!moveToBackup(directory, staging.fd(), name, path, backupPath, expected, error)) {
        staging.remove(error);
        return error;
    }
    Entry displaced;
    if (!snapshotAt(staging.fd(), backup, backupPath, displaced, error) || !movedSnapshot(displaced, expected)) {
        error = i18nc("@info", "The item changed during deletion; inspect %1.", backupPath);
    } else if (::unlinkat(staging.fd(), backup.constData(), 0) != 0) {
        error = i18nc("@info", "Could not delete %1: %2", path, QString::fromLocal8Bit(std::strerror(errno)));
        const QString restore = restoreBackup(directory, staging.fd(), backup, name, path, backupPath);
        if (!restore.isEmpty()) {
            error += QLatin1Char(' ') + restore;
        }
    }
    staging.remove(error);
    return error;
}
}

FolderSync::ApplyResult
FolderSync::apply(const Comparison &comparison, const QList<Action> &actions, bool allowOverwrite, bool allowDelete, const std::function<bool()> &canceled)
{
    ApplyResult result;
    result.error = planError(comparison, actions, allowOverwrite, allowDelete);
    if (!result.error.isEmpty()) {
        return result;
    }
    FileDescriptor left(::open(QFile::encodeName(comparison.leftRoot).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    FileDescriptor right(::open(QFile::encodeName(comparison.rightRoot).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    if (!sameDirectory(left.get(), comparison.leftRootEntry) || !sameDirectory(right.get(), comparison.rightRootEntry)) {
        result.error = i18nc("@info", "A root folder changed since comparison.");
        return result;
    }
    QMap<QString, Entry> directories[2];
    directories[0].insert({}, comparison.leftRootEntry);
    directories[1].insert({}, comparison.rightRootEntry);
    QList<qsizetype> order;
    for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
        const Row &row = comparison.rows.at(i);
        if (row.left.kind == Kind::Directory) {
            directories[0].insert(row.relativePath, row.left);
        }
        if (row.right.kind == Kind::Directory) {
            directories[1].insert(row.relativePath, row.right);
        }
        if (actions.at(i) != Action::Skip) {
            order.append(i);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&comparison, &actions](qsizetype a, qsizetype b) {
        const auto directoryCopy = [&comparison, &actions](qsizetype i) {
            const auto action = actions.at(i);
            const auto &row = comparison.rows.at(i);
            return (action == Action::CopyToLeft && row.right.kind == Kind::Directory) || (action == Action::CopyToRight && row.left.kind == Kind::Directory);
        };
        if (directoryCopy(a) != directoryCopy(b)) {
            return directoryCopy(a);
        }
        return directoryCopy(a) && comparison.rows.at(a).relativePath.count(QLatin1Char('/')) < comparison.rows.at(b).relativePath.count(QLatin1Char('/'));
    });
    const int roots[] = {left.get(), right.get()};
    const QString rootPaths[] = {comparison.leftRoot, comparison.rightRoot};
    for (const qsizetype i : std::as_const(order)) {
        if (canceled && canceled()) {
            result.error = i18nc("@info", "Synchronization canceled. Listed operations have already completed.");
            break;
        }
        const Row &row = comparison.rows.at(i);
        const Action action = actions.at(i);
        const int targetSide = action == Action::CopyToLeft || action == Action::DeleteLeft ? 0 : 1;
        const int sourceSide = 1 - targetSide;
        const Entry &target = targetSide == 0 ? row.left : row.right;
        const Entry &source = sourceSide == 0 ? row.left : row.right;
        const QString parent = parentOf(row.relativePath);
        const QByteArray name = nameOf(row.relativePath);
        const QString targetPath = QDir(rootPaths[targetSide]).filePath(row.relativePath);
        FileDescriptor targetParent(checkedParent(roots[targetSide], parent, directories[targetSide], result.error));
        if (targetParent.get() < 0) {
            break;
        }
        const auto validateParent = [&](int side) -> QString {
            FileDescriptor currentRoot(::open(QFile::encodeName(rootPaths[side]).constData(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
            if (!sameDirectory(currentRoot.get(), directories[side].value(QString()))) {
                return i18nc("@info", "A root folder changed during synchronization: %1", rootPaths[side]);
            }
            QString error;
            FileDescriptor currentParent(checkedParent(currentRoot.get(), parent, directories[side], error));
            return error;
        };
        if (action == Action::DeleteLeft || action == Action::DeleteRight) {
            result.error = validateParent(targetSide);
            if (result.error.isEmpty()) {
                result.error = deleteFile(targetParent.get(), name, targetPath, target);
            }
        } else {
            const QString sourcePath = QDir(rootPaths[sourceSide]).filePath(row.relativePath);
            FileDescriptor sourceParent(checkedParent(roots[sourceSide], parent, directories[sourceSide], result.error));
            if (sourceParent.get() < 0) {
                break;
            }
            if (source.kind == Kind::Directory) {
                if (!currentEntry(sourceParent.get(), name, sourcePath, source, result.error)
                    || !currentEntry(targetParent.get(), name, targetPath, target, result.error)) {
                    break;
                }
                result.error = validateParent(sourceSide);
                if (result.error.isEmpty()) {
                    result.error = validateParent(targetSide);
                }
                if (!result.error.isEmpty()) {
                    break;
                }
                if (::mkdirat(targetParent.get(), name.constData(), 0777) != 0) {
                    result.error = systemError(targetPath);
                } else {
                    Entry created;
                    if (snapshotAt(targetParent.get(), name, targetPath, created, result.error)) {
                        directories[targetSide].insert(row.relativePath, created);
                    }
                }
            } else {
                const auto validateParents = [&] {
                    const QString error = validateParent(sourceSide);
                    return error.isEmpty() ? validateParent(targetSide) : error;
                };
                result.error = copyFile(sourceParent.get(), targetParent.get(), name, sourcePath, targetPath, source, target, validateParents, canceled);
            }
        }
        if (!result.error.isEmpty()) {
            result.error += QLatin1Char(' ') + i18nc("@info", "This or earlier operations may already have changed files.");
            break;
        }
        result.completed.append(row.relativePath);
    }
    return result;
}
