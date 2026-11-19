// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldercomparison.h"

#include <QDialog>
#include <QUrl>

class QLabel;
class QPushButton;
class QTreeWidget;
namespace KIO
{
class ListJob;
}

class FolderComparisonDialog : public QDialog
{
    Q_OBJECT

public:
    FolderComparisonDialog(const QUrl &left, const QUrl &right, QWidget *parent = nullptr);
    ~FolderComparisonDialog() override;

Q_SIGNALS:
    void selectDifferences(const QList<QUrl> &left, const QList<QUrl> &right);

private:
    void finish();
    void cancelJobs();

    QUrl m_folders[2];
    KIO::ListJob *m_jobs[2]{nullptr, nullptr};
    KFileItemList m_items[2];
    QList<QUrl> m_differences[2];
    QLabel *m_status;
    QPushButton *m_cancel;
    QPushButton *m_select;
    QTreeWidget *m_results;
};
