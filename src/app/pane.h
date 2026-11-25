// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "navigationhistory.h"
#include "session.h"
#include "yazimode.h"

#include <QColor>
#include <QPair>
#include <QStringList>
#include <QUrl>
#include <QWidget>

class FilterBar;
class FolderView;
class KFilePlacesModel;
class KUrlNavigator;
class TabColors;
class TabModel;
class YaziColumns;
class QTabBar;
class QGraphicsOpacityEffect;
class QBoxLayout;
class QHBoxLayout;
class QToolBar;
class QToolButton;
class QAction;
class QVBoxLayout;
class QResizeEvent;
class QStackedWidget;

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
    enum class LocationPresentation {
        FullBreadcrumbs,
        AbbreviatedBreadcrumbs,
        EditablePath
    };
    /// Which panes a background tint applies to; matches the PaneTint setting.
    enum class BackgroundTint {
        None,
        AllPanes,
        ActivePane,
        InactivePane
    };
    /**
     * @param places the window's places model, for the location bar
     * @param colors the tab colours shared by all panes
     * @param toolbarConfigId the stable settings group name for this pane's toolbar
     * @param parent the parent widget
     */
    Pane(KFilePlacesModel *places, TabColors *colors, QWidget *parent = nullptr, const QString &toolbarConfigId = QStringLiteral("Standalone"));
    ~Pane() override;

    TabModel *tabs() const;
    FolderView *view() const;
    QToolBar *toolBar() const;
    QString toolbarConfigId() const;
    QAction *toolbarAction(const QString &name) const;
    QAction *toolbarActionForSharedAction(const QString &name) const;
    QString sharedActionName(QAction *action) const;
    void insertToolbarAction(QAction *action, const QPoint &position = QPoint());
    void removeToolbarAction(QAction *action);
    void resetToolbarConfiguration();
    QStringList toolbarActions() const;
    QStringList visibleToolbarActions() const;
    QStringList hiddenToolbarActions() const;
    void setToolbarConfiguration(const QStringList &actions, const QStringList &visible, const QStringList &hidden);
    QUrl url() const;

    bool isActive() const;
    void setActive(bool active);
    void setInactiveDimming(bool enabled, int opacityPercent = 65);
    /**
     * Tints the folder view's background, keeping view text readable.
     * @param scope whether the tint applies to this pane given its active state
     * @param color the tint colour
     * @param strengthPercent how much of the tint to mix into the background
     */
    void setBackgroundTint(BackgroundTint scope, const QColor &color, int strengthPercent);
    /**
     * Makes the folder view's background translucent when the window is,
     * keeping text, icons and selection opaque.
     * @param opacityPercent the background opacity; 100 is opaque
     */
    void setBackgroundOpacity(int opacityPercent);

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
    /// Sets the tab font; a default QFont follows the window's font.
    void setTabFont(const QFont &font);
    /// Redraws folder colours on tabs, breadcrumbs and the view.
    void refreshFolderColors();
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
     * Replaces the pane's tabs and filter with a saved snapshot.
     * @param state the snapshot to restore
     */
    void replaceState(const PaneSession &state);

    /**
     * Shows or hides the row with the history buttons and the location bar.
     * @param visible true to show the row
     */
    void setNavigationBarVisible(bool visible);
    bool isNavigationBarVisible() const;
    /// Sets whether this pane shows full breadcrumbs, abbreviated breadcrumbs or an editable path.
    void setLocationPresentation(LocationPresentation presentation);
    LocationPresentation locationPresentation() const;
    /// Sets the presentation used when no saved choice applies, and applies it when it changes.
    void setDefaultLocationPresentation(LocationPresentation presentation);
    LocationPresentation defaultLocationPresentation() const;
    QWidget *navigationBar() const;
    void setStatusBarWidget(QWidget *widget);
    QWidget *statusBarWidget() const;

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

    /**
     * Shows the folder view in Yazi mode's columns, with the parent folder
     * before it, or back on its own. The tabs, location, selection, filter,
     * sorting and view mode are kept either way.
     * @param enabled true to enter Yazi mode
     */
    void setYaziMode(bool enabled);
    bool yaziMode() const;
    /**
     * Shows or hides the preview column of the focused item while the pane is
     * in Yazi mode, and records the choice for the session. The choice is kept
     * when the mode is off. Until a choice is made or restored, the pane
     * follows the YaziPreviewVisible setting.
     * @param visible true to show the preview column
     */
    void setYaziPreview(bool visible);
    bool yaziPreview() const;
    /// Applies a changed YaziPreviewVisible setting unless the pane has its own choice.
    void reloadYaziPreviewSetting();
    /// Applies the YaziKeyPreset setting to the keys of Yazi mode.
    void reloadYaziKeySettings();
    /// The columns that host the folder view in Yazi mode; they exist even when the mode is off.
    YaziColumns *yaziColumns() const;
    /**
     * Runs a Yazi mode command in this pane, as its keys or actions do.
     * Does nothing while the pane is not in Yazi mode. Showing or hiding the
     * preview, leaving the mode and file commands happen once control returns
     * to the event loop, since they can delete the focused widget.
     * @param command the command to run
     * @param count how many times to repeat a command that takes a count
     */
    void runYaziCommand(YaziMode::Command command, int count = 1);

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
    void workspaceLayoutChanged();
    void statusMessage(const QString &message);
    void searchRequested();
    void yaziModeChanged(bool enabled);
    void yaziPreviewChanged(bool visible);
    /**
     * A Yazi mode file command, such as Trash, should run through the
     * window's action. For a command that uses the selection, the focused
     * item has been selected if nothing was.
     */
    void yaziFileCommandRequested(YaziMode::Command command);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updateBackgroundTint();
    /// Applies a saved preview choice (1, 0 or -1 for the setting) without marking the workspace changed.
    void restoreYaziPreview(int choice);
    void applyYaziPreview(bool visible);
    /// Closes the pane's tabs and opens those of @p state with their pins, locks and names.
    void rebuildTabs(const PaneSession &state);
    void saveHistoryViewState();
    void restoreHistoryViewState();
    void finishReveal();
    /// Opens @p folder and selects @p item once it is listed.
    void showFolderItem(const QUrl &folder, const QUrl &item);
    void syncTabBar();
    void updateBreadcrumbs(const QUrl &url);
    void updateTabStripVisibility();
    void updateNavigationLayout();
    void configureToolbar();
    void restoreToolbarConfiguration();
    void saveToolbarConfiguration(const QStringList &order, const QStringList &visible);
    void saveCurrentToolbarConfiguration();
    void updateTabText(int index);
    void updateNavigationButtons();
    void showTabContextMenu(int index, const QPoint &globalPos);
    static QString tabTitle(const QUrl &url);

    TabModel *m_tabs = nullptr;
    QTabBar *m_tabBar = nullptr;
    QWidget *m_tabStrip = nullptr;
    QAction *m_back = nullptr;
    QAction *m_forward = nullptr;
    QAction *m_up = nullptr;
    QAction *m_reload = nullptr;
    QToolBar *m_toolBar = nullptr;
    QList<QPair<QAction *, bool>> m_defaultToolbarActions;
    QString m_toolbarConfigId;
    QWidget *m_navigationBar = nullptr;
    QBoxLayout *m_navigationLayout = nullptr;
    QWidget *m_statusBarWidget = nullptr;
    KUrlNavigator *m_navigator = nullptr;
    QStackedWidget *m_locationStack = nullptr;
    QWidget *m_breadcrumbBar = nullptr;
    QHBoxLayout *m_breadcrumbLayout = nullptr;
    QToolButton *m_locationModeButton = nullptr;
    TabColors *m_colors = nullptr;
    FolderView *m_view = nullptr;
    YaziColumns *m_yaziColumns = nullptr;
    bool m_yaziMode = false;
    bool m_yaziPreview = true;
    /// The saved preview choice: 1 shown, 0 hidden, -1 none, following the setting.
    int m_yaziPreviewChoice = -1;
    FilterBar *m_filterBar = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QHBoxLayout *m_outerLayout = nullptr;
    QBoxLayout *m_stripLayout = nullptr;
    bool m_active = false;
    bool m_inactiveDimming = false;
    QGraphicsOpacityEffect *m_dimEffect = nullptr;
    BackgroundTint m_tintScope = BackgroundTint::None;
    QColor m_tintColor;
    int m_tintStrength = 0;
    int m_backgroundOpacity = 100;
    bool m_tabBarForced = false;
    bool m_showTabIcons = true;
    bool m_newTabsAfterCurrent = false;
    bool m_followSymlinks = false;
    LocationPresentation m_locationPresentation = LocationPresentation::FullBreadcrumbs;
    LocationPresentation m_defaultLocationPresentation = LocationPresentation::FullBreadcrumbs;
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
