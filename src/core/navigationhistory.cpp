// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationhistory.h"

NavigationHistory::NavigationHistory(const QUrl &initial)
{
    if (initial.isValid()) {
        push(initial);
    }
}

QUrl NavigationHistory::current() const
{
    return m_current >= 0 ? m_entries.at(m_current) : QUrl();
}

bool NavigationHistory::isEmpty() const
{
    return m_entries.isEmpty();
}

int NavigationHistory::size() const
{
    return m_entries.size();
}

int NavigationHistory::currentIndex() const
{
    return m_current;
}

QList<QUrl> NavigationHistory::entries() const
{
    return m_entries;
}

void NavigationHistory::push(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }
    const QUrl stripped = url.adjusted(QUrl::StripTrailingSlash);
    if (m_current >= 0 && m_entries.at(m_current) == stripped) {
        return;
    }
    if (m_current < m_entries.size() - 1) {
        m_entries.erase(m_entries.begin() + m_current + 1, m_entries.end());
    }
    m_entries.append(stripped);
    if (m_entries.size() > MaxEntries) {
        m_entries.removeFirst();
    }
    m_current = m_entries.size() - 1;
}

void NavigationHistory::replaceCurrent(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }
    if (m_current < 0) {
        push(url);
        return;
    }
    m_entries[m_current] = url.adjusted(QUrl::StripTrailingSlash);
}

bool NavigationHistory::canGoBack() const
{
    return m_current > 0;
}

bool NavigationHistory::canGoForward() const
{
    return m_current >= 0 && m_current < m_entries.size() - 1;
}

QUrl NavigationHistory::goBack()
{
    if (canGoBack()) {
        --m_current;
    }
    return current();
}

QUrl NavigationHistory::goForward()
{
    if (canGoForward()) {
        ++m_current;
    }
    return current();
}
