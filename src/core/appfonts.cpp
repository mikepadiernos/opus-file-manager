// SPDX-License-Identifier: GPL-3.0-or-later

#include "appfonts.h"

#include <QFontInfo>

QFont AppFonts::fromSetting(const QString &value)
{
    const QString trimmed = value.trimmed();
    QFont font;
    if (trimmed.isEmpty() || !font.fromString(trimmed) || font.family().isEmpty()) {
        return QFont();
    }
    return font;
}

QString AppFonts::toSetting(const QFont &font)
{
    if (!isOverride(font)) {
        return {};
    }
    QFont stored = font;
    if (stored.pointSizeF() <= 0 && stored.pixelSize() > 0) {
        stored.setPointSizeF(QFontInfo(font).pointSizeF());
    }
    return stored.toString();
}

bool AppFonts::isOverride(const QFont &font)
{
    return font.resolveMask() != 0;
}
