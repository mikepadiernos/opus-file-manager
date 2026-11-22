// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacecatalog.h"

#include <KConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace WorkspaceCatalog;

class WorkspaceCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void managesStableIdentities()
    {
        Catalog catalog;
        QString error;
        QVERIFY2(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error), qPrintable(error));
        QVERIFY2(create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error), qPrintable(error));
        const QString workId = catalog.workspaces.at(0).id;
        const QString homeId = catalog.workspaces.at(1).id;
        QVERIFY(!workId.isEmpty());
        QVERIFY(workId != homeId);
        QVERIFY2(activate(catalog, workId, error), qPrintable(error));
        QVERIFY2(rename(catalog, workId, QStringLiteral("Office"), QStringLiteral("O"), error), qPrintable(error));
        QCOMPARE(catalog.activeId, workId);
        QCOMPARE(catalog.workspaces.at(0).id, workId);
        QVERIFY2(remove(catalog, homeId, error), qPrintable(error));
        QCOMPARE(catalog.activeId, workId);
        QVERIFY2(remove(catalog, workId, error), qPrintable(error));
        QVERIFY(catalog.activeId.isEmpty());
        QVERIFY(catalog.workspaces.isEmpty());
    }

    void rejectsInvalidChangesWithoutMutation()
    {
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const Catalog original = catalog;
        QVERIFY(!create(catalog, QStringLiteral("  work "), QStringLiteral("X"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!rename(catalog, original.workspaces.first().id, QStringLiteral("Other"), QStringLiteral(" "), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!activate(catalog, QStringLiteral("missing"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QVERIFY(!remove(catalog, QStringLiteral("missing"), error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
    }

    void persistsCatalog()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(activate(catalog, catalog.workspaces.first().id, error));
        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(save(config.group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        }
        KConfig config(path, KConfig::SimpleConfig);
        Catalog loaded;
        QVERIFY2(load(config.group(QStringLiteral("Workspaces")), loaded, error), qPrintable(error));
        QCOMPARE(loaded, catalog);
    }

    void rejectsCorruptDataWithoutOverwriting()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        group.writeEntry("Catalog", QByteArray("{broken"));
        Catalog catalog;
        catalog.activeId = QStringLiteral("unchanged");
        const Catalog original = catalog;
        QString error;
        QVERIFY(!load(group, catalog, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
        QCOMPARE(group.readEntry("Catalog", QByteArray()), QByteArray("{broken"));
    }

    void rejectsInvalidCatalogBeforeSaving()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(save(group, catalog, error));
        const QByteArray previous = group.readEntry("Catalog", QByteArray());

        catalog.activeId = QStringLiteral("missing");
        QVERIFY(!save(group, catalog, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(group.readEntry("Catalog", QByteArray()), previous);

        group.writeEntry("Catalog", QByteArray(R"({"version":2,"workspaces":[],"activeId":""})"));
        const Catalog original = catalog;
        QVERIFY(!load(group, catalog, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);
    }
};

QTEST_GUILESS_MAIN(WorkspaceCatalogTest)

#include "workspacecatalogtest.moc"
