// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QPalette>
#include <QString>
#include <QStringList>

#include <optional>

/**
 * App-local colour schemes. Plasma Opus follows the active Plasma colour
 * scheme unless the ColorScheme setting chooses a bundled preset or an
 * installed KDE colour scheme. Only this application's palette changes; the
 * desktop's colour scheme, widget style and window decoration theme are left
 * alone.
 */
namespace ColorSchemes
{
/// Setting value of the bundled light preset.
inline constexpr auto LightPreset = "preset:light";
/// Setting value of the bundled dark preset.
inline constexpr auto DarkPreset = "preset:dark";

struct Scheme {
    /// Setting value: a preset id or an installed scheme's file base name.
    QString id;
    /// Display name.
    QString name;
    /// Whether this is a bundled preset rather than an installed scheme.
    bool preset = false;
};

/**
 * Lists the bundled presets, then the installed KDE colour schemes sorted
 * by name. A user scheme hides a system scheme with the same file name.
 */
QList<Scheme> available();

/**
 * Finds a scheme's colour file. Bundled presets are written to the
 * application data folder first, so other KDE components can read them.
 * @param id a ColorScheme setting value
 * @returns the file path, or an empty string when following Plasma or when
 *          the scheme is not available
 */
QString path(const QString &id);

/**
 * Whether the platform asks applications for high contrast. Always false
 * with Qt versions before 6.10, which do not report the preference.
 */
bool highContrastPreferred();

/**
 * Makes highContrastPreferred() return @p preferred instead of the platform
 * preference, for tests. std::nullopt follows the platform again.
 */
void setHighContrastPreferredForTesting(std::optional<bool> preferred);

/// A palette role that the colour editor can override.
struct Role {
    QPalette::ColorRole role;
    /// Key stored in the CustomColors setting.
    QString key;
    /// Display name.
    QString name;
};

/// The editable palette roles, in display order.
QList<Role> roles();

using Overrides = QHash<QPalette::ColorRole, QColor>;

/**
 * Reads CustomColors setting entries of the form "Role=#rrggbb". Unknown
 * roles and invalid colours are skipped.
 */
Overrides parseOverrides(const QStringList &entries);

/// Writes overrides as CustomColors setting entries, in role order.
QStringList serializeOverrides(const Overrides &overrides);

/**
 * The palette of a scheme without applying it.
 * @param id a ColorScheme setting value; empty or unavailable schemes give
 *        the Plasma colour scheme's palette
 */
QPalette palette(const QString &id);

/// Sets each overridden role in the active and inactive colour groups.
QPalette withOverrides(QPalette palette, const Overrides &overrides);

/**
 * Overrides that apply an accent colour to selection, focus, progress and
 * tab indicators. Selection text keeps the palette's colour when it stays
 * readable on the accent, otherwise it becomes black or white.
 * @param palette the palette the accent is layered over
 * @param accent the accent colour; an invalid colour gives no overrides
 */
Overrides accentOverrides(const QPalette &palette, const QColor &accent);

/// Roles that accentOverrides() sets.
QList<QPalette::ColorRole> accentRoles();

/**
 * Combines the AccentColor and CustomColors settings into CustomColors
 * entries for apply(). Custom colours take precedence over the accent.
 * @param id the ColorScheme setting value the accent is layered over
 * @param accent an AccentColor setting value; empty follows the scheme
 * @param customColors CustomColors setting entries
 */
QStringList withAccent(const QString &id, const QString &accent, const QStringList &customColors);

/**
 * Overrides that blend a tint into the view background roles, Base and
 * AlternateBase. The strength is lowered as needed so that view text keeps a
 * 4.5:1 contrast, or its original contrast when that was already lower.
 * @param palette the palette the tint is layered over
 * @param tint the tint colour; an invalid colour gives no overrides
 * @param strengthPercent how much of the tint to mix in, from 0 to 100
 */
Overrides paneTint(const QPalette &palette, const QColor &tint, int strengthPercent);

/**
 * The strength that paneTint() uses: @p strengthPercent, lowered as needed
 * so that view text stays readable, or 0 when no tint is applied.
 */
int paneTintStrength(const QPalette &palette, const QColor &tint, int strengthPercent);

/**
 * Overrides that make background roles translucent, keeping their colour.
 * @param palette the palette whose colours are used
 * @param roles the background roles to change; text and selection roles stay opaque
 * @param opacityPercent the opacity from 0 to 100; 100 gives no overrides
 */
Overrides translucent(const QPalette &palette, const QList<QPalette::ColorRole> &roles, int opacityPercent);

/// WCAG 2 contrast ratio between two colours, from 1 to 21.
double contrastRatio(const QColor &first, const QColor &second);

/**
 * A colour as close as possible to @p color that has at least @p minimum
 * contrast against @p background, mixing in black or white as needed. Lines
 * and other non-text indicators need 3:1 under WCAG 2.
 */
QColor readable(const QColor &color, const QColor &background, double minimum = 3.0);

struct ContrastIssue {
    /// Describes the colour pair, such as "Text on view background".
    QString pair;
    double ratio = 1;
    double minimum = 1;
};

/**
 * Checks text, selection, link and focus indicator colour pairs against WCAG
 * 2 AA minimums: 4.5:1 for text and 3:1 for selection and focus indicators.
 * @param changed when not empty, only pairs that include one of these roles
 *        are checked, so unchanged scheme colours are not reported
 */
QList<ContrastIssue> contrastIssues(const QPalette &palette, const QList<QPalette::ColorRole> &changed = {});

/**
 * Applies a scheme to this application's palette and to the colours that
 * KDE widgets read through KColorScheme.
 * @param id a ColorScheme setting value; empty follows the Plasma scheme
 * @param customColors CustomColors setting entries layered over the scheme;
 *        KDE widgets that read colours through KColorScheme still use the
 *        scheme's colours
 * @param highContrast whether the platform asks for high contrast, in which
 *        case the Plasma scheme is followed without custom colours so that
 *        app-local colours do not override the accessibility preference
 * @returns false when the scheme is not available, in which case the Plasma
 *          scheme is followed and the setting is left unchanged
 */
bool apply(const QString &id, const QStringList &customColors = {}, bool highContrast = highContrastPreferred());
}
