// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"
#include "savedsearches.h"

#include <KSharedConfig>
#include <QDialog>
#include <QFutureWatcher>
#include <QUrl>

#include <atomic>
#include <memory>

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class PathFilterEditor;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(const QUrl &folder, QWidget *parent = nullptr, KSharedConfig::Ptr config = {});
    ~SearchDialog() override;

Q_SIGNALS:
    void revealRequested(const QUrl &url);

private:
    void start();
    void cancel();
    void updateSavedSearches();
    void updateCollection(const QList<QUrl> &urls);
    PathFilter pathFilter() const;
    SavedSearches::Search currentSearch(const QString &name) const;

    QUrl m_folder;
    KSharedConfig::Ptr m_config;
    QList<SavedSearches::Search> m_savedSearches;
    QLabel *m_folderLabel;
    QLineEdit *m_query;
    QComboBox *m_field;
    QComboBox *m_scope;
    PathFilterEditor *m_filterEditor;
    QComboBox *m_saved;
    QPushButton *m_search;
    QPushButton *m_cancel;
    QLabel *m_status;
    QProgressBar *m_progress;
    QTreeWidget *m_results;
    QTreeWidget *m_collection;
    QTabWidget *m_resultTabs;
    QTreeWidget *m_errors;
    QFutureWatcher<LiveSearch::Result> m_watcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
