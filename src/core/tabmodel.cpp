// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabmodel.h"

TabModel::TabModel(QObject *parent)
    : QObject(parent)
{
}

int TabModel::count() const
{
    return m_tabs.size();
}

int TabModel::currentIndex() const
{
    return m_current;
}

QUrl TabModel::urlAt(int index) const
{
    return index >= 0 && index < m_tabs.size() ? m_tabs.at(index).current() : QUrl();
}

QUrl TabModel::currentUrl() const
{
    return urlAt(m_current);
}

QList<QUrl> TabModel::urls() const
{
    QList<QUrl> result;
    result.reserve(m_tabs.size());
    for (const NavigationHistory &tab : m_tabs) {
        result.append(tab.current());
    }
    return result;
}

const NavigationHistory &TabModel::historyAt(int index) const
{
    static const NavigationHistory empty;
    return index >= 0 && index < m_tabs.size() ? m_tabs.at(index) : empty;
}

int TabModel::addTab(const QUrl &url, bool activate)
{
    return insertTab(m_tabs.size(), url, activate);
}

int TabModel::insertTab(int index, const QUrl &url, bool activate)
{
    if (!url.isValid()) {
        return -1;
    }
    if (index < 0 || index > m_tabs.size()) {
        index = m_tabs.size();
    }
    m_tabs.insert(index, NavigationHistory(url));
    if (m_current >= index) {
        ++m_current;
    }
    Q_EMIT tabInserted(index);
    if (activate || m_current < 0) {
        setCurrentIndex(index);
    }
    return index;
}

bool TabModel::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size() || m_tabs.size() <= 1) {
        return false;
    }
    m_closedTabs.append(m_tabs.takeAt(index));
    if (m_closedTabs.size() > MaxClosedTabs) {
        m_closedTabs.removeFirst();
    }
    Q_EMIT tabRemoved(index);

    if (index < m_current) {
        --m_current;
        Q_EMIT currentIndexChanged(m_current);
    } else if (index == m_current) {
        m_current = qMin(index, m_tabs.size() - 1);
        Q_EMIT currentIndexChanged(m_current);
        Q_EMIT currentUrlChanged(currentUrl());
    }
    return true;
}

int TabModel::reopenClosedTab()
{
    if (m_closedTabs.isEmpty()) {
        return -1;
    }
    const NavigationHistory tab = m_closedTabs.takeLast();
    const int index = m_tabs.size();
    m_tabs.append(tab);
    Q_EMIT tabInserted(index);
    setCurrentIndex(index);
    return index;
}

bool TabModel::moveTab(int from, int to)
{
    if (from < 0 || from >= m_tabs.size() || to < 0 || to >= m_tabs.size() || from == to) {
        return false;
    }
    m_tabs.move(from, to);
    if (m_current == from) {
        m_current = to;
    } else if (from < m_current && to >= m_current) {
        --m_current;
    } else if (from > m_current && to <= m_current) {
        ++m_current;
    }
    Q_EMIT tabMoved(from, to);
    return true;
}

void TabModel::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_current) {
        return;
    }
    m_current = index;
    Q_EMIT currentIndexChanged(index);
    Q_EMIT currentUrlChanged(currentUrl());
}

void TabModel::activateNext()
{
    if (m_tabs.size() > 1) {
        setCurrentIndex((m_current + 1) % m_tabs.size());
    }
}

void TabModel::activatePrevious()
{
    if (m_tabs.size() > 1) {
        setCurrentIndex((m_current + m_tabs.size() - 1) % m_tabs.size());
    }
}

void TabModel::setCurrentUrl(const QUrl &url)
{
    if (m_current < 0) {
        addTab(url);
        return;
    }
    const QUrl before = currentUrl();
    m_tabs[m_current].push(url);
    if (currentUrl() != before) {
        changeCurrentUrlTo(currentUrl());
    }
}

void TabModel::replaceCurrentUrl(const QUrl &url)
{
    if (m_current < 0 || !url.isValid()) {
        return;
    }
    const QUrl before = currentUrl();
    m_tabs[m_current].replaceCurrent(url);
    if (currentUrl() != before) {
        changeCurrentUrlTo(currentUrl());
    }
}

bool TabModel::canGoBack() const
{
    return m_current >= 0 && m_tabs.at(m_current).canGoBack();
}

bool TabModel::canGoForward() const
{
    return m_current >= 0 && m_tabs.at(m_current).canGoForward();
}

void TabModel::goBack()
{
    if (canGoBack()) {
        changeCurrentUrlTo(m_tabs[m_current].goBack());
    }
}

void TabModel::goForward()
{
    if (canGoForward()) {
        changeCurrentUrlTo(m_tabs[m_current].goForward());
    }
}

void TabModel::changeCurrentUrlTo(const QUrl &url)
{
    Q_EMIT tabUrlChanged(m_current, url);
    Q_EMIT currentUrlChanged(url);
}
