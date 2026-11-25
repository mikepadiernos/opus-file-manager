// SPDX-License-Identifier: GPL-3.0-or-later

#include "savedsearches.h"

#include <KLocalizedString>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

namespace
{
bool valid(const SavedSearches::Search &search)
{
    return !search.name.trimmed().isEmpty() && !search.text.trimmed().isEmpty()
        && (search.field == LiveSearch::Field::FileName || search.field == LiveSearch::Field::Contents || search.field == LiveSearch::Field::Query)
        && !(search.indexed && search.searchFolders)
        && (search.folder.isEmpty() ? search.indexed || search.searchFolders
                                    : (search.folder.isLocalFile() && QDir::isAbsolutePath(search.folder.toLocalFile())))
        && search.filter.error().isEmpty() && (search.filter.maxDepth < 0 || !search.folder.isEmpty() || search.searchFolders);
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool validId(const QString &id)
{
    const QUuid uuid = QUuid::fromString(id);
    return !uuid.isNull();
}

QString canonicalId(const QString &id)
{
    return QUuid::fromString(id).toString(QUuid::WithoutBraces);
}
}

QList<SavedSearches::Search> SavedSearches::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Searches", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) {
            *error = i18nc("@info", "Saved searches are invalid and could not be loaded.");
        }
        return {};
    }
    QList<Search> searches;
    QSet<QString> names;
    QSet<QString> ids;
    bool needsMigration = false;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject entry = value.toObject();
        const QString field = entry.value(QStringLiteral("field")).toString();
        const QJsonValue indexed = entry.value(QStringLiteral("indexed"));
        const QJsonValue idValue = entry.value(QStringLiteral("id"));
        Search search{entry.value(QStringLiteral("name")).toString(),
                      entry.value(QStringLiteral("text")).toString(),
                      QUrl(entry.value(QStringLiteral("folder")).toString(), QUrl::StrictMode),
                      field == QLatin1String("contents")    ? LiveSearch::Field::Contents
                          : field == QLatin1String("query") ? LiveSearch::Field::Query
                                                            : LiveSearch::Field::FileName,
                      indexed.toBool(),
                      {},
                      {}};
        if (idValue.isUndefined()) {
            search.id = newId();
            needsMigration = true;
        } else if (idValue.isString()) {
            const QUuid uuid = QUuid::fromString(idValue.toString());
            search.id = uuid.toString(QUuid::WithoutBraces);
        }
        const QJsonValue filterValue = entry.value(QStringLiteral("filter"));
        const bool validFilter = filterValue.isUndefined() || PathFilter::fromJson(filterValue, &search.filter);
        const QJsonValue searchFolders = entry.value(QStringLiteral("searchFolders"));
        search.searchFolders = searchFolders.toBool();
        const QString key = search.name.trimmed().toCaseFolded();
        if (!value.isObject() || !indexed.isBool() || !validFilter || (!searchFolders.isUndefined() && !searchFolders.isBool())
            || (field != QLatin1String("name") && field != QLatin1String("contents") && field != QLatin1String("query")) || !valid(search)
            || !validId(search.id) || names.contains(key) || ids.contains(search.id)) {
            if (error) {
                *error = i18nc("@info", "Saved searches are invalid and could not be loaded.");
            }
            return {};
        }
        names.insert(key);
        ids.insert(search.id);
        searches.append(search);
    }
    if (needsMigration && !save(group, searches, error)) {
        return {};
    }
    return searches;
}

bool SavedSearches::save(KConfigGroup group, const QList<Search> &searches, QString *error)
{
    if (error) {
        error->clear();
    }
    QJsonArray entries;
    QSet<QString> names;
    QSet<QString> ids;
    for (const Search &search : searches) {
        const QString key = search.name.trimmed().toCaseFolded();
        const QString id = search.id.isEmpty() ? newId() : canonicalId(search.id);
        if (!valid(search) || !validId(id) || names.contains(key) || ids.contains(id)) {
            if (error) {
                *error = i18nc("@info", "Search names and identifiers must be unique and each search must have valid criteria.");
            }
            return false;
        }
        names.insert(key);
        ids.insert(id);
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), search.name);
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("text"), search.text);
        entry.insert(QStringLiteral("folder"), search.folder.toString(QUrl::FullyEncoded));
        entry.insert(QStringLiteral("field"),
                     search.field == LiveSearch::Field::Contents    ? QStringLiteral("contents")
                         : search.field == LiveSearch::Field::Query ? QStringLiteral("query")
                                                                    : QStringLiteral("name"));
        entry.insert(QStringLiteral("indexed"), search.indexed);
        entry.insert(QStringLiteral("filter"), search.filter.toJson());
        if (search.searchFolders) {
            entry.insert(QStringLiteral("searchFolders"), true);
        }
        entries.append(entry);
    }
    const QByteArray previous = group.readEntry("Searches", QByteArray());
    group.writeEntry("Searches", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Searches", previous);
        if (error) {
            *error = i18nc("@info", "Could not save searches to configuration.");
        }
        return false;
    }
    return true;
}
