// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "session.h"
#include "settings.h"

#include <QUrl>

/**
 * Startup settings that decide what a new window shows.
 */
struct StartupOptions {
    Settings::EnumStartupMode::type mode = Settings::EnumStartupMode::RestoreSession;
    Settings::EnumDualPaneAtStartup::type dualPane = Settings::EnumDualPaneAtStartup::AsLastUsed;
    QUrl home;
    QUrl leftFolder;
    QUrl rightFolder;
};

namespace Startup
{
/**
 * Builds the session a new window opens with.
 * @param saved the session stored at the last close; may be empty
 * @param options the startup settings
 * @returns a sanitized session: every pane has at least one tab, and the
 *          stacked flag is always carried over from @p saved
 */
WindowSession resolve(const WindowSession &saved, const StartupOptions &options);

/**
 * Reads the startup options from the Settings singleton.
 * @returns the current startup options
 */
StartupOptions fromSettings();
}
