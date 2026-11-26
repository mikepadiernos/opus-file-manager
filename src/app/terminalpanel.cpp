// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpanel.h"

#include "settings.h"
#include "symbolicicon.h"

#include <KLocalizedString>
#include <KParts/PartLoader>
#include <KParts/ReadOnlyPart>
#include <KPluginMetaData>
#include <kde_terminal_interface.h>

#include <QActionGroup>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_status(new QLabel(this))
    , m_title(new QLabel(i18nc("@title", "Terminal"), this))
    , m_statusRow(new QWidget(this))
    , m_panelMenu(new QMenu(this))
    , m_tabs(new QTabWidget(this))
{
    setObjectName(QStringLiteral("terminalPanel"));
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_panelMenu->setTitle(i18nc("@title:menu", "Terminal Panel"));
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("terminalPanelHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 8, 0);
    m_title->setObjectName(QStringLiteral("terminalPanelTitle"));
    headerLayout->addWidget(m_title, 1);
    auto *menuButton = new QToolButton(header);
    menuButton->setObjectName(QStringLiteral("terminalPanelMenuButton"));
    menuButton->setIcon(SymbolicIcon::fromTheme(QStringLiteral("application-menu")));
    menuButton->setToolTip(i18nc("@info:tooltip", "Terminal panel options"));
    menuButton->setAccessibleName(i18nc("@action", "Terminal panel options"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setMenu(m_panelMenu);
    headerLayout->addWidget(menuButton);
    auto *close = new QToolButton(header);
    close->setObjectName(QStringLiteral("terminalPanelClose"));
    close->setIcon(SymbolicIcon::fromTheme(QStringLiteral("window-close")));
    close->setToolTip(i18nc("@info:tooltip", "Hide terminal panel"));
    close->setAccessibleName(i18nc("@action", "Hide terminal panel"));
    connect(close, &QToolButton::clicked, this, &TerminalPanel::closeRequested);
    headerLayout->addWidget(close);
    m_layout->addWidget(header);

    m_status->setObjectName(QStringLiteral("terminalPanelStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    auto *statusLayout = new QHBoxLayout(m_statusRow);
    statusLayout->setContentsMargins(8, 0, 8, 0);
    statusLayout->addWidget(m_status, 1);
    auto *dismiss = new QToolButton(m_statusRow);
    dismiss->setObjectName(QStringLiteral("terminalPanelDismissStatus"));
    dismiss->setIcon(SymbolicIcon::fromTheme(QStringLiteral("window-close")));
    dismiss->setToolTip(i18nc("@info:tooltip", "Dismiss terminal message"));
    dismiss->setAccessibleName(i18nc("@action", "Dismiss terminal message"));
    connect(dismiss, &QToolButton::clicked, this, &TerminalPanel::dismissStatus);
    statusLayout->addWidget(dismiss);
    m_statusRow->hide();
    m_layout->addWidget(m_statusRow);
    auto *dismissAction = m_panelMenu->addAction(i18nc("@action", "Dismiss Message"));
    dismissAction->setObjectName(QStringLiteral("terminalPanelDismissAction"));
    connect(dismissAction, &QAction::triggered, this, &TerminalPanel::dismissStatus);
    connect(m_panelMenu, &QMenu::aboutToShow, this, [this, dismissAction] {
        dismissAction->setEnabled(!m_statusRow->isHidden());
    });
    auto *closeAction = m_panelMenu->addAction(i18nc("@action", "Hide Terminal Panel"));
    closeAction->setObjectName(QStringLiteral("terminalPanelHideAction"));
    connect(closeAction, &QAction::triggered, this, &TerminalPanel::closeRequested);
    m_panelMenu->addSeparator();
    QMenu *components = m_panelMenu->addMenu(i18nc("@title:menu", "Terminal Component"));
    components->setObjectName(QStringLiteral("terminalComponentMenu"));
    components->setToolTipsVisible(true);
    auto *componentGroup = new QActionGroup(components);
    for (const TerminalProviders::Provider provider : TerminalProviders::embeddedProviders()) {
        QAction *component = components->addAction(TerminalProviders::name(provider));
        component->setObjectName(QStringLiteral("terminalComponent_") + TerminalProviders::id(provider));
        component->setCheckable(true);
        component->setData(static_cast<int>(provider));
        componentGroup->addAction(component);
        connect(component, &QAction::triggered, this, [this, provider] {
            setEmbeddedProvider(provider);
        });
    }
    connect(components, &QMenu::aboutToShow, this, [components] {
        const TerminalProviders::Provider current = TerminalPanel::embeddedProvider();
        const QList<QAction *> entries = components->actions();
        for (QAction *component : entries) {
            const auto provider = static_cast<TerminalProviders::Provider>(component->data().toInt());
            const QString reason = TerminalProviders::partUnavailableReason(provider);
            component->setChecked(provider == current);
            component->setEnabled(reason.isEmpty() || provider == current);
            component->setToolTip(reason.isEmpty() ? i18nc("@info:tooltip %1 is a terminal application",
                                                           "Start new terminal sessions with %1. Open sessions are unchanged.",
                                                           TerminalProviders::name(provider))
                                                   : reason);
        }
    });
    const auto showPanelMenu = [this](QWidget *widget, const QPoint &pos) {
        m_panelMenu->popup(widget->mapToGlobal(pos));
    };
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QWidget::customContextMenuRequested, this, [header, showPanelMenu](const QPoint &pos) {
        showPanelMenu(header, pos);
    });
    m_tabs->setObjectName(QStringLiteral("terminalSessions"));
    m_tabs->setTabsClosable(true);
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this, [this, showPanelMenu](const QPoint &pos) {
        showPanelMenu(m_tabs->tabBar(), pos);
    });
    m_layout->addWidget(m_tabs);
    installEventFilter(this);
    m_tabs->installEventFilter(this);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        scheduleBlurUpdate();
    });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *widget = m_tabs->widget(index);
        for (const auto &part : std::as_const(m_parts)) {
            if (part && part->widget() == widget) {
                if (QMessageBox::question(this,
                                          i18nc("@title:window", "Close Terminal Session"),
                                          i18nc("@info", "Close this terminal session? Running commands and unsaved input may be lost."),
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No)
                    == QMessageBox::Yes) {
                    delete part.data();
                }
                return;
            }
        }
    });
}

