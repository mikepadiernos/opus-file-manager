// SPDX-License-Identifier: GPL-3.0-or-later

#include "rsyncsync.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace
{
using FolderSync::Action;
using FolderSync::Kind;

constexpr qsizetype MaxErrorBytes = 8192;
constexpr int KillDelayMs = 5000;

bool unsafeName(const QString &name)
{
    for (const QChar c : name) {
        if (c.isSpace() || c == QLatin1Char('@') || c == QLatin1Char('/') || c.unicode() < 0x20) {
            return true;
        }
    }
    return name.startsWith(QLatin1Char('-'));
}

bool validRelative(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('/')) || path.contains(QChar::Null)) {
        return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String("..")) {
            return false;
        }
    }
    return true;
}

QString parentOf(const QString &path)
{
    const qsizetype separator = path.lastIndexOf(QLatin1Char('/'));
    return separator < 0 ? QString() : path.left(separator);
}

QString decodeName(const QString &text)
{
    static const QRegularExpression escape(QStringLiteral("\\\\#([0-7]{3})"));
    QString result;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = escape.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += QStringView(text).mid(last, match.capturedStart() - last);
        result += QChar(match.captured(1).toUShort(nullptr, 8));
        last = match.capturedEnd();
    }
    result += QStringView(text).mid(last);
    return result;
}

Kind kindOf(QChar type)
{
    switch (type.unicode()) {
    case 'f':
        return Kind::File;
    case 'd':
        return Kind::Directory;
    case 'L':
        return Kind::Symlink;
    default:
        return Kind::Other;
    }
}

QStringList commonArguments(const RsyncSync::Endpoint &left, const RsyncSync::Endpoint &right, bool batchMode)
{
    // -s keeps remote paths with spaces intact; -8 keeps non-ASCII names readable.
    QStringList arguments{QStringLiteral("-s"), QStringLiteral("-8"), QStringLiteral("-t")};
    if (left.isRemote() || right.isRemote()) {
        arguments << QStringLiteral("-e") << RsyncSync::remoteShell(left.isRemote() ? left : right, batchMode);
    }
    return arguments;
}

QByteArray nulList(const QStringList &paths)
{
    QByteArray result;
    for (const QString &path : paths) {
        result += QFile::encodeName(path);
        result += '\0';
    }
    return result;
}
}

