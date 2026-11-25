// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

class KCoreConfigSkeleton;

/**
 * Safeguards and resets for the appearance options in the Themes settings
 * group, so Plasma Opus can always be returned to Plasma's own appearance.
 */
namespace AppearanceDefaults
{
/// The settings group that holds the global appearance options.
inline constexpr auto Group = "Themes";

/**
 * Themes entries that a reset keeps, because they are user-made lists rather
 * than styling: folder colour rules.
 */
QStringList keptEntries();

/**
 * Whether blur, translucency, pane tints, custom corner radii, tinted tabs
 * and the custom stylesheet are suspended: the SuspendEffectsInHighContrast
 * setting is on and the platform asks for high contrast.
 */
bool effectsSuspended();

/**
 * Resets the appearance options in @p settings to their defaults, which
 * follow Plasma, except keptEntries(). Nothing is saved.
 * @returns the names of the entries that changed
 */
QStringList reset(KCoreConfigSkeleton *settings);

/**
 * Whether appearance options attached to workspaces are ignored in this
 * session, as requested with --no-workspace-appearance. Not saved.
 */
bool workspaceOverridesDisabled();
void setWorkspaceOverridesDisabled(bool disabled);
}
