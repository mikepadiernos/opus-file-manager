// SPDX-License-Identifier: GPL-3.0-or-later

#include "dropdownwindow.h"

#include "kwinintegration.h"

#include <KLocalizedString>
#include <KWindowEffects>
#include <KWindowSystem>
#include <QApplication>
#include <QEvent>
#include <QWidget>
#include <QWindow>

namespace
{
// Long enough for a dialog or another Plasma Opus window to become active.
constexpr int FocusLossDelay = 200;

KWindowEffects::SlideFromLocation slideLocation(const KWinScript::DropDown &dropDown)
{
    if (!dropDown.slide) {
        return KWindowEffects::NoEdge;
    }
    switch (dropDown.edge) {
    case KWinScript::Edge::Top:
        return KWindowEffects::TopEdge;
    case KWinScript::Edge::Bottom:
        return KWindowEffects::BottomEdge;
    case KWinScript::Edge::Left:
        return KWindowEffects::LeftEdge;
    case KWinScript::Edge::Right:
        return KWindowEffects::RightEdge;
    }
    return KWindowEffects::NoEdge;
}
}

DropDownWindow::DropDownWindow(QWidget *window, KWinIntegration *kwin)
    : QObject(window)
    , m_window(window)
    , m_kwin(kwin)
{
    m_focusLossTimer.setSingleShot(true);
    m_focusLossTimer.setInterval(FocusLossDelay);
    connect(&m_focusLossTimer, &QTimer::timeout, this, &DropDownWindow::hideAfterFocusLoss);
    window->installEventFilter(this);
}

DropDownWindow::~DropDownWindow()
{
    restoreQuitOnLastWindowClosed();
}

void DropDownWindow::toggle()
{
    if (!m_window) {
        return;
    }
    if (!m_kwin->dropDown().enabled) {
        Q_EMIT notice(i18nc("@info:status", "The drop-down window is turned off. Turn it on under Settings › Configure KWin Integration."));
        return;
    }
    if (m_window->isVisible() && !m_window->isMinimized() && m_window->isActiveWindow()) {
        hide();
    } else {
        show();
    }
}

void DropDownWindow::show()
{
    if (!m_window) {
        return;
    }
    const KWinScript::DropDown dropDown = m_kwin->dropDown();
    if (!m_kwin->scriptActive() && !m_fallbackNoticeShown) {
        m_fallbackNoticeShown = true;
        Q_EMIT notice(i18nc("@info:status", "The Plasma Opus KWin script is not running, so the drop-down window opens as a regular window."));
    }
    m_window->winId();
    if (QWindow *handle = m_window->windowHandle()) {
        KWindowEffects::slideWindow(handle, slideLocation(dropDown));
    }
    // The script styles and places the window when KWin maps it.
    m_kwin->requestSync();
    m_window->setWindowState(m_window->windowState() & ~Qt::WindowMinimized);
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
    if (QWindow *handle = m_window->windowHandle()) {
        KWindowSystem::activateWindow(handle);
    }
    if (!dropDown.workspaceId.isEmpty()) {
        Q_EMIT workspaceRequested(dropDown.workspaceId);
    }
}

void DropDownWindow::hide()
{
    if (!m_window) {
        return;
    }
    m_focusLossTimer.stop();
    // Dialogs closing while the window is hidden must not end the application.
    m_hidden = true;
    QApplication::setQuitOnLastWindowClosed(false);
    m_window->hide();
}

bool DropDownWindow::isHidden() const
{
    return m_hidden;
}

bool DropDownWindow::attentionNeeded(const QWidget *window)
{
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) {
        return true;
    }
    QWidget *active = QApplication::activeWindow();
    return active && active != window;
}

bool DropDownWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window) {
        return false;
    }
    switch (event->type()) {
    case QEvent::Show:
        restoreQuitOnLastWindowClosed();
        break;
    case QEvent::ActivationChange: {
        const KWinScript::DropDown dropDown = m_kwin->dropDown();
        if (dropDown.enabled && dropDown.hideOnFocusLoss && m_window->isVisible() && !m_window->isActiveWindow()) {
            m_focusLossTimer.start();
        } else {
            m_focusLossTimer.stop();
        }
        break;
    }
    default:
        break;
    }
    return false;
}

void DropDownWindow::hideAfterFocusLoss()
{
    if (!m_window || !m_window->isVisible() || m_window->isActiveWindow() || attentionNeeded(m_window)) {
        return;
    }
    const KWinScript::DropDown dropDown = m_kwin->dropDown();
    if (dropDown.enabled && dropDown.hideOnFocusLoss) {
        hide();
    }
}

void DropDownWindow::restoreQuitOnLastWindowClosed()
{
    if (m_hidden) {
        m_hidden = false;
        QApplication::setQuitOnLastWindowClosed(true);
    }
}
