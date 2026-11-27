// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktoptray.h"

#include "appidentity.h"
#include "settings.h"
#include "symbolicicon.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStatusNotifierItem>
#include <KWindowSystem>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QEvent>
#include <QMenu>
#include <QTimer>
#include <QWidget>
#include <QWindow>

namespace
{
/// Whether Plasma, or another StatusNotifierItem host, can show tray icons.
bool trayHostRegistered()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.StatusNotifierWatcher"),
                                                          QStringLiteral("/StatusNotifierWatcher"),
                                                          QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
    message << QStringLiteral("org.kde.StatusNotifierWatcher") << QStringLiteral("IsStatusNotifierHostRegistered");
    const QDBusReply<QDBusVariant> reply = bus.call(message, QDBus::Block, 1000);
    return reply.isValid() && reply.value().variant().toBool();
}

/// Where the global shortcut's key is kept while the shortcut is turned off,
/// because KDE forgets the keys of shortcuts that are removed.
KConfigGroup trayState()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("Tray"));
}

constexpr auto DisabledGlobalShortcutKey = "DisabledGlobalShortcut";
}

DesktopTray::DesktopTray(QWidget *window, KActionCollection *actions)
    : QObject(window)
    , m_window(window)
    , m_toggleAction(actions->addAction(QStringLiteral("toggle_window_visibility")))
    , m_hostProbe(trayHostRegistered)
{
    m_toggleAction->setText(i18nc("@action", "Show or Hide Plasma Opus"));
    m_toggleAction->setIcon(AppIdentity::applicationIcon());
    connect(m_toggleAction, &QAction::triggered, this, &DesktopTray::toggleWindow);
    applyGlobalShortcut();
    window->installEventFilter(this);
}

DesktopTray::~DesktopTray()
{
    if (m_hiddenToTray) {
        QApplication::setQuitOnLastWindowClosed(true);
    }
}

void DesktopTray::applySettings()
{
    if (Settings::showTrayIcon() || m_forced) {
        createItem();
    } else if (m_item) {
        delete m_item;
        if (m_window && m_hiddenToTray) {
            bringToFront(m_window);
        }
    }
    updateMenu();
    applyGlobalShortcut();
}

void DesktopTray::applyGlobalShortcut()
{
    const bool registered = globalShortcutRegistered();
    if (Settings::globalShortcut() && !registered) {
        // KDE keeps a shortcut the user has chosen in System Settings, and
        // leaves the action without one when the default is already taken.
        KGlobalAccel::setGlobalShortcut(m_toggleAction, defaultGlobalShortcut());
        KConfigGroup state = trayState();
        if (state.hasKey(DisabledGlobalShortcutKey)) {
            const QKeySequence kept = QKeySequence::fromString(state.readEntry(DisabledGlobalShortcutKey, QString()), QKeySequence::PortableText);
            KGlobalAccel::self()->setShortcut(m_toggleAction, kept.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{kept}, KGlobalAccel::NoAutoloading);
            state.deleteEntry(DisabledGlobalShortcutKey);
            state.sync();
        }
    } else if (!Settings::globalShortcut() && registered) {
        KConfigGroup state = trayState();
        state.writeEntry(DisabledGlobalShortcutKey, KGlobalAccel::self()->shortcut(m_toggleAction).value(0).toString(QKeySequence::PortableText));
        state.sync();
        KGlobalAccel::self()->removeAllShortcuts(m_toggleAction);
    }
}

QKeySequence DesktopTray::defaultGlobalShortcut()
{
    return QKeySequence(Qt::META | Qt::ALT | Qt::Key_O);
}

bool DesktopTray::globalShortcutServiceAvailable()
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()) {
        return false;
    }
    const QString service = QStringLiteral("org.kde.kglobalaccel");
    return bus.interface()->isServiceRegistered(service) || bus.interface()->activatableServiceNames().value().contains(service);
}

bool DesktopTray::globalShortcutRegistered() const
{
    return KGlobalAccel::self()->hasShortcut(m_toggleAction);
}

QKeySequence DesktopTray::globalShortcut() const
{
    if (globalShortcutRegistered()) {
        return KGlobalAccel::self()->shortcut(m_toggleAction).value(0);
    }
    const KConfigGroup state = trayState();
    if (!state.hasKey(DisabledGlobalShortcutKey)) {
        return defaultGlobalShortcut();
    }
    return QKeySequence::fromString(state.readEntry(DisabledGlobalShortcutKey, QString()), QKeySequence::PortableText);
}

