// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalsetup.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class PortalSetupTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    std::unique_ptr<KConfig> m_state;

    PortalSetup::Paths paths() const
    {
        PortalSetup::Paths paths;
        paths.dataHome = m_root.filePath(QStringLiteral("data"));
        paths.configHome = m_root.filePath(QStringLiteral("config"));
        paths.configDirs = {m_root.filePath(QStringLiteral("etc/xdg")), m_root.filePath(QStringLiteral("etc"))};
        paths.dataDirs = {m_root.filePath(QStringLiteral("usr/share"))};
        paths.helperExecutable = m_root.filePath(QStringLiteral("bin/opus-file-manager-portal"));
        return paths;
    }

    static void write(const QString &path, const QString &content)
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content.toUtf8());
    }

    static QString read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll());
    }

    KConfigGroup backup()
    {
        return m_state->group(QStringLiteral("FileChooserPortal"));
    }

    static QString chooser(const QString &config)
    {
        return PortalSetup::preferredEntry(config, PortalSetup::fileChooserInterface());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
    }

    void init()
    {
        QDir(m_root.path()).removeRecursively();
        QVERIFY(QDir().mkpath(m_root.path()));
        m_state = std::make_unique<KConfig>(m_root.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        const QString helper = paths().helperExecutable;
        write(helper, QStringLiteral("#!/bin/sh\n"));
        QVERIFY(QFile::setPermissions(helper, QFile::permissions(helper) | QFile::ExeOwner));
    }

    void readsAndWritesPreferredEntries()
    {
        const QString config = QStringLiteral("# Plasma\n[preferred]\ndefault=kde\norg.freedesktop.impl.portal.Settings=kde;gtk\n\n[other]\nkey=value\n");
        QCOMPARE(PortalSetup::preferredEntry(config, QStringLiteral("default")), QStringLiteral("kde"));
        QVERIFY(chooser(config).isEmpty());
        QVERIFY(PortalSetup::preferredEntry(config, QStringLiteral("key")).isEmpty());

        const QString added = PortalSetup::setPreferredEntry(config, PortalSetup::fileChooserInterface(), QStringLiteral("opusfilemanager;kde"));
        QCOMPARE(added,
                 QStringLiteral("# Plasma\n[preferred]\ndefault=kde\norg.freedesktop.impl.portal.Settings=kde;gtk\n"
                                "org.freedesktop.impl.portal.FileChooser=opusfilemanager;kde\n\n[other]\nkey=value\n"));
        const QString replaced = PortalSetup::setPreferredEntry(added, PortalSetup::fileChooserInterface(), QStringLiteral("gtk"));
        QCOMPARE(chooser(replaced), QStringLiteral("gtk"));
        QCOMPARE(PortalSetup::setPreferredEntry(replaced, PortalSetup::fileChooserInterface(), QString()), config);

        QCOMPARE(PortalSetup::setPreferredEntry(QStringLiteral("[other]\nkey=value"), QStringLiteral("default"), QStringLiteral("kde")),
                 QStringLiteral("[other]\nkey=value\n\n[preferred]\ndefault=kde\n"));
        QCOMPARE(PortalSetup::setPreferredEntry(QString(), QStringLiteral("default"), QStringLiteral("kde")), QStringLiteral("[preferred]\ndefault=kde\n"));
    }

    void ordersThePreference()
    {
        QCOMPARE(PortalSetup::preferOpus(QStringLiteral("kde")), QStringLiteral("opusfilemanager;kde"));
        QCOMPARE(PortalSetup::preferOpus(QStringLiteral("gtk; kde;opusfilemanager;")), QStringLiteral("opusfilemanager;gtk;kde"));
        QCOMPARE(PortalSetup::preferOpus(QString()), QStringLiteral("opusfilemanager;kde"));
        QCOMPARE(PortalSetup::withoutOpus(QStringLiteral("opusfilemanager;kde")), QStringLiteral("kde"));
        QVERIFY(PortalSetup::withoutOpus(QStringLiteral("opusfilemanager")).isEmpty());
    }

    void findsTheEffectiveSystemConfig()
    {
        QVERIFY(PortalSetup::effectiveSystemConfig(paths()).isEmpty());
        write(m_root.filePath(QStringLiteral("usr/share/xdg-desktop-portal/kde-portals.conf")), QStringLiteral("data"));
        QCOMPARE(PortalSetup::effectiveSystemConfig(paths()), QStringLiteral("data"));
        write(m_root.filePath(QStringLiteral("etc/xdg-desktop-portal/portals.conf")), QStringLiteral("etc generic"));
        QCOMPARE(PortalSetup::effectiveSystemConfig(paths()), QStringLiteral("etc generic"));
        write(m_root.filePath(QStringLiteral("etc/xdg-desktop-portal/kde-portals.conf")), QStringLiteral("etc kde"));
        QCOMPARE(PortalSetup::effectiveSystemConfig(paths()), QStringLiteral("etc kde"));
        write(m_root.filePath(QStringLiteral("etc/xdg/xdg-desktop-portal/portals.conf")), QStringLiteral("xdg generic"));
        QCOMPARE(PortalSetup::effectiveSystemConfig(paths()), QStringLiteral("xdg generic"));
        write(m_root.filePath(QStringLiteral("config/xdg-desktop-portal/portals.conf")), QStringLiteral("user generic"));
        QCOMPARE(PortalSetup::effectiveSystemConfig(paths()), QStringLiteral("user generic"));
    }

    void installsFromTheSystemConfigAndRestoresIt()
    {
        const QString system = QStringLiteral("[preferred]\ndefault=kde\norg.freedesktop.impl.portal.Settings=kde;gtk\n");
        write(m_root.filePath(QStringLiteral("usr/share/xdg-desktop-portal/kde-portals.conf")), system);
        QCOMPARE(PortalSetup::state(paths()), PortalSetup::State::NotInstalled);

        KConfigGroup group = backup();
        QCOMPARE(PortalSetup::install(paths(), group), QString());
        QCOMPARE(PortalSetup::state(paths()), PortalSetup::State::Installed);
        QCOMPARE(read(paths().portalFile()), PortalSetup::portalFileContent());
        QCOMPARE(read(paths().serviceFile()), PortalSetup::serviceFileContent(paths().helperExecutable));
        QVERIFY(read(paths().serviceFile()).contains(QStringLiteral("Exec=") + paths().helperExecutable));
        const QString user = read(paths().configFile());
        QCOMPARE(chooser(user), QStringLiteral("opusfilemanager;kde"));
        QCOMPARE(PortalSetup::preferredEntry(user, QStringLiteral("default")), QStringLiteral("kde"));
        QVERIFY(user.contains(QStringLiteral("org.freedesktop.impl.portal.Settings=kde;gtk")));

        QCOMPARE(PortalSetup::uninstall(paths(), group), QString());
        QVERIFY(!QFile::exists(paths().configFile()));
        QVERIFY(!QFile::exists(paths().portalFile()));
        QVERIFY(!QFile::exists(paths().serviceFile()));
        QVERIFY(!backup().exists());
        QCOMPARE(PortalSetup::state(paths()), PortalSetup::State::NotInstalled);
    }

    void installsWithoutAnyConfig()
    {
        KConfigGroup group = backup();
        QCOMPARE(PortalSetup::install(paths(), group), QString());
        const QString user = read(paths().configFile());
        QCOMPARE(PortalSetup::preferredEntry(user, QStringLiteral("default")), QStringLiteral("kde"));
        QCOMPARE(chooser(user), QStringLiteral("opusfilemanager;kde"));
    }

    void keepsTheUsersChooserAsTheFallback()
    {
        const QString previous = QStringLiteral("# mine\n[preferred]\ndefault=gtk\norg.freedesktop.impl.portal.FileChooser=gtk;kde\n");
        write(paths().configFile(), previous);
        KConfigGroup group = backup();
        QCOMPARE(PortalSetup::install(paths(), group), QString());
        QCOMPARE(chooser(read(paths().configFile())), QStringLiteral("opusfilemanager;gtk;kde"));
        QVERIFY(read(paths().configFile()).startsWith(QStringLiteral("# mine\n")));

        // Setting up again refreshes the files but keeps the original backup.
        QCOMPARE(PortalSetup::install(paths(), group), QString());
        QCOMPARE(PortalSetup::uninstall(paths(), group), QString());
        QCOMPARE(read(paths().configFile()), previous);
    }

    void removesOnlyPlasmaOpusAfterTheUserChangedTheConfig()
    {
        write(paths().configFile(), QStringLiteral("[preferred]\ndefault=kde\n"));
        KConfigGroup group = backup();
        QCOMPARE(PortalSetup::install(paths(), group), QString());
        const QString edited = read(paths().configFile()) + QStringLiteral("\n[extra]\nkey=value\n");
        write(paths().configFile(), edited);

        QCOMPARE(PortalSetup::uninstall(paths(), group), QString());
        const QString restored = read(paths().configFile());
        QCOMPARE(chooser(restored), QStringLiteral("kde"));
        QVERIFY(restored.contains(QStringLiteral("[extra]\nkey=value")));
        QVERIFY(!QFile::exists(paths().portalFile()));
    }

    void removesPlasmaOpusWithoutABackup()
    {
        write(paths().configFile(), QStringLiteral("[preferred]\norg.freedesktop.impl.portal.FileChooser=opusfilemanager\n"));
        QCOMPARE(PortalSetup::state(paths()), PortalSetup::State::Partial);
        KConfigGroup group = backup();
        QCOMPARE(PortalSetup::uninstall(paths(), group), QString());
        QCOMPARE(read(paths().configFile()), QStringLiteral("[preferred]\n"));
        QCOMPARE(PortalSetup::state(paths()), PortalSetup::State::NotInstalled);
    }

    void reportsPartialSetup()
    {
        KConfigGroup group = backup();
        QCOMPARE(PortalSetup::install(paths(), group), QString());
        write(paths().configFile(), QStringLiteral("[preferred]\norg.freedesktop.impl.portal.FileChooser=kde\n"));
        QCOMPARE(PortalSetup::state(paths()), PortalSetup::State::Partial);
    }

    void reportsAMissingHelper()
    {
        QVERIFY(QFile::remove(paths().helperExecutable));
        KConfigGroup group = backup();
        QVERIFY(PortalSetup::install(paths(), group).contains(paths().helperExecutable));
        QVERIFY(!QFile::exists(paths().configFile()));
        QVERIFY(!QFile::exists(paths().portalFile()));
        QVERIFY(!backup().exists());
    }

    void followsTheEnvironment()
    {
        qputenv("XDG_CONFIG_DIRS", "/one:relative:/two");
        qunsetenv("XDG_DATA_DIRS");
        const auto paths = PortalSetup::Paths::fromEnvironment(QStringLiteral("/bin/helper"));
        QCOMPARE(paths.configDirs, (QStringList{QStringLiteral("/one"), QStringLiteral("/two"), QStringLiteral("/etc")}));
        QCOMPARE(paths.dataDirs, (QStringList{QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share"), QStringLiteral("/usr/share")}));
        QVERIFY(paths.portalFile().endsWith(QStringLiteral("/xdg-desktop-portal/portals/opusfilemanager.portal")));
        QVERIFY(paths.serviceFile().endsWith(QStringLiteral("/dbus-1/services/org.freedesktop.impl.portal.desktop.opusfilemanager.service")));
        QVERIFY(paths.configFile().endsWith(QStringLiteral("/xdg-desktop-portal/kde-portals.conf")));
        QVERIFY(paths.configFile().startsWith(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)));
        qunsetenv("XDG_CONFIG_DIRS");
    }
};

QTEST_GUILESS_MAIN(PortalSetupTest)

#include "portalsetuptest.moc"