namespace RsyncSync
{
QString program()
{
    return QStandardPaths::findExecutable(QStringLiteral("rsync"));
}

std::optional<Endpoint> endpoint(const QUrl &url, QString *error)
{
    const auto failed = [error](const QString &message) -> std::optional<Endpoint> {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };
    Endpoint result;
    if (url.isLocalFile()) {
        result.path = QDir::cleanPath(url.toLocalFile());
        if (!QDir::isAbsolutePath(result.path)) {
            return failed(i18nc("@info", "The folder %1 is not an absolute path.", url.toLocalFile()));
        }
        return result;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("sftp") && scheme != QLatin1String("fish")) {
        return failed(i18nc("@info", "rsync works with local folders and SSH locations only, not %1.", url.toDisplayString()));
    }
    result.host = url.host();
    result.user = url.userName();
    result.port = url.port(-1);
    result.path = QDir::cleanPath(url.path().isEmpty() ? QStringLiteral("/") : url.path());
    const bool ipv6 = result.host.contains(QLatin1Char(':'));
    if (result.host.isEmpty() || (!ipv6 && unsafeName(result.host)) || (ipv6 && result.host.startsWith(QLatin1Char('-')))) {
        return failed(i18nc("@info", "The SSH host name of %1 is not supported.", url.toDisplayString()));
    }
    if (!result.user.isEmpty() && (unsafeName(result.user) || result.user.contains(QLatin1Char(':')))) {
        return failed(i18nc("@info", "The SSH user name of %1 is not supported.", url.toDisplayString()));
    }
    if (!QDir::isAbsolutePath(result.path)) {
        return failed(i18nc("@info", "The folder %1 is not an absolute path.", url.toDisplayString()));
    }
    return result;
}

QString displayName(const Endpoint &endpoint)
{
    if (!endpoint.isRemote()) {
        return endpoint.path;
    }
    QString host = endpoint.host.contains(QLatin1Char(':')) ? QLatin1Char('[') + endpoint.host + QLatin1Char(']') : endpoint.host;
    if (endpoint.port > 0) {
        host += QLatin1Char(':') + QString::number(endpoint.port);
    }
    return (endpoint.user.isEmpty() ? QString() : endpoint.user + QLatin1Char('@')) + host + QLatin1Char(':') + endpoint.path;
}

QString location(const Endpoint &endpoint)
{
    QString path = endpoint.path;
    if (!path.endsWith(QLatin1Char('/'))) {
        path += QLatin1Char('/');
    }
    if (!endpoint.isRemote()) {
        return path;
    }
    const QString host = endpoint.host.contains(QLatin1Char(':')) ? QLatin1Char('[') + endpoint.host + QLatin1Char(']') : endpoint.host;
    return (endpoint.user.isEmpty() ? QString() : endpoint.user + QLatin1Char('@')) + host + QLatin1Char(':') + path;
}

QUrl url(const Endpoint &endpoint)
{
    if (!endpoint.isRemote()) {
        return QUrl::fromLocalFile(endpoint.path);
    }
    QUrl result;
    result.setScheme(QStringLiteral("sftp"));
    result.setHost(endpoint.host);
    if (!endpoint.user.isEmpty()) {
        result.setUserName(endpoint.user);
    }
    result.setPort(endpoint.port);
    result.setPath(endpoint.path);
    return result;
}

QString endpointsError(const Endpoint &left, const Endpoint &right)
{
    if (left.isRemote() && right.isRemote()) {
        return i18nc("@info", "rsync cannot synchronize two SSH locations directly. Synchronize one of them with a local folder.");
    }
    if (left.path == QLatin1String("/") || right.path == QLatin1String("/")) {
        return i18nc("@info", "Choose two separate folders that do not contain one another.");
    }
    if (left.isRemote() != right.isRemote()) {
        return {};
    }
    if (left.path == right.path || left.path.startsWith(right.path + QLatin1Char('/')) || right.path.startsWith(left.path + QLatin1Char('/'))) {
        return i18nc("@info", "Choose two separate folders that do not contain one another.");
    }
    return {};
}

QStringList filterRules(const FolderSync::Options &options, QString *error)
{
    static const QRegularExpression stars(QStringLiteral("\\*+"));
    QStringList excludes;
    QStringList includes;
    const auto translate = [error](const QStringList &patterns, const QString &prefix, QStringList &rules) {
        for (const QString &pattern : patterns) {
            if (pattern.isEmpty() || pattern.contains(QLatin1Char('?')) || pattern.contains(QLatin1Char('\\')) || pattern.startsWith(QLatin1Char('/'))
                || pattern.endsWith(QLatin1Char('/')) || pattern.contains(QLatin1Char('\n')) || pattern.contains(QLatin1Char('\r'))) {
                if (error) {
                    *error = i18nc("@info", "rsync cannot apply the filter pattern “%1” the same way. Remove “?”, “\\” and leading or trailing “/”.", pattern);
                }
                return false;
            }
            QString rule = pattern;
            // Folder synchronization lets "*" match "/", as rsync's "**" does.
            rule.replace(stars, QStringLiteral("**"));
            rules.append(prefix + QLatin1Char('/') + rule);
        }
        return true;
    };
    if (!translate(options.excludePatterns, QStringLiteral("- "), excludes) || !translate(options.includePatterns, QStringLiteral("+ "), includes)) {
        return {};
    }
    QStringList rules = excludes;
    if (!includes.isEmpty()) {
        // Folders are always compared; include patterns select the other items.
        rules.append(QStringLiteral("+ */"));
        rules.append(includes);
        rules.append(QStringLiteral("- *"));
    }
    return rules;
}

QString remoteShell(const Endpoint &endpoint, bool batchMode)
{
    // OpenSSH verifies host keys; this only adds a timeout and, without a
    // password helper, makes it fail instead of waiting for input.
    QStringList shell{QStringLiteral("ssh"), QStringLiteral("-o"), QStringLiteral("ConnectTimeout=30")};
    if (batchMode) {
        shell << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    }
    if (endpoint.port > 0) {
        shell << QStringLiteral("-p") << QString::number(endpoint.port);
    }
    return shell.join(QLatin1Char(' '));
}

QProcessEnvironment environment(const QString &askPass)
{
    QProcessEnvironment result = QProcessEnvironment::systemEnvironment();
    if (!askPass.isEmpty()) {
        result.insert(QStringLiteral("SSH_ASKPASS"), askPass);
        result.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    }
    return result;
}

Step listStep(const Endpoint &source, const Endpoint &destination, const FolderSync::Options &options, bool batchMode)
{
    Step step;
    step.arguments = commonArguments(source, destination, batchMode);
    step.arguments << QStringLiteral("-n") << QStringLiteral("-ii") << QStringLiteral("--out-format=%i %l %M %n");
    step.arguments << (options.recursive ? QStringLiteral("-r") : QStringLiteral("-d"));
    if (options.compareSymlinks) {
        step.arguments << QStringLiteral("-l");
    }
    if (options.compareContents) {
        step.arguments << QStringLiteral("--checksum");
    }
    const QStringList rules = filterRules(options);
    for (const QString &rule : rules) {
        step.arguments << QStringLiteral("--filter=") + rule;
    }
    step.arguments << QStringLiteral("--") << location(source) << location(destination);
    return step;
}

std::optional<Item> parseItem(const QString &line, bool withDetails)
{
    static const QRegularExpression detailed(QStringLiteral("^(.{11}) (\\d+) (\\d{4}/\\d\\d/\\d\\d-\\d\\d:\\d\\d:\\d\\d) (.+)$"));
    static const QRegularExpression plain(QStringLiteral("^(.{11}) (.+)$"));
    static const QRegularExpression changes(QStringLiteral("^(\\*deleting  |[<>ch.][fdLDS][^ ]{9}|[<>ch.][fdLDS] {9})$"));
    QRegularExpressionMatch match = withDetails ? detailed.match(line) : QRegularExpressionMatch();
    if (!match.hasMatch()) {
        match = plain.match(line);
    }
    if (!match.hasMatch() || !changes.match(match.captured(1)).hasMatch()) {
        return std::nullopt;
    }
    Item item;
    item.changes = match.captured(1);
    QString name = match.lastCapturedIndex() == 4 ? match.captured(4) : match.captured(2);
    if (match.lastCapturedIndex() == 4) {
        item.size = match.captured(2).toLongLong();
        item.modifiedSeconds = QDateTime::fromString(match.captured(3), QStringLiteral("yyyy/MM/dd-HH:mm:ss")).toSecsSinceEpoch();
    }
    const bool folder = name.endsWith(QLatin1Char('/'));
    if (folder) {
        name.chop(1);
    }
    item.path = decodeName(name);
    if (item.path.isEmpty() || item.path == QLatin1String(".")) {
        return std::nullopt;
    }
    item.kind = item.isDeletion() ? (folder ? Kind::Directory : Kind::File) : kindOf(item.changes.at(1));
    return item;
}

FolderSync::Comparison
comparison(const Endpoint &left, const Endpoint &right, const QStringList &leftToRight, const QStringList &rightToLeft, const FolderSync::Options &options)
{
    FolderSync::Comparison result;
    result.leftRoot = displayName(left);
    result.rightRoot = displayName(right);
    result.leftRootEntry.kind = Kind::Directory;
    result.rightRootEntry.kind = Kind::Directory;
    // Remote entries have no device or inode; give each side its own device so
    // that FolderSync::copyAllowed never mistakes two entries for one file.
    result.leftRootEntry.device = 1;
    result.rightRootEntry.device = 2;
    QMap<QString, Item> leftItems;
    QMap<QString, Item> rightItems;
    for (const QString &line : leftToRight) {
        if (const auto item = parseItem(line, true); item && !item->isDeletion()) {
            leftItems.insert(item->path, *item);
        }
    }
    for (const QString &line : rightToLeft) {
        if (const auto item = parseItem(line, true); item && !item->isDeletion()) {
            rightItems.insert(item->path, *item);
        }
    }
    QStringList paths = leftItems.keys() + rightItems.keys();
    paths.removeDuplicates();
    std::sort(paths.begin(), paths.end());
    quint64 inode = 0;
    const auto entry = [&inode](const Item *item, quint64 device) {
        FolderSync::Entry result;
        if (!item) {
            return result;
        }
        result.kind = item->kind;
        result.device = device;
        result.inode = ++inode;
        result.size = item->size;
        result.modifiedSeconds = item->modifiedSeconds;
        return result;
    };
    for (const QString &path : std::as_const(paths)) {
        const auto leftIt = leftItems.constFind(path);
        const auto rightIt = rightItems.constFind(path);
        const Item *leftItem = leftIt == leftItems.cend() ? nullptr : &leftIt.value();
        const Item *rightItem = rightIt == rightItems.cend() ? nullptr : &rightIt.value();
        FolderSync::Row row;
        row.relativePath = path;
        row.left = entry(leftItem, 1);
        row.right = entry(rightItem, 2);
        if (!rightItem) {
            row.difference = FolderSync::Difference::LeftOnly;
        } else if (!leftItem) {
            row.difference = FolderSync::Difference::RightOnly;
        } else if (row.left.kind != row.right.kind) {
            row.difference = FolderSync::Difference::Type;
        } else if (row.left.kind == Kind::Directory) {
            row.difference = FolderSync::Difference::Directory;
        } else if (row.left.kind == Kind::Symlink) {
            row.difference = leftItem->changes.at(0) == QLatin1Char('.') ? FolderSync::Difference::LinkEqual : FolderSync::Difference::LinkDifferent;
        } else if (row.left.kind != Kind::File) {
            row.difference = FolderSync::Difference::Unsupported;
        } else if (options.compareContents) {
            const bool same = leftItem->changes.at(0) == QLatin1Char('.') && leftItem->changes.at(2) != QLatin1Char('c');
            row.difference = same ? FolderSync::Difference::ContentEqual : FolderSync::Difference::ContentDifferent;
        } else if (leftItem->changes.at(3) == QLatin1Char('s')) {
            row.difference = FolderSync::Difference::Size;
        } else if (leftItem->changes.at(4) == QLatin1Char('t') || leftItem->changes.at(4) == QLatin1Char('T')) {
            row.difference = FolderSync::Difference::Time;
        } else {
            row.difference = FolderSync::Difference::MetadataEqual;
        }
        result.rows.append(row);
    }
    return result;
}

QString planError(const FolderSync::Comparison &comparison, const QList<Action> &actions, Transfer *transfer)
{
    if (!comparison.error.isEmpty() || comparison.rows.size() != actions.size()) {
        return i18nc("@info", "The synchronization preview is invalid.");
    }
    std::optional<Direction> direction;
    for (const Action action : actions) {
        if (action == Action::Skip) {
            continue;
        }
        const Direction actionDirection = action == Action::CopyToRight || action == Action::DeleteRight ? Direction::ToRight : Direction::ToLeft;
        if (direction && *direction != actionDirection) {
            return i18nc("@info",
                         "rsync copies and deletes in one direction at a time, but this plan works in both. "
                         "Plan one direction, or synchronize with Plasma Opus instead.");
        }
        direction = actionDirection;
    }
    if (!direction) {
        return i18nc("@info", "Choose at least one operation.");
    }
    Transfer result;
    result.direction = *direction;
    QSet<QString> destinationFolders{QString()};
    QSet<QString> paths;
    for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
        const FolderSync::Row &row = comparison.rows.at(i);
        if (!validRelative(row.relativePath) || paths.contains(row.relativePath)) {
            return i18nc("@info", "The synchronization preview contains an invalid or duplicate path.");
        }
        paths.insert(row.relativePath);
        const FolderSync::Entry &destination = *direction == Direction::ToRight ? row.right : row.left;
        if (destination.kind == Kind::Directory) {
            destinationFolders.insert(row.relativePath);
        }
    }
    for (qsizetype i = 0; i < comparison.rows.size(); ++i) {
        const FolderSync::Row &row = comparison.rows.at(i);
        const Action action = actions.at(i);
        if (action == Action::Skip) {
            continue;
        }
        const FolderSync::Entry &source = *direction == Direction::ToRight ? row.left : row.right;
        const FolderSync::Entry &destination = *direction == Direction::ToRight ? row.right : row.left;
        if (action == Action::DeleteLeft || action == Action::DeleteRight) {
            if (destination.kind != Kind::File || source.kind != Kind::Missing) {
                return i18nc("@info", "rsync can delete only files that are missing from the other folder: %1", row.relativePath);
            }
            result.deletions.append(row.relativePath);
            continue;
        }
        if (source.kind == Kind::Directory && destination.kind == Kind::Missing) {
            destinationFolders.insert(row.relativePath);
        } else if (source.kind != Kind::File || (destination.kind != Kind::Missing && destination.kind != Kind::File)) {
            return i18nc("@info", "This item cannot be copied by synchronization: %1", row.relativePath);
        }
        result.copies.append(row.relativePath);
    }
    const QStringList planned = result.copies + result.deletions;
    for (const QString &path : planned) {
        if (!destinationFolders.contains(parentOf(path))) {
            return i18nc("@info", "A required parent folder is missing from the plan: %1", path);
        }
    }
    if (transfer) {
        *transfer = result;
    }
    return {};
}

