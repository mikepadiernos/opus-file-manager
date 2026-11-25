// SPDX-License-Identifier: GPL-3.0-or-later

#include "themeprofiles.h"

#include "appfonts.h"
#include "colorschemes.h"
#include "stylesheets.h"

#include <KCoreConfigSkeleton>
#include <KLocalizedString>

#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace
{
constexpr qsizetype MaximumStringLength = 4096;
constexpr qsizetype MaximumListLength = 100;
constexpr qsizetype MaximumPaletteColors = 8;

const QStringList &fontEntries()
{
    static const QStringList entries{QStringLiteral("FileListFont"),
                                     QStringLiteral("FolderTreeFont"),
                                     QStringLiteral("TabFont"),
                                     QStringLiteral("StatusBarFont")};
    return entries;
}

bool hasControlCharacter(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar ch) {
        return ch.category() == QChar::Other_Control || ch == QChar::LineSeparator || ch == QChar::ParagraphSeparator;
    });
}

QString invalidValue(const QString &entry)
{
    return i18nc("@info %1 is a setting name such as PaneTint", "The theme profile value %1 is invalid.", entry);
}

QString colorName(const QColor &color)
{
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

/// Checks entry-specific rules beyond the setting type.
bool validValue(const QString &entry, const QVariant &value)
{
    if (entry == QLatin1String("ColorScheme")) {
        const QString scheme = value.toString();
        return scheme.size() <= 255 && !scheme.contains(QLatin1Char('/')) && !scheme.contains(QLatin1Char('\\')) && !hasControlCharacter(scheme);
    }
    if (entry == QLatin1String("AccentColor")) {
        const QString accent = value.toString();
        return accent.isEmpty() || QColor::fromString(accent).isValid();
    }
    if (entry == QLatin1String("CustomColors")) {
        const QStringList colors = value.toStringList();
        return std::all_of(colors.cbegin(), colors.cend(), [](const QString &color) {
            return ColorSchemes::parseOverrides({color}).size() == 1;
        });
    }
    if (entry == QLatin1String("TreeLinePalette")) {
        const QStringList colors = value.toStringList();
        return !colors.isEmpty() && colors.size() <= MaximumPaletteColors && std::all_of(colors.cbegin(), colors.cend(), [](const QString &color) {
            return QColor::fromString(color.trimmed()).isValid();
        });
    }
    if (fontEntries().contains(entry)) {
        const QString font = value.toString();
        QFont parsed;
        return font.isEmpty() || (!hasControlCharacter(font) && parsed.fromString(font));
    }
    if (entry == QLatin1String("CustomStyleSheet")) {
        return StyleSheets::check(value.toString()).isEmpty();
    }
    return true;
}

qsizetype maximumLength(const KConfigSkeletonItem *item)
{
    return item->name() == QLatin1String("CustomStyleSheet") ? StyleSheets::MaximumSize : MaximumStringLength;
}

QJsonValue encode(const KConfigSkeletonItem *item, const QVariant &value)
{
    if (const auto *enumItem = dynamic_cast<const KCoreConfigSkeleton::ItemEnum *>(item)) {
        const auto choices = enumItem->choices();
        const int index = value.toInt();
        return index >= 0 && index < choices.size() ? QJsonValue(choices.at(index).name) : QJsonValue();
    }
    switch (item->property().typeId()) {
    case QMetaType::Bool:
        return value.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return value.toLongLong();
    case QMetaType::QColor:
        return colorName(value.value<QColor>());
    case QMetaType::QStringList:
        return QJsonArray::fromStringList(value.toStringList());
    default:
        return value.toString();
    }
}

std::optional<QVariant> decode(const KConfigSkeletonItem *item, const QJsonValue &json)
{
    std::optional<QVariant> value;
    if (const auto *enumItem = dynamic_cast<const KCoreConfigSkeleton::ItemEnum *>(item)) {
        const auto choices = enumItem->choices();
        for (int i = 0; json.isString() && i < choices.size(); ++i) {
            if (choices.at(i).name == json.toString()) {
                value = i;
            }
        }
    } else {
        switch (item->property().typeId()) {
        case QMetaType::Bool:
            if (json.isBool()) {
                value = json.toBool();
            }
            break;
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong: {
            const double number = json.toDouble(std::nan(""));
            const QVariant minimum = item->minValue();
            const QVariant maximum = item->maxValue();
            if (json.isDouble() && std::isfinite(number) && std::trunc(number) == number && std::abs(number) < 1e9
                && (!minimum.isValid() || number >= minimum.toDouble()) && (!maximum.isValid() || number <= maximum.toDouble())) {
                if (QVariant converted = static_cast<qlonglong>(number); converted.convert(item->property().metaType())) {
                    value = converted;
                }
            }
            break;
        }
        case QMetaType::QColor:
            if (json.isString() && json.toString().size() <= 9) {
                if (const QColor color = QColor::fromString(json.toString()); color.isValid()) {
                    value = color;
                }
            }
            break;
        case QMetaType::QStringList:
            if (json.isArray() && json.toArray().size() <= MaximumListLength) {
                QStringList list;
                for (const QJsonValue &entry : json.toArray()) {
                    if (!entry.isString() || entry.toString().size() > MaximumStringLength) {
                        return std::nullopt;
                    }
                    list.append(entry.toString());
                }
                value = list;
            }
            break;
        case QMetaType::QString:
            if (json.isString() && json.toString().size() <= maximumLength(item)) {
                value = json.toString();
            }
            break;
        default:
            break;
        }
    }
    if (value && !validValue(item->name(), *value)) {
        value.reset();
    }
    return value;
}

QJsonObject encodeValues(const KCoreConfigSkeleton *settings, const QVariantMap &values)
{
    QJsonObject appearance;
    for (const KConfigSkeletonItem *item : ThemeProfiles::items(settings)) {
        const QJsonValue encoded = encode(item, values.value(item->name(), item->getDefault()));
        if (!encoded.isNull() && !encoded.isUndefined()) {
            appearance.insert(item->name(), encoded);
        }
    }
    return appearance;
}

/// Reads an "appearance" object, filling missing entries with defaults.
std::optional<QVariantMap> decodeValues(const KCoreConfigSkeleton *settings, const QJsonValue &json, QString *error, QStringList *warnings)
{
    if (!json.isObject()) {
        *error = i18nc("@info", "The theme profile has no appearance options.");
        return std::nullopt;
    }
    const QJsonObject appearance = json.toObject();
    QVariantMap values;
    QSet<QString> known;
    for (const KConfigSkeletonItem *item : ThemeProfiles::items(settings)) {
        known.insert(item->name());
        if (!appearance.contains(item->name())) {
            values.insert(item->name(), item->getDefault());
            continue;
        }
        const std::optional<QVariant> value = decode(item, appearance.value(item->name()));
        if (!value) {
            *error = invalidValue(item->name());
            return std::nullopt;
        }
        values.insert(item->name(), *value);
    }
    if (warnings) {
        for (auto it = appearance.constBegin(); it != appearance.constEnd(); ++it) {
            if (!known.contains(it.key())) {
                warnings->append(i18nc("@info %1 is a setting name", "Skipped the unknown option %1.", it.key().left(ThemeProfiles::MaximumNameLength)));
            }
        }
    }
    return values;
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

QList<KConfigSkeletonItem *> ThemeProfiles::items(const KCoreConfigSkeleton *settings)
{
    static const QStringList excluded{QStringLiteral("FolderColorRules"), QStringLiteral("SuspendEffectsInHighContrast")};
    QList<KConfigSkeletonItem *> result;
    const KConfigSkeletonItem::List all = settings->items();
    for (KConfigSkeletonItem *item : all) {
        if (item->group() == QLatin1String("Themes") && !excluded.contains(item->name())) {
            result.append(item);
        }
    }
    return result;
}

QStringList ThemeProfiles::entries(const KCoreConfigSkeleton *settings)
{
    QStringList names;
    for (const KConfigSkeletonItem *item : items(settings)) {
        names.append(item->name());
    }
    return names;
}

QVariantMap ThemeProfiles::capture(const KCoreConfigSkeleton *settings)
{
    QVariantMap values;
    for (const KConfigSkeletonItem *item : items(settings)) {
        values.insert(item->name(), item->property());
    }
    return values;
}

QVariantMap ThemeProfiles::defaults(const KCoreConfigSkeleton *settings)
{
    QVariantMap values;
    for (const KConfigSkeletonItem *item : items(settings)) {
        values.insert(item->name(), item->getDefault());
    }
    return values;
}

QStringList ThemeProfiles::styleSheetEntries()
{
    return {QStringLiteral("UseCustomStyleSheet"), QStringLiteral("CustomStyleSheet")};
}

bool ThemeProfiles::hasStyleSheet(const QVariantMap &values)
{
    return !values.value(QStringLiteral("CustomStyleSheet")).toString().trimmed().isEmpty();
}

QString ThemeProfiles::nameError(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return i18nc("@info", "Enter a name for the theme profile.");
    }
    if (trimmed.size() > MaximumNameLength) {
        return i18ncp("@info", "Theme profile names can have at most %1 character.", "Theme profile names can have at most %1 characters.", MaximumNameLength);
    }
    if (hasControlCharacter(trimmed)) {
        return i18nc("@info", "Theme profile names cannot contain line breaks or control characters.");
    }
    return {};
}

QByteArray ThemeProfiles::toJson(const KCoreConfigSkeleton *settings, const Profile &profile)
{
    QJsonObject document;
    document.insert(QStringLiteral("format"), QLatin1String(Format));
    document.insert(QStringLiteral("version"), Version);
    document.insert(QStringLiteral("name"), profile.name.trimmed());
    document.insert(QStringLiteral("appearance"), encodeValues(settings, profile.values));
    return QJsonDocument(document).toJson(QJsonDocument::Indented);
}

std::optional<ThemeProfiles::Profile>
ThemeProfiles::fromJson(const KCoreConfigSkeleton *settings, const QByteArray &json, QString *error, QStringList *warnings)
{
    QString reason;
    const auto reject = [&reason, error] {
        if (error) {
            *error = reason;
        }
        return std::nullopt;
    };
    if (json.size() > MaximumFileSize) {
        reason = i18nc("@info", "The theme profile is larger than %1 and was not read.", QLocale().formattedDataSize(MaximumFileSize));
        return reject();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || object.value(QStringLiteral("format")).toString() != QLatin1String(Format)) {
        reason = i18nc("@info", "The file is not a Plasma Opus theme profile.");
        return reject();
    }
    const QJsonValue version = object.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != std::trunc(version.toDouble()) || version.toDouble() < 1) {
        reason = i18nc("@info", "The theme profile has no valid format version.");
        return reject();
    }
    if (version.toDouble() > Version) {
        reason = i18nc("@info",
                       "The theme profile uses format version %1, which needs a newer version of Plasma Opus.",
                       QString::number(version.toDouble(), 'f', 0));
        return reject();
    }
    const QJsonValue name = object.value(QStringLiteral("name"));
    if (!name.isString()) {
        reason = i18nc("@info", "The theme profile has no name.");
        return reject();
    }
    if (reason = nameError(name.toString()); !reason.isEmpty()) {
        return reject();
    }
    const std::optional<QVariantMap> values = decodeValues(settings, object.value(QStringLiteral("appearance")), &reason, warnings);
    if (!values) {
        return reject();
    }
    if (error) {
        error->clear();
    }
    return Profile{QString(), name.toString().trimmed(), *values};
}

