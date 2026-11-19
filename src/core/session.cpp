// SPDX-License-Identifier: GPL-3.0-or-later

#include "session.h"

#include <KConfigGroup>

namespace
{
constexpr auto LeftGroup = "LeftPane";
constexpr auto RightGroup = "RightPane";
constexpr auto TabsKey = "Tabs";
constexpr auto CurrentTabKey = "CurrentTab";
constexpr auto ActivePaneKey = "ActivePane";
constexpr auto DualKey = "Dual";
constexpr auto StackedKey = "Stacked";
constexpr auto PinnedCountKey = "PinnedCount";
constexpr auto LockedTabsKey = "LockedTabs";
constexpr auto NamesKey = "Names";

QStringList toStrings(const QList<QUrl> &urls)
{
    QStringList result;
    result.reserve(urls.size());
    for (const QUrl &url : urls) {
        result.append(url.toString(QUrl::FullyEncoded));
    }
    return result;
}

QList<QUrl> fromStrings(const QStringList &strings)
{
    QList<QUrl> result;
    result.reserve(strings.size());
    for (const QString &string : strings) {
        result.append(QUrl(string, QUrl::StrictMode));
    }
    return result;
}

void savePane(const PaneSession &pane, KConfigGroup group)
{
    group.writeEntry(TabsKey, toStrings(pane.tabs));
    group.writeEntry(CurrentTabKey, pane.currentTab);
    group.writeEntry(PinnedCountKey, pane.pinnedCount);
    group.writeEntry(LockedTabsKey, pane.lockedTabs);
    group.writeEntry(NamesKey, pane.names);
}

PaneSession loadPane(const KConfigGroup &group)
{
    PaneSession pane;
    pane.tabs = fromStrings(group.readEntry(TabsKey, QStringList()));
    pane.currentTab = group.readEntry(CurrentTabKey, 0);
    pane.pinnedCount = group.readEntry(PinnedCountKey, 0);
    pane.lockedTabs = group.readEntry(LockedTabsKey, QList<int>());
    pane.names = group.readEntry(NamesKey, QStringList());
    return pane;
}

PaneSession sanitizePane(const PaneSession &pane, const QUrl &fallback)
{
    PaneSession result;
    for (int i = 0; i < pane.tabs.size(); ++i) {
        const QUrl &url = pane.tabs.at(i);
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }
        if (i < pane.pinnedCount) {
            ++result.pinnedCount;
        }
        if (pane.lockedTabs.contains(i)) {
            result.lockedTabs.append(result.tabs.size());
        }
        result.names.append(pane.names.value(i));
        result.tabs.append(url);
    }
    if (result.tabs.isEmpty()) {
        result.tabs.append(fallback);
        result.names.append(QString());
    }
    result.currentTab = qBound(0, pane.currentTab, result.tabs.size() - 1);
    return result;
}
}

bool WindowSession::isEmpty() const
{
    return left.tabs.isEmpty() && right.tabs.isEmpty();
}

WindowSession WindowSession::sanitized(const QUrl &fallback) const
{
    WindowSession result;
    result.left = sanitizePane(left, fallback);
    result.right = sanitizePane(right, fallback);
    result.activePane = activePane == 1 ? 1 : 0;
    result.dual = dual;
    result.stacked = stacked;
    return result;
}

void Session::save(const WindowSession &session, KConfigGroup &group)
{
    savePane(session.left, group.group(QLatin1String(LeftGroup)));
    savePane(session.right, group.group(QLatin1String(RightGroup)));
    group.writeEntry(ActivePaneKey, session.activePane);
    group.writeEntry(DualKey, session.dual);
    group.writeEntry(StackedKey, session.stacked);
}

WindowSession Session::load(const KConfigGroup &group)
{
    WindowSession session;
    session.left = loadPane(group.group(QLatin1String(LeftGroup)));
    session.right = loadPane(group.group(QLatin1String(RightGroup)));
    session.activePane = group.readEntry(ActivePaneKey, 0);
    session.dual = group.readEntry(DualKey, true);
    session.stacked = group.readEntry(StackedKey, false);
    return session;
}
