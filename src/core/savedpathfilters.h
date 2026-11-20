// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pathfilter.h"

#include <KConfigGroup>

namespace SavedPathFilters
{
struct Preset {
    QString name;
    PathFilter filter;
    bool operator==(const Preset &other) const = default;
};

QList<Preset> load(const KConfigGroup &group, QString *error);
bool save(KConfigGroup group, const QList<Preset> &presets, QString *error);
}
