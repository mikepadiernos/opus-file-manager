// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QUrl>

/**
 * Back/forward history of one tab.
 *
 * Pushing a URL while positioned before the end discards the forward
 * entries, like a browser. Pushing the current URL again is a no-op so
 * that reloads and redirections do not pollute the history. URLs are
 * stored without a trailing slash.
 */
class NavigationHistory
{
public:
    struct ViewState {
        QList<QUrl> selectedUrls;
        int verticalScroll = 0;
        int horizontalScroll = 0;
        bool valid = false;
    };

    explicit NavigationHistory(const QUrl &initial = QUrl());

    QUrl current() const;
    bool isEmpty() const;
    int size() const;
    int currentIndex() const;
    QList<QUrl> entries() const;
    ViewState currentViewState() const;
    void setCurrentViewState(const ViewState &state);

    void push(const QUrl &url);
    /// Replaces the current entry without touching the forward list.
    void replaceCurrent(const QUrl &url);

    bool canGoBack() const;
    bool canGoForward() const;
    QUrl goBack();
    QUrl goForward();

    static constexpr int MaxEntries = 100;

private:
    QList<QUrl> m_entries;
    QList<ViewState> m_viewStates;
    int m_current = -1;
};
