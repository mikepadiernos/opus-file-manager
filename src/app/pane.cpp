// SPDX-License-Identifier: GPL-3.0-or-later

#include "pane.h"

#include "filterbar.h"
#include "folderview.h"
#include "settings.h"
#include "symbolicicon.h"
#include "symlinks.h"
#include "tabcolors.h"
#include "tabmodel.h"

#include <KConfigGroup>
#include <KDirModel>
#include <KFilePlacesModel>
#include <KIO/Global>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KUrlNavigator>

#include <QAbstractItemView>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

Pane::Pane(KFilePlacesModel *places, TabColors *colors, QWidget *parent, const QString &toolbarConfigId)
    : QWidget(parent)
    , m_tabs(new TabModel(this))
    , m_tabBar(new QTabBar(this))
    , m_tabStrip(new QWidget(this))
    , m_toolbarConfigId(toolbarConfigId)
    , m_navigator(new KUrlNavigator(places, QUrl(), this))
    , m_colors(colors)
    , m_view(new FolderView(this))
{
    m_dimEffect = new QGraphicsOpacityEffect(this);
    m_dimEffect->setObjectName(QStringLiteral("inactivePaneEffect"));
    m_dimEffect->setOpacity(0.65);
    m_dimEffect->setEnabled(false);
    setGraphicsEffect(m_dimEffect);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setExpanding(Settings::expandTabs());
    m_showTabIcons = Settings::tabIcons();
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabBar->installEventFilter(this);

    auto *addTab = new QToolButton(m_tabStrip);
    addTab->setAutoRaise(true);
    addTab->setIcon(SymbolicIcon::fromTheme(QStringLiteral("tab-new")));
    addTab->setToolTip(i18nc("@info:tooltip", "Open a new tab"));
    connect(addTab, &QToolButton::clicked, this, &Pane::newTab);

    m_stripLayout = new QHBoxLayout(m_tabStrip);
    m_stripLayout->setContentsMargins(0, 0, 0, 0);
    m_stripLayout->setSpacing(0);
    m_stripLayout->addWidget(m_tabBar, 1);
    m_stripLayout->addWidget(addTab);

    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("paneToolbar"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    const auto addNavigationAction = [this](const QString &name, const QString &icon, const QString &text) {
        QAction *action = m_toolBar->addAction(SymbolicIcon::fromTheme(icon), text);
        action->setObjectName(name);
        action->setToolTip(text);
        return action;
    };
    m_back = addNavigationAction(QStringLiteral("paneBack"), QStringLiteral("go-previous"), i18nc("@action", "Back"));
    m_forward = addNavigationAction(QStringLiteral("paneForward"), QStringLiteral("go-next"), i18nc("@action", "Forward"));
    m_up = addNavigationAction(QStringLiteral("paneUp"), QStringLiteral("go-up"), i18nc("@action", "Parent Folder"));
    m_reload = addNavigationAction(QStringLiteral("paneReload"), QStringLiteral("view-refresh"), i18nc("@action", "Reload"));
    connect(m_back, &QAction::triggered, this, &Pane::goBack);
    connect(m_forward, &QAction::triggered, this, &Pane::goForward);
    connect(m_up, &QAction::triggered, this, &Pane::goUp);
    connect(m_reload, &QAction::triggered, this, &Pane::reload);

    QMenu *viewMenu = new QMenu(m_toolBar);
    auto *viewGroup = new QActionGroup(viewMenu);
    viewGroup->setExclusive(true);
    const QList<QPair<FolderView::ViewMode, QString>> viewModes{
        {FolderView::ViewMode::Details, i18nc("@item:inmenu", "Details")},
        {FolderView::ViewMode::Icons, i18nc("@item:inmenu", "Icons")},
        {FolderView::ViewMode::Compact, i18nc("@item:inmenu", "Compact")},
    };
    for (const auto &[mode, label] : viewModes) {
        QAction *action = viewMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_view->viewMode() == mode);
        action->setData(static_cast<int>(mode));
        viewGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] {
            m_view->setViewMode(mode);
        });
    }
    auto *viewButton = new QToolButton(m_toolBar);
    viewButton->setObjectName(QStringLiteral("paneViewModeButton"));
    viewButton->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-list-details")));
    viewButton->setText(i18nc("@action", "View Mode"));
    viewButton->setToolTip(viewButton->text());
    viewButton->setAutoRaise(true);
    viewButton->setMenu(viewMenu);
    viewButton->setPopupMode(QToolButton::InstantPopup);
    QAction *viewButtonAction = m_toolBar->addWidget(viewButton);
    viewButtonAction->setObjectName(QStringLiteral("paneViewModeButton"));
    viewButtonAction->setText(viewButton->text());

    const auto addZoomAction = [this](const QString &name, const QString &icon, const QString &label, int steps) {
        QAction *action = m_toolBar->addAction(SymbolicIcon::fromTheme(icon), label);
        action->setObjectName(name);
        action->setToolTip(label);
        connect(action, &QAction::triggered, this, [this, steps] {
            m_view->adjustIconSize(steps);
        });
    };
    addZoomAction(QStringLiteral("paneZoomOut"), QStringLiteral("zoom-out"), i18nc("@action", "Zoom Out"), -1);
    addZoomAction(QStringLiteral("paneZoomIn"), QStringLiteral("zoom-in"), i18nc("@action", "Zoom In"), 1);

    QMenu *sortMenu = new QMenu(m_toolBar);
    auto *sortGroup = new QActionGroup(sortMenu);
    sortGroup->setExclusive(true);
    const QList<QPair<int, QString>> sortColumns{
        {KDirModel::Name, i18nc("@item:inmenu sort order", "Name")},
        {KDirModel::Size, i18nc("@item:inmenu sort order", "Size")},
        {KDirModel::ModifiedTime, i18nc("@item:inmenu sort order", "Modified")},
        {KDirModel::Type, i18nc("@item:inmenu sort order", "Type")},
    };
    for (const auto &[column, label] : sortColumns) {
        QAction *action = sortMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_view->sortColumn() == column);
        action->setData(column);
        sortGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, column] {
            m_view->setSorting(column, m_view->sortOrder());
        });
    }
    sortMenu->addSeparator();
    QAction *ascending = sortMenu->addAction(i18nc("@action:inmenu", "Ascending"));
    ascending->setCheckable(true);
    ascending->setChecked(m_view->sortOrder() == Qt::AscendingOrder);
    connect(ascending, &QAction::triggered, this, [this](bool checked) {
        m_view->setSorting(m_view->sortColumn(), checked ? Qt::AscendingOrder : Qt::DescendingOrder);
    });
    auto *sortButton = new QToolButton(m_toolBar);
    sortButton->setObjectName(QStringLiteral("paneSortingButton"));
    sortButton->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-sort-ascending")));
    sortButton->setText(i18nc("@action", "Sort"));
    sortButton->setToolTip(sortButton->text());
    sortButton->setAutoRaise(true);
    sortButton->setMenu(sortMenu);
    sortButton->setPopupMode(QToolButton::InstantPopup);
    QAction *sortButtonAction = m_toolBar->addWidget(sortButton);
    sortButtonAction->setObjectName(QStringLiteral("paneSortingButton"));
    sortButtonAction->setText(sortButton->text());

    QAction *previews = m_toolBar->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-preview")), i18nc("@action", "Show Previews"));
    previews->setObjectName(QStringLiteral("panePreviews"));
    previews->setCheckable(true);
    previews->setChecked(m_view->previewsEnabled());
    connect(previews, &QAction::toggled, m_view, &FolderView::setPreviewsEnabled);
    connect(m_view, &FolderView::previewsEnabledChanged, previews, &QAction::setChecked);

    QAction *hiddenFiles = m_toolBar->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-hidden")), i18nc("@action", "Show Hidden Files"));
    hiddenFiles->setObjectName(QStringLiteral("paneHiddenFiles"));
    hiddenFiles->setCheckable(true);
    hiddenFiles->setChecked(m_view->showHiddenFiles());
    connect(hiddenFiles, &QAction::toggled, m_view, &FolderView::setShowHiddenFiles);
    connect(m_view, &FolderView::hiddenFilesChanged, hiddenFiles, &QAction::setChecked);

    QAction *filter = m_toolBar->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-filter")), i18nc("@action", "Filter"));
    filter->setObjectName(QStringLiteral("paneFilter"));
    filter->setToolTip(i18nc("@action", "Filter This Pane"));
    connect(filter, &QAction::triggered, this, &Pane::showFilterBar);
    QAction *search = m_toolBar->addAction(SymbolicIcon::fromTheme(QStringLiteral("edit-find")), i18nc("@action", "Search"));
    search->setObjectName(QStringLiteral("paneSearch"));
    search->setToolTip(i18nc("@action", "Search This Pane"));
    connect(search, &QAction::triggered, this, &Pane::searchRequested);

    QAction *configure = m_toolBar->addAction(SymbolicIcon::fromTheme(QStringLiteral("configure")), i18nc("@action", "Customize Toolbar"));
    configure->setObjectName(QStringLiteral("paneCustomizeToolbar"));
    configure->setToolTip(configure->text());
    connect(configure, &QAction::triggered, this, &Pane::configureToolbar);
    restoreToolbarConfiguration();

    m_navigator->setPlacesSelectorVisible(false);
    m_navigator->setObjectName(QStringLiteral("paneLocationBar"));
    m_navigationBar = new QWidget(this);
    m_navigationBar->setObjectName(QStringLiteral("paneNavigationBar"));
    m_navigationBar->installEventFilter(this);
    m_navigationLayout = new QBoxLayout(QBoxLayout::LeftToRight, m_navigationBar);
    m_navigationLayout->setContentsMargins(0, 0, 0, 0);
    m_navigationLayout->setSpacing(0);
    m_navigationLayout->addWidget(m_toolBar);
    m_navigationLayout->addWidget(m_navigator, 1);

    m_outerLayout = new QHBoxLayout(this);
    m_outerLayout->setContentsMargins(0, 0, 0, 0);
    m_outerLayout->setSpacing(0);
    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_outerLayout->addLayout(m_layout, 1);
    m_layout->addWidget(m_tabStrip);
    m_layout->addWidget(m_navigationBar);
    m_layout->addWidget(m_view, 1);
    m_statusBarWidget = new QWidget(this);
    m_statusBarWidget->setObjectName(QStringLiteral("paneStatusBar"));
    m_statusBarWidget->hide();
    m_layout->addWidget(m_statusBarWidget);
    m_filterBar = new FilterBar(this);
    m_defaultFilterScope = Settings::defaultFilterScope();
    m_layout->addWidget(m_filterBar);

    // Model -> widgets
    connect(m_tabs, &TabModel::tabInserted, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::tabRemoved, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::tabMoved, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::currentIndexChanged, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::currentIndexChanged, this, [this](int index) {
        if (m_revealTab >= 0 && index != m_revealTab) {
            m_revealFolder.clear();
            m_revealItem.clear();
            m_revealTab = -1;
        }
    });
    connect(m_tabs, &TabModel::tabUrlChanged, this, [this](int index, const QUrl &) {
        updateTabText(index);
    });
    connect(m_tabs, &TabModel::tabStateChanged, this, [this](int index) {
        updateTabText(index);
        updateNavigationButtons();
    });
    connect(m_colors, &TabColors::colorChanged, this, [this] {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            updateTabText(i);
        }
    });
    connect(m_tabs, &TabModel::currentAboutToChange, this, &Pane::saveHistoryViewState);
    connect(m_tabs, &TabModel::currentUrlChanged, this, [this](const QUrl &url) {
        if (!m_revealFolder.isEmpty() && url != m_revealFolder) {
            m_revealFolder.clear();
            m_revealItem.clear();
            m_revealTab = -1;
        }
        if (m_view->url().isValid() && m_view->url() != url && !Settings::retainFilterOnNavigation()) {
            m_filterBar->setFilter(NameFilter(QString(), m_filterBar->filter().scope()));
        }
        ++m_viewStateGeneration;
        m_view->cancelHistoryExpansion();
        m_historyExpansionStarted = false;
        m_historyExpansionPending = false;
        m_pendingViewState = m_tabs->historyAt(m_tabs->currentIndex()).currentViewState();
        if (!Settings::restoreHistorySelection()) {
            m_pendingViewState.selectedUrls.clear();
        }
        if (!Settings::restoreHistoryScroll()) {
            m_pendingViewState.verticalScroll = 0;
            m_pendingViewState.horizontalScroll = 0;
        }
        if (!Settings::restoreHistorySelection() && !Settings::restoreHistoryScroll()) {
            m_pendingViewState.valid = false;
        }
        m_navigator->setLocationUrl(url);
        if (m_view->url() == url && !m_pendingViewState.valid) {
            m_view->clearSelection();
            m_view->currentView()->verticalScrollBar()->setValue(0);
            m_view->currentView()->horizontalScrollBar()->setValue(0);
        }
        m_view->setUrl(url);
        restoreHistoryViewState();
        updateNavigationButtons();
        Q_EMIT urlChanged(url);
    });

    // Widgets -> model
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (!m_updatingTabBar) {
            m_tabs->setCurrentIndex(index);
        }
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        m_tabs->closeTab(index);
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (!m_updatingTabBar) {
            m_updatingTabBar = true;
            const bool moved = m_tabs->moveTab(from, to);
            m_updatingTabBar = false;
            if (!moved) {
                syncTabBar();
            }
        }
    });
    connect(m_tabBar, &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index < 0) {
            newTab();
        }
    });
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        showTabContextMenu(m_tabBar->tabAt(pos), m_tabBar->mapToGlobal(pos));
    });
    connect(m_tabBar, &QTabBar::tabBarClicked, this, &Pane::activated);

    connect(m_navigator, &KUrlNavigator::urlChanged, this, [this](const QUrl &url) {
        setUrl(url);
    });
    connect(m_navigator, &KUrlNavigator::tabRequested, this, [this](const QUrl &url) {
        openInNewTab(url);
    });
    connect(m_navigator, &KUrlNavigator::activated, this, &Pane::activated);
    connect(m_navigator, &KUrlNavigator::returnPressed, this, [this] {
        m_view->currentView()->setFocus();
    });

    connect(m_view, &FolderView::redirected, this, [this](const QUrl &url) {
        m_tabs->replaceCurrentUrl(url);
        m_navigator->setLocationUrl(url);
    });
    connect(m_view, &FolderView::activated, this, &Pane::activated);
    connect(m_view, &FolderView::typeAheadRequested, m_filterBar, &FilterBar::open);
    connect(m_filterBar, &FilterBar::filterChanged, m_view, &FolderView::setNameFilter);
    connect(m_filterBar, &FilterBar::activateFirstRequested, m_view, &FolderView::activateFirstItem);
    connect(m_filterBar, &FilterBar::closed, this, [this] {
        m_view->currentView()->setFocus();
    });
    connect(m_view, &FolderView::viewModeChanged, this, [viewGroup, this] {
        for (QAction *action : viewGroup->actions()) {
            action->setChecked(action->data().toInt() == static_cast<int>(m_view->viewMode()));
        }
    });
    connect(m_view, &FolderView::sortingChanged, this, [ascending, sortGroup, this] {
        for (QAction *action : sortGroup->actions()) {
            action->setChecked(action->data().toInt() == m_view->sortColumn());
        }
        ascending->setChecked(m_view->sortOrder() == Qt::AscendingOrder);
    });
    connect(m_view, &FolderView::goUpRequested, this, &Pane::goUp);
    connect(m_view, &FolderView::listingFailed, this, &Pane::statusMessage);
    connect(m_view, &FolderView::listingFailed, this, [this] {
        m_revealFolder.clear();
        m_revealItem.clear();
        m_revealTab = -1;
    });
    connect(m_view, &FolderView::listingReady, this, &Pane::restoreHistoryViewState);
    connect(m_view, &FolderView::listingReady, this, &Pane::finishReveal);
    connect(m_view, &FolderView::historyExpansionFinished, this, [this] {
        m_historyExpansionPending = false;
        restoreHistoryViewState();
    });
    connect(m_view, &FolderView::loadingChanged, this, [this](bool loading) {
        if (!loading) {
            restoreHistoryViewState();
        }
    });

    syncTabBar();
    updateNavigationButtons();
}

