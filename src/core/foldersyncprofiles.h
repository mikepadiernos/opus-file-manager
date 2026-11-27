// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"

#include <KConfigGroup>

#include <optional>

namespace FolderSyncProfiles
{
struct Profile {
    QString name;
    FolderSync::Options options;
    QString id;
};

QList<Profile> load(const KConfigGroup &group, QString *error = nullptr);
bool save(KConfigGroup group, const QList<Profile> &profiles, QString *error = nullptr);
/// The profile with the identifier @p id, in any notation QUuid accepts.
std::optional<Profile> find(const QList<Profile> &profiles, const QString &id);
}
