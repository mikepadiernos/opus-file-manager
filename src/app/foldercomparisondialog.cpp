// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercomparisondialog.h"

#include <KIO/Global>
#include <KIO/ListJob>
#include <KIO/UDSEntry>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
QString statusText(FolderComparison::Status status)
{
    using Status = FolderComparison::Status;
    switch (status) {
    case Status::LeftOnly:
        return i18nc("@info:status", "Only in first folder");
    case Status::RightOnly:
        return i18nc("@info:status", "Only in second folder");
    case Status::TypeDifferent:
        return i18nc("@info:status", "Different types");
    case Status::SizeDifferent:
        return i18nc("@info:status", "Different sizes");
    case Status::TimeDifferent:
        return i18nc("@info:status", "Different modification times");
    case Status::MetadataMatches:
        return i18nc("@info:status", "Matching size and time");
    case Status::BothFolders:
        return i18nc("@info:status", "Folders not compared");
    case Status::LinkDifferent:
        return i18nc("@info:status", "Different link targets");
    case Status::LinkMatches:
        return i18nc("@info:status", "Matching link targets");
    case Status::Unknown:
        return i18nc("@info:status", "Insufficient information");
    }
    return i18nc("@info:status", "Unknown comparison");
}

QString describe(const KFileItem &item)
{
    if (item.isNull()) {
        return QString();
    }
    if (item.isLink()) {
        return i18nc("@info:status", "Link: %1", item.linkDest());
    }
    if (item.isDir()) {
        return i18nc("@info:status", "Folder");
    }
    if (!item.isFile()) {
        return i18nc("@info:status", "Other item");
    }
    const QString size = item.entry().contains(KIO::UDSEntry::UDS_SIZE) ? KIO::convertSize(item.size()) : i18nc("@info:status", "Unknown size");
    const QString date = item.time(KFileItem::ModificationTime).isValid() ? item.timeString() : i18nc("@info:status", "Unknown time");
    return i18nc("@info:status size and modification time", "%1, %2", size, date);
}
}

FolderComparisonDialog::FolderComparisonDialog(const QUrl &left, const QUrl &right, QWidget *parent)
    : QDialog(parent)
    , m_folders{left, right}
    , m_status(new QLabel(i18nc("@info", "Listing both folders…"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel"), this))
    , m_select(new QPushButton(i18nc("@action:button", "Select Differences in Panes"), this))
    , m_results(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("folderComparisonDialog"));
    setWindowTitle(i18nc("@title:window", "Compare Pane Folders"));
    resize(900, 500);
    auto *layout = new QVBoxLayout(this);
    auto *paths = new QLabel(i18nc("@info", "First: %1\nSecond: %2", left.toDisplayString(), right.toDisplayString()), this);
    paths->setTextFormat(Qt::PlainText);
    paths->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(paths);
    m_status->setObjectName(QStringLiteral("folderComparisonStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_results->setObjectName(QStringLiteral("folderComparisonResults"));
    m_results->setColumnCount(4);
    m_results->setHeaderLabels({i18nc("@title:column", "Name"),
                                i18nc("@title:column", "Comparison"),
                                i18nc("@title:column", "First folder"),
                                i18nc("@title:column", "Second folder")});
    m_results->setRootIsDecorated(false);
    for (int column = 0; column < 4; ++column) {
        m_results->header()->setSectionResizeMode(column, QHeaderView::Stretch);
    }
    layout->addWidget(m_results);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_select->setObjectName(QStringLiteral("folderComparisonSelect"));
    m_select->setEnabled(false);
    buttons->addButton(m_select, QDialogButtonBox::ActionRole);
    m_cancel->setObjectName(QStringLiteral("folderComparisonCancel"));
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    connect(m_select, &QPushButton::clicked, this, [this] {
        Q_EMIT selectDifferences(m_differences[0], m_differences[1]);
    });
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        cancelJobs();
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Comparison canceled."));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);

    for (int side = 0; side < 2; ++side) {
        m_jobs[side] = KIO::listDir(m_folders[side], KIO::HideProgressInfo);
        connect(m_jobs[side], &KIO::ListJob::entries, this, [this, side](KIO::Job *, const KIO::UDSEntryList &entries) {
            for (const KIO::UDSEntry &entry : entries) {
                m_items[side].append(KFileItem(entry, m_folders[side], true, true));
            }
        });
        connect(m_jobs[side], &KJob::result, this, [this, side](KJob *job) {
            m_jobs[side] = nullptr;
            if (job->error()) {
                const QString error = job->errorString();
                cancelJobs();
                m_cancel->setEnabled(false);
                m_status->setText(i18nc("@info", "Could not list %1: %2", m_folders[side].toDisplayString(), error));
            } else if (!m_jobs[0] && !m_jobs[1]) {
                finish();
            }
        });
    }
}

FolderComparisonDialog::~FolderComparisonDialog()
{
    cancelJobs();
}

void FolderComparisonDialog::cancelJobs()
{
    for (KIO::ListJob *&job : m_jobs) {
        if (job) {
            job->disconnect(this);
            job->kill(KJob::Quietly);
            job = nullptr;
        }
    }
}

void FolderComparisonDialog::finish()
{
    m_cancel->setEnabled(false);
    const FolderComparison::Result comparison = FolderComparison::compare(m_items[0], m_items[1]);
    if (!comparison.error.isEmpty()) {
        m_status->setText(comparison.error);
        return;
    }
    int differing = 0;
    for (const FolderComparison::Row &row : comparison.rows) {
        auto *item = new QTreeWidgetItem(m_results);
        item->setText(0, row.name);
        item->setText(1, statusText(row.status));
        item->setText(2, describe(row.left));
        item->setText(3, describe(row.right));
        if (FolderComparison::isDifference(row.status)) {
            ++differing;
            if (!row.left.isNull()) {
                m_differences[0].append(row.left.url());
            }
            if (!row.right.isNull()) {
                m_differences[1].append(row.right.url());
            }
        }
    }
    m_select->setEnabled(differing > 0);
    m_status->setText(i18nc("@info", "Compared immediate entries. Matching size and time do not prove equal contents; subfolders were not compared."));
}
