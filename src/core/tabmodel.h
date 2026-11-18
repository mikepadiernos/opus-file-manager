// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "navigationhistory.h"

#include <QList>
#include <QObject>
#include <QUrl>

/**
 * The tabs of one pane: each tab is a navigation history whose current
 * entry is the folder shown. The model always keeps at least one tab.
 *
 * Widgets observe the signals; nothing here depends on Qt Widgets.
 */
class TabModel : public QObject
{
    Q_OBJECT

public:
    explicit TabModel(QObject *parent = nullptr);

    int count() const;
    int currentIndex() const;
    QUrl urlAt(int index) const;
    QUrl currentUrl() const;
    QList<QUrl> urls() const;
    const NavigationHistory &historyAt(int index) const;

    /// Appends a tab and returns its index. Does nothing for invalid URLs.
    int addTab(const QUrl &url, bool activate = true);
    /// Inserts a tab at @p index; indexes out of range append.
    int insertTab(int index, const QUrl &url, bool activate = true);
    /// Closes a tab; refuses to close the last one.
    bool closeTab(int index);
    /// Restores the most recently closed tab, or returns -1.
    int reopenClosedTab();
    bool moveTab(int from, int to);
    void setCurrentIndex(int index);
    void activateNext();
    void activatePrevious();

    /// Navigates the current tab, pushing onto its history.
    void setCurrentUrl(const QUrl &url);
    /// Updates the current tab's URL without a history entry (redirection).
    void replaceCurrentUrl(const QUrl &url);
    bool canGoBack() const;
    bool canGoForward() const;
    void goBack();
    void goForward();

    static constexpr int MaxClosedTabs = 10;

Q_SIGNALS:
    void tabInserted(int index);
    void tabRemoved(int index);
    void tabMoved(int from, int to);
    void currentIndexChanged(int index);
    /// Emitted when the URL shown by the tab at @p index changes.
    void tabUrlChanged(int index, const QUrl &url);
    /// Emitted whenever the current tab's URL changes for any reason.
    void currentUrlChanged(const QUrl &url);

private:
    void changeCurrentUrlTo(const QUrl &url);

    QList<NavigationHistory> m_tabs;
    QList<NavigationHistory> m_closedTabs;
    int m_current = -1;
};
