// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <QList>
#include <QUrl>

namespace SearchCollection
{
struct SavedCollection {
    QString name;
    QList<QUrl> urls;
    bool operator==(const SavedCollection &other) const = default;
};

QList<QUrl> load(const KConfigGroup &group, QString *error);
bool save(KConfigGroup group, const QList<QUrl> &urls, QString *error);
QList<SavedCollection> loadSaved(const KConfigGroup &group, QString *error);
bool saveSaved(KConfigGroup group, const QList<SavedCollection> &collections, QString *error);
}
