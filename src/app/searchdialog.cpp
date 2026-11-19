// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchdialog.h"

#include "indexedsearch.h"
#include "searchcollection.h"

#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPromise>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

SearchDialog::SearchDialog(const QUrl &folder, QWidget *parent, KSharedConfig::Ptr config)
    : QDialog(parent)
    , m_folder(folder)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_folderLabel(new QLabel(folder.toDisplayString(), this))
    , m_query(new QLineEdit(this))
    , m_field(new QComboBox(this))
    , m_scope(new QComboBox(this))
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
    setObjectName(QStringLiteral("searchDialog"));
    setWindowTitle(i18nc("@title:window", "Search Files"));
    resize(750, 500);
    auto *layout = new QVBoxLayout(this);
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
    form->addRow(i18nc("@label", "Find:"), m_query);
    form->addRow(i18nc("@label", "Search in:"), m_field);
    form->addRow(i18nc("@label", "Scope:"), m_scope);
    layout->addLayout(form);
    auto *savedRow = new QHBoxLayout;
    m_saved->setObjectName(QStringLiteral("searchSaved"));
    auto *saveSearch = new QPushButton(i18nc("@action:button", "Save Search…"), this);
    auto *loadSearch = new QPushButton(i18nc("@action:button", "Load"), this);
    auto *deleteSearch = new QPushButton(i18nc("@action:button", "Delete Saved Search"), this);
    saveSearch->setObjectName(QStringLiteral("searchSave"));
    loadSearch->setObjectName(QStringLiteral("searchLoad"));
    deleteSearch->setObjectName(QStringLiteral("searchDelete"));
    savedRow->addWidget(m_saved, 1);
    savedRow->addWidget(saveSearch);
    savedRow->addWidget(loadSearch);
    savedRow->addWidget(deleteSearch);
    layout->addLayout(savedRow);
    m_status->setObjectName(QStringLiteral("searchStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setText(i18nc("@info", "Searches local folders recursively. Symbolic links are skipped."));
    layout->addWidget(m_status);
    QString savedError;
    m_savedSearches = SavedSearches::load(m_config->group(QStringLiteral("SavedSearches")), &savedError);
    updateSavedSearches();
    if (!savedError.isEmpty()) {
        m_status->setText(savedError);
        saveSearch->setEnabled(false);
    }
    connect(m_saved, &QComboBox::currentIndexChanged, this, [this, loadSearch, deleteSearch](int index) {
        loadSearch->setEnabled(index >= 0);
        deleteSearch->setEnabled(index >= 0);
    });
    loadSearch->setEnabled(m_saved->currentIndex() >= 0);
    deleteSearch->setEnabled(m_saved->currentIndex() >= 0);
    connect(saveSearch, &QPushButton::clicked, this, [this] {
        bool accepted = false;
        const QString name =
            QInputDialog::getText(this, i18nc("@title:window", "Save Search"), i18nc("@label", "Name:"), QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            return;
        }
        SavedSearches::Search search{name,
                                     m_query->text(),
                                     m_folder.isLocalFile() ? m_folder : QUrl(),
                                     m_field->currentIndex() == 0 ? LiveSearch::Field::FileName : LiveSearch::Field::Contents,
                                     m_scope->currentIndex() == 1};
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
        if (!search.folder.isEmpty()) {
            m_folder = search.folder;
            m_folderLabel->setText(m_folder.toDisplayString());
        }
        m_query->setText(search.text);
        m_scope->setCurrentIndex(search.indexed ? 1 : 0);
        m_field->setCurrentIndex(search.field == LiveSearch::Field::FileName ? 0 : 1);
        m_status->setText(i18nc("@info", "Saved criteria loaded. Review them and start the search."));
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
    layout->addWidget(m_progress);
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
    m_resultTabs->addTab(m_collection, i18nc("@title:tab", "Collection"));
    layout->addWidget(m_resultTabs);
    QString collectionError;
    const auto collection = SearchCollection::load(m_config->group(QStringLiteral("SearchCollection")), &collectionError);
    updateCollection(collection);
    if (!collectionError.isEmpty()) {
        m_status->setText(collectionError);
    }
    m_errors->setObjectName(QStringLiteral("searchErrors"));
    m_errors->setHeaderLabels({i18nc("@title:column", "Search errors")});
    m_errors->hide();
    layout->addWidget(m_errors);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *copy = new QPushButton(i18nc("@action:button", "Copy Selected URL"), this);
    auto *reveal = new QPushButton(i18nc("@action:button", "Show in Folder"), this);
    auto *addReference = new QPushButton(i18nc("@action:button", "Add to Collection"), this);
    auto *removeReference = new QPushButton(i18nc("@action:button", "Remove from Collection"), this);
    addReference->setObjectName(QStringLiteral("searchCollect"));
    removeReference->setObjectName(QStringLiteral("searchUncollect"));
    reveal->setObjectName(QStringLiteral("searchReveal"));
    reveal->setEnabled(false);
    copy->setObjectName(QStringLiteral("searchCopy"));
    copy->setEnabled(false);
    m_search->setObjectName(QStringLiteral("searchStart"));
    m_cancel->setObjectName(QStringLiteral("searchCancel"));
    m_cancel->setEnabled(false);
    buttons->addButton(m_search, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    buttons->addButton(copy, QDialogButtonBox::ActionRole);
    buttons->addButton(reveal, QDialogButtonBox::ActionRole);
    buttons->addButton(addReference, QDialogButtonBox::ActionRole);
    buttons->addButton(removeReference, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    const auto updateActions = [this, copy, reveal, addReference, removeReference] {
        const bool showingResults = m_resultTabs->currentWidget() == m_results;
        const auto selected = showingResults ? m_results->selectedItems() : m_collection->selectedItems();
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
    connect(reveal, &QPushButton::clicked, this, [this] {
        const auto selected = (m_resultTabs->currentWidget() == m_results ? m_results : m_collection)->selectedItems();
        if (!selected.isEmpty()) {
            Q_EMIT revealRequested(QUrl::fromLocalFile(selected.first()->text(0)));
        }
    });
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
    connect(m_search, &QPushButton::clicked, this, &SearchDialog::start);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_field->setItemText(1, index == 0 ? i18nc("@item:inlistbox", "File contents (UTF-8 text)") : i18nc("@item:inlistbox", "Indexed file contents"));
        m_status->setText(
            index == 0
                ? i18nc("@info", "Searches local folders recursively. Symbolic links are skipped.")
                : i18nc("@info", "Searches configured Baloo locations, not just the current folder. Baloo matches indexed terms, not literal substrings."));
    });
    connect(m_query, &QLineEdit::returnPressed, this, &SearchDialog::start);
    connect(m_cancel, &QPushButton::clicked, this, &SearchDialog::cancel);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(&m_watcher, &QFutureWatcher<LiveSearch::Result>::progressValueChanged, this, [this](int count) {
        m_status->setText(m_scope->currentIndex() == 0 ? i18np("Searching: %1 entry scanned.", "Searching: %1 entries scanned.", count)
                                                       : i18np("Searching: %1 indexed match retrieved.", "Searching: %1 indexed matches retrieved.", count));
    });
    connect(&m_watcher, &QFutureWatcher<LiveSearch::Result>::finished, this, [this] {
        const LiveSearch::Result result = m_watcher.result();
        m_search->setEnabled(true);
        m_query->setEnabled(true);
        m_field->setEnabled(true);
        m_scope->setEnabled(true);
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

SearchDialog::~SearchDialog()
{
    cancel();
}

void SearchDialog::updateSavedSearches()
{
    m_saved->clear();
    for (const auto &search : std::as_const(m_savedSearches)) {
        m_saved->addItem(search.name);
    }
}

void SearchDialog::updateCollection(const QList<QUrl> &urls)
{
    m_collection->clear();
    for (const QUrl &url : urls) {
        new QTreeWidgetItem(m_collection, {url.toLocalFile()});
    }
}

void SearchDialog::start()
{
    if (m_watcher.isRunning()) {
        return;
    }
    if (m_scope->currentIndex() == 0 && !m_folder.isLocalFile()) {
        m_status->setText(i18nc("@info", "Live search currently supports local folders only."));
        return;
    }
    if (m_query->text().trimmed().isEmpty()) {
        m_status->setText(i18nc("@info", "Enter text to search for."));
        return;
    }
    m_results->clear();
    m_errors->clear();
    m_errors->hide();
    m_search->setEnabled(false);
    m_query->setEnabled(false);
    m_field->setEnabled(false);
    m_scope->setEnabled(false);
    m_cancel->setEnabled(true);
    m_progress->show();
    m_status->setText(i18nc("@info", "Searching…"));
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    const QString folder = m_folder.toLocalFile();
    const QString text = m_query->text();
    const auto field = m_field->currentIndex() == 0 ? LiveSearch::Field::FileName : LiveSearch::Field::Contents;
    const bool indexed = m_scope->currentIndex() == 1;
    m_watcher.setFuture(QtConcurrent::run([folder, text, field, canceled, indexed](QPromise<LiveSearch::Result> &promise) {
        promise.setProgressRange(0, 200000);
        const LiveSearch::Cancel cancel = [canceled] {
            return canceled->load();
        };
        const LiveSearch::Progress progress = [&promise](int count, const QString &) {
            promise.setProgressValue(count);
        };
        promise.addResult(indexed ? IndexedSearch::find(folder, text, field, cancel, progress) : LiveSearch::find(folder, text, field, cancel, progress));
    }));
}

void SearchDialog::cancel()
{
    if (m_canceled && m_watcher.isRunning()) {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(m_scope->currentIndex() == 1 ? i18nc("@info", "Canceling search; the current Baloo query must finish first…")
                                                       : i18nc("@info", "Canceling search…"));
    }
}
