// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "navigationhistory.h"
#include "session.h"

#include <QUrl>
#include <QWidget>

class FilterBar;
class FolderView;
class KFilePlacesModel;
class KUrlNavigator;
class TabColors;
class TabModel;
class QTabBar;
class QGraphicsOpacityEffect;
class QBoxLayout;
class QHBoxLayout;
class QToolButton;
class QWidget;
class QVBoxLayout;

/**
 * One side of the dual-pane layout: a tab strip, a path row with
 * history buttons and a KUrlNavigator, and the FolderView of the
 * current tab. All state lives in the TabModel.
 */
class Pane : public QWidget
{
    Q_OBJECT

public:
    enum class TabBarPosition {
        Top,
        Bottom,
        Left,
        Right
    };
    /**
     * @param places the window's places model, for the location bar
     * @param colors the tab colours shared by all panes
     * @param parent the parent widget
     */
    Pane(KFilePlacesModel *places, TabColors *colors, QWidget *parent = nullptr);
    ~Pane() override;

    TabModel *tabs() const;
    FolderView *view() const;
    QUrl url() const;

    bool isActive() const;
    void setActive(bool active);
    void setInactiveDimming(bool enabled);

    /// Forces the tab strip to show even with a single tab.
    void setTabBarForcedVisible(bool visible);

    /**
     * Places the tab strip below the folder view, with tabs facing down,
     * or back above the path row.
     * @param atBottom true to put the tabs along the pane's bottom edge
     */
    void setTabBarAtBottom(bool atBottom);
    bool isTabBarAtBottom() const;
    void setTabBarPosition(TabBarPosition position);
    TabBarPosition tabBarPosition() const;
    void reloadTabSettings();
    void reloadFilterSettings();
    void setBarDensity(const QString &density);

    /// The tabs with their pins, locks and names, and the current tab.
    PaneSession saveState() const;
    /**
     * Opens the tabs of @p state with their pins, locks and names.
     * @param state the saved tabs; the pane must not have tabs yet
     */
    void restoreState(const PaneSession &state);

    /**
     * Shows or hides the row with the history buttons and the location bar.
     * @param visible true to show the row
     */
    void setNavigationBarVisible(bool visible);
    bool isNavigationBarVisible() const;
    QWidget *navigationBar() const;

    /**
     * Chooses where openInNewTab() and newTab() insert tabs.
     * @param afterCurrent true to insert next to the current tab, false to
     *        append at the end
     */
    void setNewTabsAfterCurrent(bool afterCurrent);

    /**
     * Makes navigation resolve symbolic links in local folder paths, so a
     * folder opened through a link shows its real location.
     * @param follow true to resolve links
     */
    void setFollowSymlinks(bool follow);

public Q_SLOTS:
    void setUrl(const QUrl &url);
    void revealUrl(const QUrl &url);
    void openInNewTab(const QUrl &url, bool activate = false);
    void newTab();
    void closeCurrentTab();
    void goBack();
    void goForward();
    void goUp();
    void reload();
    void editUrl();
    /// Shows the filter bar and focuses it.
    void showFilterBar();

Q_SIGNALS:
    void activated();
    void urlChanged(const QUrl &url);
    void tabCountChanged(int count);
    void statusMessage(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void saveHistoryViewState();
    void restoreHistoryViewState();
    void finishReveal();
    void syncTabBar();
    void updateTabStripVisibility();
    void updateTabText(int index);
    void updateNavigationButtons();
    void showTabContextMenu(int index, const QPoint &globalPos);
    static QString tabTitle(const QUrl &url);

    TabModel *m_tabs = nullptr;
    QTabBar *m_tabBar = nullptr;
    QWidget *m_tabStrip = nullptr;
    QToolButton *m_back = nullptr;
    QToolButton *m_forward = nullptr;
    QToolButton *m_up = nullptr;
    QToolButton *m_reload = nullptr;
    QWidget *m_navigationBar = nullptr;
    KUrlNavigator *m_navigator = nullptr;
    TabColors *m_colors = nullptr;
    FolderView *m_view = nullptr;
    FilterBar *m_filterBar = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QHBoxLayout *m_outerLayout = nullptr;
    QBoxLayout *m_stripLayout = nullptr;
    bool m_active = false;
    bool m_inactiveDimming = false;
    QGraphicsOpacityEffect *m_dimEffect = nullptr;
    bool m_tabBarForced = false;
    bool m_showTabIcons = true;
    bool m_newTabsAfterCurrent = false;
    bool m_followSymlinks = false;
    int m_defaultFilterScope = 0;
    bool m_updatingTabBar = false;
    int m_lastTabCount = -1;
    NavigationHistory::ViewState m_pendingViewState;
    QUrl m_revealFolder;
    QUrl m_revealItem;
    int m_revealTab = -1;
    quint64 m_viewStateGeneration = 0;
    bool m_historyExpansionStarted = false;
    bool m_historyExpansionPending = false;
};
