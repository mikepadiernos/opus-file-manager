// SPDX-License-Identifier: GPL-3.0-or-later

#include "imageediting.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr qint64 PixelLimit = 40 * 1000 * 1000;
}

QImage ImageEditing::crop(const QImage &image, const QRect &rectangle)
{
    const QRect bounds = rectangle.normalized().intersected(image.rect());
    if (image.isNull() || bounds.isEmpty()) {
        return {};
    }
    return image.copy(bounds);
}

QImage ImageEditing::resize(const QImage &image, const QSize &size)
{
    if (image.isNull() || !size.isValid() || qint64(size.width()) * size.height() > PixelLimit) {
        return {};
    }
    return image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage ImageEditing::adjust(const QImage &image, const Adjustments &adjustments)
{
    if (image.isNull() || adjustments.brightness < 50 || adjustments.brightness > 150 || adjustments.contrast < 50 || adjustments.contrast > 150
        || adjustments.saturation < 0 || adjustments.saturation > 200 || adjustments.grayscale < 0 || adjustments.grayscale > 100) {
        return {};
    }
    if (adjustments.brightness == 100 && adjustments.contrast == 100 && adjustments.saturation == 100 && adjustments.grayscale == 0) {
        return image;
    }
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    if (result.isNull()) {
        return {};
    }
    const double brightness = adjustments.brightness / 100.0;
    const double contrast = adjustments.contrast / 100.0;
    const double saturation = adjustments.saturation / 100.0 * (1.0 - adjustments.grayscale / 100.0);
    const auto channel = [brightness, contrast, saturation](double value, double gray) {
        const double saturated = gray + (value - gray) * saturation;
        return std::clamp(int(std::lround((saturated * brightness - 127.5) * contrast + 127.5)), 0, 255);
    };
    for (int y = 0; y < result.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = row[x];
            const double gray = 0.2126 * qRed(pixel) + 0.7152 * qGreen(pixel) + 0.0722 * qBlue(pixel);
            row[x] = qRgba(channel(qRed(pixel), gray), channel(qGreen(pixel), gray), channel(qBlue(pixel), gray), qAlpha(pixel));
        }
    }
    return result;
}
