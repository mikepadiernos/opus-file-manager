// SPDX-License-Identifier: GPL-3.0-or-later

#include "rclone.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>

class RcloneTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_bin.isValid());
        QVERIFY(m_runtime.isValid());
        m_path = qgetenv("PATH");
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toLocal8Bit());
        m_arguments = m_bin.filePath(QStringLiteral("arguments"));
        qputenv("FAKE_RCLONE_ARGS", m_arguments.toLocal8Bit());
    }

    void cleanup()
    {
        qputenv("PATH", m_path);
        qunsetenv("FAKE_RCLONE_FAIL");
        QFile::remove(m_arguments);
        QFile::remove(m_arguments + QStringLiteral(".pass"));
    }

    void parsesRemotes()
    {
        const QList<Rclone::Remote> remotes = Rclone::parseRemotes("gdrive:        drive\nmy box:        box\nplain:\n-bad:          x\n\nnot a remote\n");
        QCOMPARE(remotes.size(), 3);
        QCOMPARE(remotes.at(0).name, QStringLiteral("gdrive"));
        QCOMPARE(remotes.at(0).type, QStringLiteral("drive"));
        QCOMPARE(remotes.at(1).name, QStringLiteral("my box"));
        QCOMPARE(remotes.at(1).type, QStringLiteral("box"));
        QCOMPARE(remotes.at(2).name, QStringLiteral("plain"));
        QVERIFY(remotes.at(2).type.isEmpty());
    }

    void validatesNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("valid");
        QTest::newRow("simple") << "gdrive" << true;
        QTest::newRow("space") << "my box" << true;
        QTest::newRow("symbols") << "a.b+c@d_e-f" << true;
        QTest::newRow("unicode") << QStringLiteral("dépôt") << true;
        QTest::newRow("empty") << "" << false;
        QTest::newRow("option") << "-x" << false;
        QTest::newRow("leading space") << " x" << false;
        QTest::newRow("trailing space") << "x " << false;
        QTest::newRow("slash") << "a/b" << false;
        QTest::newRow("colon") << "a:b" << false;
        QTest::newRow("dot dot") << ".." << false;
    }

    void validatesNames()
    {
        QFETCH(QString, name);
        QFETCH(bool, valid);
        QCOMPARE(Rclone::isValidName(name), valid);
    }

    void buildsArgumentsAndPaths()
    {
        QCOMPARE(Rclone::mountRoot(), QDir(m_runtime.path()).filePath(QStringLiteral("opus-file-manager/rclone")));
        QCOMPARE(Rclone::mountPoint(QStringLiteral("my box")), QDir(Rclone::mountRoot()).filePath(QStringLiteral("my box")));
        QCOMPARE(Rclone::mountArguments(QStringLiteral("gdrive"), QStringLiteral("/run/x")),
                 (QStringList{QStringLiteral("mount"),
                              QStringLiteral("--vfs-cache-mode"),
                              QStringLiteral("writes"),
                              QStringLiteral("--"),
                              QStringLiteral("gdrive:"),
                              QStringLiteral("/run/x")}));
        QCOMPARE(Rclone::environment().value(QStringLiteral("RCLONE_ASK_PASSWORD")), QStringLiteral("false"));
        QCOMPARE(Rclone::lastError("2024/05/01 10:00:00 NOTICE: starting\n2024/05/01 10:00:01 CRITICAL: Fatal error: no FUSE\n\n"),
                 QStringLiteral("Fatal error: no FUSE"));
        QVERIFY(Rclone::isMountPoint(QStringLiteral("/")));
        QVERIFY(!Rclone::isMountPoint(m_runtime.path()));
    }

    void reportsMissingRclone()
    {
        qputenv("PATH", m_bin.filePath(QStringLiteral("empty")).toLocal8Bit());
        RcloneMounts mounts;
        QSignalSpy listed(&mounts, &RcloneMounts::remotesListed);
        mounts.listRemotes();
        QCOMPARE(listed.size(), 1);
        QVERIFY(listed.first().at(1).toString().contains(QStringLiteral("not installed")));
        QSignalSpy failed(&mounts, &RcloneMounts::failed);
        mounts.mount(QStringLiteral("gdrive"));
        QCOMPARE(failed.size(), 1);
        mounts.mount(QStringLiteral("-x"));
        QCOMPARE(failed.size(), 2);
    }

    void listsRemotesInTheBackground()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        QSignalSpy listed(&mounts, &RcloneMounts::remotesListed);
        mounts.listRemotes();
        QVERIFY(listed.wait(5000));
        QVERIFY(listed.first().at(1).toString().isEmpty());
        const auto remotes = listed.first().at(0).value<QList<Rclone::Remote>>();
        QCOMPARE(remotes.size(), 2);
        QCOMPARE(remotes.at(1).name, QStringLiteral("my box"));

        qputenv("FAKE_RCLONE_FAIL", "1");
        listed.clear();
        mounts.listRemotes();
        QVERIFY(listed.wait(5000));
        QVERIFY(listed.first().at(1).toString().contains(QStringLiteral("couldn't decrypt")));
    }

    void mountsAndUnmounts()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        mounts.setMountCheck([this](const QString &) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy mounted(&mounts, &RcloneMounts::mounted);
        QSignalSpy unmounted(&mounts, &RcloneMounts::unmounted);
        mounts.mount(QStringLiteral("my box"));
        QVERIFY(mounts.isStarting(QStringLiteral("my box")));
        QVERIFY(mounted.wait(5000));
        const QString path = Rclone::mountPoint(QStringLiteral("my box"));
        QCOMPARE(mounted.first().at(1).toString(), path);
        QVERIFY(mounts.isMounted(QStringLiteral("my box")));
        QCOMPARE(mounts.mountedRemotes(), QStringList{QStringLiteral("my box")});
        QCOMPARE(QFile::permissions(Rclone::mountRoot()) & (QFileDevice::ReadOther | QFileDevice::ReadGroup), QFileDevice::Permissions());

        QFile file(m_arguments);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts), Rclone::mountArguments(QStringLiteral("my box"), path));
        file.close();

        mounted.clear();
        mounts.mount(QStringLiteral("my box"));
        QCOMPARE(mounted.size(), 1);

        mounts.unmount(QStringLiteral("my box"));
        QVERIFY(!mounts.isMounted(QStringLiteral("my box")));
        QVERIFY(unmounted.wait(5000));
        QVERIFY(!QFileInfo::exists(path));
    }

    void reportsMountFailures()
    {
        writeFakeRclone();
        qputenv("FAKE_RCLONE_FAIL", "1");
        RcloneMounts mounts;
        mounts.setMountCheck([](const QString &) {
            return false;
        });
        QSignalSpy failed(&mounts, &RcloneMounts::failed);
        mounts.mount(QStringLiteral("gdrive"));
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(1).toString(), QStringLiteral("Fatal error: failed to mount FUSE fs"));
        QVERIFY(!mounts.isStarting(QStringLiteral("gdrive")));
        QVERIFY(!QFileInfo::exists(Rclone::mountPoint(QStringLiteral("gdrive"))));
    }

    void timesOutAndStopsRclone()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        mounts.setTimeout(400);
        mounts.setMountCheck([](const QString &) {
            return false;
        });
        QSignalSpy failed(&mounts, &RcloneMounts::failed);
        mounts.mount(QStringLiteral("gdrive"));
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("not mounted in time")));
        QVERIFY(mounts.mountedRemotes().isEmpty());
    }

    void stopsMountsWhenDestroyed()
    {
        writeFakeRclone();
        auto *mounts = new RcloneMounts;
        mounts->setMountCheck([this](const QString &) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy mounted(mounts, &RcloneMounts::mounted);
        mounts->mount(QStringLiteral("gdrive"));
        QVERIFY(mounted.wait(5000));
        delete mounts;
        QVERIFY(!QFileInfo::exists(Rclone::mountPoint(QStringLiteral("gdrive"))));
    }

    void buildsServeArguments()
    {
        const QStringList arguments = Rclone::serveArguments(QStringLiteral("my box"), 8123, {QStringLiteral("--transfers"), QStringLiteral("2")});
        QCOMPARE(arguments,
                 (QStringList{QStringLiteral("serve"),
                              QStringLiteral("webdav"),
                              QStringLiteral("--addr"),
                              QStringLiteral("127.0.0.1:8123"),
                              QStringLiteral("--user"),
                              Rclone::serveUser(),
                              QStringLiteral("--realm"),
                              Rclone::serveRealm(),
                              QStringLiteral("--transfers"),
                              QStringLiteral("2"),
                              QStringLiteral("--"),
                              QStringLiteral("my box:")}));
        QVERIFY(Rclone::serveArguments(QStringLiteral("gdrive"), 1).contains(QStringLiteral("writes")));
        // The password stays out of the command line and the location.
        QCOMPARE(Rclone::serveEnvironment(QStringLiteral("secret")).value(QStringLiteral("RCLONE_PASS")), QStringLiteral("secret"));
        QCOMPARE(Rclone::serveEnvironment(QStringLiteral("secret")).value(QStringLiteral("RCLONE_ASK_PASSWORD")), QStringLiteral("false"));
        const QUrl url = Rclone::serveUrl(8123);
        QCOMPARE(url.toString(), QStringLiteral("webdav://rclone@127.0.0.1:8123/"));
        QVERIFY(url.password().isEmpty());

        const QString password = Rclone::randomPassword();
        QCOMPARE(password.size(), 32);
        QVERIFY(password != Rclone::randomPassword());
        const quint16 preferred = Rclone::preferredServePort(QStringLiteral("gdrive"));
        QCOMPARE(Rclone::preferredServePort(QStringLiteral("gdrive")), preferred);
        QVERIFY(preferred >= 24000 && preferred < 32000);
        QVERIFY(Rclone::preferredServePort(QStringLiteral("gdrive2")) != preferred);
        const quint16 port = Rclone::freeLoopbackPort();
        QVERIFY(port > 0);
        QVERIFY(!Rclone::isListening(port));
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, port));
        QVERIFY(Rclone::isListening(port));
        // A busy preferred port moves to a later one.
        const quint16 next = Rclone::freeLoopbackPort(port);
        QVERIFY(next != port && next != 0);
    }

    void servesAndStops()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        mounts.setServeCheck([this](quint16) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy served(&mounts, &RcloneMounts::served);
        QSignalSpy mounted(&mounts, &RcloneMounts::mounted);
        QSignalSpy stopped(&mounts, &RcloneMounts::serveStopped);
        QString password;
        QUrl asked;
        mounts.setPasswordProvider([&](const QString &remote, const QUrl &url, const std::function<void(const QString &)> &done) {
            QCOMPARE(remote, QStringLiteral("my box"));
            asked = url;
            password = Rclone::randomPassword();
            done(password);
        });
        mounts.serve(QStringLiteral("my box"));
        QVERIFY(mounts.isStarting(QStringLiteral("my box")));
        QVERIFY(served.wait(5000));
        const QUrl url = served.first().at(1).toUrl();
        QCOMPARE(url, asked);
        QCOMPARE(url.scheme(), QStringLiteral("webdav"));
        QCOMPARE(url.host(), QStringLiteral("127.0.0.1"));
        QVERIFY(url.password().isEmpty());
        QVERIFY(mounts.isServed(QStringLiteral("my box")));
        QVERIFY(!mounts.isMounted(QStringLiteral("my box")));
        QCOMPARE(mounts.servedRemotes(), QStringList{QStringLiteral("my box")});
        QVERIFY(mounts.mountedRemotes().isEmpty());
        QCOMPARE(mounts.servedUrl(QStringLiteral("my box")), url);
        QVERIFY(!QFileInfo::exists(Rclone::mountPoint(QStringLiteral("my box"))));

        QFile file(m_arguments);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QStringList arguments = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        file.close();
        QCOMPARE(arguments, Rclone::serveArguments(QStringLiteral("my box"), quint16(url.port())));
        QVERIFY(!arguments.join(QLatin1Char(' ')).contains(password));
        QFile pass(m_arguments + QStringLiteral(".pass"));
        QVERIFY(pass.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromLatin1(pass.readAll()), password);

        // A served remote stays served when it is opened again, whatever is asked.
        served.clear();
        mounts.mount(QStringLiteral("my box"));
        mounts.serve(QStringLiteral("my box"));
        QCOMPARE(served.size(), 2);
        QCOMPARE(mounted.size(), 0);
        QCOMPARE(served.last().at(1).toUrl(), url);

        mounts.unmount(QStringLiteral("my box"));
        QVERIFY(!mounts.isServed(QStringLiteral("my box")));
        QVERIFY(stopped.wait(5000));
        QCOMPARE(stopped.first().at(1).toUrl(), url);
        QVERIFY(mounts.servedRemotes().isEmpty());
    }

    void reportsServeFailures()
    {
        writeFakeRclone();
        qputenv("FAKE_RCLONE_FAIL", "1");
        RcloneMounts mounts;
        mounts.setServeCheck([](quint16) {
            return false;
        });
        QSignalSpy failed(&mounts, &RcloneMounts::failed);
        mounts.serve(QStringLiteral("gdrive"));
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(1).toString(), QStringLiteral("Fatal error: failed to start server"));
        QVERIFY(!mounts.isStarting(QStringLiteral("gdrive")));

        qunsetenv("FAKE_RCLONE_FAIL");
        failed.clear();
        mounts.setTimeout(400);
        mounts.serve(QStringLiteral("gdrive"));
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("not served in time")));
        QVERIFY(mounts.servedRemotes().isEmpty());

        failed.clear();
        mounts.serve(QStringLiteral("-x"));
        QCOMPARE(failed.size(), 1);

        // Without a password, nothing is served.
        mounts.setPasswordProvider([](const QString &, const QUrl &, const std::function<void(const QString &)> &done) {
            done(QString());
        });
        mounts.serve(QStringLiteral("gdrive"));
        QCOMPARE(failed.size(), 2);
        QVERIFY(failed.last().at(1).toString().contains(QStringLiteral("password")));
        QVERIFY(!mounts.isStarting(QStringLiteral("gdrive")));

        qputenv("PATH", m_bin.filePath(QStringLiteral("empty")).toLocal8Bit());
        mounts.serve(QStringLiteral("gdrive"));
        QCOMPARE(failed.size(), 3);
        QVERIFY(failed.last().at(1).toString().contains(QStringLiteral("not installed")));
    }

    void stopsServingWhileWaitingForThePassword()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        std::function<void(const QString &)> pending;
        mounts.setPasswordProvider([&](const QString &, const QUrl &, const std::function<void(const QString &)> &done) {
            pending = done;
        });
        QSignalSpy served(&mounts, &RcloneMounts::served);
        QSignalSpy failed(&mounts, &RcloneMounts::failed);
        mounts.serve(QStringLiteral("gdrive"));
        QVERIFY(pending);
        QVERIFY(mounts.isStarting(QStringLiteral("gdrive")));
        // Opening it again waits for the same password.
        mounts.serve(QStringLiteral("gdrive"));
        mounts.unmount(QStringLiteral("gdrive"));
        QVERIFY(!mounts.isStarting(QStringLiteral("gdrive")));
        pending(QStringLiteral("secret"));
        QTest::qWait(300);
        QVERIFY(!QFile::exists(m_arguments));
        QCOMPARE(served.size(), 0);
        QCOMPARE(failed.size(), 0);

        // The answer can also come after the mounts are gone.
        pending = {};
        auto *gone = new RcloneMounts;
        gone->setPasswordProvider([&](const QString &, const QUrl &, const std::function<void(const QString &)> &done) {
            pending = done;
        });
        gone->serve(QStringLiteral("gdrive"));
        delete gone;
        pending(QStringLiteral("secret"));
        QVERIFY(!QFile::exists(m_arguments));
    }

    void stopsServersAtQuit()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        mounts.setServeCheck([this](quint16) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy served(&mounts, &RcloneMounts::served);
        QSignalSpy stopped(&mounts, &RcloneMounts::serveStopped);
        mounts.serve(QStringLiteral("gdrive"));
        QVERIFY(served.wait(5000));
        mounts.stopAllNow();
        QVERIFY(mounts.servedRemotes().isEmpty());
        QTest::qWait(100);
        QCOMPARE(stopped.size(), 0);
    }

    void stopsMountsAtQuit()
    {
        writeFakeRclone();
        RcloneMounts mounts;
        mounts.setMountCheck([this](const QString &) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy mounted(&mounts, &RcloneMounts::mounted);
        QSignalSpy unmounted(&mounts, &RcloneMounts::unmounted);
        mounts.mount(QStringLiteral("gdrive"));
        QVERIFY(mounted.wait(5000));
        mounts.stopAllNow();
        QVERIFY(mounts.mountedRemotes().isEmpty());
        QVERIFY(!mounts.isMounted(QStringLiteral("gdrive")));
        QVERIFY(!QFileInfo::exists(Rclone::mountPoint(QStringLiteral("gdrive"))));
        QTest::qWait(100);
        QCOMPARE(unmounted.size(), 0);
    }

private:
    void writeFakeRclone()
    {
        QVERIFY(QDir(m_bin.path()).mkpath(QStringLiteral("tools")));
        const QString path = m_bin.filePath(QStringLiteral("tools/rclone"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(
            "#!/bin/sh\n"
            "case \"$1\" in\n"
            "listremotes)\n"
            "  if [ -n \"$FAKE_RCLONE_FAIL\" ]; then echo '2024/05/01 10:00:00 ERROR : Failed to load config file: couldn'\"'\"'t decrypt' >&2; exit 1; fi\n"
            "  printf 'gdrive:   drive\\nmy box:   box\\n'; exit 0;;\n"
            "mount)\n"
            "  if [ -n \"$FAKE_RCLONE_FAIL\" ]; then echo '2024/05/01 10:00:00 CRITICAL: Fatal error: failed to mount FUSE fs' >&2; exit 1; fi\n"
            "  printf '%s\\n' \"$@\" > \"$FAKE_RCLONE_ARGS\"\n"
            "  exec sleep 30;;\n"
            "serve)\n"
            "  if [ -n \"$FAKE_RCLONE_FAIL\" ]; then echo '2024/05/01 10:00:00 CRITICAL: Fatal error: failed to start server' >&2; exit 1; fi\n"
            "  printf '%s' \"$RCLONE_PASS\" > \"$FAKE_RCLONE_ARGS.pass\"\n"
            "  printf '%s\\n' \"$@\" > \"$FAKE_RCLONE_ARGS\"\n"
            "  exec sleep 30;;\n"
            "esac\n"
            "exit 2\n");
        file.close();
        QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
        qputenv("PATH", QByteArray(m_bin.filePath(QStringLiteral("tools")).toLocal8Bit() + ':' + m_path));
    }

    QTemporaryDir m_bin;
    QTemporaryDir m_runtime;
    QByteArray m_path;
    QString m_arguments;
};

QTEST_GUILESS_MAIN(RcloneTest)

#include "rclonetest.moc"
