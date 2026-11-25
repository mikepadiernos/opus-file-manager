// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchstoredialog.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
template<typename T>
auto findNamed(QList<T> &items, const QString &name)
{
    return std::find_if(items.begin(), items.end(), [&name](const T &item) {
        return item.name == name;
    });
}
}

SearchStoreDialog::SearchStoreDialog(QWidget *parent, KSharedConfig::Ptr config)
    : QDialog(parent)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_searchList(new QListWidget(this))
    , m_collectionList(new QListWidget(this))
    , m_references(new QListWidget(this))
    , m_searchDetails(new QLabel(this))
    , m_status(new QLabel(this))
{
    setObjectName(QStringLiteral("searchStoreDialog"));
    setWindowTitle(i18nc("@title:window", "Manage Saved Searches and Collections"));
    resize(650, 500);
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(
        i18nc("@info", "Changes are saved immediately, even if Preferences is canceled. Removing searches, collections or references never removes files."),
        this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *tabs = new QTabWidget(this);
    auto *searchPage = new QWidget(tabs);
    auto *searchLayout = new QVBoxLayout(searchPage);
    m_searchList->setObjectName(QStringLiteral("managedSearches"));
    searchLayout->addWidget(m_searchList);
    m_searchDetails->setObjectName(QStringLiteral("managedSearchDetails"));
    m_searchDetails->setTextFormat(Qt::PlainText);
    m_searchDetails->setWordWrap(true);
    searchLayout->addWidget(m_searchDetails);
    auto *searchHint = new QLabel(i18nc("@info", "Load and update search criteria in the search panel."), searchPage);
    searchHint->setWordWrap(true);
    searchLayout->addWidget(searchHint);
    auto *searchActions = new QHBoxLayout;
    auto *searchRename = new QPushButton(i18nc("@action:button", "Rename"), searchPage);
    searchRename->setObjectName(QStringLiteral("managedSearchRename"));
    auto *searchDelete = new QPushButton(i18nc("@action:button", "Delete"), searchPage);
    searchDelete->setObjectName(QStringLiteral("managedSearchDelete"));
    auto *searchRefresh = new QPushButton(i18nc("@action:button", "Refresh"), searchPage);
    searchRefresh->setObjectName(QStringLiteral("managedSearchRefresh"));
    searchActions->addWidget(searchRename);
    searchActions->addWidget(searchDelete);
    searchActions->addWidget(searchRefresh);
    searchActions->addStretch();
    searchLayout->addLayout(searchActions);
    tabs->addTab(searchPage, i18nc("@title:tab", "Saved Searches"));
    auto *collectionPage = new QWidget(tabs);
    auto *collectionLayout = new QVBoxLayout(collectionPage);
    m_collectionList->setObjectName(QStringLiteral("managedCollections"));
    collectionLayout->addWidget(m_collectionList);
    collectionLayout->addWidget(new QLabel(i18nc("@label", "References:"), collectionPage));
    m_references->setObjectName(QStringLiteral("managedCollectionReferences"));
    m_references->setSelectionMode(QAbstractItemView::ExtendedSelection);
    collectionLayout->addWidget(m_references);
    auto *collectionActions = new QHBoxLayout;
    auto *collectionRename = new QPushButton(i18nc("@action:button", "Rename"), collectionPage);
    collectionRename->setObjectName(QStringLiteral("managedCollectionRename"));
    auto *collectionDelete = new QPushButton(i18nc("@action:button", "Delete"), collectionPage);
    collectionDelete->setObjectName(QStringLiteral("managedCollectionDelete"));
    auto *remove = new QPushButton(i18nc("@action:button", "Remove References"), collectionPage);
    remove->setObjectName(QStringLiteral("managedCollectionRemove"));
    auto *collectionRefresh = new QPushButton(i18nc("@action:button", "Refresh"), collectionPage);
    collectionRefresh->setObjectName(QStringLiteral("managedCollectionRefresh"));
    collectionActions->addWidget(collectionRename);
    collectionActions->addWidget(collectionDelete);
    collectionActions->addWidget(remove);
    collectionActions->addWidget(collectionRefresh);
    collectionActions->addStretch();
    collectionLayout->addLayout(collectionActions);
    tabs->addTab(collectionPage, i18nc("@title:tab", "Saved Collections"));
    layout->addWidget(tabs);
    m_status->setObjectName(QStringLiteral("searchStoreStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_searchList, &QListWidget::currentRowChanged, this, [this] {
        showSearch();
    });
    connect(m_collectionList, &QListWidget::currentRowChanged, this, [this] {
        showCollection();
    });
    connect(searchRename, &QPushButton::clicked, this, &SearchStoreDialog::renameSearch);
    connect(searchDelete, &QPushButton::clicked, this, &SearchStoreDialog::deleteSearch);
    connect(searchRefresh, &QPushButton::clicked, this, [this] {
        reloadSearches();
    });
    connect(collectionRename, &QPushButton::clicked, this, &SearchStoreDialog::renameCollection);
    connect(collectionDelete, &QPushButton::clicked, this, &SearchStoreDialog::deleteCollection);
    connect(remove, &QPushButton::clicked, this, &SearchStoreDialog::removeReferences);
    connect(collectionRefresh, &QPushButton::clicked, this, [this] {
        reloadCollections();
    });
    reloadSearches();
    reloadCollections();
}

void SearchStoreDialog::reloadSearches(const QString &selected)
{
    QString error;
    const auto searches = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    m_searches = searches;
    m_searchList->clear();
    for (const auto &search : m_searches) {
        m_searchList->addItem(search.name);
    }
    const auto matches = m_searchList->findItems(selected, Qt::MatchExactly);
    m_searchList->setCurrentRow(matches.isEmpty() ? (m_searches.isEmpty() ? -1 : 0) : m_searchList->row(matches.first()));
    showSearch();
}

void SearchStoreDialog::reloadCollections(const QString &selected)
{
    QString error;
    const auto collections = SearchCollection::loadSaved(m_config->group(QStringLiteral("SearchCollection")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    m_collections = collections;
    m_collectionList->clear();
    for (const auto &collection : m_collections) {
        m_collectionList->addItem(collection.name);
    }
    const auto matches = m_collectionList->findItems(selected, Qt::MatchExactly);
    m_collectionList->setCurrentRow(matches.isEmpty() ? (m_collections.isEmpty() ? -1 : 0) : m_collectionList->row(matches.first()));
    showCollection();
}

void SearchStoreDialog::showSearch()
{
    const int row = m_searchList->currentRow();
    if (row < 0 || row >= m_searches.size()) {
        m_searchDetails->clear();
        return;
    }
    const auto &search = m_searches.at(row);
    m_searchDetails->setText(i18nc("@info",
                                   "Query: %1\nFolder: %2\nScope: %3\nField: %4",
                                   search.text,
                                   search.searchFolders          ? i18nc("@info", "Search folders")
                                       : search.folder.isEmpty() ? i18nc("@info", "All indexed locations")
                                                                 : search.folder.toDisplayString(),
                                   search.indexed             ? i18nc("@info", "Indexed")
                                       : search.searchFolders ? i18nc("@info", "Search folders (live)")
                                                              : i18nc("@info", "Current folder"),
                                   search.field == LiveSearch::Field::FileName    ? i18nc("@info", "File name")
                                       : search.field == LiveSearch::Field::Query ? i18nc("@info", "Advanced query")
                                                                                  : i18nc("@info", "File contents")));
}

void SearchStoreDialog::showCollection()
{
    m_references->clear();
    const int row = m_collectionList->currentRow();
    if (row >= 0 && row < m_collections.size()) {
        for (const auto &url : m_collections.at(row).urls) {
            auto *item = new QListWidgetItem(url.toLocalFile(), m_references);
            item->setData(Qt::UserRole, url);
        }
    }
}

void SearchStoreDialog::renameSearch()
{
    const int row = m_searchList->currentRow();
    if (row < 0 || row >= m_searches.size()) {
        m_status->setText(i18nc("@info", "Select a saved search."));
        return;
    }
    const auto original = m_searches.at(row);
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, i18nc("@title:window", "Rename Saved Search"), i18nc("@label", "Name:"), QLineEdit::Normal, original.name, &accepted)
            .trimmed();
    if (!accepted) {
        return;
    }
    QString error;
    auto current = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    auto it = findNamed(current, original.name);
    if (it == current.end() || *it != original) {
        m_status->setText(i18nc("@info", "This search changed elsewhere. Refresh the list."));
        return;
    }
    it->name = name;
    if (!SavedSearches::save(m_config->group(QStringLiteral("SavedSearches")), current, &error)) {
        m_status->setText(error);
        return;
    }
    reloadSearches(name);
    m_status->setText(i18nc("@info", "Saved search renamed."));
}

void SearchStoreDialog::deleteSearch()
{
    const int row = m_searchList->currentRow();
    if (row < 0 || row >= m_searches.size()) {
        m_status->setText(i18nc("@info", "Select a saved search."));
        return;
    }
    const auto original = m_searches.at(row);
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Delete Saved Search"),
                              i18nc("@info", "Delete “%1”? No files will be removed.", original.name),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    auto current = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    const auto it = findNamed(current, original.name);
    if (it == current.end() || *it != original) {
        m_status->setText(i18nc("@info", "This search changed elsewhere. Refresh the list."));
        return;
    }
    current.erase(it);
    if (!SavedSearches::save(m_config->group(QStringLiteral("SavedSearches")), current, &error)) {
        m_status->setText(error);
        return;
    }
    reloadSearches();
    m_status->setText(i18nc("@info", "Saved search deleted. No files were removed."));
}

void SearchStoreDialog::renameCollection()
{
    const int row = m_collectionList->currentRow();
    if (row < 0 || row >= m_collections.size()) {
        m_status->setText(i18nc("@info", "Select a saved collection."));
        return;
    }
    const auto original = m_collections.at(row);
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, i18nc("@title:window", "Rename Collection"), i18nc("@label", "Name:"), QLineEdit::Normal, original.name, &accepted)
            .trimmed();
    if (!accepted) {
        return;
    }
    QString error;
    auto current = SearchCollection::loadSaved(m_config->group(QStringLiteral("SearchCollection")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    auto it = findNamed(current, original.name);
    if (it == current.end() || *it != original) {
        m_status->setText(i18nc("@info", "This collection changed elsewhere. Refresh the list."));
        return;
    }
    it->name = name;
    if (!SearchCollection::saveSaved(m_config->group(QStringLiteral("SearchCollection")), current, &error)) {
        m_status->setText(error);
        return;
    }
    reloadCollections(name);
    m_status->setText(i18nc("@info", "Saved collection renamed."));
}

void SearchStoreDialog::deleteCollection()
{
    const int row = m_collectionList->currentRow();
    if (row < 0 || row >= m_collections.size()) {
        m_status->setText(i18nc("@info", "Select a saved collection."));
        return;
    }
    const auto original = m_collections.at(row);
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Delete Collection"),
                              i18nc("@info", "Delete “%1”? No files will be removed.", original.name),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    auto current = SearchCollection::loadSaved(m_config->group(QStringLiteral("SearchCollection")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    const auto it = findNamed(current, original.name);
    if (it == current.end() || *it != original) {
        m_status->setText(i18nc("@info", "This collection changed elsewhere. Refresh the list."));
        return;
    }
    current.erase(it);
    if (!SearchCollection::saveSaved(m_config->group(QStringLiteral("SearchCollection")), current, &error)) {
        m_status->setText(error);
        return;
    }
    reloadCollections();
    m_status->setText(i18nc("@info", "Saved collection deleted. No files were removed."));
}

void SearchStoreDialog::removeReferences()
{
    const int row = m_collectionList->currentRow();
    const auto selected = m_references->selectedItems();
    if (row < 0 || row >= m_collections.size() || selected.isEmpty()) {
        m_status->setText(i18nc("@info", "Select a collection and references to remove."));
        return;
    }
    const auto original = m_collections.at(row);
    QList<QUrl> removed;
    for (const auto *item : selected) {
        removed.append(item->data(Qt::UserRole).toUrl());
    }
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Remove References"),
                              i18nc("@info", "Remove the selected references from “%1”? No files will be removed.", original.name),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    auto current = SearchCollection::loadSaved(m_config->group(QStringLiteral("SearchCollection")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    auto it = findNamed(current, original.name);
    if (it == current.end() || *it != original) {
        m_status->setText(i18nc("@info", "This collection changed elsewhere. Refresh the list."));
        return;
    }
    for (const auto &url : removed) {
        it->urls.removeAll(url);
    }
    if (!SearchCollection::saveSaved(m_config->group(QStringLiteral("SearchCollection")), current, &error)) {
        m_status->setText(error);
        return;
    }
    reloadCollections(original.name);
    m_status->setText(i18nc("@info", "References removed. No files were removed."));
}
