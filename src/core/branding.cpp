// SPDX-License-Identifier: GPL-3.0-or-later

#include "branding.h"

#include <QPalette>
#include <QRegularExpression>

#include <algorithm>

Branding::PresentationColors Branding::presentationColors(LogoVariant variant, const QPalette &palette, bool highContrast)
{
    const QColor accent = palette.color(QPalette::Active, QPalette::Accent);
    if (highContrast || variant == LogoVariant::Automatic) {
        return {palette.color(QPalette::Active, QPalette::Window), palette.color(QPalette::Active, QPalette::WindowText), accent};
    }
    // Breeze's light and dark window and text colours, which the logo files use.
    const QColor light(0xfc, 0xfc, 0xfc);
    const QColor dark(0x23, 0x26, 0x29);
    if (variant == LogoVariant::DarkText) {
        return {light, dark, accent};
    }
    return {dark, light, accent};
}

QByteArray Branding::recolor(const QByteArray &svg, const QColor &text, const QColor &accent)
{
    QString source = QString::fromUtf8(svg);
    const auto replace = [&source](const QString &className, const QColor &color) {
        static const QString pattern = QStringLiteral(R"((\.ColorScheme-%1\s*\{\s*color\s*:\s*)[^;}]+)");
        source.replace(QRegularExpression(pattern.arg(className)), QStringLiteral("\\1") + color.name(QColor::HexRgb));
    };
    replace(QStringLiteral("Text"), text);
    replace(QStringLiteral("Accent"), accent);
    return source.toUtf8();
}

int Branding::holdDuration()
{
    return 2000;
}

int Branding::fadeDuration(int styleAnimationDuration)
{
    if (styleAnimationDuration <= 0) {
        return 0;
    }
    // Styles report short durations for small transitions; a full-window card
    // fades a little more slowly, but never long enough to be in the way.
    return std::clamp(styleAnimationDuration * 2, 150, 500);
}
