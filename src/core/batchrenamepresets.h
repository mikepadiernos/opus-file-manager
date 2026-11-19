// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "batchrename.h"

#include <KConfigGroup>

namespace BatchRenamePresets
{
struct Preset {
    QString name;
    BatchRename::Rule rule;
};

QList<Preset> load(const KConfigGroup &group, QString *error);
bool save(KConfigGroup group, const QList<Preset> &presets, QString *error);
}
