// SPDX-License-Identifier: GPL-3.0-or-later

#include "remotemounts.h"

#include <KConfig>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using RemoteMounts::Backend;
using RemoteProfiles::Protocol;

class RemoteMountsTest : public QObject
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
        m_marker = m_bin.filePath(QStringLiteral("mounted"));
        qputenv("FAKE_ARGS", m_arguments.toLocal8Bit());
        qputenv("FAKE_MARKER", m_marker.toLocal8Bit());
        writeFakeTools();
    }

    void cleanup()
    {
        qunsetenv("FAKE_FAIL");
        qunsetenv("FAKE_BUSY");
        QFile::remove(m_arguments);
        QFile::remove(m_marker);
    }

    void offersBackendsPerProtocol()
    {
        QCOMPARE(RemoteMounts::backends(Protocol::Sftp), (QList<Backend>{Backend::KioFuse, Backend::Sshfs}));
        QCOMPARE(RemoteMounts::backends(Protocol::WebDavs), (QList<Backend>{Backend::KioFuse, Backend::Davfs2}));
        QCOMPARE(RemoteMounts::backends(Protocol::Smb), (QList<Backend>{Backend::KioFuse, Backend::Cifs}));
        QCOMPARE(RemoteMounts::backends(Protocol::Ftps), QList<Backend>{Backend::KioFuse});
    }

    void reportsMissingTools()
    {
        const RemoteMounts::ExecutableLookup none = [](const QString &) {
            return QString();
        };
        const RemoteMounts::ExecutableLookup noFuse = [](const QString &name) {
            return name == QLatin1String("sshfs") ? QStringLiteral("/usr/bin/sshfs") : QString();
        };
        const RemoteMounts::ExecutableLookup all = [](const QString &name) {
            return QStringLiteral("/usr/bin/") + name;
        };
        QVERIFY(RemoteMounts::unavailableReason(Backend::KioFuse, none).isEmpty());
        QVERIFY(RemoteMounts::unavailableReason(Backend::Sshfs, none).contains(QStringLiteral("sshfs package")));
        QVERIFY(RemoteMounts::unavailableReason(Backend::Sshfs, noFuse).contains(QStringLiteral("fuse3")));
        QVERIFY(RemoteMounts::unavailableReason(Backend::Davfs2, none).contains(QStringLiteral("davfs2 package")));
        QVERIFY(RemoteMounts::unavailableReason(Backend::Cifs, none).contains(QStringLiteral("cifs-utils")));
        for (Backend backend : {Backend::Sshfs, Backend::Davfs2, Backend::Cifs}) {
            QVERIFY(RemoteMounts::unavailableReason(backend, all).isEmpty());
        }
    }

    void validatesOptions()
    {
        const RemoteProfiles::Profile ftp = profile(Protocol::Ftp);
        RemoteMounts::Options options;
        QVERIFY(RemoteMounts::optionsError(ftp, options).isEmpty());
        options.backend = Backend::Sshfs;
        QVERIFY(!RemoteMounts::optionsError(ftp, options).isEmpty());

        const RemoteProfiles::Profile sftp = profile(Protocol::Sftp);
        options = {};
        options.readOnly = true;
        QVERIFY(RemoteMounts::optionsError(sftp, options).contains(QStringLiteral("read-only")));
        options.readOnly = false;
        options.location = QStringLiteral("/mnt/x");
        QVERIFY(RemoteMounts::optionsError(sftp, options).contains(QStringLiteral("own folder")));
        options.backend = Backend::Sshfs;
        options.readOnly = true;
        QVERIFY(RemoteMounts::optionsError(sftp, options).isEmpty());
        options.location = QStringLiteral("relative/x");
        QVERIFY(RemoteMounts::optionsError(sftp, options).contains(QStringLiteral("absolute")));
    }

    void choosesMountPoints()
    {
        RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        remote.name = QStringLiteral(" Work/Server ");
        RemoteMounts::Options options;
        options.backend = Backend::Sshfs;
        QCOMPARE(RemoteMounts::mountRoot(), m_runtime.filePath(QStringLiteral("opus-file-manager/mounts")));
        QCOMPARE(RemoteMounts::mountPoint(remote, options), QDir(RemoteMounts::mountRoot()).filePath(QStringLiteral("Work-Server")));
        remote.name = QStringLiteral("..");
        QCOMPARE(QFileInfo(RemoteMounts::mountPoint(remote, options)).fileName(), QStringLiteral("_.."));
        options.location = QStringLiteral("/mnt/work/");
        QCOMPARE(RemoteMounts::mountPoint(remote, options), QStringLiteral("/mnt/work"));
    }

    void buildsSshfsArguments()
    {
        RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        remote.user = QStringLiteral("me");
        remote.port = 2222;
        remote.path = QStringLiteral("/srv//data/");
        remote.authentication = RemoteProfiles::Authentication::KeyOrAgent;
        remote.identityFile = QStringLiteral("/home/me/.ssh/id,work");
        QCOMPARE(RemoteMounts::sshfsTarget(remote), QStringLiteral("me@example.org:/srv/data"));
        QCOMPARE(RemoteMounts::sshfsArguments(remote, QStringLiteral("/run/m"), true, true),
                 (QStringList{QStringLiteral("-f"),
                              QStringLiteral("-o"),
                              QStringLiteral("reconnect,ServerAliveInterval=15,ServerAliveCountMax=3"),
                              QStringLiteral("-o"),
                              QStringLiteral("ro"),
                              QStringLiteral("-p"),
                              QStringLiteral("2222"),
                              QStringLiteral("-o"),
                              QStringLiteral("IdentityFile=/home/me/.ssh/id\\,work"),
                              QStringLiteral("-o"),
                              QStringLiteral("BatchMode=yes"),
                              QStringLiteral("--"),
                              QStringLiteral("me@example.org:/srv/data"),
                              QStringLiteral("/run/m")}));

        RemoteProfiles::Profile plain = profile(Protocol::Sftp);
        plain.host = QStringLiteral("::1");
        QCOMPARE(RemoteMounts::sshfsTarget(plain), QStringLiteral("[::1]:"));
        const QStringList arguments = RemoteMounts::sshfsArguments(plain, QStringLiteral("/run/m"), false, false);
        QVERIFY(!arguments.contains(QStringLiteral("ro")));
        QVERIFY(!arguments.contains(QStringLiteral("BatchMode=yes")));
        QVERIFY(!arguments.contains(QStringLiteral("-p")));

        const QProcessEnvironment environment = RemoteMounts::sshfsEnvironment(QStringLiteral("/usr/bin/ksshaskpass"));
        QCOMPARE(environment.value(QStringLiteral("SSH_ASKPASS")), QStringLiteral("/usr/bin/ksshaskpass"));
        QCOMPARE(environment.value(QStringLiteral("SSH_ASKPASS_REQUIRE")), QStringLiteral("force"));
    }

    void checksFstabEntries()
    {
        const QList<RemoteMounts::FstabEntry> entries = RemoteMounts::parseFstab(
            "# comment\n"
            "UUID=1 / ext4 defaults 0 1\n"
            "https://dav.example.org/files /mnt/my\\040dav davfs user,noauto 0 0\n"
            "//nas/share /mnt/nas cifs noauto # admin only\n"
            "//nas/ro /mnt/ro cifs users,ro 0 0\n"
            "broken\n");
        QCOMPARE(entries.size(), 4);
        QCOMPARE(entries.at(1).mountPoint, QStringLiteral("/mnt/my dav"));
        QCOMPARE(entries.at(1).options, (QStringList{QStringLiteral("user"), QStringLiteral("noauto")}));

        QVERIFY(RemoteMounts::fstabError(entries, Backend::Davfs2, QStringLiteral("/mnt/my dav/"), false).isEmpty());
        QVERIFY(RemoteMounts::fstabError(entries, Backend::Davfs2, QStringLiteral("/mnt/my dav"), true).contains(QStringLiteral("ro option")));
        QVERIFY(RemoteMounts::fstabError(entries, Backend::Cifs, QStringLiteral("/mnt/my dav"), false).contains(QStringLiteral("davfs")));
        QVERIFY(RemoteMounts::fstabError(entries, Backend::Cifs, QStringLiteral("/mnt/nas"), false).contains(QStringLiteral("administrator authorization")));
        QVERIFY(RemoteMounts::fstabError(entries, Backend::Cifs, QStringLiteral("/mnt/ro"), true).isEmpty());
        QVERIFY(RemoteMounts::fstabError(entries, Backend::Cifs, QStringLiteral("/mnt/other"), false).contains(QStringLiteral("needs a user-mountable")));

        RemoteProfiles::Profile dav = profile(Protocol::WebDavs);
        dav.user = QStringLiteral("me");
        dav.port = 8443;
        dav.path = QStringLiteral("/remote.php/dav");
        QCOMPARE(RemoteMounts::fstabExample(dav, Backend::Davfs2, QStringLiteral("/run/user/1000/My Dav"), false),
                 QStringLiteral("https://example.org:8443/remote.php/dav /run/user/1000/My\\040Dav davfs user,noauto 0 0"));
        RemoteProfiles::Profile smb = profile(Protocol::Smb);
        smb.user = QStringLiteral("me");
        smb.path = QStringLiteral("/share/docs");
        QCOMPARE(RemoteMounts::fstabExample(smb, Backend::Cifs, QStringLiteral("/mnt/nas"), true),
                 QStringLiteral("//example.org/share/docs /mnt/nas cifs user,noauto,username=me,ro 0 0"));
        QVERIFY(RemoteMounts::fstabExample(smb, Backend::Sshfs, QStringLiteral("/mnt/nas"), false).isEmpty());
    }

    void savesOptions()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("RemoteMounts"));
        RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        QCOMPARE(RemoteMounts::loadOptions(group, remote).backend, Backend::KioFuse);

        RemoteMounts::Options options;
        options.backend = Backend::Sshfs;
        options.readOnly = true;
        options.location = QStringLiteral("/mnt/server");
        options.atStartup = true;
        RemoteMounts::saveOptions(group, remote.id, options);
        const RemoteMounts::Options loaded = RemoteMounts::loadOptions(group, remote);
        QCOMPARE(loaded.backend, Backend::Sshfs);
        QVERIFY(loaded.readOnly);
        QCOMPARE(loaded.location, QStringLiteral("/mnt/server"));
        QVERIFY(loaded.atStartup);

        // A protocol change falls back to a backend that can mount it, dropping unsupported options.
        remote.protocol = Protocol::Ftp;
        const RemoteMounts::Options fallback = RemoteMounts::loadOptions(group, remote);
        QCOMPARE(fallback.backend, Backend::KioFuse);
        QVERIFY(!fallback.readOnly);
        QVERIFY(fallback.location.isEmpty());
        QVERIFY(fallback.atStartup);

        RemoteMounts::saveOptions(group, QStringLiteral("other"), options);
        RemoteMounts::pruneOptions(group, {remote.id});
        QCOMPARE(group.groupList(), QStringList{remote.id});
    }

    void mountsThroughKioFuse()
    {
        RemoteMountManager manager;
        QUrl requested;
        manager.setKioFuseMount([&requested](const QUrl &url, QObject *, const RemoteMountManager::KioFuseReply &reply) {
            requested = url;
            reply(QStringLiteral("/run/user/1000/kio-fuse-x/sftp/example.org"), {});
        });
        QSignalSpy mounted(&manager, &RemoteMountManager::mounted);
        QSignalSpy unmounted(&manager, &RemoteMountManager::unmounted);
        const RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        manager.mount(remote, {});
        QCOMPARE(mounted.size(), 1);
        QCOMPARE(requested, RemoteProfiles::url(remote));
        QVERIFY(requested.password().isEmpty());
        QVERIFY(manager.isMounted(remote.id));
        QCOMPARE(manager.path(remote.id), QStringLiteral("/run/user/1000/kio-fuse-x/sftp/example.org"));
        manager.unmount(remote.id);
        QCOMPARE(unmounted.size(), 1);
        QVERIFY(manager.mountedIds().isEmpty());

        QSignalSpy failed(&manager, &RemoteMountManager::failed);
        manager.setKioFuseMount([](const QUrl &, QObject *, const RemoteMountManager::KioFuseReply &reply) {
            reply({}, QStringLiteral("Access denied"));
        });
        manager.mount(remote, {});
        QCOMPARE(failed.size(), 1);
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("Access denied")));
        QVERIFY(!manager.isStarting(remote.id));
    }

    void mountsAndUnmountsWithSshfs()
    {
        RemoteMountManager manager;
        manager.setMountCheck([this](const QString &) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy mounted(&manager, &RemoteMountManager::mounted);
        QSignalSpy unmounted(&manager, &RemoteMountManager::unmounted);
        RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        remote.user = QStringLiteral("me");
        RemoteMounts::Options options;
        options.backend = Backend::Sshfs;
        options.readOnly = true;
        manager.mount(remote, options);
        QVERIFY(manager.isStarting(remote.id));
        QVERIFY(mounted.wait(5000));
        const QString path = RemoteMounts::mountPoint(remote, options);
        QCOMPARE(mounted.first().at(1).toString(), path);
        QCOMPARE(QFile::permissions(RemoteMounts::mountRoot()) & (QFileDevice::ReadOther | QFileDevice::ReadGroup), QFileDevice::Permissions());
        QCOMPARE(readArguments(), RemoteMounts::sshfsArguments(remote, path, true, RemoteMounts::findAskPass().isEmpty()));

        manager.unmount(remote.id);
        QVERIFY(!manager.isMounted(remote.id));
        QVERIFY(unmounted.wait(5000));
        QVERIFY(!QFileInfo::exists(path));
    }

    void reportsSshfsFailures()
    {
        qputenv("FAKE_FAIL", "1");
        RemoteMountManager manager;
        manager.setMountCheck([](const QString &) {
            return false;
        });
        QSignalSpy failed(&manager, &RemoteMountManager::failed);
        RemoteMounts::Options options;
        options.backend = Backend::Sshfs;
        const RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        manager.mount(remote, options);
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(1).toString(), QStringLiteral("read: Connection reset by peer"));
        QVERIFY(!QFileInfo::exists(RemoteMounts::mountPoint(remote, options)));
    }

    void refusesFoldersThatArentEmpty()
    {
        QTemporaryDir target;
        QFile file(target.filePath(QStringLiteral("note.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        RemoteMountManager manager;
        manager.setMountCheck([](const QString &) {
            return false;
        });
        QSignalSpy failed(&manager, &RemoteMountManager::failed);
        RemoteMounts::Options options;
        options.backend = Backend::Sshfs;
        options.location = target.path();
        manager.mount(profile(Protocol::Sftp), options);
        QCOMPARE(failed.size(), 1);
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("empty folder")));
        QVERIFY(QFile::exists(file.fileName()));
    }

    void requiresUserFstabEntries()
    {
        QTemporaryDir dir;
        RemoteMountManager manager;
        manager.setFstabPath(dir.filePath(QStringLiteral("fstab")));
        QSignalSpy failed(&manager, &RemoteMountManager::failed);
        RemoteMounts::Options options;
        options.backend = Backend::Cifs;
        manager.mount(profile(Protocol::Smb), options);
        QCOMPARE(failed.size(), 1);
        const QString error = failed.first().at(1).toString();
        QVERIFY(error.contains(QStringLiteral("needs a user-mountable fstab entry")));
        QVERIFY(error.contains(QStringLiteral("//example.org/share ")));
        QVERIFY(error.contains(QStringLiteral(" cifs user,noauto 0 0")));
    }

    void mountsAndUnmountsFstabEntries()
    {
        QTemporaryDir dir;
        const RemoteProfiles::Profile remote = profile(Protocol::WebDavs);
        RemoteMounts::Options options;
        options.backend = Backend::Davfs2;
        options.location = dir.filePath(QStringLiteral("dav"));
        QVERIFY(QDir().mkpath(options.location));
        QFile fstab(dir.filePath(QStringLiteral("fstab")));
        QVERIFY(fstab.open(QIODevice::WriteOnly));
        fstab.write(RemoteMounts::fstabExample(remote, Backend::Davfs2, options.location, false).toLocal8Bit() + '\n');
        fstab.close();

        RemoteMountManager manager;
        manager.setFstabPath(fstab.fileName());
        manager.setMountCheck([this](const QString &) {
            return QFile::exists(m_marker);
        });
        QSignalSpy mounted(&manager, &RemoteMountManager::mounted);
        QSignalSpy unmounted(&manager, &RemoteMountManager::unmounted);
        QSignalSpy failed(&manager, &RemoteMountManager::failed);
        manager.mount(remote, options);
        QVERIFY(mounted.wait(5000));
        QCOMPARE(readArguments(), (QStringList{QStringLiteral("mount"), options.location}));

        qputenv("FAKE_BUSY", "1");
        manager.unmount(remote.id);
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("is busy")));
        QVERIFY(manager.isMounted(remote.id));

        qunsetenv("FAKE_BUSY");
        manager.unmount(remote.id);
        QVERIFY(unmounted.wait(5000));
        QCOMPARE(readArguments(), (QStringList{QStringLiteral("umount"), options.location}));
        // A chosen folder belongs to the user and stays.
        QVERIFY(QFileInfo(options.location).isDir());

        qputenv("FAKE_FAIL", "1");
        failed.clear();
        manager.mount(remote, options);
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(1).toString(), QStringLiteral("mount: only root can do that"));
    }

    void stopsMountsAtQuit()
    {
        RemoteMountManager manager;
        manager.setMountCheck([this](const QString &) {
            return QFile::exists(m_arguments);
        });
        QSignalSpy mounted(&manager, &RemoteMountManager::mounted);
        QSignalSpy unmounted(&manager, &RemoteMountManager::unmounted);
        RemoteMounts::Options options;
        options.backend = Backend::Sshfs;
        const RemoteProfiles::Profile remote = profile(Protocol::Sftp);
        manager.mount(remote, options);
        QVERIFY(mounted.wait(5000));
        manager.stopAllNow();
        QVERIFY(manager.mountedIds().isEmpty());
        QVERIFY(!QFileInfo::exists(RemoteMounts::mountPoint(remote, options)));
        QTest::qWait(100);
        QCOMPARE(unmounted.size(), 0);
    }