Pane::~Pane() = default;

void Pane::saveHistoryViewState()
{
    if (m_pendingViewState.valid || m_view->url() != m_tabs->currentUrl()) {
        return;
    }
    NavigationHistory::ViewState state;
    for (const KFileItem &item : m_view->selectedItems()) {
        state.selectedUrls.append(item.url());
    }
    state.verticalScroll = m_view->currentView()->verticalScrollBar()->value();
    state.horizontalScroll = m_view->currentView()->horizontalScrollBar()->value();
    state.valid = true;
    m_tabs->setCurrentViewState(state);
}

void Pane::restoreHistoryViewState()
{
    if (!m_pendingViewState.valid || m_view->isLoading() || m_historyExpansionPending) {
        return;
    }
    const quint64 generation = m_viewStateGeneration;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation != m_viewStateGeneration || m_view->isLoading() || !m_pendingViewState.valid || m_historyExpansionPending) {
            return;
        }
        if (!m_historyExpansionStarted) {
            m_historyExpansionStarted = true;
            m_historyExpansionPending = true;
            if (m_view->expandForHistorySelection(m_pendingViewState.selectedUrls)) {
                return;
            }
            m_historyExpansionPending = false;
        }
        const auto state = m_pendingViewState;
        m_pendingViewState = {};
        m_view->selectUrls(state.selectedUrls);
        m_view->currentView()->verticalScrollBar()->setValue(state.verticalScroll);
        m_view->currentView()->horizontalScrollBar()->setValue(state.horizontalScroll);
    });
}