QString deletionsError(const Transfer &transfer, bool allowDeletions)
{
    if (allowDeletions || transfer.deletions.isEmpty()) {
        return {};
    }
    return i18ncp("@info",
                  "The plan deletes one file, but deleting files with rsync is turned off in Preferences › Remote and Cloud › Sync. "
                  "Choose Skip for it, or allow deletions there.",
                  "The plan deletes %1 files, but deleting files with rsync is turned off in Preferences › Remote and Cloud › Sync. "
                  "Choose Skip for them, or allow deletions there.",
                  transfer.deletions.size());
}

QList<Step> transferSteps(const Endpoint &left,
                          const Endpoint &right,
                          const Transfer &transfer,
                          FolderSync::CopyPolicy policy,
                          quint64 speedLimitKiB,
                          bool dryRun,
                          bool batchMode)
{
    const Endpoint &source = transfer.direction == Direction::ToRight ? left : right;
    const Endpoint &destination = transfer.direction == Direction::ToRight ? right : left;
    QStringList base = commonArguments(left, right, batchMode);
    base << QStringLiteral("--out-format=%i %n") << QStringLiteral("--from0") << QStringLiteral("--files-from=-");
    if (dryRun) {
        base << QStringLiteral("-n");
    }
    const QStringList locations{QStringLiteral("--"), location(source), location(destination)};
    QList<Step> steps;
    if (!transfer.copies.isEmpty()) {
        Step copy;
        // The listed files are planned explicitly, so copy them even when rsync's quick check finds no change.
        copy.arguments = base;
        copy.arguments << QStringLiteral("--ignore-times");
        if (policy == FolderSync::CopyPolicy::SkipExisting) {
            copy.arguments << QStringLiteral("--ignore-existing");
        } else if (policy == FolderSync::CopyPolicy::NewerOnly) {
            copy.arguments << QStringLiteral("--update");
        }
        if (speedLimitKiB > 0) {
            copy.arguments << QStringLiteral("--bwlimit=%1").arg(speedLimitKiB);
        }
        if (!dryRun) {
            copy.arguments << QStringLiteral("--info=progress2");
        }
        copy.arguments << locations;
        copy.input = nulList(transfer.copies);
        steps.append(copy);
    }
    if (!transfer.deletions.isEmpty()) {
        Step remove;
        // Delete listed files missing from the source, and never copy one that has reappeared.
        remove.arguments = base;
        remove.arguments << QStringLiteral("--delete-missing-args") << QStringLiteral("--no-implied-dirs") << QStringLiteral("--existing")
                         << QStringLiteral("--ignore-existing") << locations;
        remove.input = nulList(transfer.deletions);
        // Folders that are missing from the source are reported as vanished.
        remove.successCodes = {0, 24};
        steps.append(remove);
    }
    return steps;
}

