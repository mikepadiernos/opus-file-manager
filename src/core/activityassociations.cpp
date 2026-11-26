// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociations.h"

#include <KLocalizedString>

#include <QFileInfo>

#include <algorithm>

namespace
{
using WorkspaceCatalog::BehaviorMode;
using WorkspaceCatalog::BehaviorSelection;

bool validFolder(const QUrl &url)
{
    return url.isEmpty() || (url.isValid() && !url.scheme().isEmpty());
}

QString folderName(const QUrl &url)
{
    return url.toDisplayString(QUrl::PreferLocalFile | QUrl::RemovePassword);
}

void writeBehaviors(KConfigGroup &group, const BehaviorSelection &behaviors)
{
    group.writeEntry("BehaviorMode", int(behaviors.mode));
    group.writeEntry("LocationPresentation", behaviors.locationPresentation);
    group.writeEntry("SearchScope", behaviors.searchScope);
    group.writeEntry("Layout", behaviors.layout);
    group.writeEntry("PaneTabRestoration", behaviors.paneTabRestoration);
    group.writeEntry("Appearance", behaviors.appearance);
    group.writeEntry("YaziMode", behaviors.yaziMode.value_or(behaviors.layout));
}

BehaviorSelection readBehaviors(const KConfigGroup &group)
{
    const BehaviorSelection defaults;
    BehaviorSelection behaviors;
    const int mode = group.readEntry("BehaviorMode", int(defaults.mode));
    behaviors.mode = mode >= int(BehaviorMode::All) && mode <= int(BehaviorMode::None) ? static_cast<BehaviorMode>(mode) : defaults.mode;
    behaviors.locationPresentation = group.readEntry("LocationPresentation", defaults.locationPresentation);
    behaviors.searchScope = group.readEntry("SearchScope", defaults.searchScope);
    behaviors.layout = group.readEntry("Layout", defaults.layout);
    behaviors.paneTabRestoration = group.readEntry("PaneTabRestoration", defaults.paneTabRestoration);
    behaviors.appearance = group.readEntry("Appearance", defaults.appearance);
    behaviors.yaziMode = group.readEntry("YaziMode", behaviors.layout);
    return behaviors;
}
}

bool ActivityAssociations::Association::isEmpty() const
{
    return workspaceId.isEmpty() && primaryFolder.isEmpty() && secondaryFolder.isEmpty();
}

bool ActivityAssociations::Plan::isEmpty() const
{
    return workspaceId.isEmpty() && primaryFolder.isEmpty() && secondaryFolder.isEmpty();
}

QList<ActivityAssociations::Association> ActivityAssociations::load(const KConfigGroup &group)
{
    QList<Association> associations;
    QStringList ids = group.groupList();
    std::sort(ids.begin(), ids.end());
    for (const QString &id : std::as_const(ids)) {
        const KConfigGroup entry = group.group(id);
        Association association;
        association.activityId = id;
        association.activityName = entry.readEntry("Name", QString());
        association.workspaceId = entry.readEntry("Workspace", QString());
        association.inheritBehaviors = entry.readEntry("InheritBehaviors", true);
        association.behaviors = readBehaviors(entry);
        association.primaryFolder = QUrl(entry.readEntry("PrimaryFolder", QString()), QUrl::StrictMode);
        association.secondaryFolder = QUrl(entry.readEntry("SecondaryFolder", QString()), QUrl::StrictMode);
        association.removed = entry.readEntry("Removed", false);
        if (!validFolder(association.primaryFolder)) {
            association.primaryFolder.clear();
        }
        if (!validFolder(association.secondaryFolder)) {
            association.secondaryFolder.clear();
        }
        if (!association.isEmpty()) {
            associations.append(association);
        }
    }
    return associations;
}