TabModel *Pane::tabs() const
{
    return m_tabs;
}

FolderView *Pane::view() const
{
    return m_view;
}

QToolBar *Pane::toolBar() const
{
    return m_toolBar;
}

QString Pane::toolbarConfigId() const
{
    return m_toolbarConfigId;
}

QAction *Pane::toolbarAction(const QString &name) const
{
    for (QAction *action : m_toolBar->actions()) {
        if (action->objectName() == name) {
            return action;
        }
    }
    for (QAction *action : findChildren<QAction *>()) {
        if (action->objectName() == name) {
            return action;
        }
    }
    return nullptr;
}

QAction *Pane::toolbarActionForSharedAction(const QString &name) const
{
    static const QHash<QString, QString> sharedActions{
        {QStringLiteral("go_back"), QStringLiteral("paneBack")},
        {QStringLiteral("go_forward"), QStringLiteral("paneForward")},
        {QStringLiteral("go_up"), QStringLiteral("paneUp")},
        {QStringLiteral("view_redisplay"), QStringLiteral("paneReload")},
        {QStringLiteral("toggle_view_mode"), QStringLiteral("paneViewModeButton")},
        {QStringLiteral("show_previews"), QStringLiteral("panePreviews")},
        {QStringLiteral("show_hidden_files"), QStringLiteral("paneHiddenFiles")},
        {QStringLiteral("show_filter_bar"), QStringLiteral("paneFilter")},
        {QStringLiteral("search_folder"), QStringLiteral("paneSearch")},
    };
    return toolbarAction(sharedActions.value(name));
}

