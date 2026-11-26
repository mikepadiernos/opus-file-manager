// SPDX-License-Identifier: GPL-3.0-or-later

#include "atuin.h"

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

private:
    QTemporaryDir m_home;
    QList<std::pair<const char *, std::optional<QByteArray>>> m_saved;
};

QTEST_GUILESS_MAIN(AtuinTest)

#include "atuintest.moc"
