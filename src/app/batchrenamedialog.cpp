// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrenamedialog.h"

#include "imagedetails.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

BatchRenameDialog::BatchRenameDialog(const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_paths(paths)
    , m_options(new QWidget(this))
    , m_pattern(new QLineEdit(QStringLiteral("{name}-{nn}"), m_options))
    , m_startNumber(new QSpinBox(m_options))
    , m_padding(new QSpinBox(m_options))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_preview(new QPushButton(i18nc("@action:button", "Preview"), this))
    , m_apply(new QPushButton(i18nc("@action:button", "Rename"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Operation"), this))
{
    setObjectName(QStringLiteral("batchRenameDialog"));
    setWindowTitle(i18nc("@title:window", "Batch Rename"));
    resize(850, 520);
    m_paths.sort();
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout(m_options);
    form->setContentsMargins(0, 0, 0, 0);
    m_pattern->setObjectName(QStringLiteral("batchRenamePattern"));
    m_startNumber->setObjectName(QStringLiteral("batchRenameStartNumber"));
    m_startNumber->setRange(0, 1000000000);
    m_startNumber->setValue(1);
    m_padding->setObjectName(QStringLiteral("batchRenamePadding"));
    m_padding->setRange(1, 16);
    m_padding->setValue(2);
    form->addRow(i18nc("@label", "Pattern:"), m_pattern);
    form->addRow(i18nc("@label", "Start number:"), m_startNumber);
    form->addRow(i18nc("@label", "Number padding:"), m_padding);
    layout->addWidget(m_options);
    auto *help = new QLabel(i18nc("@info",
                                  "Tokens: {name}, {ext}, {n}, {nn}, {title}, {artist}, {album}, {year}, {date}, {camera}. "
                                  "The original extension is retained unless {ext} is used."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    m_status->setObjectName(QStringLiteral("batchRenameStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setText(i18nc("@info", "Preview changes before renaming. Existing items are never overwritten."));
    layout->addWidget(m_status);
    m_results->setObjectName(QStringLiteral("batchRenameResults"));
    m_results->setRootIsDecorated(false);
    m_results->setColumnCount(3);
    m_results->setHeaderLabels({i18nc("@title:column", "Current path"), i18nc("@title:column", "New path"), i18nc("@title:column", "Problem")});
    for (int i = 0; i < 3; ++i) {
        m_results->header()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
    layout->addWidget(m_results);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_preview->setObjectName(QStringLiteral("batchRenamePreview"));
    m_apply->setObjectName(QStringLiteral("batchRenameApply"));
    m_cancel->setObjectName(QStringLiteral("batchRenameCancel"));
    m_apply->setEnabled(false);
    m_cancel->setEnabled(false);
    buttons->addButton(m_preview, QDialogButtonBox::ActionRole);
    buttons->addButton(m_apply, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &BatchRenameDialog::reject);
    connect(m_preview, &QPushButton::clicked, this, &BatchRenameDialog::preview);
    connect(m_apply, &QPushButton::clicked, this, &BatchRenameDialog::apply);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling; completed renames are not undone."));
    });
    connect(m_pattern, &QLineEdit::textChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(m_startNumber, &QSpinBox::valueChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(m_padding, &QSpinBox::valueChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(&m_previewWatcher, &QFutureWatcher<BatchRename::Plan>::finished, this, [this] {
        m_plan = m_previewWatcher.result();
        m_results->clear();
        for (const BatchRename::Row &row : std::as_const(m_plan.rows)) {
            new QTreeWidgetItem(m_results, {row.source, row.destination, row.error});
        }
        m_options->setEnabled(true);
        m_preview->setEnabled(true);
        m_apply->setEnabled(m_plan.valid());
        m_status->setText(!m_plan.error.isEmpty() ? m_plan.error
                              : m_plan.valid()    ? i18nc("@info", "Review the preview before renaming.")
                                                  : i18nc("@info", "Resolve the problems in the preview before renaming."));
    });
    connect(&m_applyWatcher, &QFutureWatcher<BatchRename::Result>::finished, this, [this] {
        const auto result = m_applyWatcher.result();
        m_cancel->setEnabled(false);
        m_apply->setEnabled(false);
        for (int i = 0; i < m_plan.rows.size(); ++i) {
            const auto &row = m_plan.rows.at(i);
            const QString state = result.renamed.contains(row.destination) ? i18nc("@info:status", "Renamed")
                : row.source == row.destination                            ? i18nc("@info:status", "Unchanged")
                                                                           : i18nc("@info:status", "Not renamed");
            m_results->topLevelItem(i)->setText(2, state);
        }
        m_status->setText(
            result.error.isEmpty()
                ? i18np("Renamed %1 item.", "Renamed %1 items.", result.renamed.size())
                : i18nc("@info", "%1 Completed renames: %2. Close and review the filesystem before trying again.", result.error, result.renamed.size()));
    });
}

BatchRenameDialog::~BatchRenameDialog()
{
    if (m_canceled) {
        m_canceled->store(true);
    }
}

void BatchRenameDialog::reject()
{
    if (m_applyWatcher.isRunning()) {
        m_canceled->store(true);
        m_status->setText(i18nc("@info", "Canceling; close this window after the operation stops."));
        return;
    }
    QDialog::reject();
}

void BatchRenameDialog::invalidatePreview()
{
    m_plan = {};
    m_apply->setEnabled(false);
    m_results->clear();
    m_status->setText(i18nc("@info", "Preview the updated pattern before renaming."));
}

void BatchRenameDialog::preview()
{
    const QStringList paths = m_paths;
    const QString pattern = m_pattern->text();
    const int start = m_startNumber->value();
    const int padding = m_padding->value();
    m_options->setEnabled(false);
    m_preview->setEnabled(false);
    m_apply->setEnabled(false);
    m_status->setText(i18nc("@info", "Preparing preview…"));
    m_previewWatcher.setFuture(QtConcurrent::run([paths, pattern, start, padding] {
        const bool metadataNeeded = pattern.contains(QStringLiteral("{title}")) || pattern.contains(QStringLiteral("{artist}"))
            || pattern.contains(QStringLiteral("{album}")) || pattern.contains(QStringLiteral("{year}")) || pattern.contains(QStringLiteral("{date}"))
            || pattern.contains(QStringLiteral("{camera}"));
        QMimeDatabase mime;
        QList<BatchRename::Input> inputs;
        for (const QString &path : paths) {
            const QFileInfo info(path);
            KFileMetaData::PropertyMultiMap metadata;
            if (metadataNeeded && info.isFile() && !info.isSymLink()) {
                metadata = ImageDetails::extractMetadata(path, mime.mimeTypeForFile(path).name());
            }
            inputs.append({path, metadata});
        }
        return BatchRename::plan(inputs, pattern, start, padding);
    }));
}

void BatchRenameDialog::apply()
{
    if (!m_plan.valid() || m_previewWatcher.isRunning() || m_applyWatcher.isRunning()) {
        return;
    }
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Confirm Batch Rename"),
                              i18nc("@info",
                                    "Rename the previewed items? Completed renames cannot be undone by this tool. "
                                    "Folder labels, colors, ratings, notes and pins remain attached to the old names."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    const auto plan = m_plan;
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    m_options->setEnabled(false);
    m_preview->setEnabled(false);
    m_apply->setEnabled(false);
    m_cancel->setEnabled(true);
    m_status->setText(i18nc("@info", "Renaming…"));
    m_applyWatcher.setFuture(QtConcurrent::run([plan, canceled] {
        return BatchRename::apply(plan, [canceled] {
            return canceled->load();
        });
    }));
}
