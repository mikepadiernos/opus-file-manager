// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacelocations.h"
#include "favoritesmodel.h"

#include <KConfig>
#include <KFilePlacesModel>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>

class WorkspaceLocationsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void presentsAndFiltersSavedLocations()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        FavoritesModel favorites(config.group(QStringLiteral("Favorites")));
        KFilePlacesModel places;
        const QUrl favorite = QUrl::fromLocalFile(directory.filePath(QStringLiteral("favorite")));
        const QUrl place = QUrl::fromLocalFile(directory.filePath(QStringLiteral("place")));
        const QUrl unassigned = QUrl::fromLocalFile(directory.filePath(QStringLiteral("unassigned")));
        QVERIFY(favorites.add(favorite));
        QVERIFY(favorites.add(unassigned));
        places.addPlace(QStringLiteral("Workspace test"), place);
        const auto cleanup = qScopeGuard([&] {
            for (int row = places.rowCount() - 1; row >= 0; --row) {
                if (places.url(places.index(row, 0)) == place) {
                    places.removePlace(places.index(row, 0));
                }
            }
        });
        WorkspaceLocations locations(&favorites, &places);
        const auto entries = locations.locations();
        for (const QUrl &url : {favorite, place, unassigned}) {
            QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [&url](const auto &entry) {
                return entry.id == WorkspaceCatalog::locationId(url);
            }));
        }
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString work = catalog.workspaces.first().id;
        const QString home = catalog.workspaces.last().id;
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, WorkspaceCatalog::locationId(favorite), work, error));
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, WorkspaceCatalog::locationId(place), home, error));
        QVERIFY(locations.isVisible(favorite, catalog, work));
        QVERIFY(!locations.isVisible(place, catalog, work));
        QVERIFY(locations.isVisible(unassigned, catalog, work));
        QVERIFY(locations.isVisible(place, catalog, QString()));
        QCOMPARE(favorites.urls(), QList<QUrl>({favorite, unassigned}));
    }

    void refusesToHideAmbiguousLocations()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        FavoritesModel favorites(config.group(QStringLiteral("Favorites")));
        KFilePlacesModel places;
        const QUrl alice(QStringLiteral("sftp://alice:secret@example.invalid/folder"));
        const QUrl bob(QStringLiteral("sftp://bob:other@example.invalid/folder"));
        QVERIFY(favorites.add(alice));
        QVERIFY(favorites.add(bob));
        WorkspaceLocations locations(&favorites, &places);
        const auto entries = locations.locations();
        const auto entry = std::find_if(entries.cbegin(), entries.cend(), [&alice](const auto &location) {
            return location.id == WorkspaceCatalog::locationId(alice);
        });
        QVERIFY(entry != entries.cend());
        QVERIFY(entry->ambiguous);
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, WorkspaceCatalog::locationId(alice), catalog.workspaces.first().id, error));
        QVERIFY(locations.isVisible(alice, catalog, catalog.workspaces.last().id));
        QVERIFY(locations.isVisible(bob, catalog, catalog.workspaces.last().id));
    }
};

QTEST_MAIN(WorkspaceLocationsTest)

#include "workspacelocationstest.moc"