void DesktopTray::setGlobalShortcut(const QKeySequence &sequence)
{
    if (globalShortcutRegistered()) {
        KGlobalAccel::self()->setShortcut(m_toggleAction,
                                          sequence.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{sequence},
                                          KGlobalAccel::NoAutoloading);
        return;
    }
    KConfigGroup state = trayState();
    state.writeEntry(DisabledGlobalShortcutKey, sequence.toString(QKeySequence::PortableText));
    state.sync();
}

void DesktopTray::setHostProbe(std::function<bool()> probe)
{
    m_hostProbe = std::move(probe);
}

bool DesktopTray::trayIconShown() const
{
    return m_item;
}

TrayPolicy::State DesktopTray::state() const
{
    return {.trayIconEnabled = trayIconShown(), .trayHostAvailable = trayIconShown() && m_hostProbe && m_hostProbe()};
}

QAction *DesktopTray::toggleAction() const
{
    return m_toggleAction;
}

QMenu *DesktopTray::contextMenu() const
{
    return m_item ? m_item->contextMenu() : nullptr;
}

void DesktopTray::bringToFront(QWidget *window)
{
    window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
    window->show();
    window->raise();
    window->activateWindow();
    if (QWindow *handle = window->windowHandle()) {
        KWindowSystem::activateWindow(handle);
    }
}

void DesktopTray::toggleWindow()
{
    if (!m_window) {
        return;
    }
    applyToggle(TrayPolicy::toggle(state(), m_window->isVisible(), m_window->isMinimized(), m_window->isActiveWindow()));
}

void DesktopTray::applyToggle(TrayPolicy::Toggle toggle)
{
    switch (toggle) {
    case TrayPolicy::Toggle::Show:
        bringToFront(m_window);
        break;
    case TrayPolicy::Toggle::Hide:
        hideToTray();
        break;
    case TrayPolicy::Toggle::Minimize:
        m_window->showMinimized();
        break;
    }
}

bool DesktopTray::hideToTray()
{
    if (!m_window || !TrayPolicy::canHide(state())) {
        return false;
    }
    // Dialogs and gallery windows closing while the main window is hidden
    // must not end the application.
    m_hiddenToTray = true;
    QApplication::setQuitOnLastWindowClosed(false);
    m_window->hide();
    updateStatus();
    return true;
}

bool DesktopTray::startHidden(bool explicitRequest)
{
    return TrayPolicy::startHidden(state(), Settings::startHiddenInTray(), explicitRequest) && hideToTray();
}

void DesktopTray::startInBackground()
{
    m_forced = true;
    createItem();
    m_hiddenToTray = true;
    QApplication::setQuitOnLastWindowClosed(false);
    updateStatus();
}

void DesktopTray::setAttention(const QString &message)
{
    m_attention = message;
    updateStatus();
}

void DesktopTray::setQuitCheck(std::function<bool()> check)
{
    m_quitCheck = std::move(check);
}

bool DesktopTray::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window) {
        return false;
    }
    switch (event->type()) {
    case QEvent::Close:
        if (TrayPolicy::hideOnClose(state(), Settings::closeToTray() || m_forced, event->spontaneous())) {
            event->ignore();
            hideToTray();
            return true;
        }
        break;
    case QEvent::WindowStateChange:
        if (m_window->isMinimized() && TrayPolicy::hideOnMinimize(state(), Settings::minimizeToTray())) {
            // Hide once the minimize has finished.
            QTimer::singleShot(0, this, [this] {
                if (m_window && m_window->isMinimized()) {
                    hideToTray();
                }
            });
        }
        break;
    case QEvent::Show:
        if (m_hiddenToTray) {
            m_hiddenToTray = false;
            QApplication::setQuitOnLastWindowClosed(true);
        }
        updateStatus();
        break;
    default:
        break;
    }
    return false;
}

