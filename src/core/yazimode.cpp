// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfigGroup>
#include <KIO/Global>

#include <QJsonObject>

namespace
{
constexpr auto SessionKey = "YaziMode";
constexpr auto WorkspaceKey = "yaziMode";
}

bool YaziMode::readSession(const KConfigGroup &group)
{
    return group.readEntry(SessionKey, false);
}

void YaziMode::writeSession(KConfigGroup &group, bool enabled)
{
    group.writeEntry(SessionKey, enabled);
}

void YaziMode::writeWorkspace(QJsonObject &object, bool enabled)
{
    object.insert(QLatin1String(WorkspaceKey), enabled);
}

bool YaziMode::readWorkspace(const QJsonObject &object, bool &enabled)
{
    const auto value = object.value(QLatin1String(WorkspaceKey));
    if (value.isUndefined()) {
        enabled = false;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    enabled = value.toBool();
    return true;
}

QUrl YaziMode::parentFolder(const QUrl &current)
{
    if (!current.isValid() || current.isEmpty()) {
        return {};
    }
    const QUrl parent = KIO::upUrl(current);
    if (!parent.isValid() || parent.isEmpty() || parent.adjusted(QUrl::StripTrailingSlash) == current.adjusted(QUrl::StripTrailingSlash)) {
        return {};
    }
    return parent;
}
