// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTreeWidget;
class TransferQueue;
class QueuedTransfer;

class TransferQueueDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransferQueueDialog(TransferQueue *queue, QWidget *parent = nullptr);

private:
    QueuedTransfer *selected() const;
    void refresh();
    void updateButtons();

    TransferQueue *m_queue;
    QTreeWidget *m_list;
    QLabel *m_status;
    QPushButton *m_pause;
    QPushButton *m_resume;
    QPushButton *m_cancel;
};
