// SPDX-License-Identifier: GPL-3.0-or-later

#include "labelcolor.h"

namespace
{
struct LabelInfo {
    LabelColor color;
    const char *key;
    QRgb rgb;
};

constexpr LabelInfo Labels[] = {
    {LabelColor::Red, "red", 0xda4453},
    {LabelColor::Orange, "orange", 0xf67400},
    {LabelColor::Yellow, "yellow", 0xfdbc4b},
    {LabelColor::Green, "green", 0x27ae60},
    {LabelColor::Blue, "blue", 0x3daee9},
    {LabelColor::Purple, "purple", 0x9b59b6},
    {LabelColor::Pink, "pink", 0xe93a9a},
    {LabelColor::Gray, "gray", 0x7f8c8d},
};
}

QList<LabelColor> LabelColors::all()
{
    QList<LabelColor> colors;
    for (const LabelInfo &label : Labels) {
        colors.append(label.color);
    }
    return colors;
}

QString LabelColors::key(LabelColor color)
{
    for (const LabelInfo &label : Labels) {
        if (label.color == color) {
            return QLatin1String(label.key);
        }
    }
    return {};
}

LabelColor LabelColors::fromKey(const QString &key)
{
    for (const LabelInfo &label : Labels) {
        if (key == QLatin1String(label.key)) {
            return label.color;
        }
    }
    return LabelColor::None;
}

QColor LabelColors::rgb(LabelColor color)
{
    for (const LabelInfo &label : Labels) {
        if (label.color == color) {
            return QColor(label.rgb);
        }
    }
    return {};
}
