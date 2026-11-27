// SPDX-License-Identifier: GPL-3.0-or-later

#include "traypolicy.h"

bool TrayPolicy::canHide(const State &state)
{
    return state.trayIconEnabled && state.trayHostAvailable;
}

bool TrayPolicy::hideOnClose(const State &state, bool closeToTray, bool spontaneous)
{
    return closeToTray && spontaneous && canHide(state);
}

bool TrayPolicy::hideOnMinimize(const State &state, bool minimizeToTray)
{
    return minimizeToTray && canHide(state);
}

TrayPolicy::Toggle TrayPolicy::toggle(const State &state, bool visible, bool minimized, bool active)
{
    if (!visible || minimized || !active) {
        return Toggle::Show;
    }
    return canHide(state) ? Toggle::Hide : Toggle::Minimize;
}
