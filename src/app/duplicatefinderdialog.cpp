// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicatefinderdialog.h"

#include <KFileItem>
#include <KIO/Global>
#include <KIO/ListJob>
#include <KIO/UDSEntry>
#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPromise>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

DuplicateFinderDialog::DuplicateFinderDialog(const QUrl &folder, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
    , m_canceled(std::make_shared<std::atomic_bool>(false))
    , m_status(new QLabel(i18nc("@info", "Listing files…"), this))
    , m_progress(new QProgressBar(this))
    , m_results(new QTreeWidget(this))
    , m_errorsView(new QTreeWidget(this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Scan"), this))
    , m_copy(new QPushButton(i18nc("@action:button", "Copy Checked URLs"), this))
{
    setObjectName(QStringLiteral("duplicateFinderDialog"));
    setWindowTitle(i18nc("@title:window", "Find Duplicate Files"));
    resize(750, 550);
    auto *layout = new QVBoxLayout(this);
    auto *criteria = new QLabel(i18nc("@info",
                                      "Known regular files below %1 are grouped by size and SHA-256 contents. Symbolic links are skipped. "
                                      "Check paths for manual review; this dialog does not remove files.",
                                      folder.toDisplayString()),
                                this);
    criteria->setTextFormat(Qt::PlainText);
    criteria->setWordWrap(true);
    layout->addWidget(criteria);
    m_status->setObjectName(QStringLiteral("duplicateFinderStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);
    m_results->setObjectName(QStringLiteral("duplicateFinderResults"));
    m_results->setColumnCount(2);
    m_results->setHeaderLabels({i18nc("@title:column", "Group or file"), i18nc("@title:column", "Size / SHA-256")});
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(m_results);
    m_errorsView->setObjectName(QStringLiteral("duplicateFinderErrors"));
    m_errorsView->setHeaderLabels({i18nc("@title:column", "Scan errors")});
    m_errorsView->hide();
    layout->addWidget(m_errorsView);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_cancel->setObjectName(QStringLiteral("duplicateFinderCancel"));
    m_copy->setObjectName(QStringLiteral("duplicateFinderCopy"));
    m_copy->setEnabled(false);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    buttons->addButton(m_copy, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(m_cancel, &QPushButton::clicked, this, &DuplicateFinderDialog::cancel);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_copy, &QPushButton::clicked, this, [this] {
        QStringList selected;
        for (int i = 0; i < m_results->topLevelItemCount(); ++i) {
            const QTreeWidgetItem *group = m_results->topLevelItem(i);
            for (int j = 0; j < group->childCount(); ++j) {
                if (group->child(j)->checkState(0) == Qt::Checked) {
                    selected.append(QUrl::fromLocalFile(group->child(j)->text(0)).toString(QUrl::FullyEncoded));
                }
            }
        }
        if (selected.isEmpty()) {
            m_status->setText(i18nc("@info", "Check at least one file to copy its URL."));
            return;
        }
        QApplication::clipboard()->setText(selected.join(QLatin1Char('\n')));
        m_status->setText(i18np("Copied %1 URL. No files were removed.", "Copied %1 URLs. No files were removed.", selected.size()));
    });
    connect(&m_watcher, &QFutureWatcher<DuplicateFinder::Result>::progressRangeChanged, m_progress, &QProgressBar::setRange);
    connect(&m_watcher, &QFutureWatcher<DuplicateFinder::Result>::progressValueChanged, m_progress, &QProgressBar::setValue);
    connect(&m_watcher, &QFutureWatcher<DuplicateFinder::Result>::finished, this, [this] {
        const auto result = m_watcher.result();
        m_errors.append(result.errors);
        m_cancel->setEnabled(false);
        m_progress->setRange(0, 1);
        m_progress->setValue(result.canceled ? 0 : 1);
        if (result.canceled) {
            m_status->setText(i18nc("@info", "Scan canceled; no results are shown."));
            return;
        }
        for (const auto &group : result.groups) {
            auto *parent = new QTreeWidgetItem(m_results);
            parent->setText(0, i18np("%1 duplicate file", "%1 duplicate files", group.paths.size()));
            parent->setText(1, i18nc("@info size and SHA-256", "%1 / %2", KIO::convertSize(group.size), QString::fromLatin1(group.digest)));
            for (const QString &path : group.paths) {
                auto *child = new QTreeWidgetItem(parent);
                child->setText(0, path);
                child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
                child->setCheckState(0, Qt::Unchecked);
            }
            parent->setExpanded(true);
        }
        for (const QString &error : std::as_const(m_errors)) {
            auto *row = new QTreeWidgetItem(m_errorsView);
            row->setText(0, error);
        }
        m_errorsView->setVisible(!m_errors.isEmpty());
        m_copy->setEnabled(!result.groups.isEmpty());
        m_status->setText(
            i18nc("@info", "Found %1 groups. Scan errors: %2. Results can become stale; review files before removal.", result.groups.size(), m_errors.size()));
    });
    m_listJob = KIO::listRecursive(folder, KIO::HideProgressInfo);
    connect(m_listJob, &KIO::ListJob::entries, this, [this](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const auto &entry : entries) {
            const KFileItem item(entry, m_folder, true, true);
            if (item.isFile() && !item.isLink() && item.url().isLocalFile()) {
                m_files.append(item.url().toLocalFile());
            }
        }
    });
    connect(m_listJob, &KIO::ListJob::subError, this, [this](KIO::ListJob *, KIO::ListJob *subJob) {
        m_errors.append(i18nc("@info", "Could not list %1: %2", subJob->url().toDisplayString(), subJob->errorString()));
    });
    connect(m_listJob, &KJob::result, this, [this](KJob *job) {
        m_listJob = nullptr;
        if (job->error()) {
            m_cancel->setEnabled(false);
            m_progress->setRange(0, 1);
            m_status->setText(i18nc("@info", "Could not list folder: %1", job->errorString()));
            return;
        }
        startHashing();
    });
}

DuplicateFinderDialog::~DuplicateFinderDialog()
{
    cancel();
}

void DuplicateFinderDialog::cancel()
{
    m_canceled->store(true);
    if (m_listJob) {
        m_listJob->disconnect(this);
        m_listJob->kill(KJob::Quietly);
        m_listJob = nullptr;
        m_progress->setRange(0, 1);
        m_status->setText(i18nc("@info", "Scan canceled; no results are shown."));
    } else if (m_watcher.isRunning()) {
        m_status->setText(i18nc("@info", "Canceling…"));
    }
    m_cancel->setEnabled(false);
}

void DuplicateFinderDialog::startHashing()
{
    const QStringList files = m_files;
    const auto canceled = m_canceled;
    m_status->setText(i18nc("@info", "Comparing file sizes and SHA-256 hashes…"));
    m_watcher.setFuture(QtConcurrent::run([files, canceled](QPromise<DuplicateFinder::Result> &promise) {
        const Checksums::Cancel check = [canceled] {
            return canceled->load();
        };
        const Checksums::Progress progress = [&promise](int current, int total, const QString &) {
            promise.setProgressRange(0, qMax(1, total));
            promise.setProgressValue(current);
        };
        promise.addResult(DuplicateFinder::find(files, check, progress));
    }));
}
