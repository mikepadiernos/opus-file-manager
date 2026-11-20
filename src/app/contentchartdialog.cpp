// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentchartdialog.h"
#include "cushiontreemap.h"
#include "localizednumbers.h"
#include "settings.h"

#include <KIO/DirectorySizeJob>
#include <KIO/Global>
#include <KIO/ListJob>
#include <KIO/UDSEntry>
#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QStyleOptionProgressBar>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <limits>
#include <utility>

namespace
{
class ChartRow : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        const QTreeWidget *tree = treeWidget();
        if (tree->property("listingOrder").toBool()) {
            return data(0, Qt::UserRole).toInt() < other.data(0, Qt::UserRole).toInt();
        }
        if (tree->sortColumn() == 0) {
            return QString::localeAwareCompare(text(0), other.text(0)) < 0;
        }
        const QVariant left = data(1, Qt::UserRole);
        const QVariant right = other.data(1, Qt::UserRole);
        if (left.isValid() != right.isValid()) {
            return tree->header()->sortIndicatorOrder() == Qt::AscendingOrder ? left.isValid() : !left.isValid();
        }
        if (!left.isValid()) {
            return QString::localeAwareCompare(text(0), other.text(0)) < 0;
        }
        return left.toULongLong() < right.toULongLong();
    }
};

class ShareDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QVariant value = index.data(Qt::UserRole);
        if (!value.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionProgressBar bar;
        bar.rect = option.rect.adjusted(3, 3, -3, -3);
        bar.minimum = 0;
        bar.maximum = 1000;
        bar.progress = value.toInt();
        bar.textVisible = true;
        bar.text = index.data(Qt::DisplayRole).toString();
        bar.textAlignment = Qt::AlignCenter;
        bar.state = option.state;
        bar.palette = option.palette;
        option.widget->style()->drawControl(QStyle::CE_ProgressBar, &bar, painter, option.widget);
    }
};
}

