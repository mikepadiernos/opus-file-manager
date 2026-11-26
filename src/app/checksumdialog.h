// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "checksums.h"
#include "operationjob.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;

class ChecksumDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Create,
        Verify
    };
    ChecksumDialog(Mode mode, const QStringList &paths, const QString &manifest, QWidget *parent = nullptr);
    ~ChecksumDialog() override;

private:
    QFutureWatcher<Checksums::Result> m_watcher;
    QLabel *m_status;
    QProgressBar *m_progress;
    QPushButton *m_cancel;
    QTreeWidget *m_results;
    QPointer<OperationJob> m_job;
};
