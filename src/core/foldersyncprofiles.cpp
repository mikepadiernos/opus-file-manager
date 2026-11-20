// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersyncprofiles.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace
{
bool valid(const FolderSyncProfiles::Profile &profile)
{
    return !profile.name.trimmed().isEmpty() && !profile.name.contains(QChar::Null) && !profile.name.contains(QLatin1Char('\n'))
        && !profile.name.contains(QLatin1Char('\r')) && FolderSync::optionsError(profile.options).isEmpty();
}

bool readPatterns(const QJsonValue &value, QStringList &patterns)
{
    if (!value.isArray()) {
        return false;
    }
    for (const auto &item : value.toArray()) {
        if (!item.isString()) {
            return false;
        }
        patterns.append(item.toString());
    }
    return true;
}
}

QList<FolderSyncProfiles::Profile> FolderSyncProfiles::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Profiles", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    QList<Profile> profiles;
    QSet<QString> names;
    bool validData = parseError.error == QJsonParseError::NoError && document.isArray();
    if (validData) {
        for (const auto &value : document.array()) {
            const auto object = value.toObject();
            const auto recursive = object.value(QStringLiteral("recursive"));
            const auto contents = object.value(QStringLiteral("contents"));
            const auto links = object.value(QStringLiteral("links"));
            Profile profile;
            profile.name = object.value(QStringLiteral("name")).toString();
            validData = value.isObject() && object.value(QStringLiteral("name")).isString() && recursive.isBool() && contents.isBool() && links.isBool()
                && readPatterns(object.value(QStringLiteral("include")), profile.options.includePatterns)
                && readPatterns(object.value(QStringLiteral("exclude")), profile.options.excludePatterns);
            profile.options.recursive = recursive.toBool();
            profile.options.compareContents = contents.toBool();
            profile.options.compareSymlinks = links.toBool();
            const QString key = profile.name.trimmed().toCaseFolded();
            if (!validData || !valid(profile) || names.contains(key)) {
                validData = false;
                break;
            }
            names.insert(key);
            profiles.append(profile);
        }
    }
    if (!validData) {
        if (error) {
            *error = i18nc("@info", "Saved folder comparison profiles are invalid and could not be loaded.");
        }
        return {};
    }
    return profiles;
}

bool FolderSyncProfiles::save(KConfigGroup group, const QList<Profile> &profiles, QString *error)
{
    if (error) {
        error->clear();
    }
    QSet<QString> names;
    QJsonArray entries;
    for (const Profile &profile : profiles) {
        const QString key = profile.name.trimmed().toCaseFolded();
        if (!valid(profile) || names.contains(key)) {
            if (error) {
                *error = i18nc("@info", "Profile names must be unique and each profile must have valid filters.");
            }
            return false;
        }
        names.insert(key);
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), profile.name);
        entry.insert(QStringLiteral("include"), QJsonArray::fromStringList(profile.options.includePatterns));
        entry.insert(QStringLiteral("exclude"), QJsonArray::fromStringList(profile.options.excludePatterns));
        entry.insert(QStringLiteral("recursive"), profile.options.recursive);
        entry.insert(QStringLiteral("contents"), profile.options.compareContents);
        entry.insert(QStringLiteral("links"), profile.options.compareSymlinks);
        entries.append(entry);
    }
    const QByteArray previous = group.readEntry("Profiles", QByteArray());
    group.writeEntry("Profiles", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Profiles", previous);
        if (error) {
            *error = i18nc("@info", "Could not save folder comparison profiles.");
        }
        return false;
    }
    return true;
}