QString Pane::sharedActionName(QAction *action) const
{
    static const QHash<QString, QString> sharedActions{
        {QStringLiteral("paneBack"), QStringLiteral("go_back")},
        {QStringLiteral("paneForward"), QStringLiteral("go_forward")},
        {QStringLiteral("paneUp"), QStringLiteral("go_up")},
        {QStringLiteral("paneReload"), QStringLiteral("view_redisplay")},
        {QStringLiteral("paneViewModeButton"), QStringLiteral("toggle_view_mode")},
        {QStringLiteral("panePreviews"), QStringLiteral("show_previews")},
        {QStringLiteral("paneHiddenFiles"), QStringLiteral("show_hidden_files")},
        {QStringLiteral("paneFilter"), QStringLiteral("show_filter_bar")},
        {QStringLiteral("paneSearch"), QStringLiteral("search_folder")},
    };
    return action ? sharedActions.value(action->objectName()) : QString();
}

void Pane::insertToolbarAction(QAction *action, const QPoint &position)
{
    if (!action || action->objectName() == QLatin1String("paneCustomizeToolbar")) {
        return;
    }
    QAction *before = nullptr;
    const int coordinate = m_toolBar->orientation() == Qt::Horizontal ? position.x() : position.y();
    for (QAction *candidate : m_toolBar->actions()) {
        if (candidate == action) {
            continue;
        }
        const QRect rect = m_toolBar->actionGeometry(candidate);
        const int center = m_toolBar->orientation() == Qt::Horizontal ? rect.center().x() : rect.center().y();
        if (rect.isValid() && coordinate < center) {
            before = candidate;
            break;
        }
    }
    m_toolBar->removeAction(action);
    m_toolBar->insertAction(before, action);
    action->setVisible(true);
    saveCurrentToolbarConfiguration();
}

void Pane::removeToolbarAction(QAction *action)
{
    if (!action || !m_toolBar->actions().contains(action) || action->objectName() == QLatin1String("paneCustomizeToolbar")) {
        return;
    }
    m_toolBar->removeAction(action);
    KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("PaneToolbars")).group(m_toolbarConfigId);
    QStringList hidden = group.readEntry(QStringLiteral("Hidden"), QStringList());
    if (!hidden.contains(action->objectName())) {
        hidden.append(action->objectName());
    }
    group.writeEntry(QStringLiteral("Hidden"), hidden);
    saveCurrentToolbarConfiguration();
}

QUrl Pane::url() const
{
    return m_tabs->currentUrl();
}

bool Pane::isActive() const
{
    return m_active;
}