ContentChartDialog::ContentChartDialog(const QUrl &folder, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
    , m_status(new QLabel(i18nc("@info", "Listing folder…"), this))
    , m_progress(new QProgressBar(this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Scan"), this))
    , m_tree(new QTreeWidget(this))
    , m_treemap(new CushionTreemap(this))
{
    setObjectName(QStringLiteral("contentChartDialog"));
    setWindowTitle(i18nc("@title:window", "Folder Content Chart"));
    resize(750, 500);
    auto *layout = new QVBoxLayout(this);
    auto *path = new QLabel(folder.toDisplayString(), this);
    path->setObjectName(QStringLiteral("contentChartFolder"));
    path->setTextFormat(Qt::PlainText);
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(path);
    m_status->setObjectName(QStringLiteral("contentChartStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_progress->setObjectName(QStringLiteral("contentChartProgress"));
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);
    m_tree->setObjectName(QStringLiteral("contentChartEntries"));
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({i18nc("@title:column", "Item"), i18nc("@title:column", "Size"), i18nc("@title:column", "Share")});
    m_tree->setRootIsDecorated(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->setItemDelegateForColumn(2, new ShareDelegate(m_tree));
    auto *controls = new QHBoxLayout;
    auto *showTreemap = new QCheckBox(i18nc("@option:check", "Show treemap"), this);
    showTreemap->setObjectName(QStringLiteral("contentChartShowTreemap"));
    showTreemap->setChecked(Settings::contentChartShowTreemap());
    controls->addWidget(showTreemap);
    controls->addWidget(new QLabel(i18nc("@label:listbox", "Sort:"), this));
    auto *sort = new QComboBox(this);
    sort->setObjectName(QStringLiteral("contentChartSort"));
    sort->addItems(
        {i18nc("@item:inlistbox", "As listed"), i18nc("@item:inlistbox", "Name"), i18nc("@item:inlistbox", "Size"), i18nc("@item:inlistbox", "Share")});
    sort->setCurrentIndex(Settings::contentChartSort());
    controls->addWidget(sort);
    controls->addStretch();
    layout->addLayout(controls);
    auto *hint = new QLabel(i18nc("@info", "Select a treemap tile to locate its entry; activate a folder in the list to inspect it."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_treemap);
    splitter->addWidget(m_tree);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter);
    m_treemap->setVisible(showTreemap->isChecked());
    const auto updateHint = [hint](bool visible) {
        hint->setText(visible ? i18nc("@info", "Select a treemap tile to locate its entry; activate a folder in the list to inspect it.")
                              : i18nc("@info", "Activate a folder in the list to inspect it."));
    };
    updateHint(showTreemap->isChecked());
    connect(showTreemap, &QCheckBox::toggled, m_treemap, &QWidget::setVisible);
    connect(showTreemap, &QCheckBox::toggled, hint, updateHint);
    const auto applySort = [this](int index) {
        if (index == 0) {
            m_tree->setSortingEnabled(false);
            m_tree->setProperty("listingOrder", true);
            m_tree->sortItems(0, Qt::AscendingOrder);
            m_tree->setProperty("listingOrder", false);
            m_tree->header()->setSortIndicatorShown(false);
        } else {
            m_tree->setSortingEnabled(true);
            m_tree->sortItems(index - 1, Settings::contentChartSortDescending() ? Qt::DescendingOrder : Qt::AscendingOrder);
        }
    };
    connect(sort, &QComboBox::currentIndexChanged, this, applySort);
    connect(m_tree->header(), &QHeaderView::sortIndicatorChanged, sort, [this, sort](int column) {
        if (!m_tree->property("listingOrder").toBool() && sort->currentIndex() != 0) {
            const QSignalBlocker blocker(sort);
            sort->setCurrentIndex(column + 1);
        }
    });
    applySort(sort->currentIndex());
    connect(m_treemap, &CushionTreemap::activated, this, [this](int index) {
        m_tree->setCurrentItem(m_entries.at(index).row);
        m_tree->scrollToItem(m_entries.at(index).row);
    });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *row) {
        const KFileItem item = m_entries.at(row->data(0, Qt::UserRole).toInt()).item;
        if (item.isDir() && !item.isLink()) {
            auto *dialog = new ContentChartDialog(item.url(), this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        }
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_cancel->setObjectName(QStringLiteral("contentChartCancel"));
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    connect(m_cancel, &QPushButton::clicked, this, &ContentChartDialog::cancel);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);

    m_listJob = KIO::listDir(folder, KIO::HideProgressInfo);
    connect(m_listJob, &KIO::ListJob::entries, this, [this](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const KIO::UDSEntry &entry : entries) {
            const KFileItem item(entry, m_folder, true, true);
            if (item.name() == QLatin1String(".") || item.name() == QLatin1String("..")) {
                continue;
            }
            auto *row = new ChartRow(m_tree);
            row->setText(0, item.name());
            const int index = m_entries.size();
            m_entries.append({item, row});
            row->setData(0, Qt::UserRole, index);
            if (item.isLink()) {
                row->setText(1, i18nc("@info:status", "Link skipped"));
                ++m_skipped;
            } else if (item.isDir()) {
                row->setText(1, i18nc("@info:status", "Pending"));
                m_queue.append(index);
            } else if (item.isFile() && entry.contains(KIO::UDSEntry::UDS_SIZE)) {
                m_entries[index].size = item.size();
                m_entries[index].measured = true;
                row->setText(1, LocalizedNumbers::shape(KIO::convertSize(item.size())));
                row->setData(1, Qt::UserRole, item.size());
            } else {
                row->setText(1, i18nc("@info:status", "Size unavailable"));
                ++m_skipped;
            }
        }
    });
    connect(m_listJob, &KJob::result, this, [this](KJob *job) {
        m_listJob = nullptr;
        if (job->error()) {
            m_finished = true;
            m_cancel->setEnabled(false);
            m_progress->setRange(0, 1);
            m_status->setText(i18nc("@info", "Could not list the folder: %1", job->errorString()));
            return;
        }
        m_progress->setRange(0, m_queue.size());
        if (m_queue.isEmpty()) {
            m_progress->setRange(0, 1);
            m_progress->setValue(1);
        }
        startSizes();
    });
}

ContentChartDialog::~ContentChartDialog()
{
    cancel();
}

void ContentChartDialog::startSizes()
{
    while (m_jobs.size() < 4 && !m_queue.isEmpty()) {
        const int index = m_queue.takeFirst();
        KIO::DirectorySizeJob *job = KIO::directorySize(KFileItemList{m_entries.at(index).item});
        m_jobs.insert(job, index);
        connect(job, &KJob::result, this, [this, job] {
            const int index = m_jobs.take(job);
            Entry &entry = m_entries[index];
            if (job->error()) {
                entry.row->setText(1, i18nc("@info:status", "Error: %1", job->errorString()));
                entry.row->setToolTip(1, job->errorString());
                ++m_errors;
            } else {
                entry.size = job->totalSize();
                entry.measured = true;
                entry.row->setText(1, LocalizedNumbers::shape(KIO::convertSize(entry.size)));
                entry.row->setData(1, Qt::UserRole, entry.size);
            }
            m_progress->setValue(++m_done);
            startSizes();
        });
    }
    if (m_jobs.isEmpty() && m_queue.isEmpty()) {
        m_finished = true;
        m_cancel->setEnabled(false);
        updateChart();
    } else {
        m_status->setText(i18nc("@info", "Calculating folder sizes: %1 of %2", m_done, m_progress->maximum()));
    }
}

void ContentChartDialog::updateChart()
{
    long double total = 0;
    QList<CushionTreemap::Item> tiles;
    for (const Entry &entry : std::as_const(m_entries)) {
        tiles.append({entry.item.name(), entry.measured ? entry.size : 0});
        if (entry.measured) {
            total += entry.size;
        }
    }
    m_treemap->setItems(tiles);
    for (const Entry &entry : std::as_const(m_entries)) {
        if (!entry.measured) {
            continue;
        }
        const int share = total > 0 ? qRound(1000 * static_cast<double>(entry.size / total)) : 0;
        entry.row->setData(2, Qt::UserRole, share);
        entry.row->setText(2, i18nc("@info:percent", "%1%", LocalizedNumbers::shape(QLocale().toString(share / 10.0, 'f', 1))));
    }
    const QString size = total <= std::numeric_limits<quint64>::max() ? LocalizedNumbers::shape(KIO::convertSize(static_cast<quint64>(total)))
                                                                      : i18nc("@info", "more than the supported size");
    if (m_errors || m_skipped) {
        m_status->setText(i18nc("@info", "Measured total: %1. Skipped: %2; errors: %3. Shares exclude unmeasured items.", size, m_skipped, m_errors));
    } else {
        m_status->setText(i18nc("@info", "Total: %1", size));
    }
}

void ContentChartDialog::cancel()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    if (m_listJob) {
        m_listJob->disconnect(this);
        m_listJob->kill(KJob::Quietly);
        m_listJob = nullptr;
    }
    const auto jobs = m_jobs.keys();
    m_jobs.clear();
    m_queue.clear();
    for (KIO::DirectorySizeJob *job : jobs) {
        job->disconnect(this);
        job->kill(KJob::Quietly);
    }
    m_cancel->setEnabled(false);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_status->setText(i18nc("@info", "Scan canceled; the chart is incomplete."));
}
