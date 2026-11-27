// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "savedsearches.h"
#include "searchcollection.h"

#include <KSharedConfig>
#include <QDialog>

class KFilePlacesModel;
class QLabel;
class QListWidget;

class SearchStoreDialog : public QDialog
{
public:
    /// Saved searches are shared into @p places, or into a model of the user's places when it is null.
    explicit SearchStoreDialog(QWidget *parent = nullptr, KSharedConfig::Ptr config = {}, KFilePlacesModel *places = nullptr);

private:
    void reloadSearches(const QString &selected = {});
    void reloadCollections(const QString &selected = {});
    void showSearch();
    void showCollection();
    void renameSearch();
    void deleteSearch();
    void shareSearch();
    void renameCollection();
    void deleteCollection();
    void removeReferences();

    KSharedConfig::Ptr m_config;
    KFilePlacesModel *m_places;
    QList<SavedSearches::Search> m_searches;
    QList<SearchCollection::SavedCollection> m_collections;
    QListWidget *m_searchList;
    QListWidget *m_collectionList;
    QListWidget *m_references;
    QLabel *m_searchDetails;
    QLabel *m_status;
};
