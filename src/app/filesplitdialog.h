// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "filesplitter.h"
#include "operationjob.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>

#include <atomic>
#include <memory>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QWidget;

class FileSplitDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Split,
        Combine
    };
    FileSplitDialog(Mode mode, const QStringList &paths, QWidget *parent = nullptr);
    ~FileSplitDialog() override;

    void reject() override;

private:
    void start();
    Mode m_mode;
    QFutureWatcher<FileSplitter::Result> m_watcher;
    QPointer<OperationJob> m_job;
    std::shared_ptr<std::atomic_bool> m_canceled;
    QWidget *m_options;
    QListWidget *m_inputs;
    QLineEdit *m_destination;
    QLineEdit *m_partSize;
    QLabel *m_status;
    QListWidget *m_outputs;
    QPushButton *m_start;
    QPushButton *m_cancel;
};
