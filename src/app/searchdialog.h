// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QUrl>

#include <atomic>
#include <memory>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(const QUrl &folder, QWidget *parent = nullptr);
    ~SearchDialog() override;

private:
    void start();
    void cancel();

    QUrl m_folder;
    QLineEdit *m_query;
    QComboBox *m_field;
    QPushButton *m_search;
    QPushButton *m_cancel;
    QLabel *m_status;
    QTreeWidget *m_results;
    QTreeWidget *m_errors;
    QFutureWatcher<LiveSearch::Result> m_watcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
