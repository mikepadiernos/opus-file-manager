// SPDX-License-Identifier: GPL-3.0-or-later

#include "rsyncsync.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using FolderSync::Action;
using FolderSync::Difference;
using FolderSync::Kind;

namespace
{
RsyncSync::Endpoint local(const QString &path)
{
    RsyncSync::Endpoint result;
    result.path = path;
    return result;
}

RsyncSync::Endpoint remote(const QString &path)
{
    RsyncSync::Endpoint result;
    result.path = path;
    result.host = QStringLiteral("host");
    result.user = QStringLiteral("me");
    result.port = 2222;
    return result;
}

FolderSync::Row row(const QString &path, Kind left, Kind right)
{
    FolderSync::Row result;
    result.relativePath = path;
    result.left.kind = left;
    result.right.kind = right;
    return result;
}

bool writeFile(const QString &path, const QByteArray &data, const QDateTime &modified = {})
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
        return false;
    }
    if (modified.isValid() && !file.setFileTime(modified, QFileDevice::FileModificationTime)) {
        return false;
    }
    return true;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class RsyncSyncTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void parsesEndpoints_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<QString>("display");
        QTest::addColumn<QString>("location");
        QTest::newRow("local") << QUrl::fromLocalFile(QStringLiteral("/home/me/a/../b")) << true << "/home/me/b" << "/home/me/b/";
        QTest::newRow("sftp") << QUrl(QStringLiteral("sftp://me@host:2222/srv/data")) << true << "me@host:2222:/srv/data" << "me@host:/srv/data/";
        QTest::newRow("fish") << QUrl(QStringLiteral("fish://host/srv")) << true << "host:/srv" << "host:/srv/";
        QTest::newRow("no path") << QUrl(QStringLiteral("sftp://host")) << true << "host:/" << "host:/";
        QTest::newRow("ipv6") << QUrl(QStringLiteral("sftp://[::1]/x")) << true << "[::1]:/x" << "[::1]:/x/";
        QTest::newRow("smb") << QUrl(QStringLiteral("smb://host/share")) << false << "" << "";
        QTest::newRow("option host") << QUrl(QStringLiteral("sftp://-oProxyCommand=x/srv")) << false << "" << "";
        QTest::newRow("option user") << QUrl(QStringLiteral("sftp://-x@host/srv")) << false << "" << "";
        QTest::newRow("no host") << QUrl(QStringLiteral("sftp:///srv")) << false << "" << "";
    }

    void parsesEndpoints()
    {
        QFETCH(QUrl, url);
        QFETCH(bool, valid);
        QFETCH(QString, display);
        QFETCH(QString, location);
        QString error;
        const auto endpoint = RsyncSync::endpoint(url, &error);
        QCOMPARE(endpoint.has_value(), valid);
        QCOMPARE(error.isEmpty(), valid);
        if (endpoint) {
            QCOMPARE(RsyncSync::displayName(*endpoint), display);
            QCOMPARE(RsyncSync::location(*endpoint), location);
            QCOMPARE(RsyncSync::endpoint(RsyncSync::url(*endpoint)), endpoint);
        }
    }

    void checksEndpointPairs()
    {
        QVERIFY(RsyncSync::endpointsError(local(QStringLiteral("/a")), local(QStringLiteral("/b"))).isEmpty());
        QVERIFY(RsyncSync::endpointsError(local(QStringLiteral("/a")), remote(QStringLiteral("/a"))).isEmpty());
        QVERIFY(RsyncSync::endpointsError(local(QStringLiteral("/ab")), local(QStringLiteral("/a"))).isEmpty());
        QVERIFY(!RsyncSync::endpointsError(local(QStringLiteral("/a")), local(QStringLiteral("/a"))).isEmpty());
        QVERIFY(!RsyncSync::endpointsError(local(QStringLiteral("/a")), local(QStringLiteral("/a/b"))).isEmpty());
        QVERIFY(!RsyncSync::endpointsError(local(QStringLiteral("/a/b")), local(QStringLiteral("/a"))).isEmpty());
        QVERIFY(!RsyncSync::endpointsError(local(QStringLiteral("/")), remote(QStringLiteral("/a"))).isEmpty());
        QVERIFY(!RsyncSync::endpointsError(remote(QStringLiteral("/a")), remote(QStringLiteral("/b"))).isEmpty());
    }

    void translatesFilters()
    {
        FolderSync::Options options;
        QVERIFY(RsyncSync::filterRules(options).isEmpty());
        options.excludePatterns = {QStringLiteral("*.tmp"), QStringLiteral("build/**")};
        QCOMPARE(RsyncSync::filterRules(options), QStringList({QStringLiteral("- /**.tmp"), QStringLiteral("- /build/**")}));
        options.includePatterns = {QStringLiteral("docs/*.md")};
        QCOMPARE(
            RsyncSync::filterRules(options),
            QStringList(
                {QStringLiteral("- /**.tmp"), QStringLiteral("- /build/**"), QStringLiteral("+ */"), QStringLiteral("+ /docs/**.md"), QStringLiteral("- *")}));
        for (const QString &pattern : {QStringLiteral("a?"), QStringLiteral("a\\*"), QStringLiteral("/a"), QStringLiteral("a/")}) {
            options.includePatterns = {pattern};
            QString error;
            QVERIFY(RsyncSync::filterRules(options, &error).isEmpty());
            QVERIFY(!error.isEmpty());
        }
    }

    void buildsShellAndEnvironment()
    {
        QCOMPARE(RsyncSync::remoteShell(remote(QStringLiteral("/a")), true), QStringLiteral("ssh -o ConnectTimeout=30 -o BatchMode=yes -p 2222"));
        QCOMPARE(RsyncSync::remoteShell(local(QStringLiteral("/a")), false), QStringLiteral("ssh -o ConnectTimeout=30"));
        const QProcessEnvironment environment = RsyncSync::environment(QStringLiteral("/usr/bin/ksshaskpass"));
        QCOMPARE(environment.value(QStringLiteral("SSH_ASKPASS")), QStringLiteral("/usr/bin/ksshaskpass"));
        QCOMPARE(environment.value(QStringLiteral("SSH_ASKPASS_REQUIRE")), QStringLiteral("force"));
        QVERIFY(!RsyncSync::environment(QString()).contains(QStringLiteral("SSH_ASKPASS_REQUIRE")));
    }

    void buildsListSteps()
    {
        FolderSync::Options options;
        options.excludePatterns = {QStringLiteral("*.o")};
        RsyncSync::Step step = RsyncSync::listStep(local(QStringLiteral("/a")), local(QStringLiteral("/b")), options, false);
        QVERIFY(!step.arguments.contains(QStringLiteral("-e")));
        QVERIFY(step.arguments.contains(QStringLiteral("-n")));
        QVERIFY(step.arguments.contains(QStringLiteral("-r")));
        QVERIFY(!step.arguments.contains(QStringLiteral("-l")));
        QVERIFY(step.arguments.contains(QStringLiteral("--filter=- /**.o")));
        QCOMPARE(step.arguments.mid(step.arguments.size() - 3), QStringList({QStringLiteral("--"), QStringLiteral("/a/"), QStringLiteral("/b/")}));

        options.recursive = false;
        options.compareSymlinks = true;
        options.compareContents = true;
        step = RsyncSync::listStep(remote(QStringLiteral("/a")), local(QStringLiteral("/b")), options, true);
        const qsizetype shell = step.arguments.indexOf(QStringLiteral("-e"));
        QVERIFY(shell >= 0);
        QVERIFY(step.arguments.at(shell + 1).contains(QStringLiteral("BatchMode=yes")));
        QVERIFY(step.arguments.contains(QStringLiteral("-d")));
        QVERIFY(step.arguments.contains(QStringLiteral("-l")));
        QVERIFY(step.arguments.contains(QStringLiteral("--checksum")));
        QCOMPARE(step.arguments.last(2), QStringList({QStringLiteral("me@host:/a/"), QStringLiteral("/b/")}));
    }

    void parsesItems()
    {
        auto item = RsyncSync::parseItem(QStringLiteral(">f.st...... 6 2026/11/26-23:06:18 dir/size.txt"), true);
        QVERIFY(item);
        QCOMPARE(item->path, QStringLiteral("dir/size.txt"));
        QCOMPARE(item->kind, Kind::File);
        QCOMPARE(item->size, 6);
        QCOMPARE(item->modifiedSeconds, QDateTime(QDate(2026, 11, 26), QTime(23, 6, 18)).toSecsSinceEpoch());
        QVERIFY(!item->isNew());

        item = RsyncSync::parseItem(QStringLiteral("cd+++++++++ 4096 2026/11/26-23:06:18 new folder/"), true);
        QVERIFY(item);
        QCOMPARE(item->path, QStringLiteral("new folder"));
        QCOMPARE(item->kind, Kind::Directory);
        QVERIFY(item->isNew());

        item = RsyncSync::parseItem(QStringLiteral(">f+++++++++ a\\#012b\\#134"), false);
        QVERIFY(item);
        QCOMPARE(item->path, QStringLiteral("a\nb\\"));

        item = RsyncSync::parseItem(QStringLiteral("*deleting   old.txt"), false);
        QVERIFY(item);
        QVERIFY(item->isDeletion());
        QCOMPARE(item->path, QStringLiteral("old.txt"));

        item = RsyncSync::parseItem(QStringLiteral("cL+++++++++ link"), false);
        QVERIFY(item);
        QCOMPARE(item->kind, Kind::Symlink);

        QVERIFY(!RsyncSync::parseItem(QStringLiteral(".d..t...... 4096 2026/11/26-23:06:18 ./"), true));
        QVERIFY(!RsyncSync::parseItem(QStringLiteral("skipping non-regular file \"link\""), false));
        QVERIFY(!RsyncSync::parseItem(QStringLiteral("sending incremental file list"), false));
        QVERIFY(!RsyncSync::parseItem(QString(), false));
    }

    void buildsComparisons()
    {
        const QStringList leftToRight{
            QStringLiteral(".d..t...... 4096 2026/11/26-23:00:00 ./"),
            QStringLiteral(".d          4096 2026/11/26-23:00:00 dir/"),
            QStringLiteral(">f+++++++++ 3 2026/11/26-23:00:00 dir/new.txt"),
            QStringLiteral(">f.s....... 5 2026/11/26-23:00:00 size.txt"),
            QStringLiteral(">f..t...... 4 2026/11/26-23:00:00 time.txt"),
            QStringLiteral(".f          4 2026/11/26-23:00:00 same.txt"),
            QStringLiteral("cd+++++++++ 4096 2026/11/26-23:00:00 kind"),
        };
        const QStringList rightToLeft{
            QStringLiteral(".d          4096 2026/11/26-23:00:00 dir/"),
            QStringLiteral(">f.s....... 9 2026/11/26-23:00:00 size.txt"),
            QStringLiteral(">f..t...... 4 2026/11/26-22:00:00 time.txt"),
            QStringLiteral(".f          4 2026/11/26-23:00:00 same.txt"),
            QStringLiteral(">f+++++++++ 1 2026/11/26-23:00:00 right.txt"),
            QStringLiteral(">f+++++++++ 1 2026/11/26-23:00:00 kind"),
            QStringLiteral("*deleting   ignored.txt"),
        };
        const FolderSync::Comparison comparison =
            RsyncSync::comparison(local(QStringLiteral("/l")), remote(QStringLiteral("/r")), leftToRight, rightToLeft, {});
        QCOMPARE(comparison.leftRoot, QStringLiteral("/l"));
        QCOMPARE(comparison.rightRoot, QStringLiteral("me@host:2222:/r"));
        QVERIFY(comparison.error.isEmpty());
        QStringList paths;
        QList<Difference> differences;
        for (const FolderSync::Row &row : comparison.rows) {
            paths << row.relativePath;
            differences << row.difference;
        }
        QCOMPARE(paths,
                 QStringList({QStringLiteral("dir"),
                              QStringLiteral("dir/new.txt"),
                              QStringLiteral("kind"),
                              QStringLiteral("right.txt"),
                              QStringLiteral("same.txt"),
                              QStringLiteral("size.txt"),
                              QStringLiteral("time.txt")}));
        QCOMPARE(differences,
                 QList<Difference>({Difference::Directory,
                                    Difference::LeftOnly,
                                    Difference::Type,
                                    Difference::RightOnly,
                                    Difference::MetadataEqual,
                                    Difference::Size,
                                    Difference::Time}));
        const FolderSync::Row &size = comparison.rows.at(5);
        QCOMPARE(size.left.size, 5);
        QCOMPARE(size.right.size, 9);
        QVERIFY(size.left.device != size.right.device);
        QCOMPARE(comparison.rows.at(1).right.kind, Kind::Missing);
        QVERIFY(comparison.rows.at(6).left.modifiedSeconds > comparison.rows.at(6).right.modifiedSeconds);
    }

    void comparesContents()
    {
        FolderSync::Options options;
        options.compareContents = true;
        const FolderSync::Comparison comparison =
            RsyncSync::comparison(local(QStringLiteral("/l")),
                                  local(QStringLiteral("/r")),
                                  {QStringLiteral(">fc........ 4 2026/11/26-23:00:00 a"), QStringLiteral(".f          4 2026/11/26-23:00:00 b")},
                                  {QStringLiteral(">fc........ 4 2026/11/26-23:00:00 a"), QStringLiteral(".f          4 2026/11/26-23:00:00 b")},
                                  options);
        QCOMPARE(comparison.rows.size(), 2);
        QCOMPARE(comparison.rows.at(0).difference, Difference::ContentDifferent);
        QCOMPARE(comparison.rows.at(1).difference, Difference::ContentEqual);
    }

    void checksPlans()
    {
        FolderSync::Comparison comparison;
        comparison.rows = {row(QStringLiteral("dir"), Kind::Directory, Kind::Missing),
                           row(QStringLiteral("dir/a"), Kind::File, Kind::Missing),
                           row(QStringLiteral("b"), Kind::File, Kind::File),
                           row(QStringLiteral("c"), Kind::Missing, Kind::File),
                           row(QStringLiteral("d"), Kind::Directory, Kind::Directory)};
        RsyncSync::Transfer transfer;
        QCOMPARE(
            RsyncSync::planError(comparison, {Action::CopyToRight, Action::CopyToRight, Action::CopyToRight, Action::DeleteRight, Action::Skip}, &transfer),
            QString());
        QCOMPARE(transfer.direction, RsyncSync::Direction::ToRight);
        QCOMPARE(transfer.copies, QStringList({QStringLiteral("dir"), QStringLiteral("dir/a"), QStringLiteral("b")}));
        QCOMPARE(transfer.deletions, QStringList({QStringLiteral("c")}));

        QCOMPARE(RsyncSync::planError(comparison, {Action::Skip, Action::Skip, Action::CopyToLeft, Action::CopyToLeft, Action::Skip}, &transfer), QString());
        QCOMPARE(transfer.direction, RsyncSync::Direction::ToLeft);
        QCOMPARE(transfer.copies, QStringList({QStringLiteral("b"), QStringLiteral("c")}));

        // Both directions.
        QVERIFY(!RsyncSync::planError(comparison, {Action::Skip, Action::Skip, Action::CopyToRight, Action::CopyToLeft, Action::Skip}, nullptr).isEmpty());
        // Nothing to do.
        QVERIFY(!RsyncSync::planError(comparison, QList<Action>(5, Action::Skip), nullptr).isEmpty());
        // Wrong size.
        QVERIFY(!RsyncSync::planError(comparison, {Action::Skip}, nullptr).isEmpty());
        // Missing parent folder.
        QVERIFY(!RsyncSync::planError(comparison, {Action::Skip, Action::CopyToRight, Action::Skip, Action::Skip, Action::Skip}, nullptr).isEmpty());
        // Deleting a file the source still has.
        QVERIFY(!RsyncSync::planError(comparison, {Action::Skip, Action::Skip, Action::DeleteRight, Action::Skip, Action::Skip}, nullptr).isEmpty());
        // Deleting a folder.
        QVERIFY(!RsyncSync::planError(comparison, {Action::Skip, Action::Skip, Action::Skip, Action::Skip, Action::DeleteRight}, nullptr).isEmpty());

        FolderSync::Comparison invalid;
        invalid.rows = {row(QStringLiteral("../x"), Kind::File, Kind::Missing)};
        QVERIFY(!RsyncSync::planError(invalid, {Action::CopyToRight}, nullptr).isEmpty());
        invalid.rows = {row(QStringLiteral("x"), Kind::Symlink, Kind::Missing)};
        QVERIFY(!RsyncSync::planError(invalid, {Action::CopyToRight}, nullptr).isEmpty());
    }

    void buildsTransferSteps()
    {
        RsyncSync::Transfer transfer;
        transfer.direction = RsyncSync::Direction::ToLeft;
        transfer.copies = {QStringLiteral("a"), QStringLiteral("dir/b")};
        transfer.deletions = {QStringLiteral("c")};
        QList<RsyncSync::Step> steps =
            RsyncSync::transferSteps(local(QStringLiteral("/l")), remote(QStringLiteral("/r")), transfer, FolderSync::CopyPolicy::NewerOnly, 100, false, false);
        QCOMPARE(steps.size(), 2);
        const RsyncSync::Step &copy = steps.at(0);
        QCOMPARE(copy.input, QByteArray("a\0dir/b\0", 8));
        QVERIFY(copy.arguments.contains(QStringLiteral("--update")));
        QVERIFY(copy.arguments.contains(QStringLiteral("--ignore-times")));
        QVERIFY(copy.arguments.contains(QStringLiteral("--bwlimit=100")));
        QVERIFY(copy.arguments.contains(QStringLiteral("--info=progress2")));
        QVERIFY(!copy.arguments.contains(QStringLiteral("-n")));
        QCOMPARE(copy.arguments.last(2), QStringList({QStringLiteral("me@host:/r/"), QStringLiteral("/l/")}));
        QCOMPARE(copy.successCodes, QList<int>({0}));
        const RsyncSync::Step &remove = steps.at(1);
        QCOMPARE(remove.input, QByteArray("c\0", 2));
        QVERIFY(remove.arguments.contains(QStringLiteral("--delete-missing-args")));
        QVERIFY(remove.arguments.contains(QStringLiteral("--existing")));
        QVERIFY(remove.arguments.contains(QStringLiteral("--ignore-existing")));
        QCOMPARE(remove.successCodes, QList<int>({0, 24}));

        transfer.deletions.clear();
        steps =
            RsyncSync::transferSteps(local(QStringLiteral("/l")), local(QStringLiteral("/r")), transfer, FolderSync::CopyPolicy::SkipExisting, 0, true, false);
        QCOMPARE(steps.size(), 1);
        QVERIFY(steps.at(0).arguments.contains(QStringLiteral("-n")));
        QVERIFY(steps.at(0).arguments.contains(QStringLiteral("--ignore-existing")));
        QVERIFY(!steps.at(0).arguments.contains(QStringLiteral("--info=progress2")));
        QVERIFY(!steps.at(0).arguments.contains(QStringLiteral("-e")));
    }

    void summarizesRuns()
    {
        RsyncSync::Transfer transfer;
        transfer.copies = {QStringLiteral("dir"), QStringLiteral("dir/a"), QStringLiteral("b"), QStringLiteral("skipped")};
        transfer.deletions = {QStringLiteral("c")};
        const RsyncSync::Summary summary = RsyncSync::summarize(
            transfer,
            {QStringLiteral("cd+++++++++ dir/"), QStringLiteral(">f+++++++++ dir/a"), QStringLiteral(">f.st...... b"), QStringLiteral("*deleting   c")});
        QCOMPARE(summary.folders, 1);
        QCOMPARE(summary.created, 1);
        QCOMPARE(summary.overwritten, 1);
        QCOMPARE(summary.deleted, 1);
        QCOMPARE(summary.missing, QStringList({QStringLiteral("skipped")}));
    }

    void parsesProgressAndExitCodes()
    {
        QCOMPARE(RsyncSync::progressPercent(QStringLiteral("      9B 100%  6.00KB/s  0:00:00 (xfr#2, to-chk=1/3)")), 100);
        QCOMPARE(RsyncSync::progressPercent(QStringLiteral("  1,234,567  42%  1.00MB/s    0:00:01")), 42);
        QCOMPARE(RsyncSync::progressPercent(QStringLiteral(">f+++++++++ 100% done")), -1);
        QVERIFY(RsyncSync::exitMessage(0).isEmpty());
        QVERIFY(!RsyncSync::exitMessage(23).isEmpty());
        QVERIFY(RsyncSync::exitMessage(99).contains(QStringLiteral("99")));
    }

    void reportsMissingProgram()
    {
        RsyncSync::Process process(QString(), {RsyncSync::Step()}, RsyncSync::environment(QString()));
        QSignalSpy finished(&process, &RsyncSync::Process::finished);
        process.start();
        QVERIFY(finished.wait());
        QVERIFY(!process.succeeded());
        QVERIFY(!process.errorText().isEmpty());
    }

    void synchronizesLocalFolders()
    {
        const QString program = RsyncSync::program();
        if (program.isEmpty()) {
            QSKIP("rsync is not installed");
        }
        QTemporaryDir leftDir;
        QTemporaryDir rightDir;
        QVERIFY(leftDir.isValid() && rightDir.isValid());
        const QDateTime old = QDateTime::currentDateTime().addDays(-2);
        const QDateTime older = old.addDays(-1);
        QVERIFY(writeFile(leftDir.filePath(QStringLiteral("same.txt")), "same", old));
        QVERIFY(writeFile(rightDir.filePath(QStringLiteral("same.txt")), "same", old));
        QVERIFY(writeFile(leftDir.filePath(QStringLiteral("changed.txt")), "new text", old));
        QVERIFY(writeFile(rightDir.filePath(QStringLiteral("changed.txt")), "old", older));
        QVERIFY(writeFile(leftDir.filePath(QStringLiteral("sub dir/new\nline.txt")), "abc", old));
        QVERIFY(writeFile(rightDir.filePath(QStringLiteral("gone.txt")), "x", old));
        QVERIFY(writeFile(leftDir.filePath(QStringLiteral("skip.tmp")), "tmp", old));

        const RsyncSync::Endpoint left = local(leftDir.path());
        const RsyncSync::Endpoint right = local(rightDir.path());
        FolderSync::Options options;
        options.excludePatterns = {QStringLiteral("*.tmp")};
        RsyncSync::Process list(program,
                                {RsyncSync::listStep(left, right, options, true), RsyncSync::listStep(right, left, options, true)},
                                RsyncSync::environment(QString()));
        QSignalSpy listed(&list, &RsyncSync::Process::finished);
        list.start();
        QVERIFY(listed.wait(30000));
        QVERIFY2(list.succeeded(), qPrintable(list.errorText()));
        const FolderSync::Comparison comparison = RsyncSync::comparison(left, right, list.lines(0), list.lines(1), options);

        QList<Action> actions;
        QHash<QString, Difference> differences;
        for (const FolderSync::Row &row : comparison.rows) {
            differences.insert(row.relativePath, row.difference);
            if (row.difference == Difference::LeftOnly || row.difference == Difference::Size || row.difference == Difference::Time) {
                actions << Action::CopyToRight;
            } else if (row.difference == Difference::RightOnly) {
                actions << Action::DeleteRight;
            } else {
                actions << Action::Skip;
            }
        }
        QCOMPARE(differences.size(), 5);
        QCOMPARE(differences.value(QStringLiteral("same.txt")), Difference::MetadataEqual);
        QCOMPARE(differences.value(QStringLiteral("changed.txt")), Difference::Size);
        QCOMPARE(differences.value(QStringLiteral("sub dir")), Difference::LeftOnly);
        QCOMPARE(differences.value(QStringLiteral("sub dir/new\nline.txt")), Difference::LeftOnly);
        QCOMPARE(differences.value(QStringLiteral("gone.txt")), Difference::RightOnly);

        RsyncSync::Transfer transfer;
        QCOMPARE(RsyncSync::planError(comparison, actions, &transfer), QString());

        RsyncSync::Process dryRun(program,
                                  RsyncSync::transferSteps(left, right, transfer, FolderSync::CopyPolicy::All, 0, true, true),
                                  RsyncSync::environment(QString()));
        QSignalSpy dryRunFinished(&dryRun, &RsyncSync::Process::finished);
        dryRun.start();
        QVERIFY(dryRunFinished.wait(30000));
        QVERIFY2(dryRun.succeeded(), qPrintable(dryRun.errorText()));
        RsyncSync::Summary summary = RsyncSync::summarize(transfer, dryRun.allLines());
        QCOMPARE(summary.folders, 1);
        QCOMPARE(summary.created, 1);
        QCOMPARE(summary.overwritten, 1);
        QCOMPARE(summary.deleted, 1);
        QVERIFY(summary.missing.isEmpty());
        QCOMPARE(readFile(rightDir.filePath(QStringLiteral("changed.txt"))), QByteArray("old"));
        QVERIFY(QFile::exists(rightDir.filePath(QStringLiteral("gone.txt"))));

        RsyncSync::Process run(program,
                               RsyncSync::transferSteps(left, right, transfer, FolderSync::CopyPolicy::All, 0, false, true),
                               RsyncSync::environment(QString()));
        QSignalSpy runFinished(&run, &RsyncSync::Process::finished);
        run.start();
        QVERIFY(runFinished.wait(30000));
        QVERIFY2(run.succeeded(), qPrintable(run.errorText()));
        summary = RsyncSync::summarize(transfer, run.allLines());
        QVERIFY(summary.missing.isEmpty());
        QCOMPARE(readFile(rightDir.filePath(QStringLiteral("changed.txt"))), QByteArray("new text"));
        QCOMPARE(readFile(rightDir.filePath(QStringLiteral("sub dir/new\nline.txt"))), QByteArray("abc"));
        QVERIFY(!QFile::exists(rightDir.filePath(QStringLiteral("gone.txt"))));
        QVERIFY(!QFile::exists(rightDir.filePath(QStringLiteral("skip.tmp"))));
        QCOMPARE(readFile(leftDir.filePath(QStringLiteral("skip.tmp"))), QByteArray("tmp"));
    }

    void cancelsRuns()
    {
        const QString program = QStandardPaths::findExecutable(QStringLiteral("sleep"));
        if (program.isEmpty()) {
            QSKIP("sleep is not installed");
        }
        RsyncSync::Step step;
        step.arguments = {QStringLiteral("30")};
        RsyncSync::Process process(program, {step}, RsyncSync::environment(QString()));
        QSignalSpy finished(&process, &RsyncSync::Process::finished);
        process.start();
        QVERIFY(process.isRunning());
        process.cancel();
        QVERIFY(finished.wait(10000));
        QVERIFY(process.wasCanceled());
        QVERIFY(!process.succeeded());
        QVERIFY(!process.isRunning());
    }
};

QTEST_GUILESS_MAIN(RsyncSyncTest)

#include "rsyncsynctest.moc"