Summary summarize(const Transfer &transfer, const QStringList &itemizedLines)
{
    Summary summary;
    QSet<QString> seen;
    for (const QString &line : itemizedLines) {
        const auto item = parseItem(line, false);
        if (!item) {
            continue;
        }
        seen.insert(item->path);
        if (item->isDeletion()) {
            ++summary.deleted;
        } else if (item->kind == Kind::Directory) {
            if (item->isNew()) {
                ++summary.folders;
            }
        } else if (item->isNew()) {
            ++summary.created;
        } else if (item->changes.at(0) == QLatin1Char('<') || item->changes.at(0) == QLatin1Char('>')) {
            ++summary.overwritten;
        }
    }
    const QStringList planned = transfer.copies + transfer.deletions;
    for (const QString &path : planned) {
        if (!seen.contains(path)) {
            summary.missing.append(path);
        }
    }
    return summary;
}

int progressPercent(const QString &text)
{
    static const QRegularExpression percent(QStringLiteral("^\\s*[\\d,.]+[A-Za-z]*\\s+(\\d{1,3})%\\s"));
    const QRegularExpressionMatch match = percent.match(text);
    return match.hasMatch() ? std::min(100, match.captured(1).toInt()) : -1;
}

QString exitMessage(int exitCode)
{
    switch (exitCode) {
    case 0:
        return {};
    case 1:
    case 2:
    case 4:
        return i18nc("@info", "This version of rsync does not support the options Plasma Opus needs. Install rsync 3.1 or later.");
    case 5:
    case 10:
    case 12:
    case 255:
        return i18nc("@info", "rsync could not connect. Check that the host is reachable, that you can sign in with SSH and that rsync is installed there.");
    case 3:
        return i18nc("@info", "rsync could not open a folder.");
    case 11:
        return i18nc("@info", "rsync could not read or write a file.");
    case 20:
        return i18nc("@info", "rsync was stopped.");
    case 23:
        return i18nc("@info", "Some items were not transferred because of errors.");
    case 24:
        return i18nc("@info", "Some files disappeared before they could be transferred.");
    case 30:
    case 35:
        return i18nc("@info", "rsync timed out.");
    default:
        return i18nc("@info", "rsync failed with exit code %1.", exitCode);
    }
}

