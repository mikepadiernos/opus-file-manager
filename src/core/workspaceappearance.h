// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "themeprofiles.h"
#include "workspacecatalog.h"

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QVariantMap>

class KCoreConfigSkeleton;

/**
 * Appearance attached to workspaces: a theme profile and a few overridden
 * options, layered over the global settings while the workspace is active.
 *
 * Layers apply in this order, each replacing the options it holds:
 *  1. Plasma's own appearance, the defaults of the appearance options;
 *  2. the global settings;
 *  3. the workspace's theme profile, which holds every option it covers;
 *  4. the workspace's overrides;
 *  5. the custom stylesheet in effect, from the global settings or the
 *     profile, on top of all of them.
 * Options the profile and overrides do not hold keep their global values.
 */
namespace WorkspaceAppearance
{
/**
 * The options a workspace can override: the view mode, such as Card view,
 * folder colour rules and every option a theme profile holds except the
 * custom stylesheet, which stays in the last layer.
 */
QStringList overrideEntries(const KCoreConfigSkeleton *settings);

/// The global values of every overrideEntries() option, to save all of them with a workspace.
QVariantMap captureOverrides(const KCoreConfigSkeleton *settings);

/// Encodes override values for the catalog, skipping entries outside overrideEntries().
QJsonObject encodeOverrides(const KCoreConfigSkeleton *settings, const QVariantMap &values);

/**
 * Decodes the stored overrides that can be applied. Unknown entries and
 * invalid values are left out and described in @p problems.
 */
QVariantMap decodeOverrides(const KCoreConfigSkeleton *settings, const QJsonObject &overrides, QStringList *problems = nullptr);

struct Resolved {
    /// The values layered over the global settings, by entry name.
    QVariantMap values;
    /**
     * Attached profiles, colour schemes, fonts and values that cannot be
     * applied now. Their references stay saved, so they apply again once
     * available.
     */
    QStringList problems;
};

/**
 * Resolves the appearance attached to @p workspace against the saved
 * @p profiles. Nothing is layered when @p workspace is null, does not apply
 * its appearance as a selected behaviour, or workspace appearance is
 * disabled for this session.
 */
Resolved resolve(const KCoreConfigSkeleton *settings, const WorkspaceCatalog::Workspace *workspace, const QList<ThemeProfiles::Profile> &profiles);

/// Resolves @p appearance whether or not a workspace would apply it now.
Resolved resolve(const KCoreConfigSkeleton *settings, const WorkspaceCatalog::Appearance &appearance, const QList<ThemeProfiles::Profile> &profiles);
}
