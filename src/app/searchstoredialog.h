// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "savedsearches.h"
#include "searchcollection.h"

#include <KSharedConfig>
#include <QDialog>

class QLabel;
class QListWidget;

class SearchStoreDialog : public QDialog
{
public:
    explicit SearchStoreDialog(QWidget *parent = nullptr, KSharedConfig::Ptr config = {});

private:
    void reloadSearches(const QString &selected = {});
    void reloadCollections(const QString &selected = {});
    void showSearch();
    void showCollection();
    void renameSearch();
    void deleteSearch();
    void renameCollection();
    void deleteCollection();
    void removeReferences();

    KSharedConfig::Ptr m_config;
    QList<SavedSearches::Search> m_searches;
    QList<SearchCollection::SavedCollection> m_collections;
    QListWidget *m_searchList;
    QListWidget *m_collectionList;
    QListWidget *m_references;
    QLabel *m_searchDetails;
    QLabel *m_status;
};
