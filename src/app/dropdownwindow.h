// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class KWinIntegration;
class QWidget;

/**
 * Shows and hides the main window as a drop-down window, like Yakuake.
 *
 * The KWin script places the window at the chosen screen edge, keeps it
 * above other windows and out of the taskbar and pager whenever it is mapped
 * while the drop-down window is enabled. Without the script, the window is
 * shown and hidden as a regular window.
 */
class DropDownWindow : public QObject
{
    Q_OBJECT

public:
    DropDownWindow(QWidget *window, KWinIntegration *kwin);
    ~DropDownWindow() override;

    /// Shows the window, or hides it when it is shown and active.
    void toggle();
    void show();
    void hide();
    bool isHidden() const;
    /// Whether a dialog, menu or another Plasma Opus window needs the window to stay.
    static bool attentionNeeded(const QWidget *window);

    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    /// A short message for the status bar.
    void notice(const QString &message);
    /// The drop-down window was shown and should switch to this workspace.
    void workspaceRequested(const QString &workspaceId);

private:
    void hideAfterFocusLoss();
    void restoreQuitOnLastWindowClosed();

    QPointer<QWidget> m_window;
    KWinIntegration *m_kwin;
    QTimer m_focusLossTimer;
    bool m_hidden = false;
    bool m_fallbackNoticeShown = false;
};
