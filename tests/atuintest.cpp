// SPDX-License-Identifier: GPL-3.0-or-later

#include "atuin.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

class AtuinTest : public QObject
{
    Q_OBJECT

private:
    static bool writeFile(const QString &path, const QByteArray &contents, bool executable = false)
    {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            return false;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
            return false;
        }
        file.close();
        return !executable || file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_home.isValid());
        m_saved.clear();
        for (const char *name : {"HOME", "PATH", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "ATUIN_CONFIG_DIR", "ATUIN_DATA_DIR", "ATUIN_DB_PATH"}) {
            m_saved.append({name, qEnvironmentVariableIsSet(name) ? std::optional<QByteArray>(qgetenv(name)) : std::nullopt});
            qunsetenv(name);
        }
        qputenv("HOME", m_home.path().toLocal8Bit());
        qputenv("PATH", m_home.filePath(QStringLiteral("bin")).toLocal8Bit());
    }

    void cleanup()
    {
        for (const auto &[name, value] : std::as_const(m_saved)) {
            if (value) {
                qputenv(name, *value);
            } else {
                qunsetenv(name);
            }
        }
        QDir(m_home.path()).removeRecursively();
        QDir().mkpath(m_home.path());
    }

    void findsTheExecutable()
    {
        QVERIFY(Atuin::executable().isEmpty());
        QVERIFY(Atuin::unavailableReason().contains(QStringLiteral("not installed")));
        const QString installer = m_home.filePath(QStringLiteral(".atuin/bin/atuin"));
        QVERIFY(writeFile(installer, "#!/bin/sh\n", true));
        QCOMPARE(Atuin::executable(), installer);
        const QString inPath = m_home.filePath(QStringLiteral("bin/atuin"));
        QVERIFY(writeFile(inPath, "#!/bin/sh\n", true));
        QCOMPARE(Atuin::executable(), inPath);
    }

    void locatesConfigurationAndHistory()
    {
        const QString home = QDir(m_home.path()).absolutePath();
        QCOMPARE(Atuin::configFile(), home + QStringLiteral("/.config/atuin/config.toml"));
        QCOMPARE(Atuin::databasePath(), home + QStringLiteral("/.local/share/atuin/history.db"));
        qputenv("XDG_CONFIG_HOME", QByteArray("/xdg/config"));
        qputenv("XDG_DATA_HOME", QByteArray("/xdg/data"));
        QCOMPARE(Atuin::configFile(), QStringLiteral("/xdg/config/atuin/config.toml"));
        QCOMPARE(Atuin::databasePath(), QStringLiteral("/xdg/data/atuin/history.db"));
        qputenv("XDG_DATA_HOME", QByteArray("relative"));
        QCOMPARE(Atuin::databasePath(), home + QStringLiteral("/.local/share/atuin/history.db"));
        qputenv("ATUIN_DATA_DIR", QByteArray("/atuin/data"));
        QCOMPARE(Atuin::databasePath(), QStringLiteral("/atuin/data/history.db"));

        const QString configFolder = m_home.filePath(QStringLiteral("atuin-config"));
        qputenv("ATUIN_CONFIG_DIR", configFolder.toLocal8Bit());
        QCOMPARE(Atuin::configFile(), configFolder + QStringLiteral("/config.toml"));
        QVERIFY(writeFile(Atuin::configFile(), "# db_path = \"/commented\"\nauto_sync = false\n[sync]\ndb_path = \"/in/table\"\n"));
        QCOMPARE(Atuin::databasePath(), QStringLiteral("/atuin/data/history.db"));
        QVERIFY(writeFile(Atuin::configFile(), "auto_sync = false\n  db_path = \"~/history/atuin.db\"\n"));
        QCOMPARE(Atuin::databasePath(), home + QStringLiteral("/history/atuin.db"));
        QVERIFY(writeFile(Atuin::configFile(), "db_path = '/literal/atuin.db'\n"));
        QCOMPARE(Atuin::databasePath(), QStringLiteral("/literal/atuin.db"));
        qputenv("ATUIN_DB_PATH", QByteArray("/env/atuin.db"));
        QCOMPARE(Atuin::databasePath(), QStringLiteral("/env/atuin.db"));
    }

    void requiresExistingHistory()
    {
        QVERIFY(writeFile(m_home.filePath(QStringLiteral("bin/atuin")), "#!/bin/sh\n", true));
        const QString database = m_home.filePath(QStringLiteral(".local/share/atuin/history.db"));
        QVERIFY(Atuin::unavailableReason().contains(QStringLiteral("atuin init")));
        QVERIFY(Atuin::unavailableReason().contains(database));
        QVERIFY(!QFile::exists(database));
        QVERIFY(writeFile(database, QByteArray()));
        QVERIFY(Atuin::unavailableReason().isEmpty());
    }

    void buildsSearchInput()
    {
        QCOMPARE(Atuin::searchInput(QStringLiteral("/usr/bin/atuin")), QStringLiteral(" /usr/bin/atuin search --interactive\n"));
        QCOMPARE(Atuin::searchInput(QStringLiteral("/home/a b/.atuin/bin/atuin")), QStringLiteral(" '/home/a b/.atuin/bin/atuin' search --interactive\n"));
    }

    void readsTheSessionMode()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
        });
        Settings::self()->setDefaults();
        QVERIFY(Settings::atuinHistory());
        QCOMPARE(Atuin::sessionMode(), Atuin::SessionMode::Current);
        Settings::setAtuinSession(Settings::EnumAtuinSession::NewWhenBusy);
        QCOMPARE(Atuin::sessionMode(), Atuin::SessionMode::NewWhenBusy);
        Settings::setAtuinSession(Settings::EnumAtuinSession::New);
        QCOMPARE(Atuin::sessionMode(), Atuin::SessionMode::New);
    }

    void choosesTheSession_data()
    {
        QTest::addColumn<Atuin::SessionMode>("mode");
        QTest::addColumn<bool>("hasSession");
        QTest::addColumn<bool>("sessionIsNew");
        QTest::addColumn<bool>("busy");
        QTest::addColumn<Atuin::Session>("expected");
        using Mode = Atuin::SessionMode;
        using Session = Atuin::Session;
        QTest::newRow("current, none") << Mode::Current << false << false << false << Session::Refuse;
        QTest::newRow("current, idle") << Mode::Current << true << false << false << Session::Current;
        QTest::newRow("current, busy") << Mode::Current << true << false << true << Session::Refuse;
        QTest::newRow("when busy, none") << Mode::NewWhenBusy << false << false << false << Session::New;
        QTest::newRow("when busy, idle") << Mode::NewWhenBusy << true << false << false << Session::Current;
        QTest::newRow("when busy, busy") << Mode::NewWhenBusy << true << false << true << Session::New;
        QTest::newRow("when busy, new and busy") << Mode::NewWhenBusy << true << true << true << Session::Refuse;
        QTest::newRow("new, none") << Mode::New << false << false << false << Session::New;
        QTest::newRow("new, idle") << Mode::New << true << false << false << Session::New;
        QTest::newRow("new, busy") << Mode::New << true << false << true << Session::New;
        QTest::newRow("new, just opened") << Mode::New << true << true << false << Session::Current;
        QTest::newRow("new, just opened and busy") << Mode::New << true << true << true << Session::Refuse;
    }

    void choosesTheSession()
    {
        QFETCH(Atuin::SessionMode, mode);
        QFETCH(bool, hasSession);
        QFETCH(bool, sessionIsNew);
        QFETCH(bool, busy);
        QFETCH(Atuin::Session, expected);
        QCOMPARE(Atuin::session(mode, hasSession, sessionIsNew, busy), expected);
    }

private:
    QTemporaryDir m_home;
    QList<std::pair<const char *, std::optional<QByteArray>>> m_saved;
};

QTEST_GUILESS_MAIN(AtuinTest)

#include "atuintest.moc"