std::optional<ThemeProfiles::Profile> ThemeProfiles::importFile(const KCoreConfigSkeleton *settings, const QString &path, QString *error, QStringList *warnings)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = i18nc("@info", "Could not open %1: %2", path, file.errorString());
        }
        return std::nullopt;
    }
    if (file.size() > MaximumFileSize) {
        if (error) {
            *error = i18nc("@info", "The theme profile is larger than %1 and was not read.", QLocale().formattedDataSize(MaximumFileSize));
        }
        return std::nullopt;
    }
    return fromJson(settings, file.read(MaximumFileSize + 1), error, warnings);
}

QString ThemeProfiles::exportFile(const KCoreConfigSkeleton *settings, const Profile &profile, const QString &path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson(settings, profile)) < 0 || !file.commit()) {
        return i18nc("@info", "Could not write %1: %2", path, file.errorString());
    }
    return {};
}

QStringList ThemeProfiles::unavailable(const QVariantMap &values)
{
    QStringList missing;
    const QString scheme = values.value(QStringLiteral("ColorScheme")).toString();
    if (!scheme.isEmpty()) {
        const QList<ColorSchemes::Scheme> available = ColorSchemes::available();
        const bool installed = std::any_of(available.cbegin(), available.cend(), [&scheme](const ColorSchemes::Scheme &candidate) {
            return candidate.id == scheme;
        });
        if (!installed) {
            missing.append(i18nc("@item %1 is a colour scheme name", "Colour scheme “%1”", scheme));
        }
    }
    QStringList families;
    for (const QString &entry : fontEntries()) {
        const QFont font = AppFonts::fromSetting(values.value(entry).toString());
        if (AppFonts::isOverride(font) && !font.family().isEmpty() && !families.contains(font.family()) && !QFontDatabase::hasFamily(font.family())) {
            families.append(font.family());
            missing.append(i18nc("@item %1 is a font family", "Font “%1”", font.family()));
        }
    }
    return missing;
}

