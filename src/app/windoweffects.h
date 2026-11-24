// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QWidget;

/// Availability of window translucency and KWin's background blur.
namespace WindowEffects
{
/**
 * Whether @p window has a translucent background. Translucency is chosen when
 * the window is created, so it needs compositing at that time.
 */
bool isTranslucent(const QWidget *window);

/// Whether KWin can blur the desktop behind @p window.
bool isBlurAvailable(const QWidget *window);

/**
 * Explains why requested effects cannot be shown in @p window.
 * @param blur whether background blur is requested
 * @param translucency whether a background opacity below 100% is requested
 * @return a user-visible reason, or an empty string when the effects are available
 */
QString unavailableReason(const QWidget *window, bool blur, bool translucency);
}
