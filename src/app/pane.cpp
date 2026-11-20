// SPDX-License-Identifier: GPL-3.0-or-later

#include "pane.h"

#include "filterbar.h"
#include "folderview.h"
#include "settings.h"
#include "symbolicicon.h"
#include "symlinks.h"
#include "tabcolors.h"
#include "tabmodel.h"

#include <KFilePlacesModel>
#include <KIO/Global>
#include <KLocalizedString>
#include <KUrlNavigator>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

Pane::Pane(KFilePlacesModel *places, TabColors *colors, QWidget *parent)
    : QWidget(parent)
    , m_tabs(new TabModel(this))
    , m_tabBar(new QTabBar(this))
    , m_tabStrip(new QWidget(this))
    , m_back(new QToolButton(this))
    , m_forward(new QToolButton(this))
    , m_up(new QToolButton(this))
    , m_reload(new QToolButton(this))
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

    const auto configureButton = [](QToolButton *button, const QString &icon, const QString &tooltip) {
        button->setAutoRaise(true);
        button->setIcon(SymbolicIcon::fromTheme(icon));
        button->setToolTip(tooltip);
        button->setFocusPolicy(Qt::NoFocus);
    };
    configureButton(m_back, QStringLiteral("go-previous"), i18nc("@info:tooltip", "Back"));
    configureButton(m_forward, QStringLiteral("go-next"), i18nc("@info:tooltip", "Forward"));
    configureButton(m_up, QStringLiteral("go-up"), i18nc("@info:tooltip", "Parent folder"));
    configureButton(m_reload, QStringLiteral("view-refresh"), i18nc("@info:tooltip", "Reload"));

    m_navigator->setPlacesSelectorVisible(false);

    m_navigationBar = new QWidget(this);
    auto *pathRow = new QHBoxLayout(m_navigationBar);
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(0);
    pathRow->addWidget(m_back);
    pathRow->addWidget(m_forward);
    pathRow->addWidget(m_up);
    pathRow->addWidget(m_reload);
    pathRow->addWidget(m_navigator, 1);

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

    connect(m_back, &QToolButton::clicked, this, &Pane::goBack);
    connect(m_forward, &QToolButton::clicked, this, &Pane::goForward);
    connect(m_up, &QToolButton::clicked, this, &Pane::goUp);
    connect(m_reload, &QToolButton::clicked, this, &Pane::reload);

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

void Pane::setInactiveDimming(bool enabled)
{
    m_inactiveDimming = enabled;
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
    auto *layout = qobject_cast<QHBoxLayout *>(m_navigationBar->layout());
    if (!layout) {
        return;
    }
    const bool normal = density == QLatin1String("Normal");
    const int margin = density == QLatin1String("Compact") ? 2 : 0;
    const int defaultIconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    const int iconSize = qMax(12, defaultIconSize - (density == QLatin1String("Compact") ? 2 : normal ? 0 : 4));
    layout->setContentsMargins(normal ? 0 : margin, normal ? 0 : margin, normal ? 0 : margin, normal ? 0 : margin);
    layout->setSpacing(normal ? 0 : margin);
    for (QToolButton *button : {m_back, m_forward, m_up, m_reload}) {
        button->setIconSize(QSize(iconSize, iconSize));
        if (normal) {
            button->setMinimumSize(0, 0);
            button->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        } else {
            button->setFixedSize(iconSize + 6, iconSize + 6);
        }
    }
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
    if (watched == m_tabBar && event->type() == QEvent::MouseButtonRelease) {
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
