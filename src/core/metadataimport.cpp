// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataimport.h"

#include "tagassignment.h"

#include <KLocalizedString>

#include <QDir>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace
{
using MetadataImport::Action;
using MetadataImport::Change;

bool changesLibrary(Action action)
{
    return action == Action::AssignTag || action == Action::CreateTag || action == Action::SetField;
}

void previewTags(const TagLibrary::Library &library,
                 const TagLibrary::Entry *entry,
                 const Change &base,
                 const QStringList &items,
                 QSet<QString> &seen,
                 QList<Change> &changes)
{
    for (const QString &item : items) {
        Change change = base;
        change.value = item.trimmed();
        // Another property, or a repeated value, already imports this tag.
        if (seen.contains(change.value.toCaseFolded())) {
            continue;
        }
        seen.insert(change.value.toCaseFolded());
        const QStringList found = library.findTags(change.value);
        if (!found.isEmpty()) {
            change.target = found.first();
            change.action = entry && entry->tags.contains(change.target) ? Action::Unchanged : Action::AssignTag;
        } else {
            TagLibrary::Tag tag;
            tag.id = QStringLiteral("preview");
            tag.name = change.value;
            change.detail = library.checkTag(tag);
            change.action = change.detail.isEmpty() ? Action::CreateTag : Action::Invalid;
        }
        changes.append(change);
    }
}
}

QString MetadataImport::tagsTarget()
{
    return QStringLiteral(":tags");
}

int MetadataImport::Plan::count(Action action) const
{
    return static_cast<int>(std::count_if(changes.cbegin(), changes.cend(), [action](const Change &change) {
        return change.action == action;
    }));
}

bool MetadataImport::Plan::hasChanges() const
{
    return std::any_of(changes.cbegin(), changes.cend(), [](const Change &change) {
        return changesLibrary(change.action);
    });
}

MetadataImport::Plan MetadataImport::preview(const TagLibrary::Library &library,
                                             const QList<EmbeddedMetadata::Reading> &readings,
                                             const QMap<QString, QString> &mappings,
                                             Policy policy)
{
    Plan plan;
    plan.root = library.root();
    for (const EmbeddedMetadata::Reading &reading : readings) {
        if (!reading.supported) {
            Change change;
            change.path = reading.path;
            change.action = Action::Unsupported;
            change.detail = reading.problem;
            plan.changes.append(change);
            continue;
        }
        const TagLibrary::Entry *entry = library.entry(library.relativePath(reading.path));
        QSet<QString> seenTags;
        QSet<QString> setFields;
        for (const EmbeddedMetadata::Value &value : reading.values) {
            const QString target = mappings.value(value.key);
            if (target.isEmpty()) {
                continue;
            }
            Change change;
            change.path = reading.path;
            change.key = value.key;
            if (target == tagsTarget()) {
                previewTags(library, entry, change, value.items, seenTags, plan.changes);
                continue;
            }
            change.target = target;
            const TagLibrary::Field *field = library.field(target);
            if (!field) {
                change.value = value.text;
                change.action = Action::Invalid;
                change.detail = i18nc("@info", "The field no longer exists.");
                plan.changes.append(change);
                continue;
            }
            if (field->type == TagLibrary::FieldType::Date) {
                change.value = value.date.isValid() ? value.date.toString(Qt::ISODate) : value.text;
            } else {
                change.value = value.text;
            }
            change.current = entry ? entry->fields.value(target) : QString();
            if (setFields.contains(target)) {
                change.action = Action::Conflict;
                change.detail = i18nc("@info", "Another property already sets “%1”.", field->name);
            } else if (field->type == TagLibrary::FieldType::Date && !value.date.isValid()) {
                change.action = Action::Invalid;
                change.detail = i18nc("@info", "“%1” needs a date.", field->name);
            } else if (const QString problem = library.checkFieldValue(target, change.value); !problem.isEmpty()) {
                change.action = Action::Invalid;
                change.detail = problem;
            } else if (change.current == change.value) {
                change.action = Action::Unchanged;
                setFields.insert(target);
            } else if (!change.current.isEmpty() && policy == Policy::Keep) {
                change.action = Action::Conflict;
                change.detail = i18nc("@info", "“%1” already has a different value, which is kept.", field->name);
                setFields.insert(target);
            } else {
                change.action = Action::SetField;
                setFields.insert(target);
            }
            plan.changes.append(change);
        }
    }
    return plan;
}

