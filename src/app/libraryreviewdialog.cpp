// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryreviewdialog.h"

#include "libraryscan.h"
#include "localizeddates.h"

#include <KFormat>
#include <KLocalizedString>
#include <KMessageBox>

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
constexpr int IdRole = Qt::UserRole;

QString describe(const TagLibrary::Entry &entry)
{
    const QString modified = entry.modified ? LocalizedDates::dateTime(QDateTime::fromMSecsSinceEpoch(entry.modified)) : QString();
    if (entry.folder) {
        return modified;
    }
    return modified.isEmpty() ? KFormat().formatByteSize(entry.size)
                              : i18nc("@item size, modification time", "%1, %2", KFormat().formatByteSize(entry.size), modified);
}

QString metadataSummary(const TagLibrary::Entry &entry)
{
    QStringList parts;
    if (!entry.tags.isEmpty()) {
        parts.append(i18ncp("@item", "One tag", "%1 tags", entry.tags.size()));
    }
    if (!entry.fields.isEmpty()) {
        parts.append(i18ncp("@item", "One field", "%1 fields", entry.fields.size()));
    }
    return parts.isEmpty() ? i18nc("@item no tags or fields", "None") : parts.join(i18nc("@item list separator", ", "));
}
}

LibraryReviewDialog::LibraryReviewDialog(const QString &root, QWidget *parent)
    : QDialog(parent)
    , m_root(QDir::cleanPath(root))
    , m_missing(new QTreeWidget(this))
    , m_candidates(new QTreeWidget(this))
    , m_relink(new QPushButton(QIcon::fromTheme(QStringLiteral("link")), i18nc("@action:button", "&Relink to Suggestion"), this))
    , m_browse(new QPushButton(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:button", "&Choose Item…"), this))
    , m_remove(new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "&Forget Entry…"), this))
    , m_status(new QLabel(this))
{
    setWindowTitle(i18nc("@title:window", "Review Missing Entries"));
    setObjectName(QStringLiteral("libraryReviewDialog"));

    auto *intro = new QLabel(xi18nc("@info",
                                    "These entries weren't found by the last refresh of <filename>%1</filename>. Their tags and fields are "
                                    "kept. Relink an entry to the item it now refers to, or forget it. Suggestions are never applied "
                                    "automatically.",
                                    m_root),
                             this);
    intro->setWordWrap(true);

    m_missing->setObjectName(QStringLiteral("libraryMissingList"));
    m_missing->setAccessibleName(i18nc("@info:accessible", "Missing entries"));
    m_missing->setHeaderLabels({i18nc("@title:column", "Missing Entry"), i18nc("@title:column", "Tags and Fields"), i18nc("@title:column", "Last Seen")});
    m_missing->setRootIsDecorated(false);
    m_missing->setUniformRowHeights(true);
    m_missing->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_missing->header()->setStretchLastSection(false);

    m_candidates->setObjectName(QStringLiteral("libraryCandidateList"));
    m_candidates->setAccessibleName(i18nc("@info:accessible", "Suggested items"));
    m_candidates->setHeaderLabels({i18nc("@title:column", "Suggested Item"), i18nc("@title:column", "Match"), i18nc("@title:column", "Details")});
    m_candidates->setRootIsDecorated(false);
    m_candidates->setUniformRowHeights(true);
    m_candidates->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_candidates->header()->setStretchLastSection(false);

    auto *candidatesLabel = new QLabel(i18nc("@label", "&Suggestions:"), this);
    candidatesLabel->setBuddy(m_candidates);
    auto *missingLabel = new QLabel(i18nc("@label", "&Missing entries:"), this);
    missingLabel->setBuddy(m_missing);

    auto *top = new QWidget(this);
    auto *topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(missingLabel);
    topLayout->addWidget(m_missing);
    auto *bottom = new QWidget(this);
    auto *bottomLayout = new QVBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->addWidget(candidatesLabel);
    bottomLayout->addWidget(m_candidates);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(top);
    splitter->addWidget(bottom);
    splitter->setChildrenCollapsible(false);

    m_relink->setObjectName(QStringLiteral("libraryRelink"));
    m_browse->setObjectName(QStringLiteral("libraryRelinkBrowse"));
    m_remove->setObjectName(QStringLiteral("libraryForgetEntry"));
    m_status->setObjectName(QStringLiteral("libraryReviewStatus"));
    m_status->setWordWrap(true);
    auto *actions = new QHBoxLayout;
    actions->addWidget(m_relink);
    actions->addWidget(m_browse);
    actions->addStretch();
    actions->addWidget(m_remove);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(splitter, 1);
    layout->addLayout(actions);
    layout->addWidget(m_status);
    layout->addWidget(buttons);

    connect(m_missing, &QTreeWidget::currentItemChanged, this, &LibraryReviewDialog::showCandidates);
    connect(m_candidates, &QTreeWidget::currentItemChanged, this, &LibraryReviewDialog::updateButtons);
    connect(m_candidates, &QTreeWidget::itemActivated, this, &LibraryReviewDialog::relinkToSuggestion);
    connect(m_relink, &QPushButton::clicked, this, &LibraryReviewDialog::relinkToSuggestion);
    connect(m_browse, &QPushButton::clicked, this, &LibraryReviewDialog::browse);
    connect(m_remove, &QPushButton::clicked, this, &LibraryReviewDialog::confirmRemove);

    resize(720, 520);
    reload();
}

