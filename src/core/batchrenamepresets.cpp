// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrenamepresets.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

QList<BatchRenamePresets::Preset> BatchRenamePresets::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Presets", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QList<Preset> presets;
    QSet<QString> names;
    if (parseError.error == QJsonParseError::NoError && document.isArray()) {
        for (const QJsonValue &value : document.array()) {
            const QJsonObject entry = value.toObject();
            const auto pattern = entry.value(QStringLiteral("pattern"));
            const auto replacement = entry.value(QStringLiteral("replacement"));
            const auto regex = entry.value(QStringLiteral("regex"));
            const auto start = entry.value(QStringLiteral("start"));
            const auto padding = entry.value(QStringLiteral("padding"));
            Preset preset{entry.value(QStringLiteral("name")).toString(),
                          {pattern.toString(), replacement.toString(), regex.toBool(), start.toInt(-1), padding.toInt(-1)}};
            const QString key = preset.name.trimmed().toCaseFolded();
            if (!value.isObject() || !pattern.isString() || !replacement.isString() || !regex.isBool() || !start.isDouble() || !padding.isDouble()
                || start.toDouble() != start.toInt(-1) || padding.toDouble() != padding.toInt(-1) || key.isEmpty() || names.contains(key)
                || !BatchRename::ruleError(preset.rule).isEmpty()) {
                presets.clear();
                break;
            }
            names.insert(key);
            presets.append(preset);
        }
        if (presets.size() == document.array().size()) {
            return presets;
        }
    }
    if (error) {
        *error = i18nc("@info", "Batch rename presets are invalid and could not be loaded.");
    }
    return {};
}

bool BatchRenamePresets::save(KConfigGroup group, const QList<Preset> &presets, QString *error)
{
    if (error) {
        error->clear();
    }
    QJsonArray entries;
    QSet<QString> names;
    for (const Preset &preset : presets) {
        const QString key = preset.name.trimmed().toCaseFolded();
        if (key.isEmpty() || names.contains(key) || !BatchRename::ruleError(preset.rule).isEmpty()) {
            if (error) {
                *error = i18nc("@info", "Preset names must be unique and each preset must have a valid rule.");
            }
            return false;
        }
        names.insert(key);
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), preset.name);
        entry.insert(QStringLiteral("pattern"), preset.rule.pattern);
        entry.insert(QStringLiteral("replacement"), preset.rule.replacement);
        entry.insert(QStringLiteral("regex"), preset.rule.regularExpression);
        entry.insert(QStringLiteral("start"), preset.rule.start);
        entry.insert(QStringLiteral("padding"), preset.rule.padding);
        entries.append(entry);
    }
    const QByteArray previous = group.readEntry("Presets", QByteArray());
    group.writeEntry("Presets", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Presets", previous);
        if (error) {
            *error = i18nc("@info", "Could not save batch rename presets to configuration.");
        }
        return false;
    }
    return true;
}
