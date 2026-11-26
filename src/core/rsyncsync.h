// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <optional>

/// The optional rsync backend for folder synchronization. It compares SSH
/// locations through rsync's itemized dry runs, turns a folder
/// synchronization plan into rsync runs that copy or delete exactly the
/// planned items, and rejects plans rsync cannot carry out as planned.
///
/// rsync runs in one direction, so a plan must copy and delete in one
/// direction only, and it can delete only items missing from the source. SSH
/// connections keep OpenSSH's host-key verification; Plasma Opus never turns
/// it off and never passes passwords on the command line.
namespace RsyncSync
{
/// A local folder or a folder on an SSH host.
struct Endpoint {
    /// An absolute, clean path on the local computer or on the host.
    QString path;
    /// The SSH host, or empty for a local folder.
    QString host;
    QString user;
    /// The SSH port, or -1 for the default.
    int port = -1;

    bool isRemote() const
    {
        return !host.isEmpty();
    }
    bool operator==(const Endpoint &other) const = default;
};

/// The direction rsync copies in: from the first folder to the second, or back.
enum class Direction {
    ToRight,
    ToLeft,
};

/// The items of a plan that one rsync direction copies or deletes.
struct Transfer {
    Direction direction = Direction::ToRight;
    /// Relative paths of files to copy and folders to create in the destination.
    QStringList copies;
    /// Relative paths of destination files to delete.
    QStringList deletions;

    bool operator==(const Transfer &other) const = default;
};

/// An rsync run: its arguments, the data written to its standard input and
/// the exit codes that count as success.
struct Step {
    QStringList arguments;
    QByteArray input;
    QList<int> successCodes{0};
};

/// The rsync program, or an empty string when rsync is not installed.
QString program();

/// The endpoint for a pane location: a local folder, or an SFTP or fish
/// location on an SSH host. Returns nothing and sets @p error for other
/// locations and for host or user names rsync could mistake for options.
std::optional<Endpoint> endpoint(const QUrl &url, QString *error = nullptr);
/// The location as shown to the user, such as "user@host:/path".
QString displayName(const Endpoint &endpoint);
/// The location as passed to rsync, ending in "/" so that rsync works on the
/// folder's contents.
QString location(const Endpoint &endpoint);
/// The KIO URL of @p endpoint.
QUrl url(const Endpoint &endpoint);
/// An error when the two folders are the same or one contains the other.
QString endpointsError(const Endpoint &left, const Endpoint &right);

/// The rsync filter rules for the folder synchronization filters. Patterns
/// match whole relative paths, and "*" also matches "/", as in folder
/// synchronization. Sets @p error and returns an empty list for patterns
/// rsync cannot express the same way, such as those with "?" or "\".
QStringList filterRules(const FolderSync::Options &options, QString *error = nullptr);

/// The remote shell for SSH endpoints. Batch mode makes OpenSSH fail
/// instead of asking for a password or an unknown host key.
QString remoteShell(const Endpoint &endpoint, bool batchMode);
/// The environment for rsync: KDE's ksshaskpass asks for SSH passwords and
/// new host keys when @p askPass is not empty.
QProcessEnvironment environment(const QString &askPass);

/// A dry run that lists every item of @p source and how it compares with
/// @p destination.
Step listStep(const Endpoint &source, const Endpoint &destination, const FolderSync::Options &options, bool batchMode);

/// One line of rsync's itemized output.
struct Item {
    /// The 11-character change summary, such as ">f.st......" or "*deleting  ".
    QString changes;
    QString path;
    FolderSync::Kind kind = FolderSync::Kind::Other;
    qint64 size = -1;
    qint64 modifiedSeconds = 0;

    bool isDeletion() const
    {
        return changes.startsWith(QLatin1Char('*'));
    }
    bool isNew() const
    {
        return changes.mid(2).startsWith(QLatin1String("+++"));
    }
};

/// Parses a line of itemized output in the format "%i %n", or "%i %l %M %n"
/// when @p withDetails is set. Escaped characters in names are decoded, and
/// a trailing "/" of folders is removed. Returns nothing for other lines and
/// for the transfer root.
std::optional<Item> parseItem(const QString &line, bool withDetails);

/// Builds a folder comparison from the dry-run listings of @p leftToRight and
/// @p rightToLeft. Entries carry the sizes and modification times rsync
/// reports, and rows are sorted by path.
FolderSync::Comparison
comparison(const Endpoint &left, const Endpoint &right, const QStringList &leftToRight, const QStringList &rightToLeft, const FolderSync::Options &options);

/// Checks that rsync can carry out @p actions exactly as planned and, when it
/// can, sets @p transfer. Returns a localized reason when it cannot.
QString planError(const FolderSync::Comparison &comparison, const QList<FolderSync::Action> &actions, Transfer *transfer);

/// The runs that carry out @p transfer: copies, then deletions. A dry run
/// itemizes what rsync would change without changing anything.
QList<Step> transferSteps(const Endpoint &left,
                          const Endpoint &right,
                          const Transfer &transfer,
                          FolderSync::CopyPolicy policy,
                          quint64 speedLimitKiB,
                          bool dryRun,
                          bool batchMode);

/// What an itemized run changed or would change.
struct Summary {
    int created = 0;
    int folders = 0;
    int overwritten = 0;
    int deleted = 0;
    /// Planned paths rsync did not itemize, such as files it skipped.
    QStringList missing;
};
Summary summarize(const Transfer &transfer, const QStringList &itemizedLines);

/// The overall percentage in a progress line of --info=progress2, or -1.
int progressPercent(const QString &text);
/// A localized description of an rsync exit code.
QString exitMessage(int exitCode);

/// Runs rsync steps in order, stopping at the first failure, and reports
/// their itemized output and progress.
class Process : public QObject
{
    Q_OBJECT

public:
    Process(const QString &program, const QList<Step> &steps, const QProcessEnvironment &environment, QObject *parent = nullptr);
    ~Process() override;

    void start();
    /// Stops rsync. Files it was writing are left as rsync leaves them.
    void cancel();
    bool isRunning() const;
    bool wasCanceled() const;
    /// Whether every step finished with a success code.
    bool succeeded() const;
    /// The itemized output lines of @p step.
    QStringList lines(int step) const;
    /// All itemized output lines.
    QStringList allLines() const;
    /// A localized error, including rsync's last messages, or an empty string.
    QString errorText() const;

Q_SIGNALS:
    void progressChanged(int percent);
    void finished();

private:
    void startStep();
    void readOutput();
    void stepFinished(int exitCode, QProcess::ExitStatus status);
    void fail(const QString &error);

    QString m_program;
    QList<Step> m_steps;
    QProcess m_process;
    QTimer m_killTimer;
    int m_step = -1;
    QByteArray m_buffer;
    QList<QStringList> m_lines;
    QByteArray m_errors;
    QString m_error;
    bool m_canceled = false;
    bool m_running = false;
    bool m_succeeded = false;
};
}