void LibraryReviewDialog::reload()
{
    const QString current = selectedEntry();
    QString error;
    m_library = TagLibrary::Library::load(m_root, &error);
    m_missing->clear();
    QTreeWidgetItem *select = nullptr;
    for (const TagLibrary::Entry &entry : m_library.entries()) {
        if (!entry.missing) {
            continue;
        }
        auto *item = new QTreeWidgetItem(m_missing, {entry.path, metadataSummary(entry), describe(entry)});
        item->setIcon(0, QIcon::fromTheme(entry.folder ? QStringLiteral("folder") : QStringLiteral("unknown")));
        item->setData(0, IdRole, entry.id);
        if (entry.id == current) {
            select = item;
        }
    }
    m_missing->sortItems(0, Qt::AscendingOrder);
    if (!select && m_missing->topLevelItemCount() > 0) {
        select = m_missing->topLevelItem(0);
    }
    m_missing->setCurrentItem(select);
    m_missing->resizeColumnToContents(1);
    m_missing->resizeColumnToContents(2);
    if (m_library.isNull()) {
        setMessage(error);
    } else if (m_library.isReadOnly()) {
        setMessage(i18nc("@info", "The library is read-only, so entries can't be relinked or forgotten."));
    } else if (m_missing->topLevelItemCount() == 0) {
        setMessage(i18nc("@info", "No entries are missing."));
    }
    showCandidates();
}

QString LibraryReviewDialog::selectedEntry() const
{
    const QTreeWidgetItem *item = m_missing->currentItem();
    return item ? item->data(0, IdRole).toString() : QString();
}

void LibraryReviewDialog::showCandidates()
{
    m_candidates->clear();
    const TagLibrary::Entry *missing = m_library.entryById(selectedEntry());
    if (missing) {
        for (const QString &id : m_library.relinkCandidates(missing->id)) {
            const TagLibrary::Entry *candidate = m_library.entryById(id);
            // Candidates without the same identity always share the name, and files the size.
            const bool identity = missing->inode != 0 && candidate->inode == missing->inode && candidate->device == missing->device;
            const QString match = identity ? i18nc("@item relink match", "Same file system identity")
                : !missing->folder         ? i18nc("@item relink match", "Same name and size")
                                           : i18nc("@item relink match", "Same name");
            auto *item = new QTreeWidgetItem(m_candidates, {candidate->path, match, describe(*candidate)});
            item->setIcon(0, QIcon::fromTheme(candidate->folder ? QStringLiteral("folder") : QStringLiteral("unknown")));
            item->setData(0, IdRole, candidate->id);
        }
        m_candidates->resizeColumnToContents(1);
        m_candidates->resizeColumnToContents(2);
        if (m_candidates->topLevelItemCount() == 0) {
            auto *none = new QTreeWidgetItem(m_candidates, {i18nc("@item", "No suggestions. Choose the item yourself, or refresh the library first.")});
            none->setFlags(Qt::NoItemFlags);
        }
    }
    updateButtons();
}

