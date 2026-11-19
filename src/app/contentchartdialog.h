// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>

#include <QDialog>
#include <QHash>
#include <QList>

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace KIO
{
class DirectorySizeJob;
class ListJob;
}

class ContentChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContentChartDialog(const QUrl &folder, QWidget *parent = nullptr);
    ~ContentChartDialog() override;

private:
    struct Entry {
        KFileItem item;
        QTreeWidgetItem *row = nullptr;
        quint64 size = 0;
        bool measured = false;
    };

    void startSizes();
    void updateChart();
    void cancel();

    QUrl m_folder;
    KIO::ListJob *m_listJob = nullptr;
    QHash<KIO::DirectorySizeJob *, int> m_jobs;
    QList<int> m_queue;
    QList<Entry> m_entries;
    QLabel *m_status;
    QProgressBar *m_progress;
    QPushButton *m_cancel;
    QTreeWidget *m_tree;
    int m_done = 0;
    int m_skipped = 0;
    int m_errors = 0;
    bool m_finished = false;
};