void ActivityAssociations::save(KConfigGroup group, const QList<Association> &associations)
{
    group.deleteGroup();
    for (const Association &association : associations) {
        if (association.activityId.isEmpty() || association.isEmpty()) {
            continue;
        }
        KConfigGroup entry = group.group(association.activityId);
        entry.writeEntry("Name", association.activityName);
        entry.writeEntry("Workspace", association.workspaceId);
        entry.writeEntry("InheritBehaviors", association.inheritBehaviors);
        writeBehaviors(entry, association.behaviors);
        entry.writeEntry("PrimaryFolder", association.primaryFolder.toString(QUrl::FullyEncoded));
        entry.writeEntry("SecondaryFolder", association.secondaryFolder.toString(QUrl::FullyEncoded));
        entry.writeEntry("Removed", association.removed);
    }
}

const ActivityAssociations::Association *ActivityAssociations::find(const QList<Association> &associations, const QString &activityId)
{
    const auto it = std::find_if(associations.cbegin(), associations.cend(), [&activityId](const Association &association) {
        return association.activityId == activityId;
    });
    return it == associations.cend() ? nullptr : &*it;
}

bool ActivityAssociations::set(QList<Association> &associations, const Association &association, QString &error)
{
    if (association.activityId.isEmpty() || association.activityId.contains(QLatin1Char('/'))) {
        error = i18nc("@info", "The Activity is not valid.");
        return false;
    }
    if (!validFolder(association.primaryFolder) || !validFolder(association.secondaryFolder)) {
        error = i18nc("@info", "Choose a valid folder or leave it empty.");
        return false;
    }
    const auto it = std::find_if(associations.begin(), associations.end(), [&association](const Association &entry) {
        return entry.activityId == association.activityId;
    });
    if (association.isEmpty()) {
        if (it != associations.end()) {
            associations.erase(it);
        }
    } else if (it == associations.end()) {
        associations.append(association);
    } else {
        *it = association;
    }
    return true;
}

bool ActivityAssociations::remove(QList<Association> &associations, const QString &activityId)
{
    return associations.removeIf([&activityId](const Association &association) {
        return association.activityId == activityId;
    }) > 0;
}

bool ActivityAssociations::reconcile(QList<Association> &associations, const QMap<QString, QString> &activities)
{
    bool changed = false;
    for (Association &association : associations) {
        const auto activity = activities.constFind(association.activityId);
        const bool removed = activity == activities.cend();
        if (association.removed != removed) {
            association.removed = removed;
            changed = true;
        }
        if (!removed && !activity->isEmpty() && association.activityName != *activity) {
            association.activityName = *activity;
            changed = true;
        }
    }
    return changed;
}

bool ActivityAssociations::folderAvailable(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return url.isValid();
    }
    return QFileInfo(url.toLocalFile()).isDir();
}

ActivityAssociations::Plan ActivityAssociations::plan(const Association &association,
                                                      const WorkspaceCatalog::Catalog &catalog,
                                                      const QString &currentWorkspaceId,
                                                      const std::function<bool(const QUrl &)> &available)
{
    Plan plan;
    plan.activityId = association.activityId;
    if (!association.workspaceId.isEmpty()) {
        const auto workspace = std::find_if(catalog.workspaces.cbegin(), catalog.workspaces.cend(), [&association](const auto &entry) {
            return entry.id == association.workspaceId;
        });
        if (workspace == catalog.workspaces.cend()) {
            plan.problems.append(i18nc("@info", "The associated workspace no longer exists."));
        } else if (workspace->id != currentWorkspaceId) {
            plan.workspaceId = workspace->id;
            plan.workspaceName = workspace->name;
            if (!association.inheritBehaviors) {
                plan.behaviors = association.behaviors;
            }
        }
    }
    const auto addFolder = [&plan, &available](const QUrl &folder, QUrl &target) {
        if (folder.isEmpty()) {
            return;
        }
        if (available(folder)) {
            target = folder;
        } else {
            plan.problems.append(i18nc("@info %1 is a folder path or URL", "The folder %1 is unavailable.", folderName(folder)));
        }
    };
    addFolder(association.primaryFolder, plan.primaryFolder);
    addFolder(association.secondaryFolder, plan.secondaryFolder);
    return plan;
}