Process::Process(const QString &program, const QList<Step> &steps, const QProcessEnvironment &environment, QObject *parent)
    : QObject(parent)
    , m_program(program)
    , m_steps(steps)
{
    m_process.setProcessEnvironment(environment);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(KillDelayMs);
    connect(&m_killTimer, &QTimer::timeout, &m_process, &QProcess::kill);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &Process::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_errors += m_process.readAllStandardError();
        if (m_errors.size() > MaxErrorBytes) {
            m_errors = m_errors.right(MaxErrorBytes);
        }
    });
    connect(&m_process, &QProcess::finished, this, &Process::stepFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail(i18nc("@info", "rsync could not be started: %1", m_process.errorString()));
        }
    });
}

Process::~Process()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.disconnect(this);
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void Process::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_succeeded = false;
    m_canceled = false;
    m_error.clear();
    m_lines = QList<QStringList>(m_steps.size());
    m_step = -1;
    if (m_program.isEmpty()) {
        QTimer::singleShot(0, this, [this] {
            fail(i18nc("@info", "rsync is not installed. Install it from your distribution to use the rsync backend."));
        });
        return;
    }
    startStep();
}

void Process::cancel()
{
    if (!m_running || m_canceled) {
        return;
    }
    m_canceled = true;
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        m_killTimer.start();
    }
}