private:
    static RemoteProfiles::Profile profile(Protocol protocol)
    {
        RemoteProfiles::Profile remote;
        remote.id = QStringLiteral("0b6c4f39-5d0e-4a0c-9b8e-2f7a8a0c1d11");
        remote.name = QStringLiteral("Server");
        remote.protocol = protocol;
        remote.host = QStringLiteral("example.org");
        return remote;
    }

    QStringList readArguments() const
    {
        QFile file(m_arguments);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    void writeFakeTools()
    {
        writeScript(QStringLiteral("sshfs"),
                    "if [ -n \"$FAKE_FAIL\" ]; then echo 'read: Connection reset by peer' >&2; exit 1; fi\n"
                    "printf '%s\\n' \"$@\" > \"$FAKE_ARGS\"\n"
                    "exec /bin/sleep 30\n");
        writeScript(QStringLiteral("fusermount3"), "exit 0\n");
        writeScript(QStringLiteral("mount.davfs"), "exit 0\n");
        writeScript(QStringLiteral("mount.cifs"), "exit 0\n");
        writeScript(QStringLiteral("mount"),
                    "if [ -n \"$FAKE_FAIL\" ]; then echo 'mount: only root can do that' >&2; exit 1; fi\n"
                    "printf 'mount\\n%s\\n' \"$@\" > \"$FAKE_ARGS\"\n"
                    ": > \"$FAKE_MARKER\"\n");
        writeScript(QStringLiteral("umount"),
                    "if [ -n \"$FAKE_BUSY\" ]; then echo \"umount: $1: target is busy.\" >&2; exit 32; fi\n"
                    "printf 'umount\\n%s\\n' \"$@\" > \"$FAKE_ARGS\"\n"
                    "rm -f \"$FAKE_MARKER\"\n");
        qputenv("PATH", QByteArray(m_bin.filePath(QStringLiteral("tools")).toLocal8Bit() + ':' + m_path));
    }

    void writeScript(const QString &name, const QByteArray &body)
    {
        QVERIFY(QDir(m_bin.path()).mkpath(QStringLiteral("tools")));
        QFile file(m_bin.filePath(QStringLiteral("tools/") + name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("#!/bin/sh\n" + body);
        file.close();
        QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
    }

    QTemporaryDir m_bin;
    QTemporaryDir m_runtime;
    QByteArray m_path;
    QString m_arguments;
    QString m_marker;
};

QTEST_GUILESS_MAIN(RemoteMountsTest)

#include "remotemountstest.moc"
