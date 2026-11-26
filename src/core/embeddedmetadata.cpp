// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddedmetadata.h"

#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>
#include <KLocalizedString>

#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>

#include <algorithm>
#include <memory>
#include <optional>

namespace
{
namespace Property = KFileMetaData::Property;

const QList<Property::Property> &imageProperties()
{
    static const QList<Property::Property> properties{
        Property::PhotoDateTimeOriginal,
        Property::ImageDateTime,
        Property::Manufacturer,
        Property::Model,
        Property::Title,
        Property::Description,
        Property::Subject,
        Property::Keywords,
        Property::Artist,
        Property::Copyright,
        Property::PhotoISOSpeedRatings,
        Property::PhotoFNumber,
        Property::PhotoExposureTime,
        Property::PhotoFocalLength,
    };
    return properties;
}

const QList<Property::Property> &audioProperties()
{
    static const QList<Property::Property> properties{
        Property::Title,
        Property::Artist,
        Property::AlbumArtist,
        Property::Album,
        Property::Genre,
        Property::Composer,
        Property::ReleaseYear,
        Property::TrackNumber,
        Property::DiscNumber,
        Property::Comment,
    };
    return properties;
}

bool isDateProperty(Property::Property property)
{
    return property == Property::PhotoDateTimeOriginal || property == Property::ImageDateTime;
}

QString textOf(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QDateTime) {
        return value.toDateTime().toString(Qt::ISODate);
    }
    if (value.metaType().id() == QMetaType::QDate) {
        return value.toDate().toString(Qt::ISODate);
    }
    if (value.metaType().id() == QMetaType::QStringList) {
        return value.toStringList().join(QStringLiteral("; "));
    }
    return value.toString().trimmed();
}

QDate dateOf(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QDateTime) {
        return value.toDateTime().date();
    }
    if (value.metaType().id() == QMetaType::QDate) {
        return value.toDate();
    }
    // Some extractors report EXIF dates as text, such as "2026:02:28 10:00:00".
    const QString text = value.toString().trimmed();
    QDate date = QDate::fromString(text.left(10), QStringLiteral("yyyy:MM:dd"));
    return date.isValid() ? date : QDate::fromString(text.left(10), Qt::ISODate);
}

KFileMetaData::ExtractorCollection &extractors()
{
    // Loading the extractor plugins is slow, and searches read many files on one thread.
    static thread_local KFileMetaData::ExtractorCollection collection;
    return collection;
}

QStringList keysOf(const QList<Property::Property> &properties)
{
    QStringList result;
    for (const Property::Property property : properties) {
        const QString key = KFileMetaData::PropertyInfo(property).name();
        if (!result.contains(key)) {
            result.append(key);
        }
    }
    return result;
}
}

QStringList EmbeddedMetadata::keys()
{
    static const QStringList result = keysOf(imageProperties() + audioProperties());
    return result;
}

QStringList EmbeddedMetadata::imageKeys()
{
    static const QStringList result = keysOf(imageProperties());
    return result;
}

QStringList EmbeddedMetadata::audioKeys()
{
    static const QStringList result = keysOf(audioProperties());
    return result;
}

QStringList EmbeddedMetadata::enabledKeys(const QStringList &disabled)
{
    QStringList result = keys();
    result.removeIf([&disabled](const QString &key) {
        return disabled.contains(key);
    });
    return result;
}

QString EmbeddedMetadata::displayName(const QString &key)
{
    const KFileMetaData::PropertyInfo info = KFileMetaData::PropertyInfo::fromName(key);
    return info.property() == Property::Empty ? key : info.displayName();
}

bool EmbeddedMetadata::isDate(const QString &key)
{
    return isDateProperty(KFileMetaData::PropertyInfo::fromName(key).property());
}

bool EmbeddedMetadata::isText(const QString &key)
{
    const KFileMetaData::PropertyInfo info = KFileMetaData::PropertyInfo::fromName(key);
    return info.property() != Property::Empty && !isDateProperty(info.property())
        && (info.valueType() == QMetaType::QString || info.valueType() == QMetaType::QStringList);
}

EmbeddedMetadata::Reading EmbeddedMetadata::read(const QString &path, const QStringList &enabled)
{
    Reading result;
    result.path = path;
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isFile()) {
        result.problem = i18nc("@info", "Only regular files have embedded metadata.");
        return result;
    }
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    result.mimeType = mime.name();
    const bool image = result.mimeType.startsWith(QLatin1String("image/"));
    const bool audio = result.mimeType.startsWith(QLatin1String("audio/"));
    if (!image && !audio) {
        result.problem = i18nc("@info", "EXIF and audio tags are read from images and audio files only.");
        return result;
    }
    const auto available = extractors().fetchExtractors(result.mimeType);
    if (available.isEmpty()) {
        result.problem = i18nc("@info", "No KDE metadata reader supports %1 files.", mime.comment());
        return result;
    }
    result.supported = true;
    QList<Property::Property> wanted;
    for (const Property::Property property : image ? imageProperties() : audioProperties()) {
        if (enabled.contains(KFileMetaData::PropertyInfo(property).name())) {
            wanted.append(property);
        }
    }
    if (wanted.isEmpty()) {
        return result;
    }
    KFileMetaData::SimpleExtractionResult extraction(path, result.mimeType, KFileMetaData::ExtractionResult::ExtractMetaData);
    for (KFileMetaData::Extractor *extractor : available) {
        extractor->extract(&extraction);
    }
    const KFileMetaData::PropertyMultiMap properties = extraction.properties();
    for (const Property::Property property : std::as_const(wanted)) {
        // QMultiMap::values() lists the most recently inserted value first.
        QList<QVariant> raw = properties.values(property);
        std::reverse(raw.begin(), raw.end());
        Value value;
        for (const QVariant &item : std::as_const(raw)) {
            const QString text = textOf(item);
            if (!text.isEmpty() && !value.items.contains(text)) {
                value.items.append(text);
                if (isDateProperty(property) && !value.date.isValid()) {
                    value.date = dateOf(item);
                }
            }
        }
        if (value.items.isEmpty()) {
            continue;
        }
        const KFileMetaData::PropertyInfo info(property);
        value.key = info.name();
        value.name = info.displayName();
        value.text = value.items.join(QStringLiteral("; "));
        result.values.append(value);
    }
    return result;
}

QStringList EmbeddedMetadata::searchTexts(const Reading &reading, const QStringList &enabled)
{
    QStringList texts;
    for (const Value &value : reading.values) {
        if (!enabled.contains(value.key)) {
            continue;
        }
        texts.append(value.items);
        if (value.date.isValid()) {
            texts.append(value.date.toString(Qt::ISODate));
        }
    }
    return texts;
}

std::function<QStringList()> EmbeddedMetadata::lazySearchTexts(const QString &path, const QStringList &enabled)
{
    auto cache = std::make_shared<std::optional<QStringList>>();
    return [path, enabled, cache]() {
        if (!*cache) {
            *cache = enabled.isEmpty() ? QStringList() : searchTexts(read(path, enabled), enabled);
        }
        return **cache;
    };
}