void Pane::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    m_dimEffect->setEnabled(m_inactiveDimming && !active);
    m_navigator->setActive(active);
    if (active) {
        m_view->currentView()->setFocus();
    }
}

void Pane::setInactiveDimming(bool enabled, int opacityPercent)
{
    m_inactiveDimming = enabled;
    m_dimEffect->setOpacity(qBound(20, opacityPercent, 100) / 100.0);
    m_dimEffect->setEnabled(enabled && !m_active);
}

void Pane::setTabBarForcedVisible(bool visible)
{
    m_tabBarForced = visible;
    updateTabStripVisibility();
}

void Pane::setTabBarAtBottom(bool atBottom)
{
    setTabBarPosition(atBottom ? TabBarPosition::Bottom : TabBarPosition::Top);
}

void Pane::setTabBarPosition(TabBarPosition position)
{
    const bool vertical = position == TabBarPosition::Left || position == TabBarPosition::Right;
    switch (position) {
    case TabBarPosition::Top:
        m_tabBar->setShape(QTabBar::RoundedNorth);
        break;
    case TabBarPosition::Bottom:
        m_tabBar->setShape(QTabBar::RoundedSouth);
        break;
    case TabBarPosition::Left:
        m_tabBar->setShape(QTabBar::RoundedWest);
        break;
    case TabBarPosition::Right:
        m_tabBar->setShape(QTabBar::RoundedEast);
        break;
    }
    m_stripLayout->setDirection(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_layout->removeWidget(m_tabStrip);
    m_outerLayout->removeWidget(m_tabStrip);
    if (vertical) {
        m_outerLayout->insertWidget(position == TabBarPosition::Left ? 0 : m_outerLayout->count(), m_tabStrip);
    } else {
        m_layout->insertWidget(position == TabBarPosition::Top ? 0 : m_layout->count(), m_tabStrip);
    }
}

bool Pane::isTabBarAtBottom() const
{
    return m_tabBar->shape() == QTabBar::RoundedSouth;
}

Pane::TabBarPosition Pane::tabBarPosition() const
{
    switch (m_tabBar->shape()) {
    case QTabBar::RoundedSouth:
        return TabBarPosition::Bottom;
    case QTabBar::RoundedWest:
        return TabBarPosition::Left;
    case QTabBar::RoundedEast:
        return TabBarPosition::Right;
    default:
        return TabBarPosition::Top;
    }
}
void Pane::setNavigationBarVisible(bool visible)
{
    m_navigationBar->setVisible(visible);
}

bool Pane::isNavigationBarVisible() const
{
    return !m_navigationBar->isHidden();
}

QWidget *Pane::navigationBar() const
{
    return m_navigationBar;
}

void Pane::setStatusBarWidget(QWidget *widget)
{
    if (m_statusBarWidget == widget) {
        return;
    }
    if (m_statusBarWidget) {
        m_layout->removeWidget(m_statusBarWidget);
        m_statusBarWidget->deleteLater();
    }
    m_statusBarWidget = widget;
    if (m_statusBarWidget) {
        m_statusBarWidget->setParent(this);
        m_layout->insertWidget(m_layout->indexOf(m_filterBar), m_statusBarWidget);
    }
}

QWidget *Pane::statusBarWidget() const
{
    return m_statusBarWidget;
}

void Pane::setBarDensity(const QString &density)
{
    QBoxLayout *layout = m_navigationLayout;
    const bool normal = density == QLatin1String("Normal");
    const int margin = density == QLatin1String("Compact") ? 2 : 0;
    const int defaultIconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    const int iconSize = qMax(12, defaultIconSize - (density == QLatin1String("Compact") ? 2 : normal ? 0 : 4));
    layout->setContentsMargins(normal ? 0 : margin, normal ? 0 : margin, normal ? 0 : margin, normal ? 0 : margin);
    layout->setSpacing(normal ? 0 : margin);
    m_toolBar->setIconSize(QSize(iconSize, iconSize));
    m_toolBar->setMinimumHeight(normal ? 0 : iconSize + 6);
    m_navigator->setMinimumHeight(normal ? 0 : iconSize + 6);
}

void Pane::setNewTabsAfterCurrent(bool afterCurrent)
{
    m_newTabsAfterCurrent = afterCurrent;
}

void Pane::setFollowSymlinks(bool follow)
{
    m_followSymlinks = follow;
}

void Pane::setUrl(const QUrl &url)
{
    m_tabs->setCurrentUrl(m_followSymlinks ? Symlinks::resolve(url) : url);
}

void Pane::revealUrl(const QUrl &url)
{
    m_revealFolder.clear();
    m_revealItem.clear();
    m_revealTab = -1;
    const QFileInfo item(url.toLocalFile());
    if (!url.isLocalFile() || !item.exists() || item.fileName().isEmpty()) {
        Q_EMIT statusMessage(i18nc("@info:status", "The search result is no longer available: %1", url.toDisplayString()));
        return;
    }
    if (m_view->isFlat()) {
        Q_EMIT statusMessage(i18nc("@info:status", "Turn off flat view to reveal a search result."));
        return;
    }
    QUrl folder = QUrl::fromLocalFile(item.absolutePath());
    if (m_followSymlinks) {
        folder = Symlinks::resolve(folder);
    }
    m_revealFolder = folder;
    m_revealItem = QUrl::fromLocalFile(QDir(folder.toLocalFile()).filePath(item.fileName()));
    setUrl(folder);
    if (!m_revealItem.isEmpty()) {
        m_revealTab = m_tabs->currentIndex();
    }
    ++m_viewStateGeneration;
    m_pendingViewState = {};
    m_historyExpansionPending = false;
    m_view->cancelHistoryExpansion();
    if (m_view->url() == folder && !m_view->isLoading()) {
        finishReveal();
    }
}

void Pane::finishReveal()
{
    if (m_revealItem.isEmpty() || m_view->url() != m_revealFolder) {
        return;
    }
    const QUrl item = m_revealItem;
    m_revealFolder.clear();
    m_revealItem.clear();
    m_revealTab = -1;
    m_view->selectUrls({item});
    if (m_view->selectedItems().isEmpty()) {
        Q_EMIT statusMessage(i18nc("@info:status", "Could not reveal %1. It may have been removed, hidden or filtered.", item.toDisplayString()));
    } else {
        m_view->currentView()->setFocus();
    }
}

void Pane::openInNewTab(const QUrl &url, bool activate)
{
    const int index = m_newTabsAfterCurrent ? m_tabs->currentIndex() + 1 : m_tabs->count();
    m_tabs->insertTab(index, m_followSymlinks ? Symlinks::resolve(url) : url, activate);
}

void Pane::newTab()
{
    const QUrl url = m_tabs->currentUrl().isValid() ? m_tabs->currentUrl() : QUrl::fromLocalFile(QDir::homePath());
    openInNewTab(url, true);
}

void Pane::closeCurrentTab()
{
    m_tabs->closeTab(m_tabs->currentIndex());
}

void Pane::goBack()
{
    m_tabs->goBack();
}

void Pane::goForward()
{
    m_tabs->goForward();
}

void Pane::goUp()
{
    const QUrl parent = KIO::upUrl(url());
    if (parent.isValid() && parent != url()) {
        const QUrl child = url();
        m_tabs->setCurrentUrl(parent);
        // Highlight the folder we just left, once it is listed.
        connect(
            m_view,
            &FolderView::loadingChanged,
            this,
            [this, child](bool loading) {
                if (!loading) {
                    m_view->selectUrls({child});
                }
            },
            Qt::SingleShotConnection);
    }
}

void Pane::reload()
{
    m_view->reload();
}

void Pane::editUrl()
{
    m_navigator->setUrlEditable(true);
    m_navigator->setFocus();
}

bool Pane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_navigationBar && event->type() == QEvent::Resize) {
        updateNavigationLayout();
    } else if (watched == m_tabBar && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            const int index = m_tabBar->tabAt(mouseEvent->position().toPoint());
            if (index >= 0) {
                m_tabs->closeTab(index);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Pane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateNavigationLayout();
}

void Pane::updateNavigationLayout()
{
    const int minimumInlineWidth = m_toolBar->sizeHint().width() + 180;
    const auto direction = m_navigationBar->width() < minimumInlineWidth ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    if (m_navigationLayout->direction() != direction) {
        m_navigationLayout->setDirection(direction);
    }
}

void Pane::configureToolbar()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("paneToolbarConfigurationDialog"));
    dialog.setWindowTitle(i18nc("@title:window", "Customize Pane Toolbar"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *instructions = new QLabel(i18nc("@info", "Check entries to show them, and drag entries to change their order."), &dialog);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);
    auto *list = new QListWidget(&dialog);
    list->setObjectName(QStringLiteral("paneToolbarEntries"));
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    for (QAction *action : m_toolBar->actions()) {
        if (action->objectName() == QLatin1String("paneCustomizeToolbar")) {
            continue;
        }
        auto *item = new QListWidgetItem(action->text(), list);
        item->setData(Qt::UserRole, action->objectName());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
        item->setCheckState(action->isVisible() ? Qt::Checked : Qt::Unchecked);
    }
    layout->addWidget(list);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList order;
    QStringList visible;
    for (int row = 0; row < list->count(); ++row) {
        const QListWidgetItem *item = list->item(row);
        const QString name = item->data(Qt::UserRole).toString();
        order.append(name);
        if (item->checkState() == Qt::Checked) {
            visible.append(name);
        }
    }
    saveToolbarConfiguration(order, visible);
    restoreToolbarConfiguration();
}

