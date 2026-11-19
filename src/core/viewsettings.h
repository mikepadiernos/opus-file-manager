// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QStringList>

struct ViewSettings {
    QStringList visibleColumns{QStringLiteral("type"), QStringLiteral("size"), QStringLiteral("date")};
    QString groupBy = QStringLiteral("none");
    QString sizeFormat = QStringLiteral("human");
    QString dateFormat = QStringLiteral("locale");
    bool showSeconds = false;
    int iconNameLines = 2;
    QStringList iconInformation;

    static ViewSettings fromConfig(const KConfigGroup &group);
    static bool migrateColumns(KConfigGroup &legacy, KConfigGroup &views);
    void writeConfig(KConfigGroup &group) const;
};
