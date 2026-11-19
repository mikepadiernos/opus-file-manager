// SPDX-License-Identifier: GPL-3.0-or-later

#include "savedsearches.h"

#include <KLocalizedString>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace
{
bool valid(const SavedSearches::Search &search)
{
    return !search.name.trimmed().isEmpty() && !search.text.trimmed().isEmpty()
        && (search.field == LiveSearch::Field::FileName || search.field == LiveSearch::Field::Contents)
        && (search.folder.isEmpty() ? search.indexed : (search.folder.isLocalFile() && QDir::isAbsolutePath(search.folder.toLocalFile())))
        && search.filter.error().isEmpty() && (!search.indexed || !search.filter.isActive());
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
    for (const QJsonValue &value : document.array()) {
        const QJsonObject entry = value.toObject();
        const QString field = entry.value(QStringLiteral("field")).toString();
        const QJsonValue indexed = entry.value(QStringLiteral("indexed"));
        Search search{entry.value(QStringLiteral("name")).toString(),
                      entry.value(QStringLiteral("text")).toString(),
                      QUrl(entry.value(QStringLiteral("folder")).toString(), QUrl::StrictMode),
                      field == QLatin1String("contents") ? LiveSearch::Field::Contents : LiveSearch::Field::FileName,
                      indexed.toBool()};
        const QJsonValue filterValue = entry.value(QStringLiteral("filter"));
        bool validFilter = filterValue.isUndefined();
        if (filterValue.isObject()) {
            const QJsonObject filter = filterValue.toObject();
            const QJsonValue path = filter.value(QStringLiteral("path"));
            const QJsonValue parent = filter.value(QStringLiteral("parent"));
            const QJsonValue excluded = filter.value(QStringLiteral("excluded"));
            const QJsonValue depth = filter.value(QStringLiteral("depth"));
            validFilter = path.isString() && parent.isString() && excluded.isArray() && depth.isDouble() && depth.toDouble() == depth.toInt(-2);
            search.filter.pathText = path.toString();
            search.filter.parentText = parent.toString();
            search.filter.maxDepth = depth.toInt(-2);
            for (const QJsonValue &folder : excluded.toArray()) {
                validFilter = validFilter && folder.isString();
                search.filter.excludedFolders.append(folder.toString());
            }
        }
        const QString key = search.name.trimmed().toCaseFolded();
        if (!value.isObject() || !indexed.isBool() || !validFilter || (field != QLatin1String("name") && field != QLatin1String("contents")) || !valid(search)
            || names.contains(key)) {
            if (error) {
                *error = i18nc("@info", "Saved searches are invalid and could not be loaded.");
            }
            return {};
        }
        names.insert(key);
        searches.append(search);
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
    for (const Search &search : searches) {
        const QString key = search.name.trimmed().toCaseFolded();
        if (!valid(search) || names.contains(key)) {
            if (error) {
                *error = i18nc("@info", "Search names must be unique and each search must have valid criteria.");
            }
            return false;
        }
        names.insert(key);
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), search.name);
        entry.insert(QStringLiteral("text"), search.text);
        entry.insert(QStringLiteral("folder"), search.folder.toString(QUrl::FullyEncoded));
        entry.insert(QStringLiteral("field"), search.field == LiveSearch::Field::Contents ? QStringLiteral("contents") : QStringLiteral("name"));
        entry.insert(QStringLiteral("indexed"), search.indexed);
        QJsonObject filter;
        filter.insert(QStringLiteral("path"), search.filter.pathText);
        filter.insert(QStringLiteral("parent"), search.filter.parentText);
        filter.insert(QStringLiteral("excluded"), QJsonArray::fromStringList(search.filter.excludedFolders));
        filter.insert(QStringLiteral("depth"), search.filter.maxDepth);
        entry.insert(QStringLiteral("filter"), filter);
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
