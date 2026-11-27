// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/// Decides when the main window hides to the system tray instead of closing,
/// minimizing or staying in front. A window is hidden only when a tray host
/// can show the icon that brings it back.
namespace TrayPolicy
{
struct State {
    bool trayIconEnabled = false;
    bool trayHostAvailable = false;
};

/// Whether the tray icon can restore a hidden window.
bool canHide(const State &state);

/// Whether closing the window hides it instead. Only a close requested by the
/// window manager hides; File › Quit, the tray's Quit and logging out close it.
bool hideOnClose(const State &state, bool closeToTray, bool spontaneous);

/// Whether minimizing the window hides it instead.
bool hideOnMinimize(const State &state, bool minimizeToTray);

/// Whether Plasma Opus starts with the window hidden in the tray. A start
/// that asks for particular folders or a workspace shows the window.
bool startHidden(const State &state, bool startHiddenInTray, bool explicitRequest);

enum class Toggle {
    Show,
    Hide,
    Minimize,
};

/// What showing or hiding the window from the tray or global shortcut does.
/// A visible window that is not in front is brought to the front first.
Toggle toggle(const State &state, bool visible, bool minimized, bool active);
}
