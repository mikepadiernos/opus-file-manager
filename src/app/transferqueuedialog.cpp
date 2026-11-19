// SPDX-License-Identifier: GPL-3.0-or-later

#include "transferqueuedialog.h"

#include "transferqueue.h"

#include <KIO/Global>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
QString stateText(QueuedTransfer::State state)
{
    switch (state) {
    case QueuedTransfer::State::Waiting:
        return i18nc("@info:status", "Waiting");
    case QueuedTransfer::State::Running:
        return i18nc("@info:status", "Running");
    case QueuedTransfer::State::Paused:
        return i18nc("@info:status", "Paused");
    case QueuedTransfer::State::Completed:
        return i18nc("@info:status", "Completed");
    case QueuedTransfer::State::Failed:
        return i18nc("@info:status", "Failed");
    case QueuedTransfer::State::Canceled:
        return i18nc("@info:status", "Canceled");
    }
    return {};
}
}

TransferQueueDialog::TransferQueueDialog(TransferQueue *queue, QWidget *parent)
    : QDialog(parent)
    , m_queue(queue)
    , m_list(new QTreeWidget(this))
    , m_status(new QLabel(this))
    , m_pause(new QPushButton(i18nc("@action:button", "Pause"), this))
    , m_resume(new QPushButton(i18nc("@action:button", "Resume"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Transfer"), this))
{
    setObjectName(QStringLiteral("transferQueueDialog"));
    setWindowTitle(i18nc("@title:window", "Transfer Queue"));
    resize(900, 450);
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(i18nc("@info", "Copy and move transfers run one at a time. Paste and drag-and-drop are not queued."), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    m_list->setObjectName(QStringLiteral("transferQueueList"));
    m_list->setRootIsDecorated(false);
    m_list->setHeaderLabels({i18nc("@title:column", "Transfer"),
                             i18nc("@title:column", "Status"),
                             i18nc("@title:column", "Progress"),
                             i18nc("@title:column", "Speed"),
                             i18nc("@title:column", "Error")});
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    layout->addWidget(m_list);
    m_status->setObjectName(QStringLiteral("transferQueueStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *clear = new QPushButton(i18nc("@action:button", "Clear Finished"), this);
    m_pause->setObjectName(QStringLiteral("transferQueuePause"));
    m_resume->setObjectName(QStringLiteral("transferQueueResume"));
    m_cancel->setObjectName(QStringLiteral("transferQueueCancel"));
    clear->setObjectName(QStringLiteral("transferQueueClear"));
    buttons->addButton(m_pause, QDialogButtonBox::ActionRole);
    buttons->addButton(m_resume, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    buttons->addButton(clear, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
    connect(m_queue, &TransferQueue::changed, this, &TransferQueueDialog::refresh);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &TransferQueueDialog::updateButtons);
    connect(m_pause, &QPushButton::clicked, this, [this] {
        if (auto *entry = selected(); entry && !entry->suspend()) {
            m_status->setText(i18nc("@info", "This transfer could not be paused."));
        }
    });
    connect(m_resume, &QPushButton::clicked, this, [this] {
        if (auto *entry = selected(); entry && !entry->resume()) {
            m_status->setText(i18nc("@info", "This transfer could not be resumed."));
        }
    });
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        auto *entry = selected();
        if (!entry
            || QMessageBox::question(this,
                                     i18nc("@title:window", "Cancel Transfer"),
                                     i18nc("@info", "Cancel this transfer? A partially transferred item may remain."),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No)
                != QMessageBox::Yes) {
            return;
        }
        if (!entry->kill(KJob::EmitResult)) {
            m_status->setText(i18nc("@info", "This transfer could not be canceled."));
        }
    });
    connect(clear, &QPushButton::clicked, m_queue, &TransferQueue::clearFinished);
    refresh();
}

QueuedTransfer *TransferQueueDialog::selected() const
{
    const auto *item = m_list->currentItem();
    auto *entry = item ? qobject_cast<QueuedTransfer *>(item->data(0, Qt::UserRole).value<QObject *>()) : nullptr;
    return m_queue->entries().contains(entry) ? entry : nullptr;
}

void TransferQueueDialog::refresh()
{
    QueuedTransfer *previous = selected();
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const auto entries = m_queue->entries();
    for (QueuedTransfer *entry : entries) {
        auto *item = new QTreeWidgetItem(m_list,
                                         {entry->description(),
                                          stateText(entry->state()),
                                          i18nc("@info:progress", "%1%", entry->percent()),
                                          entry->bytesPerSecond() ? i18nc("@info:progress", "%1/s", KIO::convertSize(entry->bytesPerSecond())) : QString(),
                                          entry->failure()});
        item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<QObject *>(entry)));
        if (entry == previous) {
            m_list->setCurrentItem(item);
        }
    }
    updateButtons();
}

void TransferQueueDialog::updateButtons()
{
    const auto *entry = selected();
    const auto state = entry ? entry->state() : QueuedTransfer::State::Completed;
    m_pause->setEnabled(entry && state == QueuedTransfer::State::Running && entry->capabilities().testFlag(KJob::Suspendable));
    m_resume->setEnabled(entry && state == QueuedTransfer::State::Paused && entry->capabilities().testFlag(KJob::Suspendable));
    m_cancel->setEnabled(entry && (state == QueuedTransfer::State::Waiting || state == QueuedTransfer::State::Running || state == QueuedTransfer::State::Paused)
                         && entry->capabilities().testFlag(KJob::Killable));
}
