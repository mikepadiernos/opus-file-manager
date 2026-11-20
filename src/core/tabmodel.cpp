// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabmodel.h"

#include <utility>

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
    return isValidIndex(index) ? m_tabs.at(index).history.current() : QUrl();
}

QUrl TabModel::currentUrl() const
{
    return urlAt(m_current);
}

QList<QUrl> TabModel::urls() const
{
    QList<QUrl> result;
    result.reserve(m_tabs.size());
    for (const Tab &tab : m_tabs) {
        result.append(tab.history.current());
    }
    return result;
}

const NavigationHistory &TabModel::historyAt(int index) const
{
    static const NavigationHistory empty;
    return isValidIndex(index) ? m_tabs.at(index).history : empty;
}

void TabModel::setCurrentViewState(const NavigationHistory::ViewState &state)
{
    if (m_current >= 0) {
        m_tabs[m_current].history.setCurrentViewState(state);
    }
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
    if (index < m_pinnedCount || index > m_tabs.size()) {
        index = index < m_pinnedCount ? m_pinnedCount : m_tabs.size();
    }
    m_tabs.insert(index, Tab{NavigationHistory(url), false, QString()});
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
    if (!isValidIndex(index) || m_tabs.size() <= 1 || isPinned(index) || m_tabs.at(index).locked) {
        return false;
    }
    if (index == m_current) {
        Q_EMIT currentAboutToChange();
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
    const int index = m_tabs.size();
    m_tabs.append(m_closedTabs.takeLast());
    Q_EMIT tabInserted(index);
    setCurrentIndex(index);
    return index;
}

bool TabModel::moveTab(int from, int to)
{
    if (!isValidIndex(from) || !isValidIndex(to) || from == to || isPinned(from) != isPinned(to)) {
        return false;
    }
    relocate(from, to);
    return true;
}

bool TabModel::swapCurrent(TabModel &other)
{
    if (&other == this || !isValidIndex(m_current) || !other.isValidIndex(other.m_current)) {
        return false;
    }
    Q_EMIT currentAboutToChange();
    Q_EMIT other.currentAboutToChange();
    const int oldIndex = m_current;
    const int oldOtherIndex = other.m_current;
    const bool pinned = isPinned(m_current);
    const bool otherPinned = other.isPinned(other.m_current);
    std::swap(m_tabs[m_current], other.m_tabs[other.m_current]);
    if (pinned != otherPinned) {
        if (pinned) {
            --m_pinnedCount;
            if (m_current != m_pinnedCount) {
                relocate(m_current, m_pinnedCount);
            }
            const int position = other.m_pinnedCount++;
            if (other.m_current != position) {
                other.relocate(other.m_current, position);
            }
        } else {
            const int position = m_pinnedCount++;
            if (m_current != position) {
                relocate(m_current, position);
            }
            --other.m_pinnedCount;
            if (other.m_current != other.m_pinnedCount) {
                other.relocate(other.m_current, other.m_pinnedCount);
            }
        }
    }
    if (m_current != oldIndex) {
        Q_EMIT currentIndexChanged(m_current);
    }
    if (other.m_current != oldOtherIndex) {
        Q_EMIT other.currentIndexChanged(other.m_current);
    }
    Q_EMIT tabStateChanged(m_current);
    Q_EMIT other.tabStateChanged(other.m_current);
    changeCurrentUrlTo(currentUrl());
    other.changeCurrentUrlTo(other.currentUrl());
    return true;
}

void TabModel::relocate(int from, int to)
{
    m_tabs.move(from, to);
    if (m_current == from) {
        m_current = to;
    } else if (from < m_current && to >= m_current) {
        --m_current;
    } else if (from > m_current && to <= m_current) {
        ++m_current;
    }
    Q_EMIT tabMoved(from, to);
}

void TabModel::setCurrentIndex(int index)
{
    if (!isValidIndex(index) || index == m_current) {
        return;
    }
    Q_EMIT currentAboutToChange();
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
    if (m_tabs.at(m_current).locked) {
        if (url.isValid() && url != currentUrl()) {
            insertTab(m_current + 1, url, true);
        }
        return;
    }
    const QUrl before = currentUrl();
    if (url.isValid() && url.adjusted(QUrl::StripTrailingSlash) != before) {
        Q_EMIT currentAboutToChange();
    }
    m_tabs[m_current].history.push(url);
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
    m_tabs[m_current].history.replaceCurrent(url);
    if (currentUrl() != before) {
        changeCurrentUrlTo(currentUrl());
    }
}

bool TabModel::canGoBack() const
{
    return m_current >= 0 && !m_tabs.at(m_current).locked && m_tabs.at(m_current).history.canGoBack();
}

bool TabModel::canGoForward() const
{
    return m_current >= 0 && !m_tabs.at(m_current).locked && m_tabs.at(m_current).history.canGoForward();
}

void TabModel::goBack()
{
    if (canGoBack()) {
        Q_EMIT currentAboutToChange();
        changeCurrentUrlTo(m_tabs[m_current].history.goBack());
    }
}

void TabModel::goForward()
{
    if (canGoForward()) {
        Q_EMIT currentAboutToChange();
        changeCurrentUrlTo(m_tabs[m_current].history.goForward());
    }
}

int TabModel::pinnedCount() const
{
    return m_pinnedCount;
}

bool TabModel::isPinned(int index) const
{
    return isValidIndex(index) && index < m_pinnedCount;
}

void TabModel::setPinned(int index, bool pinned)
{
    if (!isValidIndex(index) || isPinned(index) == pinned) {
        return;
    }
    const int to = pinned ? m_pinnedCount : m_pinnedCount - 1;
    if (index != to) {
        relocate(index, to);
    }
    m_pinnedCount += pinned ? 1 : -1;
    Q_EMIT tabStateChanged(to);
}

bool TabModel::isLocked(int index) const
{
    return isValidIndex(index) && m_tabs.at(index).locked;
}

void TabModel::setLocked(int index, bool locked)
{
    if (!isValidIndex(index) || m_tabs.at(index).locked == locked) {
        return;
    }
    m_tabs[index].locked = locked;
    Q_EMIT tabStateChanged(index);
}

QString TabModel::nameAt(int index) const
{
    return isValidIndex(index) ? m_tabs.at(index).name : QString();
}

void TabModel::setName(int index, const QString &name)
{
    if (!isValidIndex(index) || m_tabs.at(index).name == name.trimmed()) {
        return;
    }
    m_tabs[index].name = name.trimmed();
    Q_EMIT tabStateChanged(index);
}

void TabModel::changeCurrentUrlTo(const QUrl &url)
{
    Q_EMIT tabUrlChanged(m_current, url);
    Q_EMIT currentUrlChanged(url);
}

bool TabModel::isValidIndex(int index) const
{
    return index >= 0 && index < m_tabs.size();
}