bool MetadataImport::apply(const Plan &plan, Result *result, QString *error)
{
    QStringList paths;
    QHash<QString, QList<qsizetype>> changesByPath;
    for (qsizetype i = 0; i < plan.changes.size(); ++i) {
        const Change &change = plan.changes.at(i);
        if (!changesLibrary(change.action)) {
            continue;
        }
        if (!changesByPath.contains(change.path)) {
            paths.append(change.path);
        }
        changesByPath[change.path].append(i);
    }
    Result applied;
    if (paths.isEmpty()) {
        if (result) {
            *result = applied;
        }
        return true;
    }
    QList<TagLibrary::Observation> items;
    if (!TagAssignment::observe(plan.root, paths, &items, error)) {
        return false;
    }
    const bool saved = TagLibrary::update(
        plan.root,
        [&](TagLibrary::Library &library, QString *problem) {
            // The library is reloaded when another process saved it first, so start over each time.
            applied = {};
            QHash<QString, QString> created;
            for (const Change &change : plan.changes) {
                const QString name = change.value.toCaseFolded();
                if (change.action != Action::CreateTag || created.contains(name)) {
                    continue;
                }
                const QStringList found = library.findTags(change.value);
                if (!found.isEmpty()) {
                    created.insert(name, found.first());
                    continue;
                }
                TagLibrary::Tag tag;
                tag.id = TagLibrary::createId();
                tag.name = change.value;
                tag.color = plan.tagColor;
                if (!library.setTag(tag, problem)) {
                    return false;
                }
                created.insert(name, tag.id);
                ++applied.tagsCreated;
            }
            for (qsizetype i = 0; i < paths.size(); ++i) {
                const TagLibrary::Entry *entry = library.entry(library.relativePath(paths.at(i)));
                QStringList add;
                QMap<QString, QString> values;
                for (const qsizetype index : changesByPath.value(paths.at(i))) {
                    const Change &change = plan.changes.at(index);
                    if (change.action == Action::CreateTag) {
                        add.append(created.value(change.value.toCaseFolded()));
                    } else if (change.action == Action::AssignTag) {
                        // Tags deleted since the preview are skipped.
                        if (library.tag(change.target)) {
                            add.append(change.target);
                        }
                    } else if (library.field(change.target) && (entry ? entry->fields.value(change.target) : QString()) == change.current) {
                        values.insert(change.target, change.value);
                    } else {
                        ++applied.changedSincePreview;
                    }
                }
                const int tagged = applied.itemsTagged;
                if (!add.isEmpty()) {
                    int changed = 0;
                    if (!library.assignTags({items.at(i)}, add, {}, &changed, problem)) {
                        return false;
                    }
                    applied.itemsTagged += changed;
                }
                if (!values.isEmpty()) {
                    if (!library.setFieldValues({items.at(i)}, values, nullptr, problem)) {
                        return false;
                    }
                    applied.valuesSet += static_cast<int>(values.size());
                }
                if (applied.itemsTagged != tagged || !values.isEmpty()) {
                    ++applied.items;
                }
            }
            return true;
        },
        error);
    TagAssignment::invalidate(plan.root);
    if (saved && result) {
        *result = applied;
    }
    return saved;
}

