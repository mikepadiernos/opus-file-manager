// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchcollection.h"

#include <KLocalizedString>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

QList<SearchCollection::SavedCollection> SearchCollection::loadSaved(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Collections", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    QList<SavedCollection> collections;
    QSet<QString> names;
    bool validData = parseError.error == QJsonParseError::NoError && document.isArray();
    for (const auto &value : document.array()) {
        const auto object = value.toObject();
        const auto nameValue = object.value(QStringLiteral("name"));
        const auto urlsValue = object.value(QStringLiteral("urls"));
        SavedCollection collection{nameValue.toString(), {}};
        const QString key = collection.name.trimmed().toCaseFolded();
        bool validUrls = urlsValue.isArray();
        for (const auto &url : urlsValue.toArray()) {
            validUrls = validUrls && url.isString();
            collection.urls.append(QUrl(url.toString(), QUrl::StrictMode));
        }
        if (!value.isObject() || !nameValue.isString() || key.isEmpty() || names.contains(key) || !validUrls || !valid(collection.urls)) {
            validData = false;
            break;
        }
        names.insert(key);
        collections.append(collection);
    }
    if (!validData) {
        if (error) {
            *error = i18nc("@info", "Saved search collections are invalid and could not be loaded.");
        }
        return {};
    }
    return collections;
}

bool SearchCollection::saveSaved(KConfigGroup group, const QList<SavedCollection> &collections, QString *error)
{
    if (error) {
        error->clear();
    }
    QSet<QString> names;
    QJsonArray entries;
    for (const auto &collection : collections) {
        const QString key = collection.name.trimmed().toCaseFolded();
        if (key.isEmpty() || names.contains(key) || !valid(collection.urls)) {
            if (error) {
                *error = i18nc("@info", "Collection names must be unique and references must be valid local URLs.");
            }
            return false;
        }
        names.insert(key);
        QJsonArray urls;
        for (const QUrl &url : collection.urls) {
            urls.append(url.toString(QUrl::FullyEncoded));
        }
        entries.append(QJsonObject{{QStringLiteral("name"), collection.name}, {QStringLiteral("urls"), urls}});
    }
    const QByteArray previous = group.readEntry("Collections", QByteArray());
    group.writeEntry("Collections", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Collections", previous);
        if (error) {
            *error = i18nc("@info", "Could not save search collections to configuration.");
        }
        return false;
    }
    return true;
}
