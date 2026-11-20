// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"

#include <KConfigGroup>

namespace FolderSyncProfiles
{
struct Profile {
    QString name;
    FolderSync::Options options;
};

QList<Profile> load(const KConfigGroup &group, QString *error = nullptr);
bool save(KConfigGroup group, const QList<Profile> &profiles, QString *error = nullptr);
}