void LibraryReviewDialog::updateButtons()
{
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    const bool hasEntry = writable && !selectedEntry().isEmpty();
    const QTreeWidgetItem *candidate = m_candidates->currentItem();
    m_relink->setEnabled(hasEntry && candidate && !candidate->data(0, IdRole).toString().isEmpty());
    m_browse->setEnabled(hasEntry);
    m_remove->setEnabled(hasEntry);
}

bool LibraryReviewDialog::change(const std::function<bool(TagLibrary::Library &library, QString *error)> &edit, const QString &done)
{
    QString error;
    if (!TagLibrary::update(m_root, edit, &error)) {
        reload();
        setMessage(error);
        return false;
    }
    Q_EMIT libraryChanged(m_root);
    reload();
    setMessage(done);
    return true;
}

bool LibraryReviewDialog::relinkToSuggestion()
{
    const QTreeWidgetItem *candidate = m_candidates->currentItem();
    const TagLibrary::Entry *target = candidate ? m_library.entryById(candidate->data(0, IdRole).toString()) : nullptr;
    if (selectedEntry().isEmpty() || !target) {
        return false;
    }
    return relinkTo(QDir(m_root).filePath(target->path));
}

bool LibraryReviewDialog::relinkTo(const QString &path)
{
    const QString id = selectedEntry();
    if (id.isEmpty()) {
        return false;
    }
    TagLibrary::Observation target;
    if (!LibraryScan::observe(m_root, path, &target)) {
        setMessage(xi18nc("@info", "<filename>%1</filename> can't be used: choose an existing item inside the library that isn't a symbolic link.", path));
        return false;
    }
    return change(
        [&](TagLibrary::Library &library, QString *error) {
            if (!library.relink(id, target, error)) {
                return false;
            }
            // Refresh the entries that moved with a relinked folder.
            QList<TagLibrary::Observation> moved;
            for (const TagLibrary::Entry &child : library.entries()) {
                TagLibrary::Observation observation;
                if (child.missing && child.path.startsWith(target.path + QLatin1Char('/'))
                    && LibraryScan::observe(m_root, QDir(m_root).filePath(child.path), &observation)) {
                    moved.append(observation);
                }
            }
            library.applyScan(moved, false);
            return true;
        },
        i18nc("@info", "Relinked to %1.", target.path));
}

bool LibraryReviewDialog::removeSelected()
{
    const QString id = selectedEntry();
    const TagLibrary::Entry *entry = m_library.entryById(id);
    if (!entry) {
        return false;
    }
    const QString path = entry->path;
    return change(
        [&](TagLibrary::Library &library, QString *) {
            return library.removeEntry(id) > 0;
        },
        i18nc("@info", "Forgot %1.", path));
}

void LibraryReviewDialog::browse()
{
    const TagLibrary::Entry *entry = m_library.entryById(selectedEntry());
    if (!entry) {
        return;
    }
    const QString start = QDir(m_root).filePath(entry->path.section(QLatin1Char('/'), 0, -2));
    const QString path = entry->folder ? QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose the Folder for %1", entry->path), start)
                                       : QFileDialog::getOpenFileName(this, i18nc("@title:window", "Choose the File for %1", entry->path), start);
    if (!path.isEmpty()) {
        relinkTo(path);
    }
}

void LibraryReviewDialog::confirmRemove()
{
    const TagLibrary::Entry *entry = m_library.entryById(selectedEntry());
    if (!entry) {
        return;
    }
    const QString text = entry->folder
        ? xi18nc("@info",
                 "Forget <filename>%1</filename> and the missing entries inside it? Their tags and fields are removed from the library. No files "
                 "are deleted.",
                 entry->path)
        : xi18nc("@info", "Forget <filename>%1</filename>? Its tags and fields are removed from the library. No files are deleted.", entry->path);
    if (KMessageBox::warningContinueCancel(this,
                                           text,
                                           i18nc("@title:window", "Forget Entry"),
                                           KGuiItem(i18nc("@action:button", "Forget Entry"), QStringLiteral("list-remove")))
        == KMessageBox::Continue) {
        removeSelected();
    }
}

QString LibraryReviewDialog::message() const
{
    return m_message;
}

void LibraryReviewDialog::setMessage(const QString &message)
{
    m_message = message;
    m_status->setText(message);
}