void TerminalPanel::setTitleVisible(bool visible)
{
    m_title->setVisible(visible);
}

bool TerminalPanel::hasSessions() const
{
    return m_tabs->count() > 0;
}

TerminalProviders::Provider TerminalPanel::embeddedProvider()
{
    return Settings::embeddedTerminal() == Settings::EnumEmbeddedTerminal::Kwi ? TerminalProviders::Provider::Kwi : TerminalProviders::Provider::Konsole;
}

void TerminalPanel::setEmbeddedProvider(TerminalProviders::Provider provider)
{
    Settings::setEmbeddedTerminal(provider == TerminalProviders::Provider::Kwi ? Settings::EnumEmbeddedTerminal::Kwi : Settings::EnumEmbeddedTerminal::Konsole);
    Settings::self()->save();
    const QString reason = TerminalProviders::partUnavailableReason(provider);
    setStatus(reason.isEmpty()
                  ? i18nc("@info %1 is a terminal application", "New terminal sessions use %1. Open sessions are unchanged.", TerminalProviders::name(provider))
                  : reason);
}

void TerminalPanel::setStatus(const QString &text, bool informational)
{
    m_status->setText(text);
    m_statusIsInformational = informational;
    m_statusRow->setVisible(!text.isEmpty() && !(informational && m_dismissedInformationalMessages.contains(text)));
}

void TerminalPanel::dismissStatus()
{
    if (m_statusIsInformational) {
        m_dismissedInformationalMessages.insert(m_status->text());
    }
    m_statusRow->hide();
}

void TerminalPanel::watchGeometry()
{
    for (QWidget *ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
        ancestor->installEventFilter(this);
    }
}

bool TerminalPanel::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ParentChange:
    case QEvent::Show:
        watchGeometry();
        [[fallthrough]];
    case QEvent::Hide:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::WindowStateChange:
        scheduleBlurUpdate();
        break;
    case QEvent::UpdateRequest:
        if (watched == window() && isVisible() && !m_blurRegion.isEmpty()) {
            // Styles can replace the window's blur request during a repaint.
            Q_EMIT blurRegionChanged(m_blurRegion);
        }
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void TerminalPanel::scheduleBlurUpdate()
{
    if (m_blurUpdatePending) {
        return;
    }
    m_blurUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        m_blurUpdatePending = false;
        updateBlur();
    });
}

void TerminalPanel::updateBlur()
{
    QRegion region;
    QWidget *widget = m_tabs->currentWidget();
    if (widget && widget->isVisible() && window()->testAttribute(Qt::WA_TranslucentBackground)) {
        for (const auto &part : std::as_const(m_parts)) {
            if (!part || part->widget() != widget) {
                continue;
            }
            bool enabled = false;
            if (!QMetaObject::invokeMethod(part.data(), "isBlurEnabled", Qt::DirectConnection, Q_RETURN_ARG(bool, enabled))) {
                if (!m_warnedBlurInterface) {
                    qWarning("The terminal component does not expose its profile blur setting");
                    m_warnedBlurInterface = true;
                }
            } else if (enabled) {
                const QWidget *top = window();
                region = QRegion(widget->rect()).translated(widget->mapTo(top, QPoint()));
                for (const QWidget *ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
                    region &= QRegion(ancestor->rect()).translated(ancestor->mapTo(top, QPoint()));
                    if (ancestor == top) {
                        break;
                    }
                }
            }
            break;
        }
    }
    if (region != m_blurRegion) {
        m_blurRegion = region;
        Q_EMIT blurRegionChanged(region);
    }
}