void Pane::restoreToolbarConfiguration()
{
    const KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("PaneToolbars")).group(m_toolbarConfigId);
    if (!group.hasKey(QStringLiteral("Actions"))) {
        return;
    }
    const QStringList order = group.readEntry(QStringLiteral("Actions"), QStringList());
    const QStringList visible = group.readEntry(QStringLiteral("Visible"), QStringList());
    const QStringList hidden = group.readEntry(QStringLiteral("Hidden"), QStringList());
    const bool hasVisibility = group.hasKey(QStringLiteral("Visible"));
    QHash<QString, QAction *> actions;
    QAction *configure = m_toolBar->actions().last();
    for (QAction *action : m_toolBar->actions()) {
        if (action->objectName() != QLatin1String("paneCustomizeToolbar")) {
            actions.insert(action->objectName(), action);
            action->setVisible(!hidden.contains(action->objectName())
                               && (!hasVisibility || visible.contains(action->objectName()) || !order.contains(action->objectName())));
        }
    }
    for (int i = order.size() - 1; i >= 0; --i) {
        const QString &name = order.at(i);
        if (QAction *action = actions.value(name)) {
            m_toolBar->insertAction(configure, action);
            configure = action;
        }
    }
}

void Pane::saveToolbarConfiguration(const QStringList &order, const QStringList &visible)
{
    KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("PaneToolbars")).group(m_toolbarConfigId);
    group.writeEntry(QStringLiteral("Actions"), order);
    group.writeEntry(QStringLiteral("Visible"), visible);
    QStringList hidden = group.readEntry(QStringLiteral("Hidden"), QStringList());
    hidden.removeIf([&order](const QString &name) {
        return order.contains(name);
    });
    group.writeEntry(QStringLiteral("Hidden"), hidden);
    group.sync();
}

