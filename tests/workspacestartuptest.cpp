// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacestartup.h"

#include <KConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace WorkspaceCatalog;

class WorkspaceStartupTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void resolvesLastUsedNoneAndChosenWorkspaces()
    {
        Catalog catalog;
        QString error;
        QVERIFY2(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error), qPrintable(error));
        QVERIFY2(create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error), qPrintable(error));
        const QString work = catalog.workspaces.at(0).id;
        const QString home = catalog.workspaces.at(1).id;
        QVERIFY2(activate(catalog, work, error), qPrintable(error));

        QString warning = QStringLiteral("stale");
        QCOMPARE(WorkspaceStartup::resolve(catalog, QString(), &warning), work);
        QVERIFY(warning.isEmpty());
        QCOMPARE(WorkspaceStartup::resolve(catalog, QLatin1String(WorkspaceStartup::NoWorkspace), &warning), QString());
        QVERIFY(warning.isEmpty());
        QCOMPARE(WorkspaceStartup::resolve(catalog, home, &warning), home);
        QVERIFY(warning.isEmpty());

        const QString missing = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QCOMPARE(WorkspaceStartup::resolve(catalog, missing, &warning), work);
        QVERIFY(!warning.isEmpty());
    }

    void appliesOnlyTheActiveWorkspace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("opusrc"));
        Catalog catalog;
        QString error;
        QVERIFY2(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error), qPrintable(error));
        QVERIFY2(create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error), qPrintable(error));
        const QString work = catalog.workspaces.at(0).id;
        const QString home = catalog.workspaces.at(1).id;
        QVERIFY2(activate(catalog, work, error), qPrintable(error));
        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(save(config.group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
            QVERIFY(config.sync());
        }

        const auto stored = [&path] {
            KConfig config(path, KConfig::SimpleConfig);
            Catalog loaded;
            QString loadError;
            [&] {
                QVERIFY2(load(config.group(QStringLiteral("Workspaces")), loaded, loadError), qPrintable(loadError));
            }();
            return loaded;
        };

        QString warning;
        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(WorkspaceStartup::apply(config.group(QStringLiteral("Workspaces")), home, &warning), qPrintable(warning));
        }
        Catalog expected = catalog;
        expected.activeId = home;
        QCOMPARE(stored(), expected);

        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(WorkspaceStartup::apply(config.group(QStringLiteral("Workspaces")), QLatin1String(WorkspaceStartup::NoWorkspace), &warning),
                     qPrintable(warning));
        }
        expected.activeId.clear();
        QCOMPARE(stored(), expected);

        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY(WorkspaceStartup::apply(config.group(QStringLiteral("Workspaces")), QUuid::createUuid().toString(QUuid::WithoutBraces), &warning));
            QVERIFY(!warning.isEmpty());
        }
        QCOMPARE(stored(), expected);

        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(WorkspaceStartup::apply(config.group(QStringLiteral("Workspaces")), QString(), &warning), qPrintable(warning));
            QVERIFY(warning.isEmpty());
        }
        QCOMPARE(stored(), expected);
    }

    void reportsAnInvalidCatalog()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("opusrc")), KConfig::SimpleConfig);
        config.group(QStringLiteral("Workspaces")).writeEntry("Catalog", QStringLiteral("{not json"));
        QVERIFY(config.sync());
        QString warning;
        QVERIFY(!WorkspaceStartup::apply(config.group(QStringLiteral("Workspaces")), QString(), &warning));
        QVERIFY(!warning.isEmpty());
    }
};

QTEST_GUILESS_MAIN(WorkspaceStartupTest)

#include "workspacestartuptest.moc"
