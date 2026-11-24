// SPDX-License-Identifier: GPL-3.0-or-later

#include "windoweffects.h"

#include <KLocalizedString>
#include <KWindowEffects>

#include <QWidget>

bool WindowEffects::isTranslucent(const QWidget *window)
{
    return window && window->window()->testAttribute(Qt::WA_TranslucentBackground);
}

bool WindowEffects::isBlurAvailable(const QWidget *window)
{
    return isTranslucent(window) && KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
}

QString WindowEffects::unavailableReason(const QWidget *window, bool blur, bool translucency)
{
    if (!blur && !translucency) {
        return {};
    }
    if (!isTranslucent(window)) {
        return i18nc("@info",
                     "Translucent backgrounds need desktop compositing, which was not active when this window opened. The background stays "
                     "opaque; enable compositing and open a new Plasma Opus window to apply these effects.");
    }
    if (blur && !isBlurAvailable(window)) {
        return i18nc("@info", "KWin's blur effect is not available, so the background is not blurred. Enable Blur in Desktop Effects to use it.");
    }
    return {};
}
