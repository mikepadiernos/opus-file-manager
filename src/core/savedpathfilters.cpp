// SPDX-License-Identifier: GPL-3.0-or-later

#include "savedpathfilters.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

QList<SavedPathFilters::Preset> SavedPathFilters::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Filters", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QList<Preset> presets;
    QSet<QString> names;
    bool valid = parseError.error == QJsonParseError::NoError && document.isArray();
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        Preset preset{object.value(QStringLiteral("name")).toString(), {}};
        const QString key = preset.name.trimmed().toCaseFolded();
        if (!value.isObject() || key.isEmpty() || names.contains(key) || !PathFilter::fromJson(object.value(QStringLiteral("filter")), &preset.filter)) {
            valid = false;
            break;
        }
        names.insert(key);
        presets.append(preset);
    }
    if (!valid) {
        if (error) {
            *error = i18nc("@info", "Saved path filters are invalid and could not be loaded.");
        }
        return {};
    }
    return presets;
}

bool SavedPathFilters::save(KConfigGroup group, const QList<Preset> &presets, QString *error)
{
    if (error) {
        error->clear();
    }
    QJsonArray entries;
    QSet<QString> names;
    for (const Preset &preset : presets) {
        const QString key = preset.name.trimmed().toCaseFolded();
        if (key.isEmpty() || names.contains(key) || !preset.filter.error().isEmpty()) {
            if (error) {
                *error = i18nc("@info", "Filter names must be unique and each filter must have valid criteria.");
            }
            return false;
        }
        names.insert(key);
        entries.append(QJsonObject{{QStringLiteral("name"), preset.name}, {QStringLiteral("filter"), preset.filter.toJson()}});
    }
    const QByteArray previous = group.readEntry("Filters", QByteArray());
    group.writeEntry("Filters", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Filters", previous);
        if (error) {
            *error = i18nc("@info", "Could not save path filters to configuration.");
        }
        return false;
    }
    return true;
}
