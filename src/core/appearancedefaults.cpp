// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancedefaults.h"

#include "colorschemes.h"
#include "settings.h"

#include <KCoreConfigSkeleton>

namespace
{
bool s_workspaceOverridesDisabled = false;
}

QStringList AppearanceDefaults::keptEntries()
{
    return {QStringLiteral("FolderColorRules")};
}

bool AppearanceDefaults::effectsSuspended()
{
    return Settings::suspendEffectsInHighContrast() && ColorSchemes::highContrastPreferred();
}

QStringList AppearanceDefaults::reset(KCoreConfigSkeleton *settings)
{
    QStringList changed;
    const QStringList kept = keptEntries();
    const KConfigSkeletonItem::List items = settings->items();
    for (KConfigSkeletonItem *item : items) {
        if (item->group() != QLatin1String(Group) || kept.contains(item->name()) || item->isDefault()) {
            continue;
        }
        item->setDefault();
        changed.append(item->name());
    }
    return changed;
}

bool AppearanceDefaults::workspaceOverridesDisabled()
{
    return s_workspaceOverridesDisabled;
}

void AppearanceDefaults::setWorkspaceOverridesDisabled(bool disabled)
{
    s_workspaceOverridesDisabled = disabled;
}
