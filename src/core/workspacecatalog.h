// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "session.h"
#include "vaultsettings.h"

#include <KConfigGroup>
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>

namespace WorkspaceCatalog
{
enum class PanePlacement {
    Split,
    Tab,
    Window
};
enum class PanelLayout {
    Combined,
    Separate
};
enum class PreviewPlacement {
    Pane,
    Below,
    Right
};
enum class EditorPlacement {
    Panel,
    Below,
    Right,
    Maximized,
    Window
};
enum class TerminalPlacement {
    Below,
    Right
};
enum class BehaviorMode {
    All,
    Selected,
    None
};

struct BehaviorSelection {
    BehaviorMode mode = BehaviorMode::All;
    bool locationPresentation = true;
    bool searchScope = true;
    bool layout = true;
    bool paneTabRestoration = true;
    bool appearance = true;
    /**
     * Whether switching restores each pane's Yazi mode, with its preview,
     * parent column and column ratios, independently of the views; the
     * restore options choose which of them (WorkspaceRestore::yaziParts()).
     * Empty in workspaces saved before the choice existed; they restore Yazi
     * mode with each pane's view mode, as part of the window and panel layout.
     */
    std::optional<bool> yaziMode = true;
    bool applies(bool selected) const
    {
        return mode == BehaviorMode::All || (mode == BehaviorMode::Selected && selected);
    }
    /// Whether switching restores Yazi mode; @p views is whether the restore options include each pane's view mode.
    bool appliesYaziMode(bool views) const
    {
        return yaziMode ? applies(*yaziMode) : applies(layout) && views;
    }
    bool operator==(const BehaviorSelection &other) const = default;
};

struct PaneToolbarState {
    QStringList actions;
    QStringList visible;
    QStringList hidden;
    bool operator==(const PaneToolbarState &other) const = default;
};

struct Presentation {
    PanePlacement panePlacement = PanePlacement::Split;
    PanelLayout panelLayout = PanelLayout::Combined;
    PreviewPlacement previewPlacement = PreviewPlacement::Pane;
    EditorPlacement editorPlacement = EditorPlacement::Panel;
    bool previewVisible = false;
    bool editorVisible = false;
    bool editorPresent = false;
    bool terminalVisible = false;
    QStringList windowObjects;
    TerminalPlacement terminalPlacement = TerminalPlacement::Below;
    QRect detachedPaneGeometry;
    std::optional<QStringList> toolbarActions;
    QMap<QString, PaneToolbarState> paneToolbars;
    bool operator==(const Presentation &other) const = default;
};

struct Layout {
    QByteArray windowState;
    QList<int> paneSizes;
    QList<int> workspaceSizes;
    bool dual = true;
    bool stacked = true;
    std::optional<Presentation> presentation;
    std::optional<WindowSession> session;
    bool operator==(const Layout &other) const = default;
};

/**
 * A theme profile and appearance options attached to a workspace. Override
 * values are kept in their JSON form, as WorkspaceAppearance encodes them, so
 * values that cannot be applied now are preserved for a later retry.
 */
struct Appearance {
    /// The attached theme profile's id, or empty for none.
    QString themeProfileId;
    /// Encoded setting values by entry name.
    QJsonObject overrides;
    bool isEmpty() const
    {
        return themeProfileId.isEmpty() && overrides.isEmpty();
    }
    bool operator==(const Appearance &other) const = default;
};

struct Workspace {
    QString id;
    QString name;
    QString shortLabel;
    std::optional<Layout> layout;
    QStringList savedSearchIds;
    QStringList folderSyncProfileIds;
    BehaviorSelection behaviorSelection;
    Appearance appearance;
    /** An assigned location used as the editor's Markdown vault, or empty. */
    QString vaultLocationId;
    /** Whether the editor uses the workspace's vault; turning it off keeps the location and options. */
    bool vaultEnabled = true;
    /** The workspace vault's own Markdown options, or empty to use the Editor settings. */
    std::optional<VaultSettings::Options> vaultOptions;
    bool operator==(const Workspace &other) const = default;
};

struct Catalog {
    QList<Workspace> workspaces;
    QString activeId;
    QMap<QString, QString> locationAssignments;
    bool operator==(const Catalog &other) const = default;
};

bool load(const KConfigGroup &group, Catalog &catalog, QString &error);
bool save(KConfigGroup group, const Catalog &catalog, QString &error);
/**
 * Saves updated only when the stored catalog still equals expected.
 * The named configuration must have no pending changes.
 * @returns false and sets error if validation, locking or saving fails
 */
bool saveIfUnchanged(KConfigGroup group, const Catalog &expected, const Catalog &updated, QString &error);
bool create(Catalog &catalog, const QString &name, const QString &shortLabel, QString &error);
bool duplicate(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error);
bool setLayout(Catalog &catalog, const QString &id, const Layout &layout, QString &error);
/**
 * Attaches @p appearance to a workspace, replacing what was attached.
 * @returns false and sets error if the workspace is missing or the values are invalid
 */
bool setAppearance(Catalog &catalog, const QString &id, const Appearance &appearance, QString &error);
bool rename(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error);
bool activate(Catalog &catalog, const QString &id, QString &error);
bool remove(Catalog &catalog, const QString &id, QString &error);
/**
 * Identifies a saved location without retaining URL credentials or query data.
 * @param url the saved location URL
 * @returns its UUID, or an empty string when the URL is invalid
 */
QString locationId(const QUrl &url);
/**
 * Assigns a location without replacing an existing different owner.
 * @param catalog the workspace catalog to update
 * @param locationId a stable, credential-free location UUID
 * @param workspaceId the workspace UUID
 * @param error describes an invalid ID, missing workspace or conflicting owner
 * @returns whether the assignment succeeded
 */
bool assignLocation(Catalog &catalog, const QString &locationId, const QString &workspaceId, QString &error);
/**
 * Removes an assignment without affecting the saved location.
 * @param catalog the workspace catalog to update
 * @param locationId the assigned location UUID
 * @param error describes an invalid or unassigned location
 * @returns whether the assignment was removed
 */
bool unassignLocation(Catalog &catalog, const QString &locationId, QString &error);
/**
 * Uses one of a workspace's assigned locations as its editor vault, and
 * turns the workspace vault on when @p locationId isn't empty.
 * @param catalog the workspace catalog to update
 * @param workspaceId the workspace UUID
 * @param locationId a location assigned to that workspace, or empty to clear the vault
 * @param error describes a missing workspace or a location it does not own
 * @returns whether the vault was set
 */
bool setVault(Catalog &catalog, const QString &workspaceId, const QString &locationId, QString &error);
/**
 * Turns a workspace's editor vault on or off and sets its own Markdown
 * options, keeping its location so the vault returns when turned on again.
 * @param options the workspace vault's options, or empty to use the Editor settings
 * @returns false and sets error if the workspace is missing
 */
bool setVaultOptions(Catalog &catalog, const QString &workspaceId, bool enabled, const std::optional<VaultSettings::Options> &options, QString &error);
}