TerminalPanel::~TerminalPanel()
{
    for (const auto &part : std::as_const(m_parts)) {
        if (part) {
            part->disconnect(this);
            delete part.data();
        }
    }
}

void TerminalPanel::start(const QUrl &folder)
{
    if (Settings::terminalFollowActiveFolder()) {
        follow(folder);
        return;
    }
    if (!m_parts.isEmpty()) {
        setStatus(i18nc("@info", "The terminal starts in the active folder. Pane navigation does not change its directory."), true);
        return;
    }
    const QUrl initial = folder.isLocalFile() ? folder : QUrl::fromLocalFile(QDir::homePath());
    follow(initial);
    if (!folder.isLocalFile() && !m_parts.isEmpty()) {
        setStatus(i18nc("@info", "Remote folders cannot be used as a terminal directory; starting in the home folder."));
    }
}

void TerminalPanel::follow(const QUrl &folder)
{
    const QFileInfo info(folder.toLocalFile());
    if (!folder.isLocalFile() || !info.isAbsolute() || !info.isDir()) {
        setStatus(i18nc("@info", "The active folder is not an existing local folder. The terminal has not changed."));
        return;
    }
    const QString path = QDir::cleanPath(info.absoluteFilePath());
    for (const auto &part : std::as_const(m_parts)) {
        if (!part) {
            continue;
        }
        auto *terminal = qobject_cast<TerminalInterface *>(part.data());
        const QString workingDirectory = terminal ? terminal->currentWorkingDirectory() : QString();
        const QString sessionPath = QFileInfo(workingDirectory).canonicalFilePath();
        if (!workingDirectory.isEmpty() && !sessionPath.isEmpty() && sessionPath == info.canonicalFilePath()) {
            m_tabs->setCurrentWidget(part->widget());
            setStatus(i18nc("@info", "Terminal sessions remain open until their tabs are closed."), true);
            return;
        }
    }
    const TerminalProviders::Provider provider = embeddedProvider();
    const QString providerName = TerminalProviders::name(provider);
    const KPluginMetaData metadata = KPluginMetaData::findPluginById(QStringLiteral("kf6/parts"), TerminalProviders::partPluginId(provider));
    if (!metadata.isValid()) {
        setStatus(TerminalProviders::partUnavailableReason(provider));
        return;
    }
    const auto result = KParts::PartLoader::instantiatePart<KParts::ReadOnlyPart>(metadata, this, this);
    if (!result) {
        setStatus(i18nc("@info %1 is a terminal application, %2 the reason", "Could not load the %1 terminal component: %2", providerName, result.errorString));
        return;
    }
    auto *terminal = qobject_cast<TerminalInterface *>(result.plugin);
    if (!terminal || !result.plugin->widget()) {
        delete result.plugin;
        setStatus(i18nc("@info %1 is a terminal application", "The %1 component does not provide a terminal.", providerName));
        return;
    }
    auto *part = result.plugin;
    m_parts.append(part);
    const QList<QAction *> panelActions{m_panelMenu->menuAction()};
    if (!QMetaObject::invokeMethod(part, "setContextMenuAdditionalActions", Qt::DirectConnection, Q_ARG(QList<QAction *>, panelActions))
        && !m_warnedContextMenuInterface) {
        qWarning("The terminal component does not expose its context menu extension; use the terminal panel menu instead");
        m_warnedContextMenuInterface = true;
    }
    part->widget()->installEventFilter(this);
    const QPointer<QWidget> widget = part->widget();
    connect(part, &QObject::destroyed, this, [this, widget] {
        if (widget) {
            const int index = m_tabs->indexOf(widget);
            if (index >= 0) {
                m_tabs->removeTab(index);
            }
        }
        m_parts.removeAll(QPointer<KParts::ReadOnlyPart>());
        setStatus(i18nc("@info", "Terminal session ended. Select another tab or hide and show the terminal to start another."));
    });
    const int index = m_tabs->addTab(part->widget(), info.fileName().isEmpty() ? path : info.fileName());
    m_tabs->setTabToolTip(index, i18nc("@info:tooltip %1 is a terminal application, %2 a folder", "%1 session started in %2", providerName, path));
    m_tabs->setCurrentIndex(index);
    setStatus(Settings::terminalFollowActiveFolder()
                  ? i18nc("@info", "Terminal sessions remain open until their tabs are closed.")
                  : i18nc("@info", "The terminal starts in the active folder. Pane navigation does not change its directory."),
              true);
    terminal->showShellInDir(path);
}
