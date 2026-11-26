// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacerestore.h"

#include "settings.h"

#include <KLocalizedString>

using WorkspaceCatalog::Layout;
using WorkspaceCatalog::Presentation;

WorkspaceRestore::Options WorkspaceRestore::fromSettings()
{
    Options options;
    options.paneArrangement = Settings::workspaceRestorePaneArrangement();
    options.views = Settings::workspaceRestoreViews();
    options.toolbars = Settings::workspaceRestoreToolbars();
    options.panels = Settings::workspaceRestorePanels();
    options.tabs = Settings::workspaceRestoreTabs();
    options.activeTabs = Settings::workspaceRestoreActiveTabs();
    options.filters = Settings::workspaceRestoreFilters();
    options.yaziMode = Settings::yaziWorkspaceRestoreMode();
    options.yaziColumns = Settings::yaziWorkspaceRestoreColumns();
    return options;
}

WorkspaceRestore::YaziParts WorkspaceRestore::yaziParts(const WorkspaceCatalog::BehaviorSelection &selection, const Options &options)
{
    const bool behavior = selection.appliesYaziMode(options.views);
    return {behavior && options.yaziMode, behavior && options.yaziColumns};
}

WorkspaceCatalog::BehaviorSelection WorkspaceRestore::defaultBehaviorSelection()
{
    WorkspaceCatalog::BehaviorSelection selection;
    selection.mode = static_cast<WorkspaceCatalog::BehaviorMode>(qBound(0, int(Settings::workspaceDefaultBehaviorMode()), 2));
    selection.locationPresentation = Settings::workspaceDefaultLocationPresentation();
    selection.searchScope = Settings::workspaceDefaultSearchScope();
    selection.layout = Settings::workspaceDefaultLayout();
    selection.paneTabRestoration = Settings::workspaceDefaultPaneTabRestoration();
    selection.appearance = Settings::workspaceDefaultAppearance();
    selection.yaziMode = Settings::workspaceDefaultYaziMode();
    return selection;
}

namespace
{
void mergePresentation(Presentation &result, const Presentation &current, bool arrangement, bool panels, bool toolbars)
{
    if (!arrangement) {
        result.panePlacement = current.panePlacement;
        result.detachedPaneGeometry = current.detachedPaneGeometry;
    }
    if (!panels) {
        result.panelLayout = current.panelLayout;
        result.previewPlacement = current.previewPlacement;
        result.editorPlacement = current.editorPlacement;
        result.previewVisible = current.previewVisible;
        result.editorVisible = current.editorVisible;
        result.editorPresent = current.editorPresent;
        result.terminalVisible = current.terminalVisible;
        result.terminalPlacement = current.terminalPlacement;
        result.windowObjects = current.windowObjects;
    }
    if (!toolbars) {
        result.toolbarActions = current.toolbarActions;
        result.paneToolbars = current.paneToolbars;
    }
}

struct PaneParts {
    bool tabs;
    bool activeTabs;
    bool filters;
    bool locationPresentation;
    bool views;
    bool yaziMode;
    bool yaziColumns;
};

PaneSession mergePane(const PaneSession &saved, const PaneSession &current, const PaneParts &parts)
{
    PaneSession result = current;
    if (parts.tabs) {
        result.tabs = saved.tabs;
        result.pinnedCount = saved.pinnedCount;
        result.lockedTabs = saved.lockedTabs;
        result.names = saved.names;
        if (parts.activeTabs) {
            result.currentTab = saved.currentTab;
        } else {
            // Keep the current folder current when the saved tabs include it.
            const QUrl currentUrl = current.tabs.value(current.currentTab);
            result.currentTab = qMax(0, saved.tabs.indexOf(currentUrl));
        }
    }
    if (parts.filters) {
        result.filterText = saved.filterText;
        result.filterScope = saved.filterScope;
    }
    if (parts.locationPresentation) {
        result.locationPresentation = saved.locationPresentation;
    }
    if (parts.views) {
        result.viewMode = saved.viewMode;
    }
    if (parts.yaziMode) {
        result.yaziMode = saved.yaziMode;
    }
    if (parts.yaziColumns) {
        result.yaziPreview = saved.yaziPreview;
        result.yaziPreviewContent = saved.yaziPreviewContent;
        result.yaziParentColumn = saved.yaziParentColumn;
        result.yaziColumnRatios = saved.yaziColumnRatios;
    }
    return result;
}
}

bool WorkspaceRestore::merge(const Layout &saved,
                             const Layout &current,
                             const WorkspaceCatalog::BehaviorSelection &selection,
                             const Options &options,
                             Layout &result,
                             QString &error)
{
    const bool layout = selection.applies(selection.layout);
    const bool panes = selection.applies(selection.paneTabRestoration);
    const bool arrangement = layout && options.paneArrangement;
    const bool panels = layout && options.panels;
    const bool toolbars = layout && options.toolbars;
    const YaziParts yazi = yaziParts(selection, options);
    const PaneParts parts{panes && options.tabs,
                          panes && options.activeTabs,
                          panes && options.filters,
                          selection.applies(selection.locationPresentation),
                          layout && options.views,
                          yazi.mode,
                          yazi.columns};

    result = saved;
    if (!arrangement) {
        result.paneSizes = current.paneSizes;
        result.dual = current.dual;
        result.stacked = current.stacked;
    }
    if (!panels) {
        result.windowState = current.windowState;
        result.workspaceSizes = current.workspaceSizes;
    }
    if (!arrangement && !panels && !toolbars) {
        result.presentation = current.presentation;
    } else if (!arrangement || !panels || !toolbars) {
        if (!saved.presentation || !current.presentation) {
            error = i18nc("@info", "This workspace has an older or incomplete layout. Save a new layout before switching to it.");
            return false;
        }
        mergePresentation(*result.presentation, *current.presentation, arrangement, panels, toolbars);
    }

    // A layout without a pane snapshot has no Yazi state to restore, so the Yazi options can't require one.
    const bool anySaved = parts.tabs || parts.activeTabs || parts.filters || parts.locationPresentation || parts.views || (yazi.any() && saved.session);
    const bool allYazi = (yazi.mode && yazi.columns) || (selection.appliesYaziMode(options.views) && !saved.session);
    const bool allSaved = parts.tabs && parts.activeTabs && parts.filters && parts.locationPresentation && parts.views && allYazi;
    if (!anySaved) {
        result.session = current.session;
    } else if (!allSaved) {
        if (!saved.session) {
            error = i18nc("@info", "The selected workspace has no pane snapshot for its selected behaviors.");
            return false;
        }
        if (!current.session) {
            error = i18nc("@info", "The current window has no pane snapshot to preserve.");
            return false;
        }
        WindowSession session = *current.session;
        session.left = mergePane(saved.session->left, current.session->left, parts);
        session.right = mergePane(saved.session->right, current.session->right, parts);
        if (parts.activeTabs) {
            session.activePane = saved.session->activePane;
        }
        result.session = session;
    }
    if (result.session && anySaved && (!arrangement || !allSaved)) {
        result.session->dual = result.dual;
        result.session->stacked = result.stacked;
    }
    error.clear();
    return true;
}
