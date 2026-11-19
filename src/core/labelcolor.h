// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QList>
#include <QString>

/// The eight label colours used for tabs and, later, file labels.
enum class LabelColor {
    None,
    Red,
    Orange,
    Yellow,
    Green,
    Blue,
    Purple,
    Pink,
    Gray,
};

namespace LabelColors
{
/// Every colour except None, in menu order.
QList<LabelColor> all();

/// The stored name of @p color, such as "red"; empty for None.
QString key(LabelColor color);

/// The colour stored as @p key, or None for an unknown key.
LabelColor fromKey(const QString &key);

/// The RGB value drawn for @p color; invalid for None.
QColor rgb(LabelColor color);
}
