// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewsettings.h"

#include <algorithm>

namespace
{
const QStringList Columns{QStringLiteral("type"),
                          QStringLiteral("size"),
                          QStringLiteral("date"),
                          QStringLiteral("mode"),
                          QStringLiteral("owner"),
                          QStringLiteral("group"),
                          QStringLiteral("rating")};
const QStringList Groups{QStringLiteral("none"),
                         QStringLiteral("name"),
                         QStringLiteral("type"),
                         QStringLiteral("size"),
                         QStringLiteral("date"),
                         QStringLiteral("ext")};
const QStringList IconInformation{QStringLiteral("size"),
                                  QStringLiteral("permissions"),
                                  QStringLiteral("octalPermissions"),
                                  QStringLiteral("owner"),
                                  QStringLiteral("ownerAndGroup"),
                                  QStringLiteral("creationTime"),
                                  QStringLiteral("modificationTime"),
                                  QStringLiteral("accessTime"),
                                  QStringLiteral("mimeType"),
                                  QStringLiteral("friendlyMimeType"),
                                  QStringLiteral("linkDest"),
                                  QStringLiteral("localPathOrUrl"),
                                  QStringLiteral("comment")};

QStringList validList(const QStringList &values, const QStringList &allowed)
{
    QStringList result;
    for (const QString &value : values) {
        if (allowed.contains(value) && !result.contains(value)) {
            result.append(value);
        }
    }
    return result;
}
}

ViewSettings ViewSettings::fromConfig(const KConfigGroup &group)
{
    ViewSettings settings;
    if (group.hasKey("VisibleColumns")) {
        settings.visibleColumns = validList(group.readEntry("VisibleColumns", QStringList()), Columns);
    }
    const QString groupBy = group.readEntry("GroupBy", settings.groupBy);
    if (Groups.contains(groupBy)) {
        settings.groupBy = groupBy;
    }
    const QString sizeFormat = group.readEntry("SizeFormat", settings.sizeFormat);
    if (sizeFormat == QLatin1String("human") || sizeFormat == QLatin1String("bytes")) {
        settings.sizeFormat = sizeFormat;
    }
    const QString dateFormat = group.readEntry("DateFormat", settings.dateFormat);
    if (dateFormat == QLatin1String("locale") || dateFormat == QLatin1String("iso")) {
        settings.dateFormat = dateFormat;
    }
    settings.showSeconds = group.readEntry("ShowSeconds", settings.showSeconds);
    settings.iconNameLines = std::clamp(group.readEntry("IconNameLines", settings.iconNameLines), 1, 5);
    settings.iconInformation = validList(group.readEntry("IconInformation", QStringList()), IconInformation);
    return settings;
}

void ViewSettings::writeConfig(KConfigGroup &group) const
{
    group.writeEntry("VisibleColumns", validList(visibleColumns, Columns));
    group.writeEntry("GroupBy", Groups.contains(groupBy) ? groupBy : QStringLiteral("none"));
    group.writeEntry("SizeFormat", sizeFormat == QLatin1String("bytes") ? sizeFormat : QStringLiteral("human"));
    group.writeEntry("DateFormat", dateFormat == QLatin1String("iso") ? dateFormat : QStringLiteral("locale"));
    group.writeEntry("ShowSeconds", showSeconds);
    group.writeEntry("IconNameLines", std::clamp(iconNameLines, 1, 5));
    group.writeEntry("IconInformation", validList(iconInformation, IconInformation));
}
