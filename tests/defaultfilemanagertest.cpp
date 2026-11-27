// SPDX-License-Identifier: GPL-3.0-or-later

#include "defaultfilemanager.h"
#include "appidentity.h"

#include <KConfig>
#include <KConfigGroup>
#include <KService>
#include <KSycoca>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

class DefaultFileManagerTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString m_applications;
    QString m_configHome;

    QString file() const
    {
        return DefaultFileManager::activationFile(m_root.filePath(QStringLiteral("data")));
    }

    QString executable(const QString &name = QStringLiteral("bin")) const
    {
        return m_root.filePath(name + QStringLiteral("/opus-file-manager"));
    }

    static void write(const QString &path, const QString &content, bool runnable = false)
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content.toUtf8());
        file.close();
        if (runnable) {
            QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
        }
    }

    static QString read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll());
    }

    void installApplication(const QString &desktopId)
    {
        write(m_applications + QLatin1Char('/') + desktopId,
              QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=true %u\nMimeType=inode/directory;\n").arg(desktopId));
        KSycoca::self()->ensureCacheValid();
    }

    QString opusId() const
    {
        return AppIdentity::desktopFileName() + QStringLiteral(".desktop");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        ksycoca_ms_between_checks = 0;
        QVERIFY(m_root.isValid());
        m_applications = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
        m_configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    }

    void init()
    {
        QDir(m_applications).removeRecursively();
        QVERIFY(QDir().mkpath(m_applications));
        QVERIFY(QDir().mkpath(m_configHome));
        QFile::remove(m_configHome + QStringLiteral("/mimeapps.list"));
        QFile::remove(m_configHome + QStringLiteral("/kde-mimeapps.list"));
        QDir(m_root.filePath(QStringLiteral("data"))).removeRecursively();
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        KSycoca::self()->ensureCacheValid();
    }

    void recognizesPlasmaOpus()
    {
        QVERIFY(DefaultFileManager::isPlasmaOpus(AppIdentity::desktopFileName()));
        QVERIFY(DefaultFileManager::isPlasmaOpus(opusId()));
        QVERIFY(!DefaultFileManager::isPlasmaOpus(QStringLiteral("org.kde.dolphin")));
        QVERIFY(!DefaultFileManager::isPlasmaOpus(QString()));
    }

    void findsADesktopSpecificOverride()
    {
        const QString list = m_root.filePath(QStringLiteral("config/kde-mimeapps.list"));
        QVERIFY(DefaultFileManager::overridingList(m_root.filePath(QStringLiteral("config")), QStringLiteral("KDE"), opusId()).isEmpty());

        write(list, QStringLiteral("[Default Applications]\ninode/directory=org.kde.dolphin.desktop;\n"));
        QCOMPARE(DefaultFileManager::overridingList(m_root.filePath(QStringLiteral("config")), QStringLiteral("KDE"), opusId()), list);
        // Desktops are matched case-insensitively, as KService does.
        QCOMPARE(DefaultFileManager::overridingList(m_root.filePath(QStringLiteral("config")), QStringLiteral("GNOME:kde"), opusId()), list);
        QVERIFY(DefaultFileManager::overridingList(m_root.filePath(QStringLiteral("config")), QStringLiteral("GNOME"), opusId()).isEmpty());

        write(list, QStringLiteral("[Default Applications]\ninode/directory=%1;\n").arg(opusId()));
        QVERIFY(DefaultFileManager::overridingList(m_root.filePath(QStringLiteral("config")), QStringLiteral("KDE"), opusId()).isEmpty());
    }

    void setsPlasmaOpusAsTheDefault()
    {
        installApplication(QStringLiteral("org.kde.dolphin.desktop"));
        installApplication(opusId());
        write(m_configHome + QStringLiteral("/mimeapps.list"), QStringLiteral("[Default Applications]\ninode/directory=org.kde.dolphin.desktop;\n"));
        KSycoca::self()->ensureCacheValid();
        QCOMPARE(DefaultFileManager::folderHandler().desktopName, QStringLiteral("org.kde.dolphin"));

        QCOMPARE(DefaultFileManager::setPlasmaOpusAsDefault(), QString());
        const KConfig list(m_configHome + QStringLiteral("/mimeapps.list"), KConfig::SimpleConfig);
        QCOMPARE(list.group(QStringLiteral("Default Applications")).readXdgListEntry(QStringLiteral("inode/directory")), QStringList{opusId()});
        QTRY_VERIFY(DefaultFileManager::isPlasmaOpus(DefaultFileManager::folderHandler().desktopName));
    }

    void reportsADesktopSpecificOverride()
    {
        installApplication(QStringLiteral("org.kde.dolphin.desktop"));
        installApplication(opusId());
        write(m_configHome + QStringLiteral("/kde-mimeapps.list"), QStringLiteral("[Default Applications]\ninode/directory=org.kde.dolphin.desktop;\n"));

        const QString error = DefaultFileManager::setPlasmaOpusAsDefault();
        QVERIFY(error.contains(QStringLiteral("kde-mimeapps.list")));
    }

    void refusesWhenPlasmaOpusIsNotInstalled()
    {
        if (KService::serviceByDesktopName(AppIdentity::desktopFileName())) {
            QSKIP("Plasma Opus is installed on this system.");
        }
        QVERIFY(!DefaultFileManager::setPlasmaOpusAsDefault().isEmpty());
        QVERIFY(!QFile::exists(m_configHome + QStringLiteral("/mimeapps.list")));
    }

    void writesAnActivationFileThatStartsPlasmaOpus()
    {
        const QString content = DefaultFileManager::activationFileContent(QStringLiteral("/opt/opus/bin/opus-file-manager"));
        QCOMPARE(content, QStringLiteral("[D-BUS Service]\nName=org.freedesktop.FileManager1\nExec=/opt/opus/bin/opus-file-manager --file-manager-service\n"));
        QVERIFY(DefaultFileManager::activationFileContent(QStringLiteral("/my apps/opus-file-manager"))
                    .contains(QStringLiteral("Exec='/my apps/opus-file-manager'")));
        QVERIFY(file().endsWith(QStringLiteral("/dbus-1/services/org.kde.opusfilemanager.FileManager1.service")));
    }

    void reportsTheActivationState()
    {
        write(executable(), QStringLiteral("#!/bin/sh\n"), true);
        write(executable(QStringLiteral("other")), QStringLiteral("#!/bin/sh\n"), true);
        QCOMPARE(DefaultFileManager::activationState(file(), executable()), DefaultFileManager::Activation::Missing);

        QCOMPARE(DefaultFileManager::installActivationFile(file(), executable()), QString());
        QString target;
        QCOMPARE(DefaultFileManager::activationState(file(), executable(), &target), DefaultFileManager::Activation::Installed);
        QCOMPARE(target, executable());

        QCOMPARE(DefaultFileManager::activationState(file(), executable(QStringLiteral("other"))), DefaultFileManager::Activation::Elsewhere);

        // The file install.sh writes.
        write(file(), DefaultFileManager::activationFileContent(executable(QStringLiteral("gone"))));
        QCOMPARE(DefaultFileManager::activationState(file(), executable()), DefaultFileManager::Activation::Broken);

        write(file(), QStringLiteral("[D-BUS Service]\nName=org.freedesktop.FileManager1\nExec=/usr/bin/dolphin --daemon\n"));
        QCOMPARE(DefaultFileManager::activationState(file(), executable()), DefaultFileManager::Activation::Foreign);
        write(file(), QStringLiteral("[D-BUS Service]\nName=org.example.Other\nExec=%1 --file-manager-service\n").arg(executable()));
        QCOMPARE(DefaultFileManager::activationState(file(), executable()), DefaultFileManager::Activation::Foreign);
    }

    void replacesOnlyItsOwnActivationFile()
    {
        write(executable(), QStringLiteral("#!/bin/sh\n"), true);
        QVERIFY(!DefaultFileManager::installActivationFile(file(), executable(QStringLiteral("gone"))).isEmpty());
        QVERIFY(!QFile::exists(file()));

        write(file(), DefaultFileManager::activationFileContent(executable(QStringLiteral("gone"))));
        QCOMPARE(DefaultFileManager::installActivationFile(file(), executable()), QString());
        QCOMPARE(read(file()), DefaultFileManager::activationFileContent(executable()));

        const QString foreign = QStringLiteral("[D-BUS Service]\nName=org.freedesktop.FileManager1\nExec=/usr/bin/dolphin --daemon\n");
        write(file(), foreign);
        QVERIFY(!DefaultFileManager::installActivationFile(file(), executable()).isEmpty());
        QCOMPARE(read(file()), foreign);
    }

    void removesOnlyItsOwnActivationFile()
    {
        write(executable(), QStringLiteral("#!/bin/sh\n"), true);
        QCOMPARE(DefaultFileManager::removeActivationFile(file()), QString());

        QCOMPARE(DefaultFileManager::installActivationFile(file(), executable()), QString());
        QCOMPARE(DefaultFileManager::removeActivationFile(file()), QString());
        QVERIFY(!QFile::exists(file()));

        write(file(), DefaultFileManager::activationFileContent(executable(QStringLiteral("gone"))));
        QCOMPARE(DefaultFileManager::removeActivationFile(file()), QString());
        QVERIFY(!QFile::exists(file()));

        write(file(), QStringLiteral("[D-BUS Service]\nName=org.freedesktop.FileManager1\nExec=/usr/bin/dolphin --daemon\n"));
        QVERIFY(!DefaultFileManager::removeActivationFile(file()).isEmpty());
        QVERIFY(QFile::exists(file()));
    }
};

QTEST_GUILESS_MAIN(DefaultFileManagerTest)

#include "defaultfilemanagertest.moc"
