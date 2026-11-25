// SPDX-License-Identifier: GPL-3.0-or-later

#include "effectiveappearance.h"

namespace
{
QVariantMap &storedLayer()
{
    static QVariantMap values;
    return values;
}
}

void EffectiveAppearance::setLayer(const QVariantMap &values)
{
    storedLayer() = values;
}

const QVariantMap &EffectiveAppearance::layer()
{
    return storedLayer();
}
