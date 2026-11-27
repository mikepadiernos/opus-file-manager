// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "traypolicy.h"

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>

class KActionCollection;
class KStatusNotifierItem;
class QAction;
class QMenu;
class QWidget;

/// Shows Plasma Opus in the system tray and registers the global shortcut
/// that shows or hides the main window.
///
/// The window hides to the tray only when a tray host is running to bring it
/// back, and while it is hidden the application keeps running when its other
/// windows close.
class DesktopTray : public QObject
{
    Q_OBJECT

public:
    DesktopTray(QWidget *window, KActionCollection *actions);
    ~DesktopTray() override;

    /// Shows or removes the tray icon to match the settings.
    void applySettings();

    /// Replaces the check for a running tray host, for tests.
    void setHostProbe(std::function<bool()> probe);

    bool trayIconShown() const;
    TrayPolicy::State state() const;

    QAction *toggleAction() const;
    QMenu *contextMenu() const;

    /// Shows, unminimizes, raises and activates @p window.
    static void bringToFront(QWidget *window);

    /// Shows the window when it is hidden, minimized or behind other windows,
    /// and otherwise hides it to the tray or minimizes it.
    void toggleWindow();

    /// Hides the window to the tray when a tray host can restore it.
    bool hideToTray();

    /// Starts with the window hidden and the tray icon shown, whatever the
    /// settings, so background synchronization keeps running after login.
    void startInBackground();
    /// Asks for attention in the tray with @p message, or stops asking when
    /// it is empty.
    void setAttention(const QString &message);

    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    void openRequested(const QUrl &url);
    void transfersRequested();
    void cloudSyncRequested();

private:
    void createItem();
    void applyToggle(TrayPolicy::Toggle toggle);
    void updateStatus();
    void updateMenu();

    QPointer<QWidget> m_window;
    QAction *m_toggleAction;
    QPointer<KStatusNotifierItem> m_item;
    QPointer<QAction> m_showHideAction;
    std::function<bool()> m_hostProbe;
    bool m_hiddenToTray = false;
    /// Whether the tray icon is shown for background synchronization.
    bool m_forced = false;
    QString m_attention;
};
