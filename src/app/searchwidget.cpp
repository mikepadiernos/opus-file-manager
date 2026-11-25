// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchwidget.h"

#include "indexedsearch.h"
#include "pathfiltereditor.h"
#include "searchcollection.h"
#include "searchlocations.h"

#include <KLocalizedString>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPromise>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

namespace
{
enum Scope {
    CurrentFolder,
    Indexed,
    SearchFolders,
};

QString scopeStatus(int index, bool workspaceOnly)
{
    if (index == SearchFolders) {
        return i18nc("@info", "Searches the search folders set in Preferences recursively. Symbolic links and excluded folders are skipped.");
    }
    if (workspaceOnly) {
        return index == 0 ? i18nc("@info", "Searches saved locations assigned to the active workspace recursively. Symbolic links are skipped.")
                          : i18nc("@info",
                                  "Searches indexed files in saved locations assigned to the active workspace. Depth is measured from each location. "
                                  "Baloo matches indexed terms, not literal substrings.");
    }
    return index == 0 ? i18nc("@info", "Searches local folders recursively. Symbolic links are skipped.")
                      : i18nc("@info",
                              "Searches configured Baloo locations. A depth limit restricts results to the current local folder. Baloo matches "
                              "indexed terms, not literal substrings.");
}
}

SearchWidget::SearchWidget(const QUrl &folder, QWidget *parent, KSharedConfig::Ptr config, WorkspaceScopeProvider workspaceScope)
    : QWidget(parent)
    , m_folder(folder)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_folderLabel(new QLabel(folder.toDisplayString(), this))
    , m_query(new QLineEdit(this))
    , m_field(new QComboBox(this))
    , m_scope(new QComboBox(this))
    , m_workspaceOnly(new QCheckBox(this))
    , m_workspaceScope(std::move(workspaceScope))
    , m_filterEditor(new PathFilterEditor(QStringLiteral("search"), this, m_config))
    , m_saved(new QComboBox(this))
    , m_search(new QPushButton(i18nc("@action:button", "Search"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Search"), this))
    , m_status(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_results(new QTreeWidget(this))
    , m_collection(new QTreeWidget(this))
    , m_resultTabs(new QTabWidget(this))
    , m_errors(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("searchWidget"));
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName(QStringLiteral("searchSplitter"));
    splitter->setChildrenCollapsible(false);
    outerLayout->addWidget(splitter);
    // Both parts scroll so a narrow panel beside the panes does not force a wide minimum size.
    auto *criteriaArea = new QScrollArea(splitter);
    criteriaArea->setObjectName(QStringLiteral("searchCriteriaArea"));
    criteriaArea->setWidgetResizable(true);
    criteriaArea->setFrameShape(QFrame::NoFrame);
    auto *criteria = new QWidget(criteriaArea);
    auto *layout = new QVBoxLayout(criteria);
    criteriaArea->setWidget(criteria);
    auto *resultsScroll = new QScrollArea(splitter);
    resultsScroll->setObjectName(QStringLiteral("searchResultsArea"));
    resultsScroll->setWidgetResizable(true);
    resultsScroll->setFrameShape(QFrame::NoFrame);
    auto *resultsArea = new QWidget(resultsScroll);
    auto *resultsLayout = new QVBoxLayout(resultsArea);
    resultsScroll->setWidget(resultsArea);
    splitter->addWidget(criteriaArea);
    splitter->addWidget(resultsScroll);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    m_folderLabel->setObjectName(QStringLiteral("searchFolder"));
    m_folderLabel->setTextFormat(Qt::PlainText);
    m_folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_folderLabel);
    auto *form = new QFormLayout;
    m_query->setObjectName(QStringLiteral("searchQuery"));
    m_field->setObjectName(QStringLiteral("searchField"));
    m_field->addItem(i18nc("@item:inlistbox", "File name"));
    m_field->addItem(i18nc("@item:inlistbox", "File contents (UTF-8 text)"));
    m_scope->setObjectName(QStringLiteral("searchScope"));
    m_scope->addItem(i18nc("@item:inlistbox", "Current folder (live)"));
    m_scope->addItem(i18nc("@item:inlistbox", "Indexed locations (Baloo)"));
    m_scope->addItem(i18nc("@item:inlistbox", "Search folders (live)"));
    const auto defaults = m_config->group(QStringLiteral("Search"));
    m_scope->setCurrentIndex(defaults.readEntry("SearchIndexedByDefault", false) ? 1 : 0);
    m_field->setCurrentIndex(defaults.readEntry("SearchContentsByDefault", false) ? 1 : 0);
    if (m_scope->currentIndex() == 1) {
        m_field->setItemText(1, i18nc("@item:inlistbox", "Indexed file contents"));
    }
    form->addRow(i18nc("@label", "Find:"), m_query);
    form->addRow(i18nc("@label", "Search in:"), m_field);
    form->addRow(i18nc("@label", "Scope:"), m_scope);
    m_workspaceOnly->setObjectName(QStringLiteral("searchWorkspaceOnly"));
    m_workspaceOnly->setText(i18nc("@option:check", "Limit to saved locations assigned to the active workspace"));
    m_workspaceOnly->setToolTip(i18nc("@info:tooltip", "Clear this to search the current folder or all indexed locations outside the workspace."));
    m_workspaceOnly->setChecked(defaults.readEntry("WorkspaceSearchByDefault", true));
    form->addRow(QString(), m_workspaceOnly);
    refreshWorkspaceScope();
    layout->addLayout(form);
    layout->addWidget(m_filterEditor);
    auto *savedRow = new QHBoxLayout;
    m_saved->setObjectName(QStringLiteral("searchSaved"));
    auto *saveSearch = new QPushButton(i18nc("@action:button", "Save Search…"), this);
    auto *loadSearch = new QPushButton(i18nc("@action:button", "Load"), this);
    auto *updateSearch = new QPushButton(i18nc("@action:button", "Update"), this);
    auto *refreshSearches = new QPushButton(i18nc("@action:button", "Refresh"), this);
    auto *deleteSearch = new QPushButton(i18nc("@action:button", "Delete Saved Search"), this);
    saveSearch->setObjectName(QStringLiteral("searchSave"));
    loadSearch->setObjectName(QStringLiteral("searchLoad"));
    updateSearch->setObjectName(QStringLiteral("searchUpdate"));
    refreshSearches->setObjectName(QStringLiteral("searchRefreshSaved"));
    deleteSearch->setObjectName(QStringLiteral("searchDelete"));
    savedRow->addWidget(m_saved, 1);
    savedRow->addWidget(saveSearch);
    savedRow->addWidget(loadSearch);
    savedRow->addWidget(updateSearch);
    savedRow->addWidget(refreshSearches);
    savedRow->addWidget(deleteSearch);
    layout->addLayout(savedRow);
    layout->addStretch(1);
    m_status->setObjectName(QStringLiteral("searchStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setText(scopeStatus(m_scope->currentIndex(), m_workspaceScope && m_workspaceScope().active && m_workspaceOnly->isChecked()));
    resultsLayout->addWidget(m_status);
    QString savedError;
    m_savedSearches = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &savedError);
    updateSavedSearches();
    if (!savedError.isEmpty()) {
        m_status->setText(savedError);
        saveSearch->setEnabled(false);
    }
    connect(m_saved, &QComboBox::currentIndexChanged, this, [loadSearch, updateSearch, deleteSearch](int index) {
        loadSearch->setEnabled(index >= 0);
        updateSearch->setEnabled(index >= 0);
        deleteSearch->setEnabled(index >= 0);
    });
    loadSearch->setEnabled(m_saved->currentIndex() >= 0);
    updateSearch->setEnabled(m_saved->currentIndex() >= 0);
    deleteSearch->setEnabled(m_saved->currentIndex() >= 0);
    connect(saveSearch, &QPushButton::clicked, this, [this] {
        bool accepted = false;
        const QString name =
            QInputDialog::getText(this, i18nc("@title:window", "Save Search"), i18nc("@label", "Name:"), QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            return;
        }
        const auto search = currentSearch(name);
        const QString filterError = search.filter.error();
        if (!filterError.isEmpty()) {
            m_status->setText(filterError);
            return;
        }
        if (search.indexed && search.filter.maxDepth >= 0 && search.folder.isEmpty()) {
            m_status->setText(i18nc("@info", "A local current folder is required to limit indexed-search depth."));
            return;
        }
        QString error;
        QList<SavedSearches::Search> updated = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        updated.append(search);
        if (!SavedSearches::save(m_config->group(QStringLiteral("SavedSearches")), updated, &error)) {
            m_status->setText(error);
            return;
        }
        m_savedSearches = updated;
        updateSavedSearches();
        m_saved->setCurrentIndex(m_savedSearches.size() - 1);
        m_status->setText(i18nc("@info", "Search saved. Load it to review its criteria before searching."));
    });
    connect(loadSearch, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_savedSearches.size()) {
            return;
        }
        if (m_watcher.isRunning()) {
            m_status->setText(i18nc("@info", "Wait for the current search to finish before loading saved criteria."));
            return;
        }
        const QString name = m_savedSearches.at(index).name;
        QString error;
        const auto current = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto it = std::find_if(current.cbegin(), current.cend(), [&name](const SavedSearches::Search &search) {
            return search.name == name;
        });
        if (it == current.cend()) {
            m_savedSearches = current;
            updateSavedSearches();
            m_status->setText(i18nc("@info", "This saved search no longer exists."));
            return;
        }
        const auto search = *it;
        m_savedSearches = current;
        updateSavedSearches();
        m_saved->setCurrentIndex(m_saved->findText(search.name));
        if (!search.folder.isEmpty()) {
            m_folder = search.folder;
            m_folderLabel->setText(m_folder.toDisplayString());
        }
        m_query->setText(search.text);
        m_scope->setCurrentIndex(search.searchFolders ? SearchFolders : search.indexed ? Indexed : CurrentFolder);
        m_field->setCurrentIndex(search.field == LiveSearch::Field::FileName ? 0 : 1);
        m_filterEditor->setFilter(search.filter);
        m_status->setText(i18nc("@info", "Saved criteria loaded. Review them and start the search."));
    });
    connect(refreshSearches, &QPushButton::clicked, this, [this, saveSearch] {
        const QString selected = m_saved->currentText();
        QString error;
        const auto searches = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        m_savedSearches = searches;
        saveSearch->setEnabled(true);
        updateSavedSearches();
        const int index = m_saved->findText(selected);
        if (index >= 0) {
            m_saved->setCurrentIndex(index);
        }
        m_status->setText(i18nc("@info", "Saved searches refreshed."));
    });
    connect(updateSearch, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_savedSearches.size()) {
            return;
        }
        if (m_watcher.isRunning()) {
            m_status->setText(i18nc("@info", "Wait for the current search to finish before updating saved criteria."));
            return;
        }
        const auto original = m_savedSearches.at(index);
        QString error;
        auto searches = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto it = std::find_if(searches.begin(), searches.end(), [&original](const auto &search) {
            return search.name == original.name;
        });
        if (it == searches.end() || *it != original) {
            m_status->setText(i18nc("@info", "This search changed elsewhere. Refresh the list before updating it."));
            return;
        }
        *it = currentSearch(original.name, original.id);
        if (!SavedSearches::save(m_config->group(QStringLiteral("SavedSearches")), searches, &error)) {
            m_status->setText(error);
            return;
        }
        m_savedSearches = searches;
        updateSavedSearches();
        m_saved->setCurrentIndex(m_saved->findText(original.name));
        m_status->setText(i18nc("@info", "Saved search updated."));
    });
    connect(deleteSearch, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_savedSearches.size()) {
            return;
        }
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Delete Saved Search"),
                                  i18nc("@info", "Delete the saved search “%1”? No files will be removed.", m_savedSearches.at(index).name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        const QString name = m_savedSearches.at(index).name;
        QString error;
        QList<SavedSearches::Search> updated = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto it = std::find_if(updated.begin(), updated.end(), [&name](const SavedSearches::Search &search) {
            return search.name == name;
        });
        if (it == updated.end()) {
            m_savedSearches = updated;
            updateSavedSearches();
            m_status->setText(i18nc("@info", "This saved search no longer exists."));
            return;
        }
        if (*it != m_savedSearches.at(index)) {
            m_status->setText(i18nc("@info", "This search changed elsewhere. Refresh the list before deleting it."));
            return;
        }
        updated.erase(it);
        if (!SavedSearches::save(m_config->group(QStringLiteral("SavedSearches")), updated, &error)) {
            m_status->setText(error);
            return;
        }
        m_savedSearches = updated;
        updateSavedSearches();
        m_status->setText(i18nc("@info", "Saved search deleted. No files were removed."));
    });
    m_progress->setObjectName(QStringLiteral("searchProgress"));
    m_progress->setRange(0, 0);
    m_progress->hide();
    resultsLayout->addWidget(m_progress);
    m_results->setObjectName(QStringLiteral("searchResults"));
    m_results->setHeaderLabels({i18nc("@title:column", "Matching path")});
    m_results->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_collection->setObjectName(QStringLiteral("searchCollection"));
    m_collection->setHeaderLabels({i18nc("@title:column", "Referenced path")});
    m_collection->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_collection->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTabs->setObjectName(QStringLiteral("searchResultTabs"));
    m_resultTabs->addTab(m_results, i18nc("@title:tab", "Results"));
    auto *collectionPage = new QWidget(m_resultTabs);
    auto *collectionLayout = new QVBoxLayout(collectionPage);
    auto *collectionRow = new QHBoxLayout;
    auto *savedCollections = new QComboBox(collectionPage);
    savedCollections->setObjectName(QStringLiteral("searchSavedCollections"));
    auto *saveCollection = new QPushButton(i18nc("@action:button", "Save Collection…"), collectionPage);
    saveCollection->setObjectName(QStringLiteral("searchSaveCollection"));
    auto *loadCollection = new QPushButton(i18nc("@action:button", "Load"), collectionPage);
    loadCollection->setObjectName(QStringLiteral("searchLoadCollection"));
    auto *updateCollectionButton = new QPushButton(i18nc("@action:button", "Update"), collectionPage);
    updateCollectionButton->setObjectName(QStringLiteral("searchUpdateCollection"));
    auto *refreshCollections = new QPushButton(i18nc("@action:button", "Refresh"), collectionPage);
    refreshCollections->setObjectName(QStringLiteral("searchRefreshCollections"));
    collectionRow->addWidget(savedCollections, 1);
    collectionRow->addWidget(saveCollection);
    collectionRow->addWidget(loadCollection);
    collectionRow->addWidget(updateCollectionButton);
    collectionRow->addWidget(refreshCollections);
    collectionLayout->addLayout(collectionRow);
    auto *collectionHint =
        new QLabel(i18nc("@info", "Saved collections are snapshots. Load replaces the working collection; Update replaces the selected snapshot."),
                   collectionPage);
    collectionHint->setWordWrap(true);
    collectionLayout->addWidget(collectionHint);
    collectionLayout->addWidget(m_collection);
    m_resultTabs->addTab(collectionPage, i18nc("@title:tab", "Collection"));
    resultsLayout->addWidget(m_resultTabs, 1);
    QString collectionError;
    const auto collection = SearchCollection::load(m_config->group(QStringLiteral("SearchCollection")), &collectionError);
    updateCollection(collection);
    if (!collectionError.isEmpty()) {
        m_status->setText(collectionError);
    }
    const auto refreshSavedCollections = [this, savedCollections, loadCollection, updateCollectionButton] {
        const QString selected = savedCollections->currentText();
        QString error;
        const auto collections = SearchCollection::loadSaved(m_config->group(QStringLiteral("SearchCollection")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        savedCollections->clear();
        for (const auto &item : collections) {
            savedCollections->addItem(item.name);
        }
        const int index = savedCollections->findText(selected);
        if (index >= 0) {
            savedCollections->setCurrentIndex(index);
        }
        loadCollection->setEnabled(!collections.isEmpty());
        updateCollectionButton->setEnabled(!collections.isEmpty());
    };
    connect(refreshCollections, &QPushButton::clicked, this, refreshSavedCollections);
    connect(saveCollection, &QPushButton::clicked, this, [this, savedCollections, refreshSavedCollections] {
        bool accepted = false;
        const QString name =
            QInputDialog::getText(this, i18nc("@title:window", "Save Collection"), i18nc("@label", "Name:"), QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            return;
        }
        const auto group = m_config->group(QStringLiteral("SearchCollection"));
        QString error;
        const auto urls = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto collections = SearchCollection::loadSaved(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        collections.append({name, urls});
        if (!SearchCollection::saveSaved(group, collections, &error)) {
            m_status->setText(error);
            return;
        }
        refreshSavedCollections();
        savedCollections->setCurrentIndex(savedCollections->findText(name));
        m_status->setText(i18nc("@info", "Collection saved. Later changes to the working collection do not change this snapshot."));
    });
    connect(loadCollection, &QPushButton::clicked, this, [this, savedCollections] {
        const QString name = savedCollections->currentText();
        const auto group = m_config->group(QStringLiteral("SearchCollection"));
        QString error;
        const auto collections = SearchCollection::loadSaved(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(collections.cbegin(), collections.cend(), [&name](const auto &item) {
            return item.name == name;
        });
        if (it == collections.cend()) {
            m_status->setText(i18nc("@info", "This saved collection no longer exists. Refresh the list."));
            return;
        }
        const auto urls = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        if (urls != it->urls
            && QMessageBox::question(this,
                                     i18nc("@title:window", "Load Collection"),
                                     i18nc("@info", "Replace the working collection with “%1”? Original files will not be removed.", name),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No)
                != QMessageBox::Yes) {
            return;
        }
        const auto selected = *it;
        const auto currentCollections = SearchCollection::loadSaved(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto current = std::find_if(currentCollections.cbegin(), currentCollections.cend(), [&name](const auto &item) {
            return item.name == name;
        });
        const auto working = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        if (current == currentCollections.cend() || *current != selected || working != urls) {
            m_status->setText(i18nc("@info", "The collection changed elsewhere. Refresh and try again."));
            return;
        }
        if (!SearchCollection::save(group, selected.urls, &error)) {
            m_status->setText(error);
            return;
        }
        updateCollection(selected.urls);
        m_status->setText(i18nc("@info", "Collection loaded. Original files were not removed."));
    });
    connect(updateCollectionButton, &QPushButton::clicked, this, [this, savedCollections] {
        const QString name = savedCollections->currentText();
        const auto group = m_config->group(QStringLiteral("SearchCollection"));
        QString error;
        auto collections = SearchCollection::loadSaved(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto it = std::find_if(collections.begin(), collections.end(), [&name](const auto &item) {
            return item.name == name;
        });
        if (it == collections.end()) {
            m_status->setText(i18nc("@info", "This saved collection no longer exists. Refresh the list."));
            return;
        }
        const auto original = *it;
        const auto urls = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Update Collection"),
                                  i18nc("@info", "Replace saved collection “%1” with the working collection? Original files will not be removed.", name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        collections = SearchCollection::loadSaved(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        it = std::find_if(collections.begin(), collections.end(), [&name](const auto &item) {
            return item.name == name;
        });
        const auto working = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        if (it == collections.end() || *it != original || working != urls) {
            m_status->setText(i18nc("@info", "The collection changed elsewhere. Refresh and try again."));
            return;
        }
        it->urls = urls;
        if (!SearchCollection::saveSaved(group, collections, &error)) {
            m_status->setText(error);
            return;
        }
        m_status->setText(i18nc("@info", "Saved collection updated. Original files were not removed."));
    });
    refreshSavedCollections();
    m_errors->setObjectName(QStringLiteral("searchErrors"));
    m_errors->setHeaderLabels({i18nc("@title:column", "Search errors")});
    m_errors->hide();
    resultsLayout->addWidget(m_errors);
    auto *buttons = new QHBoxLayout;
    auto *open = new QPushButton(i18nc("@action:button", "Open"), this);
    auto *copy = new QPushButton(i18nc("@action:button", "Copy Selected URL"), this);
    auto *reveal = new QPushButton(i18nc("@action:button", "Show in Folder"), this);
    auto *addReference = new QPushButton(i18nc("@action:button", "Add to Collection"), this);
    auto *removeReference = new QPushButton(i18nc("@action:button", "Remove from Collection"), this);
    addReference->setObjectName(QStringLiteral("searchCollect"));
    removeReference->setObjectName(QStringLiteral("searchUncollect"));
    open->setObjectName(QStringLiteral("searchOpen"));
    open->setToolTip(i18nc("@info:tooltip", "Open the selected file in its application, or the selected folder in the active pane (Enter)"));
    open->setEnabled(false);
    reveal->setObjectName(QStringLiteral("searchReveal"));
    reveal->setToolTip(i18nc("@info:tooltip", "Show the selected item in its folder in the active pane (Ctrl+Enter)"));
    reveal->setEnabled(false);
    copy->setObjectName(QStringLiteral("searchCopy"));
    copy->setEnabled(false);
    m_search->setObjectName(QStringLiteral("searchStart"));
    m_cancel->setObjectName(QStringLiteral("searchCancel"));
    m_cancel->setEnabled(false);
    buttons->addWidget(m_search);
    buttons->addWidget(m_cancel);
    buttons->addStretch(1);
    buttons->addWidget(open);
    buttons->addWidget(reveal);
    buttons->addWidget(copy);
    buttons->addWidget(addReference);
    buttons->addWidget(removeReference);
    resultsLayout->addLayout(buttons);
    m_results->installEventFilter(this);
    m_collection->installEventFilter(this);
    const auto updateActions = [this, open, copy, reveal, addReference, removeReference] {
        const bool showingResults = m_resultTabs->currentWidget() == m_results;
        const auto selected = showingResults ? m_results->selectedItems() : m_collection->selectedItems();
        open->setEnabled(selected.size() == 1);
        copy->setEnabled(selected.size() == 1);
        reveal->setEnabled(selected.size() == 1);
        addReference->setEnabled(showingResults && !selected.isEmpty());
        removeReference->setEnabled(!showingResults && !selected.isEmpty());
    };
    connect(m_results, &QTreeWidget::itemSelectionChanged, this, updateActions);
    connect(m_collection, &QTreeWidget::itemSelectionChanged, this, updateActions);
    connect(m_resultTabs, &QTabWidget::currentChanged, this, [this, updateActions](int index) {
        if (index == 1) {
            QString error;
            const auto urls = SearchCollection::load(m_config->group(QStringLiteral("SearchCollection")), &error);
            if (!error.isEmpty()) {
                m_status->setText(error);
            } else {
                updateCollection(urls);
            }
        }
        updateActions();
    });
    updateActions();
    connect(copy, &QPushButton::clicked, this, [this] {
        const auto selected = (m_resultTabs->currentWidget() == m_results ? m_results : m_collection)->selectedItems();
        if (!selected.isEmpty()) {
            QApplication::clipboard()->setText(QUrl::fromLocalFile(selected.first()->text(0)).toString(QUrl::FullyEncoded));
            m_status->setText(i18nc("@info", "Copied the selected URL."));
        }
    });
    connect(open, &QPushButton::clicked, this, &SearchWidget::openSelected);
    connect(reveal, &QPushButton::clicked, this, &SearchWidget::revealSelected);
    // Double-click rather than itemActivated: with Plasma's single-click setting, activation would open on selection.
    connect(m_results, &QTreeWidget::itemDoubleClicked, this, &SearchWidget::openSelected);
    connect(m_collection, &QTreeWidget::itemDoubleClicked, this, &SearchWidget::openSelected);
    connect(addReference, &QPushButton::clicked, this, [this] {
        QString error;
        const auto group = m_config->group(QStringLiteral("SearchCollection"));
        auto urls = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        for (const QTreeWidgetItem *item : m_results->selectedItems()) {
            const QUrl url = QUrl::fromLocalFile(item->text(0));
            if (!urls.contains(url)) {
                urls.append(url);
            }
        }
        if (!SearchCollection::save(group, urls, &error)) {
            m_status->setText(error);
            return;
        }
        updateCollection(urls);
        m_status->setText(i18nc("@info", "References added to the collection. Original files were not moved."));
    });
    connect(removeReference, &QPushButton::clicked, this, [this] {
        QString error;
        const auto group = m_config->group(QStringLiteral("SearchCollection"));
        auto urls = SearchCollection::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        for (const QTreeWidgetItem *item : m_collection->selectedItems()) {
            urls.removeAll(QUrl::fromLocalFile(item->text(0)));
        }
        if (!SearchCollection::save(group, urls, &error)) {
            m_status->setText(error);
            return;
        }
        updateCollection(urls);
        m_status->setText(i18nc("@info", "References removed from the collection. Original files were not removed."));
    });
    connect(m_search, &QPushButton::clicked, this, &SearchWidget::start);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_field->setItemText(1, index == Indexed ? i18nc("@item:inlistbox", "Indexed file contents") : i18nc("@item:inlistbox", "File contents (UTF-8 text)"));
        m_status->setText(scopeStatus(index, m_workspaceScope && m_workspaceScope().active && m_workspaceOnly->isChecked()));
    });
    connect(m_workspaceOnly, &QCheckBox::toggled, this, [this] {
        refreshWorkspaceScope();
        m_status->setText(scopeStatus(m_scope->currentIndex(), m_workspaceScope && m_workspaceScope().active && m_workspaceOnly->isChecked()));
    });
    connect(m_query, &QLineEdit::returnPressed, this, &SearchWidget::start);
    connect(m_cancel, &QPushButton::clicked, this, &SearchWidget::cancel);
    connect(&m_watcher, &QFutureWatcher<LiveSearch::Result>::progressValueChanged, this, [this](int count) {
        m_status->setText(m_scope->currentIndex() != Indexed
                              ? i18np("Searching: %1 entry scanned.", "Searching: %1 entries scanned.", count)
                              : i18np("Searching: %1 indexed match retrieved.", "Searching: %1 indexed matches retrieved.", count));
    });
    connect(&m_watcher, &QFutureWatcher<LiveSearch::Result>::finished, this, [this] {
        const LiveSearch::Result result = m_watcher.result();
        m_search->setEnabled(true);
        m_query->setEnabled(true);
        m_field->setEnabled(true);
        m_scope->setEnabled(true);
        m_workspaceOnly->setEnabled(true);
        m_filterEditor->setEnabled(true);
        m_cancel->setEnabled(false);
        m_progress->hide();
        for (const QString &path : result.paths) {
            new QTreeWidgetItem(m_results, {path});
        }
        for (const QString &error : result.errors) {
            new QTreeWidgetItem(m_errors, {error});
        }
        m_errors->setVisible(!result.errors.isEmpty());
        QString status = result.canceled ? i18nc("@info", "Search canceled; results may be incomplete.")
            : result.limited             ? i18nc("@info", "Search limit reached; results may be incomplete.")
            : !result.errors.isEmpty()   ? i18nc("@info", "Search incomplete; see errors below.")
                                         : i18np("Search finished: %1 match.", "Search finished: %1 matches.", result.paths.size());
        if (result.skippedFiles) {
            status += QLatin1Char(' ')
                + i18np("%1 binary, invalid UTF-8 or oversized file was skipped.",
                        "%1 binary, invalid UTF-8 or oversized files were skipped.",
                        result.skippedFiles);
        }
        for (const QString &notice : result.notices) {
            status += QLatin1Char(' ') + notice;
        }
        m_status->setText(status);
    });
}

SearchWidget::~SearchWidget()
{
    cancel();
}

bool SearchWidget::setFolder(const QUrl &folder)
{
    if (m_watcher.isRunning()) {
        return false;
    }
    m_folder = folder;
    m_folderLabel->setText(folder.toDisplayString());
    return true;
}

QUrl SearchWidget::folder() const
{
    return m_folder;
}

bool SearchWidget::isRunning() const
{
    return m_watcher.isRunning();
}

void SearchWidget::focusQuery()
{
    m_query->setFocus(Qt::ShortcutFocusReason);
    m_query->selectAll();
}

void SearchWidget::search(const QString &text)
{
    if (m_watcher.isRunning()) {
        m_status->setText(i18nc("@info", "Wait for the current search to finish or cancel it before starting another."));
        return;
    }
    m_query->setText(text);
    start();
}

bool SearchWidget::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_results || watched == m_collection) && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers().testFlag(Qt::ControlModifier)) {
                revealSelected();
            } else {
                openSelected();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SearchWidget::revealSelected()
{
    const auto selected = (m_resultTabs->currentWidget() == m_results ? m_results : m_collection)->selectedItems();
    if (selected.size() == 1) {
        Q_EMIT revealRequested(QUrl::fromLocalFile(selected.first()->text(0)));
    }
}

void SearchWidget::openSelected()
{
    const auto selected = (m_resultTabs->currentWidget() == m_results ? m_results : m_collection)->selectedItems();
    if (selected.size() != 1) {
        return;
    }
    const QString path = selected.first()->text(0);
    if (!QFileInfo::exists(path)) {
        m_status->setText(i18nc("@info", "%1 no longer exists. Search again to refresh the results.", path));
        return;
    }
    Q_EMIT openRequested(QUrl::fromLocalFile(path));
}

void SearchWidget::refreshWorkspaceScope()
{
    const bool active = m_workspaceScope && m_workspaceScope().active;
    m_workspaceOnly->setVisible(active);
    m_scope->setItemText(0,
                         active && m_workspaceOnly->isChecked() ? i18nc("@item:inlistbox", "Workspace locations (live)")
                                                                : i18nc("@item:inlistbox", "Current folder (live)"));
}

void SearchWidget::updateSavedSearches()
{
    m_saved->clear();
    for (const auto &search : std::as_const(m_savedSearches)) {
        m_saved->addItem(search.name);
    }
}

void SearchWidget::updateCollection(const QList<QUrl> &urls)
{
    m_collection->clear();
    for (const QUrl &url : urls) {
        new QTreeWidgetItem(m_collection, {url.toLocalFile()});
    }
}

PathFilter SearchWidget::pathFilter() const
{
    return m_filterEditor->filter();
}

SavedSearches::Search SearchWidget::currentSearch(const QString &name, const QString &id) const
{
    return {name,
            m_query->text(),
            m_folder.isLocalFile() ? m_folder : QUrl(),
            m_field->currentIndex() == 0 ? LiveSearch::Field::FileName : LiveSearch::Field::Contents,
            m_scope->currentIndex() == Indexed,
            pathFilter(),
            id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id,
            m_scope->currentIndex() == SearchFolders};
}

void SearchWidget::start()
{
    if (m_watcher.isRunning()) {
        return;
    }
    const WorkspaceScope workspace = m_workspaceScope ? m_workspaceScope() : WorkspaceScope{};
    refreshWorkspaceScope();
    const bool searchFolders = m_scope->currentIndex() == SearchFolders;
    const bool workspaceOnly = !searchFolders && workspace.active && m_workspaceOnly->isChecked();
    if (!workspaceOnly && m_scope->currentIndex() == CurrentFolder && !m_folder.isLocalFile()) {
        m_status->setText(i18nc("@info", "Live search currently supports local folders only."));
        return;
    }
    if (m_query->text().trimmed().isEmpty()) {
        m_status->setText(i18nc("@info", "Enter text to search for."));
        return;
    }
    const PathFilter chosenFilter = pathFilter();
    const QString filterError = chosenFilter.error();
    if (!filterError.isEmpty()) {
        m_status->setText(filterError);
        return;
    }
    const auto searchSettings = m_config->group(QStringLiteral("Search"));
    const QStringList exclusions = searchSettings.readEntry("SearchExclusions", QStringList());
    const PathFilter filter = SearchLocations::withExclusions(chosenFilter, exclusions);
    if (!workspaceOnly && m_scope->currentIndex() == Indexed && filter.maxDepth >= 0 && !m_folder.isLocalFile()) {
        m_status->setText(i18nc("@info", "A local current folder is required to limit indexed-search depth."));
        return;
    }
    m_results->clear();
    m_errors->clear();
    m_errors->hide();
    m_search->setEnabled(false);
    m_query->setEnabled(false);
    m_field->setEnabled(false);
    m_scope->setEnabled(false);
    m_workspaceOnly->setEnabled(false);
    m_filterEditor->setEnabled(false);
    m_cancel->setEnabled(true);
    m_progress->show();
    m_status->setText(i18nc("@info", "Searching…"));
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    const QString folder = m_folder.toLocalFile();
    const QString text = m_query->text();
    const auto field = m_field->currentIndex() == 0 ? LiveSearch::Field::FileName : LiveSearch::Field::Contents;
    const bool indexed = m_scope->currentIndex() == Indexed;
    QStringList roots;
    QStringList locationErrors;
    if (searchFolders) {
        roots = SearchLocations::searchRoots(searchSettings.readEntry("SearchFolders", QStringList()), exclusions, &locationErrors);
        if (roots.isEmpty() && locationErrors.isEmpty()) {
            locationErrors.append(i18nc("@info", "No search folders are set. Add them in Preferences, on the Search page of Tools."));
        }
    } else if (workspaceOnly) {
        for (const QUrl &url : workspace.locations) {
            if (!url.isLocalFile() || !QDir::isAbsolutePath(url.toLocalFile())) {
                locationErrors.append(i18nc("@info",
                                            "Workspace search supports local folders only: %1",
                                            url.toDisplayString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment)));
            } else {
                const QString root = QDir::cleanPath(url.toLocalFile());
                const QFileInfo folderInfo(root);
                if (!folderInfo.isDir() || folderInfo.isSymLink()) {
                    locationErrors.append(i18nc("@info", "Choose an existing local folder that is not a symbolic link: %1", root));
                } else if (!roots.contains(root)) {
                    roots.append(root);
                }
            }
        }
        if (roots.isEmpty() && locationErrors.isEmpty()) {
            locationErrors.append(i18nc("@info", "The active workspace has no searchable saved locations."));
        }
    }
    const bool multipleRoots = workspaceOnly || searchFolders;
    m_watcher.setFuture(
        QtConcurrent::run([folder, text, field, canceled, indexed, filter, multipleRoots, roots, locationErrors](QPromise<LiveSearch::Result> &promise) {
            promise.setProgressRange(0, 200000);
            const LiveSearch::Cancel cancel = [canceled] {
                return canceled->load();
            };
            const LiveSearch::Progress progress = [&promise](int count, const QString &) {
                promise.setProgressValue(count);
            };
            LiveSearch::Result result;
            if (!multipleRoots) {
                result = indexed ? IndexedSearch::find(folder, text, field, cancel, progress, filter)
                                 : LiveSearch::find(folder, text, field, cancel, progress, filter);
            } else if (indexed && !roots.isEmpty()) {
                result = IndexedSearch::find(folder, text, field, cancel, progress, filter, &roots);
            } else if (!indexed) {
                QSet<QString> seen;
                for (const QString &root : roots) {
                    const int offset = result.scannedEntries;
                    const auto part = LiveSearch::find(
                        root,
                        text,
                        field,
                        cancel,
                        [&progress, offset](int count, const QString &path) {
                            progress(offset + count, path);
                        },
                        filter,
                        200000 - offset);
                    result.errors.append(part.errors);
                    result.notices.append(part.notices);
                    result.scannedEntries += part.scannedEntries;
                    result.skippedFiles += part.skippedFiles;
                    result.canceled = part.canceled;
                    result.limited = part.limited;
                    for (const QString &path : part.paths) {
                        if (!seen.contains(path)) {
                            seen.insert(path);
                            result.paths.append(path);
                        }
                        if (result.paths.size() >= 5000) {
                            result.limited = true;
                            break;
                        }
                    }
                    if (result.scannedEntries >= 200000) {
                        result.limited = true;
                    }
                    if (result.canceled || result.limited) {
                        break;
                    }
                }
            }
            result.errors.append(locationErrors);
            promise.addResult(result);
        }));
}

void SearchWidget::cancel()
{
    if (m_canceled && m_watcher.isRunning()) {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(m_scope->currentIndex() == 1 ? i18nc("@info", "Canceling search; the current Baloo query must finish first…")
                                                       : i18nc("@info", "Canceling search…"));
    }
}
