// SPDX-License-Identifier: GPL-3.0-or-later

#include "startup.h"

namespace
{
QUrl orFallback(const QUrl &url, const QUrl &fallback)
{
    return url.isValid() && !url.isEmpty() ? url : fallback;
}

bool dualFor(Settings::EnumDualPaneAtStartup::type policy, bool lastUsed)
{
    switch (policy) {
    case Settings::EnumDualPaneAtStartup::Always:
        return true;
    case Settings::EnumDualPaneAtStartup::Never:
        return false;
    default:
        return lastUsed;
    }
}
}

WindowSession Startup::resolve(const WindowSession &saved, const StartupOptions &options)
{
    WindowSession session;
    switch (options.mode) {
    case Settings::EnumStartupMode::HomeFolder:
        session.left.tabs = {options.home};
        session.right.tabs = {options.home};
        break;
    case Settings::EnumStartupMode::FixedFolders:
        session.left.tabs = {orFallback(options.leftFolder, options.home)};
        session.right.tabs = {orFallback(options.rightFolder, options.home)};
        break;
    default:
        session = saved;
        break;
    }
    if (options.mode != Settings::EnumStartupMode::RestoreSession) {
        const int viewMode = static_cast<int>(Settings::defaultViewMode());
        session.left.viewMode = viewMode;
        session.right.viewMode = viewMode;
    }
    session.dual = dualFor(options.dualPane, saved.dual);
    session.stacked = saved.stacked;
    return session.sanitized(options.home);
}

StartupOptions Startup::fromSettings()
{
    return {Settings::startupMode(), Settings::dualPaneAtStartup(), Settings::homeUrl(), Settings::leftStartUrl(), Settings::rightStartUrl()};
}