bool Process::isRunning() const
{
    return m_running;
}

bool Process::wasCanceled() const
{
    return m_canceled;
}

bool Process::succeeded() const
{
    return m_succeeded;
}

QStringList Process::lines(int step) const
{
    return m_lines.value(step);
}

QStringList Process::allLines() const
{
    QStringList result;
    for (const QStringList &lines : m_lines) {
        result += lines;
    }
    return result;
}

QString Process::errorText() const
{
    return m_error;
}

void Process::startStep()
{
    ++m_step;
    if (m_step >= m_steps.size()) {
        m_running = false;
        m_succeeded = true;
        Q_EMIT finished();
        return;
    }
    m_buffer.clear();
    m_errors.clear();
    m_process.start(m_program, m_steps.at(m_step).arguments);
    m_process.write(m_steps.at(m_step).input);
    m_process.closeWriteChannel();
}

void Process::readOutput()
{
    m_buffer += m_process.readAllStandardOutput();
    qsizetype start = 0;
    for (qsizetype i = 0; i < m_buffer.size(); ++i) {
        const char c = m_buffer.at(i);
        if (c != '\n' && c != '\r') {
            continue;
        }
        const QString text = QFile::decodeName(m_buffer.mid(start, i - start));
        start = i + 1;
        if (text.trimmed().isEmpty()) {
            continue;
        }
        const int percent = progressPercent(text);
        if (percent >= 0) {
            Q_EMIT progressChanged(percent);
        } else if (m_step >= 0 && m_step < m_lines.size()) {
            m_lines[m_step].append(text);
        }
    }
    m_buffer.remove(0, start);
}

void Process::stepFinished(int exitCode, QProcess::ExitStatus status)
{
    m_killTimer.stop();
    m_buffer += '\n';
    readOutput();
    if (m_canceled) {
        fail(exitMessage(20));
        return;
    }
    if (status != QProcess::NormalExit || !m_steps.at(m_step).successCodes.contains(exitCode)) {
        const QString details = QString::fromLocal8Bit(m_errors).trimmed();
        const QString message = status == QProcess::NormalExit ? exitMessage(exitCode) : i18nc("@info", "rsync stopped unexpectedly.");
        fail(details.isEmpty() ? message : message + QLatin1Char('\n') + details);
        return;
    }
    startStep();
}

void Process::fail(const QString &error)
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_succeeded = false;
    m_error = error;
    Q_EMIT finished();
}
}
