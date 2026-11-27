// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "traypolicy.h"

#include <QKeySequence>
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

    /// Shows or removes the tray icon, and registers or removes the global
    /// shortcut, to match the settings.
    void applySettings();

    /// Replaces the check for a running tray host, for tests.
    void setHostProbe(std::function<bool()> probe);

    bool trayIconShown() const;
    TrayPolicy::State state() const;

    QAction *toggleAction() const;

    /// The key combination KDE assigns to the global shortcut by default.
    static QKeySequence defaultGlobalShortcut();
    /// Whether KDE's global shortcut service is running or can be started.
    static bool globalShortcutServiceAvailable();
    /// Whether the global shortcut is registered with KDE.
    bool globalShortcutRegistered() const;
    /// The global shortcut's key combination: the one KDE assigns while it is
    /// registered, otherwise the one it gets when it is turned on again.
    QKeySequence globalShortcut() const;
    /// Assigns @p sequence, or no key when it is empty, to the global
    /// shortcut, or keeps it for when the shortcut is turned on again.
    void setGlobalShortcut(const QKeySequence &sequence);
    QMenu *contextMenu() const;

    /// Shows, unminimizes, raises and activates @p window.
    static void bringToFront(QWidget *window);

    /// Shows the window when it is hidden, minimized or behind other windows,
    /// and otherwise hides it to the tray or minimizes it.
    void toggleWindow();

    /// Hides the window to the tray when a tray host can restore it.
    bool hideToTray();

    /// At start, hides the window to the tray when the settings ask for it,
    /// a tray host can restore it and @p explicitRequest is false, such as
    /// when no folder or workspace was asked for. Returns whether it did.
    bool startHidden(bool explicitRequest);

    /// Starts with the window hidden and the tray icon shown, whatever the
    /// settings, so background synchronization keeps running after login.
    void startInBackground();
    /// Asks for attention in the tray with @p message, or stops asking when
    /// it is empty.
    void setAttention(const QString &message);
    /// Sets what the tray's Quit action asks before quitting; quitting is
    /// canceled when @p check returns false.
    void setQuitCheck(std::function<bool()> check);

    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    void openRequested(const QUrl &url);
    void transfersRequested();
    void cloudSyncRequested();
    void cloudActivityRequested();

private:
    void createItem();
    void applyGlobalShortcut();
    void applyToggle(TrayPolicy::Toggle toggle);
    void updateStatus();
    void updateMenu();

    QPointer<QWidget> m_window;
    QAction *m_toggleAction;
    QPointer<KStatusNotifierItem> m_item;
    QPointer<QAction> m_showHideAction;
    QPointer<QAction> m_cloudActivityAction;
    std::function<bool()> m_hostProbe;
    std::function<bool()> m_quitCheck;
    bool m_hiddenToTray = false;
    /// Whether the tray icon is shown for background synchronization.
    bool m_forced = false;
    QString m_attention;
};
