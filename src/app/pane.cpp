// SPDX-License-Identifier: GPL-3.0-or-later

#include "pane.h"

#include "folderview.h"
#include "symlinks.h"
#include "tabmodel.h"

#include <KFilePlacesModel>
#include <KIO/Global>
#include <KLocalizedString>
#include <KUrlNavigator>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

Pane::Pane(KFilePlacesModel *places, QWidget *parent)
    : QWidget(parent)
    , m_tabs(new TabModel(this))
    , m_tabBar(new QTabBar(this))
    , m_tabStrip(new QWidget(this))
    , m_back(new QToolButton(this))
    , m_forward(new QToolButton(this))
    , m_up(new QToolButton(this))
    , m_reload(new QToolButton(this))
    , m_navigator(new KUrlNavigator(places, QUrl(), this))
    , m_view(new FolderView(this))
{
    m_tabBar->setDocumentMode(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabBar->installEventFilter(this);

    auto *addTab = new QToolButton(m_tabStrip);
    addTab->setAutoRaise(true);
    addTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    addTab->setToolTip(i18nc("@info:tooltip", "Open a new tab"));
    connect(addTab, &QToolButton::clicked, this, &Pane::newTab);

    auto *stripLayout = new QHBoxLayout(m_tabStrip);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->setSpacing(0);
    stripLayout->addWidget(m_tabBar, 1);
    stripLayout->addWidget(addTab);

    const auto configureButton = [](QToolButton *button, const QString &icon, const QString &tooltip) {
        button->setAutoRaise(true);
        button->setIcon(QIcon::fromTheme(icon));
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

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_tabStrip);
    m_layout->addWidget(m_navigationBar);
    m_layout->addWidget(m_view, 1);

    // Model -> widgets
    connect(m_tabs, &TabModel::tabInserted, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::tabRemoved, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::tabMoved, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::currentIndexChanged, this, &Pane::syncTabBar);
    connect(m_tabs, &TabModel::tabUrlChanged, this, [this](int index, const QUrl &) {
        updateTabText(index);
    });
    connect(m_tabs, &TabModel::currentUrlChanged, this, [this](const QUrl &url) {
        m_navigator->setLocationUrl(url);
        m_view->setUrl(url);
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
            m_tabs->moveTab(from, to);
            m_updatingTabBar = false;
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
    connect(m_view, &FolderView::goUpRequested, this, &Pane::goUp);
    connect(m_view, &FolderView::listingFailed, this, &Pane::statusMessage);

    syncTabBar();
    updateNavigationButtons();
}

Pane::~Pane() = default;

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
    m_navigator->setActive(active);
    if (active) {
        m_view->currentView()->setFocus();
    }
}

void Pane::setTabBarForcedVisible(bool visible)
{
    m_tabBarForced = visible;
    updateTabStripVisibility();
}

void Pane::setTabBarAtBottom(bool atBottom)
{
    m_tabBar->setShape(atBottom ? QTabBar::RoundedSouth : QTabBar::RoundedNorth);
    m_layout->removeWidget(m_tabStrip);
    m_layout->insertWidget(atBottom ? m_layout->count() : 0, m_tabStrip);
}

bool Pane::isTabBarAtBottom() const
{
    return m_tabBar->shape() == QTabBar::RoundedSouth;
}

void Pane::setNavigationBarVisible(bool visible)
{
    m_navigationBar->setVisible(visible);
}

bool Pane::isNavigationBarVisible() const
{
    return !m_navigationBar->isHidden();
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
    const bool closable = m_tabs->count() > 1;
    m_tabBar->setTabsClosable(closable);
    m_tabStrip->setVisible(m_tabBarForced || closable);
}

void Pane::updateTabText(int index)
{
    if (index < 0 || index >= m_tabBar->count()) {
        return;
    }
    const QUrl url = m_tabs->urlAt(index);
    m_tabBar->setTabText(index, tabTitle(url));
    m_tabBar->setTabToolTip(index, url.toDisplayString(QUrl::PreferLocalFile));
    m_tabBar->setTabIcon(index, QIcon::fromTheme(KIO::iconNameForUrl(url)));
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
    QAction *copyPathAction = nullptr;
    if (index >= 0) {
        duplicateAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-duplicate")), i18nc("@action:inmenu", "Duplicate Tab"));
        menu.addSeparator();
        copyPathAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy-path")), i18nc("@action:inmenu", "Copy Path"));
        menu.addSeparator();
        closeAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close")), i18nc("@action:inmenu", "Close Tab"));
        closeAction->setEnabled(m_tabs->count() > 1);
        closeOthersAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close-other")), i18nc("@action:inmenu", "Close Other Tabs"));
        closeOthersAction->setEnabled(m_tabs->count() > 1);
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
    } else if (chosen == closeAction) {
        m_tabs->closeTab(index);
    } else if (chosen == closeOthersAction) {
        m_tabs->setCurrentIndex(index);
        for (int i = m_tabs->count() - 1; i >= 0; --i) {
            if (i != m_tabs->currentIndex()) {
                m_tabs->closeTab(i);
            }
        }
    }
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
