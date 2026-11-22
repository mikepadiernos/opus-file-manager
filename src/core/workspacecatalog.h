// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <QList>
#include <QString>

namespace WorkspaceCatalog
{
struct Workspace {
    QString id;
    QString name;
    QString shortLabel;
    bool operator==(const Workspace &other) const = default;
};

struct Catalog {
    QList<Workspace> workspaces;
    QString activeId;
    bool operator==(const Catalog &other) const = default;
};

bool load(const KConfigGroup &group, Catalog &catalog, QString &error);
bool save(KConfigGroup group, const Catalog &catalog, QString &error);
bool create(Catalog &catalog, const QString &name, const QString &shortLabel, QString &error);
bool rename(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error);
bool activate(Catalog &catalog, const QString &id, QString &error);
bool remove(Catalog &catalog, const QString &id, QString &error);
}
