// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QRect>
#include <QSize>

namespace ImageEditing
{
struct Adjustments {
    int brightness = 100;
    int contrast = 100;
    int saturation = 100;
    int grayscale = 0;
};

QImage crop(const QImage &image, const QRect &rectangle);
QImage resize(const QImage &image, const QSize &size);
QImage adjust(const QImage &image, const Adjustments &adjustments);
}
