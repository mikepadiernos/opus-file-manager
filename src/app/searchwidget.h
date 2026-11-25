// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"
#include "savedsearches.h"

#include <KSharedConfig>
#include <QFutureWatcher>
#include <QUrl>
#include <QWidget>

#include <atomic>
#include <memory>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class PathFilterEditor;

/// Search criteria, progress, results and collections, embedded in the search panel.
class SearchWidget : public QWidget
{
    Q_OBJECT

public:
    struct WorkspaceScope {
        bool active = false;
        QList<QUrl> locations;
    };
    using WorkspaceScopeProvider = std::function<WorkspaceScope()>;

    explicit SearchWidget(const QUrl &folder, QWidget *parent = nullptr, KSharedConfig::Ptr config = {}, WorkspaceScopeProvider workspaceScope = {});
    ~SearchWidget() override;
    /// Refreshes the workspace scope control after a workspace change.
    void refreshWorkspaceScope();
    /// Sets the current folder searched by the live scope. Ignored while a search runs.
    bool setFolder(const QUrl &folder);
    QUrl folder() const;
    bool isRunning() const;
    void focusQuery();
    /// Replaces the query text and starts a search with the current criteria.
    void search(const QString &text);

Q_SIGNALS:
    void revealRequested(const QUrl &url);
    /// Requests opening an existing result: a folder in a pane, a file in its application.
    void openRequested(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void start();
    void cancel();
    void openSelected();
    void revealSelected();
    void updateSavedSearches();
    void updateCollection(const QList<QUrl> &urls);
    PathFilter pathFilter() const;
    SavedSearches::Search currentSearch(const QString &name, const QString &id = {}) const;

    QUrl m_folder;
    KSharedConfig::Ptr m_config;
    QList<SavedSearches::Search> m_savedSearches;
    QLabel *m_folderLabel;
    QLineEdit *m_query;
    QComboBox *m_field;
    QComboBox *m_scope;
    QCheckBox *m_workspaceOnly;
    WorkspaceScopeProvider m_workspaceScope;
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
