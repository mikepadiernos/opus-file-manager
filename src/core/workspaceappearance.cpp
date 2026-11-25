// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspaceappearance.h"

#include "appearancedefaults.h"

#include <KCoreConfigSkeleton>
#include <KLocalizedString>

#include <algorithm>

QStringList WorkspaceAppearance::overrideEntries()
{
    return {QStringLiteral("DefaultViewMode"),
            QStringLiteral("FolderColorRules"),
            QStringLiteral("BackgroundBlur"),
            QStringLiteral("CornerRadius"),
            QStringLiteral("CustomCornerRadius"),
            QStringLiteral("Dividers"),
            QStringLiteral("RevealDividersOnHover")};
}

QJsonObject WorkspaceAppearance::encodeOverrides(const KCoreConfigSkeleton *settings, const QVariantMap &values)
{
    QJsonObject encoded;
    const QStringList entries = overrideEntries();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const KConfigSkeletonItem *item = entries.contains(it.key()) ? settings->findItem(it.key()) : nullptr;
        const QJsonValue value = item ? ThemeProfiles::encodeValue(item, it.value()) : QJsonValue();
        if (!value.isNull() && !value.isUndefined()) {
            encoded.insert(it.key(), value);
        }
    }
    return encoded;
}

QVariantMap WorkspaceAppearance::decodeOverrides(const KCoreConfigSkeleton *settings, const QJsonObject &overrides, QStringList *problems)
{
    QVariantMap values;
    const QStringList entries = overrideEntries();
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        const KConfigSkeletonItem *item = entries.contains(it.key()) ? settings->findItem(it.key()) : nullptr;
        const std::optional<QVariant> value = item ? ThemeProfiles::decodeValue(item, it.value()) : std::nullopt;
        if (value) {
            values.insert(it.key(), *value);
        } else if (problems) {
            problems->append(i18nc("@item %1 is a setting name such as BackgroundBlur", "Option “%1”", it.key()));
        }
    }
    return values;
}

WorkspaceAppearance::Resolved
WorkspaceAppearance::resolve(const KCoreConfigSkeleton *settings, const WorkspaceCatalog::Workspace *workspace, const QList<ThemeProfiles::Profile> &profiles)
{
    if (!workspace || AppearanceDefaults::workspaceOverridesDisabled() || !workspace->behaviorSelection.applies(workspace->behaviorSelection.appearance)) {
        return {};
    }
    return resolve(settings, workspace->appearance, profiles);
}

WorkspaceAppearance::Resolved
WorkspaceAppearance::resolve(const KCoreConfigSkeleton *settings, const WorkspaceCatalog::Appearance &appearance, const QList<ThemeProfiles::Profile> &profiles)
{
    Resolved resolved;
    const QString &profileId = appearance.themeProfileId;
    if (!profileId.isEmpty()) {
        const auto profile = std::find_if(profiles.cbegin(), profiles.cend(), [&profileId](const ThemeProfiles::Profile &candidate) {
            return candidate.id == profileId;
        });
        if (profile != profiles.cend()) {
            resolved.values = profile->values;
        } else {
            resolved.problems.append(i18nc("@item", "Theme profile"));
        }
    }
    const QVariantMap overrides = decodeOverrides(settings, appearance.overrides, &resolved.problems);
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        resolved.values.insert(it.key(), it.value());
    }
    resolved.problems.append(ThemeProfiles::unavailable(resolved.values));
    return resolved;
}
