// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <QList>
#include <QUrl>

namespace SearchCollection
{
QList<QUrl> load(const KConfigGroup &group, QString *error);
bool save(KConfigGroup group, const QList<QUrl> &urls, QString *error);
}
