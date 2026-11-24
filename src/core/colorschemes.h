// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>

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
 * Applies a scheme to this application's palette and to the colours that
 * KDE widgets read through KColorScheme.
 * @param id a ColorScheme setting value; empty follows the Plasma scheme
 * @returns false when the scheme is not available, in which case the Plasma
 *          scheme is followed and the setting is left unchanged
 */
bool apply(const QString &id);
}
