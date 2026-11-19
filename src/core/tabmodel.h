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
 * Pinned tabs come first and are never moved past the unpinned ones.
 * Pinned and locked tabs cannot be closed. Navigating a locked tab opens
 * the folder in a new tab after it instead. A tab can have a custom name.
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
    /// Closes a tab; refuses to close the last, a pinned or a locked tab.
    bool closeTab(int index);
    /// Restores the most recently closed tab, or returns -1.
    int reopenClosedTab();
    /// Moves a tab; refuses moves across the pinned boundary.
    bool moveTab(int from, int to);
    void setCurrentIndex(int index);
    void activateNext();
    void activatePrevious();

    /**
     * Navigates the current tab, pushing onto its history. When the current
     * tab is locked, opens @p url in a new tab after it instead.
     */
    void setCurrentUrl(const QUrl &url);
    /// Updates the current tab's URL without a history entry (redirection).
    void replaceCurrentUrl(const QUrl &url);
    /// False while the current tab is locked.
    bool canGoBack() const;
    /// False while the current tab is locked.
    bool canGoForward() const;
    void goBack();
    void goForward();

    /// Number of pinned tabs; they are the first ones.
    int pinnedCount() const;
    bool isPinned(int index) const;
    /**
     * Pins a tab, moving it to the end of the pinned tabs, or unpins it,
     * moving it to the start of the unpinned ones.
     */
    void setPinned(int index, bool pinned);

    bool isLocked(int index) const;
    void setLocked(int index, bool locked);

    /// The custom name of a tab, or an empty string.
    QString nameAt(int index) const;
    /// Sets a custom name; surrounding spaces are dropped and an empty name clears it.
    void setName(int index, const QString &name);

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
    /// Emitted when a tab is pinned, unpinned, locked, unlocked or renamed.
    void tabStateChanged(int index);

private:
    struct Tab {
        NavigationHistory history;
        bool locked = false;
        QString name;
    };

    void changeCurrentUrlTo(const QUrl &url);
    bool isValidIndex(int index) const;
    /// Moves a tab without checking the pinned boundary.
    void relocate(int from, int to);

    QList<Tab> m_tabs;
    QList<Tab> m_closedTabs;
    int m_current = -1;
    int m_pinnedCount = 0;
};