MetadataImport::NamedMappings MetadataImport::parseMappings(const QStringList &entries)
{
    const QStringList known = EmbeddedMetadata::keys();
    NamedMappings mappings;
    for (const QString &entry : entries) {
        const qsizetype separator = entry.indexOf(QLatin1Char('='));
        if (separator < 0) {
            continue;
        }
        const QString key = entry.left(separator).trimmed();
        const QString target = entry.mid(separator + 1).trimmed();
        if (!known.contains(key) || mappings.contains(key)) {
            continue;
        }
        if (target.compare(QLatin1String("tags"), Qt::CaseInsensitive) == 0) {
            mappings.insert(key, QString());
        } else if (target.startsWith(QLatin1String("field:"), Qt::CaseInsensitive)) {
            const QString name = target.mid(6).trimmed();
            if (!name.isEmpty()) {
                mappings.insert(key, name);
            }
        }
    }
    return mappings;
}

QStringList MetadataImport::serializeMappings(const NamedMappings &mappings)
{
    QStringList entries;
    for (const QString &key : EmbeddedMetadata::keys()) {
        const auto it = mappings.constFind(key);
        if (it == mappings.cend()) {
            continue;
        }
        const QString name = it->trimmed();
        entries.append(name.isEmpty() ? QString(key + QLatin1String("=tags")) : QString(key + QLatin1String("=field:") + name));
    }
    return entries;
}

bool MetadataImport::canMap(const QString &key, TagLibrary::FieldType type)
{
    return type == TagLibrary::FieldType::Text || EmbeddedMetadata::isDate(key);
}

QMap<QString, QString> MetadataImport::resolveMappings(const TagLibrary::Library &library, const NamedMappings &mappings, const QStringList &enabled)
{
    QMap<QString, QString> resolved;
    for (auto it = mappings.cbegin(); it != mappings.cend(); ++it) {
        if (!enabled.contains(it.key())) {
            continue;
        }
        if (it->isEmpty()) {
            resolved.insert(it.key(), tagsTarget());
            continue;
        }
        for (const TagLibrary::Field &field : library.fields()) {
            if (field.name.compare(*it, Qt::CaseInsensitive) == 0 && canMap(it.key(), field.type)) {
                resolved.insert(it.key(), field.id);
                break;
            }
        }
    }
    return resolved;
}

QStringList MetadataImport::mappedKeys(const NamedMappings &mappings, const QStringList &enabled)
{
    QStringList keys;
    for (const QString &key : EmbeddedMetadata::keys()) {
        if (mappings.contains(key) && enabled.contains(key)) {
            keys.append(key);
        }
    }
    return keys;
}

QList<EmbeddedMetadata::Reading> MetadataImport::readFiles(const QString &root,
                                                           const QStringList &relativePaths,
                                                           const QStringList &keys,
                                                           const std::function<bool()> &canceled,
                                                           const std::function<void(int)> &progress)
{
    QList<EmbeddedMetadata::Reading> readings;
    if (keys.isEmpty()) {
        return readings;
    }
    const QDir dir(root);
    int done = 0;
    for (const QString &path : relativePaths) {
        if (canceled && canceled()) {
            break;
        }
        EmbeddedMetadata::Reading reading = EmbeddedMetadata::read(dir.filePath(path), keys);
        if (reading.supported && !reading.values.isEmpty()) {
            readings.append(reading);
        }
        if (progress) {
            progress(++done);
        }
    }
    return readings;
}

bool MetadataImport::importReadings(const QString &root,
                                    const QList<EmbeddedMetadata::Reading> &readings,
                                    const NamedMappings &mappings,
                                    const QStringList &enabled,
                                    Policy policy,
                                    const QString &tagColor,
                                    Result *result,
                                    QString *error)
{
    if (result) {
        *result = {};
    }
    QList<EmbeddedMetadata::Reading> readable;
    for (const EmbeddedMetadata::Reading &reading : readings) {
        if (reading.supported && !reading.values.isEmpty()) {
            readable.append(reading);
        }
    }
    if (readable.isEmpty()) {
        return true;
    }
    const TagLibrary::Library library = TagLibrary::Library::load(root, error);
    if (library.isNull()) {
        return false;
    }
    const QMap<QString, QString> resolved = resolveMappings(library, mappings, enabled);
    if (resolved.isEmpty()) {
        return true;
    }
    Plan plan = preview(library, readable, resolved, policy);
    plan.tagColor = tagColor;
    return apply(plan, result, error);
}
