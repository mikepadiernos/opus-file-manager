// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "duplicatefinder.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QUrl>

#include <atomic>
#include <memory>

namespace KIO
{
class ListJob;
}
class QLabel;
class QComboBox;
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

private:
    void cancel();
    void startHashing();
    QUrl m_folder;
    KIO::ListJob *m_listJob = nullptr;
    QFutureWatcher<DuplicateFinder::Result> m_watcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
    QStringList m_files;
    QStringList m_errors;
    QList<DuplicateFinder::Group> m_groups;
    QLabel *m_status;
    QProgressBar *m_progress;
    QTreeWidget *m_results;
    QTreeWidget *m_errorsView;
    QPushButton *m_cancel;
    QPushButton *m_copy;
    QComboBox *m_policy;
    QLineEdit *m_policyFolder;
    QPushButton *m_applyPolicy;
};