QList<ThemeProfiles::Profile> ThemeProfiles::load(const KCoreConfigSkeleton *settings, const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Profiles", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QList<Profile> profiles;
    int skipped = 0;
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) {
            *error = i18nc("@info", "Saved theme profiles are invalid and could not be loaded.");
        }
        return {};
    }
    QSet<QString> ids;
    for (const QJsonValue &entry : document.array()) {
        const QJsonObject object = entry.toObject();
        const QString name = object.value(QStringLiteral("name")).toString();
        const QUuid id = QUuid::fromString(object.value(QStringLiteral("id")).toString());
        QString reason;
        const std::optional<QVariantMap> values = decodeValues(settings, object.value(QStringLiteral("appearance")), &reason, nullptr);
        const QString canonical = id.toString(QUuid::WithoutBraces);
        if (!values || id.isNull() || ids.contains(canonical) || !nameError(name).isEmpty() || indexOfName(profiles, name) >= 0
            || profiles.size() >= MaximumProfiles) {
            ++skipped;
            continue;
        }
        ids.insert(canonical);
        profiles.append(Profile{canonical, name.trimmed(), *values});
    }
    if (skipped > 0 && error) {
        *error = i18ncp("@info", "%1 saved theme profile is invalid and was skipped.", "%1 saved theme profiles are invalid and were skipped.", skipped);
    }
    return profiles;
}

