// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QColor>

class QPalette;

/// Branding presentation: the logo's colours and the startup presentation's timing.
namespace Branding
{
/// Logo variants; the order matches the LogoVariant setting.
enum class LogoVariant {
    /// The colour scheme's window and text colours.
    Automatic,
    /// Dark text on a light card.
    DarkText,
    /// Light text on a dark card.
    LightText,
};

/// The colours of the startup presentation card and the logo drawn on it.
struct PresentationColors {
    QColor background;
    /// The wordmark "Plasma", the version and the description.
    QColor text;
    /// The folder and the wordmark "Opus".
    QColor accent;

    bool operator==(const PresentationColors &) const = default;
};

/**
 * The presentation colours for @p variant. The accent always follows the
 * palette. High contrast ignores the variant and uses the palette's own
 * colours, so the presentation keeps the contrast the user asked for.
 */
PresentationColors presentationColors(LogoVariant variant, const QPalette &palette, bool highContrast);

/**
 * Replaces the ColorScheme-Text and ColorScheme-Accent colours in a logo or
 * icon's colour-scheme stylesheet.
 * @returns @p svg unchanged when it has no colour-scheme stylesheet
 */
QByteArray recolor(const QByteArray &svg, const QColor &text, const QColor &accent);

/// How long the presentation stays fully visible, in milliseconds.
int holdDuration();

/**
 * How long the presentation fades out.
 * @param styleAnimationDuration the widget style's animation duration, which
 *        is 0 when Plasma or Plasma Opus turns animations off
 * @returns 0 to hide it without a transition
 */
int fadeDuration(int styleAnimationDuration);
}
