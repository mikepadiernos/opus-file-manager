// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "duplicatefinder.h"
#include "operationjob.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>
#include <QUrl>

#include <atomic>
#include <memory>

namespace KIO
{
class ListJob;
}
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTreeWidget;

class DuplicateFinderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DuplicateFinderDialog(const QUrl &folder, QWidget *parent = nullptr);
    ~DuplicateFinderDialog() override;

Q_SIGNALS:
    void trashRequested(const QList<QUrl> &urls);

private:
    void cancel();
    void startScan();
    void startHashing();
    QUrl m_folder;
    bool m_requireSameFileName = false;
    bool m_scanComplete = false;
    KIO::ListJob *m_listJob = nullptr;
    QFutureWatcher<DuplicateFinder::Result> m_watcher;
    QPointer<OperationJob> m_job;
    QFutureWatcher<QStringList> m_validationWatcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
    QStringList m_files;
    QStringList m_errors;
    QList<DuplicateFinder::Group> m_groups;
    QLabel *m_status;
    QLabel *m_criteria;
    QCheckBox *m_sameFileName;
    QProgressBar *m_progress;
    QTreeWidget *m_results;
    QTreeWidget *m_errorsView;
    QPushButton *m_cancel;
    QPushButton *m_rescan;
    QPushButton *m_copy;
    QComboBox *m_policy;
    QLineEdit *m_policyFolder;
    QPushButton *m_applyPolicy;
    QPushButton *m_trash;
};