void DesktopTray::createItem()
{
    if (m_item) {
        return;
    }
    m_item = new KStatusNotifierItem(AppIdentity::desktopFileName(), this);
    m_item->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_item->setTitle(AppIdentity::displayName());
    m_item->setToolTipTitle(AppIdentity::displayName());
    if (QIcon::hasThemeIcon(AppIdentity::desktopFileName())) {
        m_item->setIconByName(AppIdentity::desktopFileName());
        m_item->setToolTipIconByName(AppIdentity::desktopFileName());
    } else {
        m_item->setIconByPixmap(AppIdentity::applicationIcon());
        m_item->setToolTipIconByPixmap(AppIdentity::applicationIcon());
    }

    // The item takes ownership of the menu and appends its Quit action.
    auto *menu = new QMenu(m_window);
    m_showHideAction = menu->addAction(QString());
    m_showHideAction->setObjectName(QStringLiteral("trayShowHide"));
    connect(m_showHideAction, &QAction::triggered, this, [this] {
        if (m_window) {
            applyToggle(TrayPolicy::toggle(state(), m_window->isVisible(), m_window->isMinimized(), true));
        }
    });
    menu->addSeparator();
    QAction *home = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("user-home")), i18nc("@action:inmenu", "Open Home Folder"));
    home->setObjectName(QStringLiteral("trayOpenHome"));
    connect(home, &QAction::triggered, this, [this] {
        Q_EMIT openRequested(QUrl::fromLocalFile(QDir::homePath()));
        if (m_window) {
            bringToFront(m_window);
        }
    });
    QAction *transfers = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-list-details")), i18nc("@action:inmenu", "Transfer Queue…"));
    transfers->setObjectName(QStringLiteral("trayTransfers"));
    connect(transfers, &QAction::triggered, this, [this] {
        if (m_window) {
            bringToFront(m_window);
        }
        Q_EMIT transfersRequested();
    });
    QAction *cloudSync = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")), i18nc("@action:inmenu", "Cloud Sync…"));
    cloudSync->setObjectName(QStringLiteral("trayCloudSync"));
    connect(cloudSync, &QAction::triggered, this, [this] {
        if (m_window) {
            bringToFront(m_window);
        }
        Q_EMIT cloudSyncRequested();
    });
    QAction *cloudActivity = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("network-transmit-receive")), i18nc("@action:inmenu", "Cloud Activity…"));
    cloudActivity->setObjectName(QStringLiteral("trayCloudActivity"));
    m_cloudActivityAction = cloudActivity;
    connect(cloudActivity, &QAction::triggered, this, [this] {
        if (m_window) {
            bringToFront(m_window);
        }
        Q_EMIT cloudActivityRequested();
    });
    connect(menu, &QMenu::aboutToShow, this, &DesktopTray::updateMenu);
    m_item->setContextMenu(menu);
    updateMenu();
    // The tray's Quit action quits without closing the window, so pending
    // uploads are checked here.
    connect(m_item, &KStatusNotifierItem::quitRequested, this, [this] {
        if (m_quitCheck && !m_quitCheck()) {
            m_item->abortQuit();
        }
    });

    // Clicking the icon shows or hides the window regardless of focus, which
    // the tray itself may have taken.
    connect(m_item, &KStatusNotifierItem::activateRequested, this, [this] {
        if (m_window) {
            applyToggle(TrayPolicy::toggle(state(), m_window->isVisible(), m_window->isMinimized(), true));
        }
    });
    updateStatus();
}

void DesktopTray::updateStatus()
{
    if (!m_item) {
        return;
    }
    // Plasma keeps passive items in the tray's hidden section; a window
    // hidden to the tray makes the icon visible so it can be found again.
    if (!m_attention.isEmpty()) {
        m_item->setStatus(KStatusNotifierItem::NeedsAttention);
    } else {
        m_item->setStatus(m_hiddenToTray ? KStatusNotifierItem::Active : KStatusNotifierItem::Passive);
    }
    m_item->setToolTipSubTitle(m_attention);
}

void DesktopTray::updateMenu()
{
    if (m_cloudActivityAction) {
        m_cloudActivityAction->setVisible(Settings::cloudActivityInTray());
    }
    if (!m_showHideAction || !m_window) {
        return;
    }
    const bool shown = m_window->isVisible() && !m_window->isMinimized();
    m_showHideAction->setText(shown ? i18nc("@action:inmenu", "Hide Plasma Opus") : i18nc("@action:inmenu", "Show Plasma Opus"));
}
