// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidgetAction>

#include <functional>

class QLineEdit;
class QMenu;

/**
 * Optional search field that can be placed on a toolbar. Each toolbar gets its
 * own line edit. The field searches with the search panel's criteria, so it
 * shares the panel's scope, filters and saved searches.
 */
class SearchToolbarAction : public QWidgetAction
{
    Q_OBJECT

public:
    using SavedSearchProvider = std::function<QStringList()>;
    using HistoryProvider = std::function<QStringList()>;

    explicit SearchToolbarAction(QObject *parent = nullptr);
    /// Sets the source of the saved search names offered by each field's menu.
    void setSavedSearchProvider(SavedSearchProvider provider);
    /// Sets the source of the recent queries each field suggests, reread when a field gains focus.
    void setHistoryProvider(HistoryProvider provider);

Q_SIGNALS:
    /// Emitted when Return is pressed in a field; @p text may be empty.
    void searchRequested(const QString &text);
    /// Emitted when a saved search is chosen from a field's menu.
    void savedSearchRequested(const QString &name);
    /// Emitted when Down is pressed in a field, to move focus to the results.
    void resultsRequested();
    /// Emitted when Escape is pressed in a field.
    void escapePressed();

protected:
    QWidget *createWidget(QWidget *parent) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showSavedSearches(QWidget *field);
    /// Refreshes @p field's suggestions; returns whether there are any.
    bool refreshHistory(QLineEdit *field);

    SavedSearchProvider m_savedSearches;
    HistoryProvider m_history;
};
