// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchcollection.h"

#include <KLocalizedString>

#include <QDir>
#include <QSet>

namespace
{
bool valid(const QList<QUrl> &urls)
{
    QSet<QUrl> seen;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile() || !QDir::isAbsolutePath(url.toLocalFile()) || url.hasQuery() || url.hasFragment() || seen.contains(url)) {
            return false;
        }
        seen.insert(url);
    }
    return true;
}
}

QList<QUrl> SearchCollection::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QList<QUrl> urls = group.readEntry("Urls", QList<QUrl>());
    if (!valid(urls)) {
        if (error) {
            *error = i18nc("@info", "The search reference collection is invalid and could not be loaded.");
        }
        return {};
    }
    return urls;
}

bool SearchCollection::save(KConfigGroup group, const QList<QUrl> &urls, QString *error)
{
    if (error) {
        error->clear();
    }
    if (!valid(urls)) {
        if (error) {
            *error = i18nc("@info", "The search reference collection contains invalid or duplicate URLs.");
        }
        return false;
    }
    const QList<QUrl> previous = group.readEntry("Urls", QList<QUrl>());
    group.writeEntry("Urls", urls);
    if (!group.sync()) {
        group.writeEntry("Urls", previous);
        if (error) {
            *error = i18nc("@info", "Could not save the search reference collection.");
        }
        return false;
    }
    return true;
}
