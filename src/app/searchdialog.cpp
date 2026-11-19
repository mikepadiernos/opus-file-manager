// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchdialog.h"

#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPromise>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

SearchDialog::SearchDialog(const QUrl &folder, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
    , m_query(new QLineEdit(this))
    , m_field(new QComboBox(this))
    , m_search(new QPushButton(i18nc("@action:button", "Search"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Search"), this))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_errors(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("searchDialog"));
    setWindowTitle(i18nc("@title:window", "Search Current Folder"));
    resize(750, 500);
    auto *layout = new QVBoxLayout(this);
    auto *folderLabel = new QLabel(folder.toDisplayString(), this);
    folderLabel->setTextFormat(Qt::PlainText);
    folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(folderLabel);
    auto *form = new QFormLayout;
    m_query->setObjectName(QStringLiteral("searchQuery"));
    m_field->setObjectName(QStringLiteral("searchField"));
    m_field->addItem(i18nc("@item:inlistbox", "File name"));
    m_field->addItem(i18nc("@item:inlistbox", "File contents (UTF-8 text)"));
    form->addRow(i18nc("@label", "Find:"), m_query);
    form->addRow(i18nc("@label", "Search in:"), m_field);
    layout->addLayout(form);
    m_status->setObjectName(QStringLiteral("searchStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setText(i18nc("@info", "Searches local folders recursively. Symbolic links are skipped."));
    layout->addWidget(m_status);
    m_results->setObjectName(QStringLiteral("searchResults"));
    m_results->setHeaderLabels({i18nc("@title:column", "Matching path")});
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(m_results);
    m_errors->setObjectName(QStringLiteral("searchErrors"));
    m_errors->setHeaderLabels({i18nc("@title:column", "Search errors")});
    m_errors->hide();
    layout->addWidget(m_errors);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *copy = new QPushButton(i18nc("@action:button", "Copy Selected URL"), this);
    copy->setObjectName(QStringLiteral("searchCopy"));
    copy->setEnabled(false);
    m_search->setObjectName(QStringLiteral("searchStart"));
    m_cancel->setObjectName(QStringLiteral("searchCancel"));
    m_cancel->setEnabled(false);
    buttons->addButton(m_search, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    buttons->addButton(copy, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(m_results, &QTreeWidget::itemSelectionChanged, this, [this, copy] {
        copy->setEnabled(!m_results->selectedItems().isEmpty());
    });
    connect(copy, &QPushButton::clicked, this, [this] {
        const auto selected = m_results->selectedItems();
        if (!selected.isEmpty()) {
            QApplication::clipboard()->setText(QUrl::fromLocalFile(selected.first()->text(0)).toString(QUrl::FullyEncoded));
            m_status->setText(i18nc("@info", "Copied the selected URL."));
        }
    });
    connect(m_search, &QPushButton::clicked, this, &SearchDialog::start);
    connect(m_query, &QLineEdit::returnPressed, this, &SearchDialog::start);
    connect(m_cancel, &QPushButton::clicked, this, &SearchDialog::cancel);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(&m_watcher, &QFutureWatcher<LiveSearch::Result>::progressValueChanged, this, [this](int count) {
        m_status->setText(i18np("Searching: %1 entry scanned.", "Searching: %1 entries scanned.", count));
    });
    connect(&m_watcher, &QFutureWatcher<LiveSearch::Result>::finished, this, [this] {
        const LiveSearch::Result result = m_watcher.result();
        m_search->setEnabled(true);
        m_query->setEnabled(true);
        m_field->setEnabled(true);
        m_cancel->setEnabled(false);
        for (const QString &path : result.paths) {
            new QTreeWidgetItem(m_results, {path});
        }
        for (const QString &error : result.errors) {
            new QTreeWidgetItem(m_errors, {error});
        }
        m_errors->setVisible(!result.errors.isEmpty());
        QString status = result.canceled ? i18nc("@info", "Search canceled; results may be incomplete.")
            : result.limited             ? i18nc("@info", "Search limit reached; results may be incomplete.")
                                         : i18np("Search finished: %1 match.", "Search finished: %1 matches.", result.paths.size());
        if (result.skippedFiles) {
            status += QLatin1Char(' ')
                + i18np("%1 binary, invalid UTF-8 or oversized file was skipped.",
                        "%1 binary, invalid UTF-8 or oversized files were skipped.",
                        result.skippedFiles);
        }
        if (!result.errors.isEmpty()) {
            status += QLatin1Char(' ') + i18nc("@info", "Some paths could not be searched; see errors below.");
        }
        m_status->setText(status);
    });
}

SearchDialog::~SearchDialog()
{
    cancel();
}

void SearchDialog::start()
{
    if (m_watcher.isRunning()) {
        return;
    }
    if (!m_folder.isLocalFile()) {
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
    m_cancel->setEnabled(true);
    m_status->setText(i18nc("@info", "Searching…"));
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    const QString folder = m_folder.toLocalFile();
    const QString text = m_query->text();
    const auto field = m_field->currentIndex() == 0 ? LiveSearch::Field::FileName : LiveSearch::Field::Contents;
    m_watcher.setFuture(QtConcurrent::run([folder, text, field, canceled](QPromise<LiveSearch::Result> &promise) {
        promise.setProgressRange(0, 200000);
        const LiveSearch::Cancel cancel = [canceled] {
            return canceled->load();
        };
        const LiveSearch::Progress progress = [&promise](int count, const QString &) {
            promise.setProgressValue(count);
        };
        promise.addResult(LiveSearch::find(folder, text, field, cancel, progress));
    }));
}

void SearchDialog::cancel()
{
    if (m_canceled && m_watcher.isRunning()) {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling search…"));
    }
}