void Pane::saveCurrentToolbarConfiguration()
{
    QStringList order;
    QStringList visible;
    for (QAction *action : m_toolBar->actions()) {
        const QString name = action->objectName();
        if (name.isEmpty() || name == QLatin1String("paneCustomizeToolbar")) {
            continue;
        }
        order.append(name);
        if (action->isVisible()) {
            visible.append(name);
        }
    }
    saveToolbarConfiguration(order, visible);
}

void Pane::syncTabBar()
{
    m_updatingTabBar = true;
    while (m_tabBar->count() > m_tabs->count()) {
        m_tabBar->removeTab(m_tabBar->count() - 1);
    }
    while (m_tabBar->count() < m_tabs->count()) {
        m_tabBar->addTab(QString());
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        updateTabText(i);
    }
    m_tabBar->setCurrentIndex(m_tabs->currentIndex());
    m_updatingTabBar = false;

    updateTabStripVisibility();
    if (m_lastTabCount != m_tabs->count()) {
        m_lastTabCount = m_tabs->count();
        Q_EMIT tabCountChanged(m_lastTabCount);
    }
}

void Pane::updateTabStripVisibility()
{
    const bool multipleTabs = m_tabs->count() > 1;
    const bool closable = multipleTabs && Settings::showTabCloseButtons();
    if (m_tabBar->tabsClosable() != closable) {
        m_tabBar->setTabsClosable(closable);
        for (int i = 0; i < m_tabBar->count(); ++i) {
            updateTabText(i);
        }
    }
    m_tabStrip->setVisible(m_tabBarForced || multipleTabs);
}

PaneSession Pane::saveState() const
{
    PaneSession state;
    state.tabs = m_tabs->urls();
    state.currentTab = m_tabs->currentIndex();
    state.pinnedCount = m_tabs->pinnedCount();
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->isLocked(i)) {
            state.lockedTabs.append(i);
        }
        state.names.append(m_tabs->nameAt(i));
    }
    const NameFilter filter = m_filterBar->filter();
    state.filterText = filter.text();
    state.filterScope = static_cast<int>(filter.scope());
    return state;
}

void Pane::restoreState(const PaneSession &state)
{
    for (const QUrl &url : state.tabs) {
        openInNewTab(url, false);
    }
    for (int i = 0; i < state.pinnedCount; ++i) {
        m_tabs->setPinned(i, true);
    }
    for (int index : state.lockedTabs) {
        m_tabs->setLocked(index, true);
    }
    for (int i = 0; i < state.names.size(); ++i) {
        m_tabs->setName(i, state.names.at(i));
    }
    m_tabs->setCurrentIndex(state.currentTab);
    const int scope = state.filterScope < 0 ? Settings::defaultFilterScope() : state.filterScope;
    m_filterBar->setFilter(NameFilter(state.filterText, static_cast<NameFilter::Scope>(qBound(0, scope, 2))));
}

void Pane::reloadFilterSettings()
{
    if (m_defaultFilterScope != Settings::defaultFilterScope()) {
        m_defaultFilterScope = Settings::defaultFilterScope();
        m_filterBar->setFilter(NameFilter(m_filterBar->filter().text(), static_cast<NameFilter::Scope>(qBound(0, m_defaultFilterScope, 2))));
    }
}

void Pane::reloadTabSettings()
{
    m_showTabIcons = Settings::tabIcons();
    m_tabBar->setExpanding(Settings::expandTabs());
    updateTabStripVisibility();
    for (int index = 0; index < m_tabBar->count(); ++index) {
        updateTabText(index);
    }
}

void Pane::updateTabText(int index)
{
    if (index < 0 || index >= m_tabBar->count()) {
        return;
    }
    const QUrl url = m_tabs->urlAt(index);
    const QString name = m_tabs->nameAt(index);
    const bool pinned = m_tabs->isPinned(index);
    const bool locked = m_tabs->isLocked(index);
    m_tabBar->setTabText(index, name.isEmpty() ? tabTitle(url) : name);
    QString toolTip = url.toDisplayString(QUrl::PreferLocalFile);
    QStringList states;
    if (pinned) {
        states.append(i18nc("@info:tooltip tab state", "Pinned"));
    }
    if (locked) {
        states.append(i18nc("@info:tooltip tab state", "Locked"));
    }
    if (!states.isEmpty()) {
        toolTip += u'\n' + states.join(i18nc("@separator between tab states", ", "));
    }
    m_tabBar->setTabToolTip(index, toolTip);
    const QString iconName = pinned ? QStringLiteral("pin") : locked ? QStringLiteral("object-locked") : KIO::iconNameForUrl(url);
    m_tabBar->setTabIcon(index, m_showTabIcons ? SymbolicIcon::fromTheme(iconName) : QIcon());
    m_tabBar->setTabTextColor(index, Settings::showTabColors() ? LabelColors::rgb(m_colors->colorFor(url)) : QColor());

    const auto closeSide = static_cast<QTabBar::ButtonPosition>(m_tabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, m_tabBar));
    if (QWidget *closeButton = m_tabBar->tabButton(index, closeSide)) {
        closeButton->setVisible(!pinned && !locked);
    }
}

void Pane::updateNavigationButtons()
{
    m_back->setEnabled(m_tabs->canGoBack());
    m_forward->setEnabled(m_tabs->canGoForward());
    const QUrl current = url();
    m_up->setEnabled(current.isValid() && KIO::upUrl(current) != current);
}