bool ThemeProfiles::save(const KCoreConfigSkeleton *settings, KConfigGroup group, QList<Profile> &profiles, QString *error)
{
    if (error) {
        error->clear();
    }
    const auto fail = [error](const QString &reason) {
        if (error) {
            *error = reason;
        }
        return false;
    };
    if (profiles.size() > MaximumProfiles) {
        return fail(i18ncp("@info", "You can save at most %1 theme profile.", "You can save at most %1 theme profiles.", MaximumProfiles));
    }
    QSet<QString> names;
    QJsonArray entries;
    for (Profile &profile : profiles) {
        if (const QString reason = nameError(profile.name); !reason.isEmpty()) {
            return fail(reason);
        }
        const QString key = profile.name.trimmed().toCaseFolded();
        if (names.contains(key)) {
            return fail(i18nc("@info", "Another theme profile is already named “%1”.", profile.name.trimmed()));
        }
        names.insert(key);
        if (QUuid::fromString(profile.id).isNull()) {
            profile.id = newId();
        }
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), profile.id);
        entry.insert(QStringLiteral("name"), profile.name.trimmed());
        entry.insert(QStringLiteral("appearance"), encodeValues(settings, profile.values));
        entries.append(entry);
    }
    const QByteArray previous = group.readEntry("Profiles", QByteArray());
    group.writeEntry("Profiles", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Profiles", previous);
        return fail(i18nc("@info", "Could not save theme profiles."));
    }
    return true;
}

qsizetype ThemeProfiles::indexOfName(const QList<Profile> &profiles, const QString &name)
{
    const QString key = name.trimmed().toCaseFolded();
    for (qsizetype i = 0; i < profiles.size(); ++i) {
        if (profiles.at(i).name.trimmed().toCaseFolded() == key) {
            return i;
        }
    }
    return -1;
}
