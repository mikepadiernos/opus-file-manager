// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacestartup.h"

#include <KLocalizedString>

#include <algorithm>

QString WorkspaceStartup::resolve(const WorkspaceCatalog::Catalog &catalog, const QString &setting, QString *warning)
{
    if (warning) {
        warning->clear();
    }
    if (setting.isEmpty()) {
        return catalog.activeId;
    }
    if (setting == QLatin1String(NoWorkspace)) {
        return {};
    }
    const bool available = std::any_of(catalog.workspaces.cbegin(), catalog.workspaces.cend(), [&setting](const auto &workspace) {
        return workspace.id == setting;
    });
    if (available) {
        return setting;
    }
    if (warning) {
        *warning = i18nc("@info", "The startup workspace is no longer available. The last used workspace was kept; choose another in the preferences.");
    }
    return catalog.activeId;
}

bool WorkspaceStartup::apply(KConfigGroup group, const QString &setting, QString *warning)
{
    WorkspaceCatalog::Catalog catalog;
    QString error;
    if (!WorkspaceCatalog::load(group, catalog, error)) {
        if (warning) {
            *warning = error;
        }
        return false;
    }
    QString resolveWarning;
    const QString id = resolve(catalog, setting, &resolveWarning);
    if (warning) {
        *warning = resolveWarning;
    }
    if (id == catalog.activeId) {
        return true;
    }
    WorkspaceCatalog::Catalog updated = catalog;
    // activate() rejects an empty ID, so clearing the active workspace is direct.
    updated.activeId = id;
    if (!WorkspaceCatalog::saveIfUnchanged(group, catalog, updated, error)) {
        if (warning) {
            *warning = error;
        }
        return false;
    }
    return true;
}
