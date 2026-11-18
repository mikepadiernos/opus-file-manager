// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>
#include <QWidget>

class FolderView;
class KFilePlacesModel;
class KUrlNavigator;
class TabModel;
class QTabBar;
class QToolButton;
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
    explicit Pane(KFilePlacesModel *places, QWidget *parent = nullptr);
    ~Pane() override;

    TabModel *tabs() const;
    FolderView *view() const;
    QUrl url() const;

    bool isActive() const;
    void setActive(bool active);

    /// Forces the tab strip to show even with a single tab.
    void setTabBarForcedVisible(bool visible);

    /**
     * Places the tab strip below the folder view, with tabs facing down,
     * or back above the path row.
     * @param atBottom true to put the tabs along the pane's bottom edge
     */
    void setTabBarAtBottom(bool atBottom);
    bool isTabBarAtBottom() const;

public Q_SLOTS:
    void setUrl(const QUrl &url);
    void openInNewTab(const QUrl &url, bool activate = false);
    void newTab();
    void closeCurrentTab();
    void goBack();
    void goForward();
    void goUp();
    void reload();
    void editUrl();

Q_SIGNALS:
    void activated();
    void urlChanged(const QUrl &url);
    void tabCountChanged(int count);
    void statusMessage(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
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
    KUrlNavigator *m_navigator = nullptr;
    FolderView *m_view = nullptr;
    QVBoxLayout *m_layout = nullptr;
    bool m_active = false;
    bool m_tabBarForced = false;
    bool m_updatingTabBar = false;
    int m_lastTabCount = -1;
};
