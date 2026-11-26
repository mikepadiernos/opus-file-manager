// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalproviders.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using TerminalProviders::Provider;

class TerminalProvidersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void describesProviders()
    {
        const QList<Provider> separate = TerminalProviders::separateProviders();
        QCOMPARE(separate, (QList<Provider>{Provider::SystemDefault, Provider::Konsole, Provider::Yakuake, Provider::Kwi}));
        QCOMPARE(TerminalProviders::embeddedProviders(), (QList<Provider>{Provider::Konsole, Provider::Kwi}));
        QSet<QString> ids;
        for (const Provider provider : separate) {
            ids.insert(TerminalProviders::id(provider));
            QVERIFY(!TerminalProviders::name(provider).isEmpty());
            QVERIFY(!TerminalProviders::iconName(provider).isEmpty());
        }
        QCOMPARE(ids.size(), separate.size());
        QVERIFY(TerminalProviders::executable(Provider::SystemDefault).isEmpty());
        QVERIFY(TerminalProviders::desktopName(Provider::SystemDefault).isEmpty());
        QCOMPARE(TerminalProviders::executable(Provider::Kwi), QStringLiteral("kwi"));
        QCOMPARE(TerminalProviders::desktopName(Provider::Konsole), QStringLiteral("org.kde.konsole"));
        QCOMPARE(TerminalProviders::desktopName(Provider::Yakuake), QStringLiteral("org.kde.yakuake"));
        QCOMPARE(TerminalProviders::desktopName(Provider::Kwi), QStringLiteral("org.kde.kwi"));
        QCOMPARE(TerminalProviders::partPluginId(Provider::Konsole), QStringLiteral("konsolepart"));
        QCOMPARE(TerminalProviders::partPluginId(Provider::Kwi), QStringLiteral("kwipart"));
        QVERIFY(TerminalProviders::partPluginId(Provider::Yakuake).isEmpty());
        QVERIFY(TerminalProviders::partPluginId(Provider::SystemDefault).isEmpty());
    }

    void buildsLaunchArguments()
    {
        const QString folder = QStringLiteral("/tmp/with space");
        QCOMPARE(TerminalProviders::launchArguments(Provider::Konsole, folder), (QStringList{QStringLiteral("--workdir"), folder}));
        QCOMPARE(TerminalProviders::launchArguments(Provider::Kwi, folder), (QStringList{QStringLiteral("--workdir"), folder}));
        QVERIFY(TerminalProviders::launchArguments(Provider::Yakuake, folder).isEmpty());
        QVERIFY(TerminalProviders::launchArguments(Provider::SystemDefault, folder).isEmpty());
    }

    void quotesYakuakeCommands()
    {
        QCOMPARE(TerminalProviders::yakuakeService(), QStringLiteral("org.kde.yakuake"));
        QCOMPARE(TerminalProviders::yakuakeChangeDirectoryCommand(QStringLiteral("/tmp/it's here")), QStringLiteral(" cd -- '/tmp/it'\\''s here'"));
        QCOMPARE(TerminalProviders::yakuakeChangeDirectoryCommand(QStringLiteral("/tmp/-x")), QStringLiteral(" cd -- /tmp/-x"));
        QCOMPARE(TerminalProviders::firstYakuakeTerminalId(QStringLiteral("4,5")), 4);
        QCOMPARE(TerminalProviders::firstYakuakeTerminalId(QStringLiteral("0")), 0);
        QCOMPARE(TerminalProviders::firstYakuakeTerminalId(QString()), -1);
        QCOMPARE(TerminalProviders::firstYakuakeTerminalId(QStringLiteral("-1")), -1);
        QCOMPARE(TerminalProviders::firstYakuakeTerminalId(QStringLiteral("x")), -1);
    }

    void acceptsOnlyExistingLocalFolders()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QCOMPARE(TerminalProviders::localDirectory(QUrl::fromLocalFile(folder.path() + QStringLiteral("/./"))), QDir::cleanPath(folder.path()));
        QFile file(folder.filePath(QStringLiteral("file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(TerminalProviders::localDirectory(QUrl::fromLocalFile(file.fileName())).isEmpty());
        QVERIFY(TerminalProviders::localDirectory(QUrl::fromLocalFile(folder.filePath(QStringLiteral("missing")))).isEmpty());
        QVERIFY(TerminalProviders::localDirectory(QUrl(QStringLiteral("sftp://example.invalid/tmp"))).isEmpty());
        QVERIFY(TerminalProviders::localDirectory(QUrl()).isEmpty());
    }

    void reportsMissingProviders()
    {
        QTemporaryDir bin;
        QVERIFY(bin.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
        });
        qputenv("PATH", bin.path().toLocal8Bit());
        QVERIFY(TerminalProviders::isInstalled(Provider::SystemDefault));
        QVERIFY(TerminalProviders::unavailableReason(Provider::SystemDefault).isEmpty());
        for (const Provider provider : {Provider::Konsole, Provider::Yakuake, Provider::Kwi}) {
            QVERIFY(!TerminalProviders::isInstalled(provider));
            QVERIFY(TerminalProviders::unavailableReason(provider).contains(TerminalProviders::name(provider)));
        }
        QFile kwi(bin.filePath(QStringLiteral("kwi")));
        QVERIFY(kwi.open(QIODevice::WriteOnly));
        kwi.write("#!/bin/sh\n");
        kwi.close();
        QVERIFY(kwi.setPermissions(kwi.permissions() | QFileDevice::ExeOwner));
        QVERIFY(TerminalProviders::isInstalled(Provider::Kwi));
        QVERIFY(TerminalProviders::unavailableReason(Provider::Kwi).isEmpty());
    }

    void reportsMissingComponents()
    {
        QVERIFY(!TerminalProviders::isPartInstalled(Provider::Yakuake));
        QVERIFY(TerminalProviders::partUnavailableReason(Provider::Yakuake).contains(QStringLiteral("cannot be embedded")));
        const QStringList paths = QCoreApplication::libraryPaths();
        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        QCoreApplication::setLibraryPaths({empty.path()});
        const auto restore = qScopeGuard([&paths] {
            QCoreApplication::setLibraryPaths(paths);
        });
        for (const Provider provider : TerminalProviders::embeddedProviders()) {
            QVERIFY(!TerminalProviders::isPartInstalled(provider));
            QVERIFY(TerminalProviders::partUnavailableReason(provider).contains(QStringLiteral("not installed")));
        }
    }
};

QTEST_GUILESS_MAIN(TerminalProvidersTest)

#include "terminalproviderstest.moc"
