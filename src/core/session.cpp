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
constexpr auto FilterTextKey = "FilterText";
constexpr auto FilterScopeKey = "FilterScope";
constexpr auto LocationPresentationKey = "LocationPresentation";
constexpr auto ViewModeKey = "ViewMode";

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
    group.writeEntry(FilterTextKey, pane.filterText);
    group.writeEntry(FilterScopeKey, pane.filterScope);
    group.writeEntry(LocationPresentationKey, pane.locationPresentation);
    group.writeEntry(ViewModeKey, pane.viewMode);
}

PaneSession loadPane(const KConfigGroup &group, int defaultViewMode)
{
    PaneSession pane;
    pane.tabs = fromStrings(group.readEntry(TabsKey, QStringList()));
    pane.currentTab = group.readEntry(CurrentTabKey, 0);
    pane.pinnedCount = group.readEntry(PinnedCountKey, 0);
    pane.lockedTabs = group.readEntry(LockedTabsKey, QList<int>());
    pane.names = group.readEntry(NamesKey, QStringList());
    pane.filterText = group.readEntry(FilterTextKey, QString());
    pane.filterScope = group.readEntry(FilterScopeKey, -1);
    pane.locationPresentation = group.readEntry(LocationPresentationKey, -1);
    pane.viewMode = group.readEntry(ViewModeKey, defaultViewMode);
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
    result.filterText = pane.filterText;
    result.filterScope = qBound(-1, pane.filterScope, 2);
    result.locationPresentation = qBound(-1, pane.locationPresentation, 2);
    result.viewMode = qBound(0, pane.viewMode, 2);
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

WindowSession Session::load(const KConfigGroup &group, int defaultViewMode)
{
    WindowSession session;
    session.left = loadPane(group.group(QLatin1String(LeftGroup)), defaultViewMode);
    session.right = loadPane(group.group(QLatin1String(RightGroup)), defaultViewMode);
    session.activePane = group.readEntry(ActivePaneKey, 0);
    session.dual = group.readEntry(DualKey, true);
    session.stacked = group.readEntry(StackedKey, true);
    return session;
}
