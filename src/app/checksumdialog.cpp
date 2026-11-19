// SPDX-License-Identifier: GPL-3.0-or-later

#include "checksumdialog.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPromise>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

ChecksumDialog::ChecksumDialog(Mode mode, const QStringList &paths, const QString &manifest, QWidget *parent)
    : QDialog(parent)
    , m_status(new QLabel(i18nc("@info", "Starting…"), this))
    , m_progress(new QProgressBar(this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel"), this))
    , m_results(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("checksumDialog"));
    setWindowTitle(mode == Mode::Create ? i18nc("@title:window", "Create Checksum File") : i18nc("@title:window", "Verify Checksum File"));
    resize(650, 400);
    auto *layout = new QVBoxLayout(this);
    m_status->setObjectName(QStringLiteral("checksumStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_progress->setObjectName(QStringLiteral("checksumProgress"));
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);
    m_results->setObjectName(QStringLiteral("checksumResults"));
    m_results->setColumnCount(2);
    m_results->setHeaderLabels({i18nc("@title:column", "File"), i18nc("@title:column", "Result")});
    m_results->setRootIsDecorated(false);
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_results);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_watcher.cancel();
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling…"));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
    connect(&m_watcher, &QFutureWatcher<Checksums::Result>::progressRangeChanged, m_progress, &QProgressBar::setRange);
    connect(&m_watcher, &QFutureWatcher<Checksums::Result>::progressValueChanged, m_progress, &QProgressBar::setValue);
    connect(&m_watcher, &QFutureWatcher<Checksums::Result>::progressTextChanged, this, [this](const QString &name) {
        m_status->setText(i18nc("@info", "Processing: %1", name));
    });
    connect(&m_watcher, &QFutureWatcher<Checksums::Result>::finished, this, [this, mode] {
        m_cancel->setEnabled(false);
        if (m_watcher.isCanceled()) {
            m_status->setText(i18nc("@info", "Canceled. If creation had already finished, the checksum file may still exist."));
            return;
        }
        const Checksums::Result result = m_watcher.result();
        if (!result.error.isEmpty()) {
            m_status->setText(i18nc("@info", "Checksum operation failed: %1", result.error));
            return;
        }
        int failures = 0;
        for (const Checksums::Entry &entry : result.entries) {
            auto *row = new QTreeWidgetItem(m_results);
            row->setText(0, entry.name);
            row->setText(1, entry.error.isEmpty() ? i18nc("@info:status", "OK") : entry.error);
            failures += !entry.error.isEmpty();
        }
        m_progress->setRange(0, 1);
        m_progress->setValue(1);
        if (mode == Mode::Create) {
            m_status->setText(i18nc("@info", "Checksum file created."));
        } else if (failures) {
            m_status->setText(i18np("Verification finished: %1 file failed.", "Verification finished: %1 files failed.", failures));
        } else {
            m_status->setText(i18nc("@info", "All files verified."));
        }
    });
    m_watcher.setFuture(QtConcurrent::run([mode, paths, manifest](QPromise<Checksums::Result> &promise) {
        const Checksums::Cancel cancel = [&promise] {
            return promise.isCanceled();
        };
        const Checksums::Progress progress = [&promise](int current, int total, const QString &name) {
            promise.setProgressRange(0, total);
            promise.setProgressValueAndText(current, name);
        };
        promise.addResult(mode == Mode::Create ? Checksums::create(paths, manifest, cancel, progress) : Checksums::verify(manifest, cancel, progress));
    }));
}

ChecksumDialog::~ChecksumDialog()
{
    m_watcher.cancel();
}
