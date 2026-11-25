// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"
#include "savedsearches.h"
#include "searchdefaults.h"

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
class QStringListModel;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
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
    /// Focuses the visible result list when it has items; returns whether it did.
    bool focusResults();
    /// Replaces the query text and starts a search with the current criteria.
    void search(const QString &text);
    /// Cancels a running search; does nothing otherwise.
    void cancel();
    /**
     * Applies changed search defaults from Preferences. Criteria still showing
     * the previous defaults change; criteria the user changed are kept. While
     * a search runs, the defaults are applied when it finishes.
     */
    void applyDefaults();
    /// Rereads the saved searches and returns their names, for the toolbar search field.
    QStringList savedSearchNames();
    /// Loads the named saved search's criteria and starts it; returns whether it started.
    bool runSavedSearch(const QString &name);
    /// Rereads and returns the recent queries, most recent first; empty when history is not retained.
    QStringList history();

Q_SIGNALS:
    /// Escape was pressed while no search was running.
    void dismissRequested();
    void revealRequested(const QUrl &url);
    /// Requests opening an existing result: a folder in a pane, a file in its application.
    void openRequested(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool start();
    QString applyDefaultsNow();
    bool loadSavedSearch(const QString &name);
    void openSelected();
    void revealSelected();
    void updateSavedSearches();
    void updateCollection(const QList<QUrl> &urls);
    void applyPresentation();
    void setItemPath(QTreeWidgetItem *item, const QString &path) const;
    void recordHistory(const QString &query);
    void showHistory();
    PathFilter pathFilter() const;
    SavedSearches::Search currentSearch(const QString &name, const QString &id = {}) const;

    QUrl m_folder;
    KSharedConfig::Ptr m_config;
    QList<SavedSearches::Search> m_savedSearches;
    QLabel *m_folderLabel;
    QLineEdit *m_query;
    QStringListModel *m_historyModel;
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
    QComboBox *m_presentation;
    QTreeWidget *m_errors;
    QFutureWatcher<LiveSearch::Result> m_watcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
    SearchDefaults::Defaults m_appliedDefaults;
    PathFilter m_appliedFilter;
    bool m_defaultsPending = false;
};