void Pane::showTabContextMenu(int index, const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *newTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "New Tab"));
    QAction *duplicateAction = nullptr;
    QAction *closeAction = nullptr;
    QAction *closeOthersAction = nullptr;
    QAction *closeRightAction = nullptr;
    QAction *copyPathAction = nullptr;
    QAction *renameAction = nullptr;
    QAction *pinAction = nullptr;
    QAction *lockAction = nullptr;
    if (index >= 0) {
        duplicateAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-duplicate")), i18nc("@action:inmenu", "Duplicate Tab"));
        renameAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), i18nc("@action:inmenu", "Rename Tab…"));
        pinAction = menu.addAction(QIcon::fromTheme(QStringLiteral("pin")),
                                   m_tabs->isPinned(index) ? i18nc("@action:inmenu", "Unpin Tab") : i18nc("@action:inmenu", "Pin Tab"));
        lockAction = menu.addAction(QIcon::fromTheme(QStringLiteral("object-locked")),
                                    m_tabs->isLocked(index) ? i18nc("@action:inmenu", "Unlock Tab") : i18nc("@action:inmenu", "Lock Tab"));

        QMenu *colorMenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("color-management")), i18nc("@title:menu", "Tab Colour"));
        const QUrl url = m_tabs->urlAt(index);
        const QList<std::pair<LabelColor, QString>> colors{
            {LabelColor::None, i18nc("@item:inmenu no tab colour", "None")},
            {LabelColor::Red, i18nc("@item:inmenu tab colour", "Red")},
            {LabelColor::Orange, i18nc("@item:inmenu tab colour", "Orange")},
            {LabelColor::Yellow, i18nc("@item:inmenu tab colour", "Yellow")},
            {LabelColor::Green, i18nc("@item:inmenu tab colour", "Green")},
            {LabelColor::Blue, i18nc("@item:inmenu tab colour", "Blue")},
            {LabelColor::Purple, i18nc("@item:inmenu tab colour", "Purple")},
            {LabelColor::Pink, i18nc("@item:inmenu tab colour", "Pink")},
            {LabelColor::Gray, i18nc("@item:inmenu tab colour", "Gray")},
        };
        for (const auto &[color, text] : colors) {
            QAction *colorAction = colorMenu->addAction(text);
            colorAction->setCheckable(true);
            colorAction->setChecked(m_colors->colorFor(url) == color);
            if (color != LabelColor::None) {
                QPixmap swatch(16, 16);
                swatch.fill(LabelColors::rgb(color));
                colorAction->setIcon(QIcon(swatch));
            }
            connect(colorAction, &QAction::triggered, this, [this, url, color] {
                m_colors->setColor(url, color);
            });
        }

        menu.addSeparator();
        copyPathAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy-path")), i18nc("@action:inmenu", "Copy Path"));
        menu.addSeparator();
        closeAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close")), i18nc("@action:inmenu", "Close Tab"));
        closeAction->setEnabled(m_tabs->count() > 1 && !m_tabs->isPinned(index) && !m_tabs->isLocked(index));
        closeOthersAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close-other")), i18nc("@action:inmenu", "Close Other Tabs"));
        closeOthersAction->setEnabled(m_tabs->count() > 1);
        closeRightAction = menu.addAction(i18nc("@action:inmenu", "Close Tabs to the Right"));
        closeRightAction->setEnabled(index < m_tabs->count() - 1);
    }

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) {
        return;
    }
    if (chosen == newTabAction) {
        newTab();
    } else if (chosen == duplicateAction) {
        m_tabs->insertTab(index + 1, m_tabs->urlAt(index), true);
    } else if (chosen == copyPathAction) {
        QApplication::clipboard()->setText(m_tabs->urlAt(index).toDisplayString(QUrl::PreferLocalFile));
    } else if (chosen == renameAction) {
        bool accepted = false;
        const QString name = QInputDialog::getText(this,
                                                   i18nc("@title:window", "Rename Tab"),
                                                   i18nc("@label:textbox", "Tab name (empty for the folder name):"),
                                                   QLineEdit::Normal,
                                                   m_tabs->nameAt(index),
                                                   &accepted);
        if (accepted) {
            m_tabs->setName(index, name);
        }
    } else if (chosen == pinAction) {
        m_tabs->setPinned(index, !m_tabs->isPinned(index));
    } else if (chosen == lockAction) {
        m_tabs->setLocked(index, !m_tabs->isLocked(index));
    } else if (chosen == closeAction) {
        m_tabs->closeTab(index);
    } else if (chosen == closeRightAction) {
        for (int i = m_tabs->count() - 1; i > index; --i) {
            m_tabs->closeTab(i);
        }
    } else if (chosen == closeOthersAction) {
        m_tabs->setCurrentIndex(index);
        for (int i = m_tabs->count() - 1; i >= 0; --i) {
            if (i != m_tabs->currentIndex()) {
                m_tabs->closeTab(i);
            }
        }
    }
}

void Pane::showFilterBar()
{
    m_filterBar->open();
}

QString Pane::tabTitle(const QUrl &url)
{
    if (url.isLocalFile()) {
        const QString path = url.toLocalFile();
        if (path == QLatin1String("/")) {
            return i18nc("@title:tab the filesystem root", "Root");
        }
        if (path == QDir::homePath()) {
            return i18nc("@title:tab the user's home folder", "Home");
        }
    }
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? url.toDisplayString(QUrl::PreferLocalFile) : name;
}
