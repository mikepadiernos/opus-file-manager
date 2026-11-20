// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagebackground.h"

#include "settings.h"

#include <QPainter>
#include <QPixmap>

QBrush imageBackgroundBrush()
{
    switch (Settings::imageBackground()) {
    case Settings::EnumImageBackground::Light:
        return QBrush(QColor(250, 250, 250));
    case Settings::EnumImageBackground::Dark:
        return QBrush(QColor(35, 35, 35));
    case Settings::EnumImageBackground::Checkerboard: {
        QPixmap tile(16, 16);
        tile.fill(QColor(220, 220, 220));
        QPainter painter(&tile);
        painter.fillRect(0, 0, 8, 8, QColor(170, 170, 170));
        painter.fillRect(8, 8, 8, 8, QColor(170, 170, 170));
        return QBrush(tile);
    }
    default:
        return QBrush(Qt::NoBrush);
    }
}
