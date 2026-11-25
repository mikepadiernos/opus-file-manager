// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "embeddedmetadata.h"
#include "taglibrary.h"

#include <QList>
#include <QMap>
#include <QString>

#include <functional>

/**
 * Imports embedded EXIF and audio metadata into a tag library as tags or
 * field values. A preview lists every change first; applying it only changes
 * the library document, never the files.
 */
namespace MetadataImport
{
/// The mapping target that turns each value of a property into a tag.
QString tagsTarget();

enum class Policy {
    /// Fields that already have a different value keep it.
    Keep,
    /// Imported values replace different field values.
    Replace,
};

enum class Action {
    AssignTag,
    CreateTag,
    SetField,
    /// The item already has the tag or value.
    Unchanged,
    /// The value is not imported because the field has a different value, or another property already sets it.
    Conflict,
    /// The value can't be stored, such as a tag name that is too long or a field that needs a date.
    Invalid,
    /// The file's metadata can't be read; key and value are empty.
    Unsupported,
};

struct Change {
    /// The item's absolute path.
    QString path;
    /// The property key, or empty for unsupported files.
    QString key;
    /// The tag name or field value to import.
    QString value;
    Action action = Action::Unchanged;
    /// The existing tag's identifier, or the field identifier, or empty.
    QString target;
    /// The field's value when the preview was made.
    QString current;
    /// Why the value is not imported, for conflicts, invalid values and unsupported files.
    QString detail;
};

struct Plan {
    QString root;
    QList<Change> changes;
    /// The #rrggbb colour given to tags the plan creates, or empty for none.
    QString tagColor;

    int count(Action action) const;
    /// Whether applying the plan changes anything.
    bool hasChanges() const;
};

/**
 * Previews importing @p readings into @p library. @p mappings maps property
 * keys to tagsTarget() or a field identifier; unmapped properties are ignored.
 * Tags are matched by name, alias or shorthand, ignoring case; other names
 * create one new tag each.
 */
Plan preview(const TagLibrary::Library &library, const QList<EmbeddedMetadata::Reading> &readings, const QMap<QString, QString> &mappings, Policy policy);

struct Result {
    int tagsCreated = 0;
    /// The number of items whose tags changed.
    int itemsTagged = 0;
    /// The number of field values set.
    int valuesSet = 0;
    /// Field values not set because the item's value changed after the preview.
    int changedSincePreview = 0;
    /// The number of items whose tags or field values changed.
    int items = 0;
};

/**
 * Applies the changes of @p plan to its library, indexing items that aren't
 * indexed yet. Tags created by another window in the meantime are reused, and
 * field values that changed since the preview are kept.
 */
bool apply(const Plan &plan, Result *result = nullptr, QString *error = nullptr);

/**
 * Saved mappings, independent of a library: property keys mapped to a field
 * name, or to an empty name to import each value as a tag.
 */
using NamedMappings = QMap<QString, QString>;

/**
 * Reads mappings saved as "key=tags" or "key=field:Name". "tags" and "field:"
 * match ignoring case and spaces around the name are dropped. Entries for
 * unknown keys, with another target or without a name are ignored, as are
 * later entries for a key that is already mapped.
 */
NamedMappings parseMappings(const QStringList &entries);

/// Writes @p mappings in EmbeddedMetadata::keys() order, as parseMappings() reads them; unknown keys are left out.
QStringList serializeMappings(const NamedMappings &mappings);

/// Whether a field of @p type can hold property @p key: text fields hold any property, date fields only dates.
bool canMap(const QString &key, TagLibrary::FieldType type);

/**
 * Resolves @p mappings against @p library into the mappings preview() takes.
 * Fields are found by name, ignoring case, among those whose type can hold
 * the property. Keys not in @p enabled and fields the library doesn't have
 * are left out.
 */
QMap<QString, QString>
resolveMappings(const TagLibrary::Library &library, const NamedMappings &mappings, const QStringList &enabled = EmbeddedMetadata::keys());

/// The keys of @p enabled that @p mappings map, in keys() order: the properties an import with them reads.
QStringList mappedKeys(const NamedMappings &mappings, const QStringList &enabled = EmbeddedMetadata::keys());

/**
 * Reads the properties @p keys of the files at @p relativePaths below @p root
 * and keeps the readings that have values. It can run on a worker thread; it
 * stops when @p canceled returns true and reports the number of files read so
 * far to @p progress.
 */
QList<EmbeddedMetadata::Reading> readFiles(const QString &root,
                                           const QStringList &relativePaths,
                                           const QStringList &keys,
                                           const std::function<bool()> &canceled = {},
                                           const std::function<void(int)> &progress = {});

/**
 * Imports @p readings into the library at @p root without a review, as a
 * library refresh does: @p mappings are resolved against the library with
 * @p enabled, and the changes preview() finds with @p policy are applied, with
 * @p tagColor for new tags. Unreadable files are left out. Like apply(), it
 * uses the library registry, so call it on the thread that owns the
 * configuration.
 */
bool importReadings(const QString &root,
                    const QList<EmbeddedMetadata::Reading> &readings,
                    const NamedMappings &mappings,
                    const QStringList &enabled,
                    Policy policy,
                    const QString &tagColor,
                    Result *result = nullptr,
                    QString *error = nullptr);
}
