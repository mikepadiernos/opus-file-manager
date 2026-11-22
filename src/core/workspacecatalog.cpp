// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacecatalog.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

namespace
{
bool validText(const QString &value)
{
    return !value.trimmed().isEmpty() && !value.contains(QChar::Null) && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'));
}

bool validCatalog(const WorkspaceCatalog::Catalog &catalog)
{
    QSet<QString> ids;
    QSet<QString> names;
    for (const auto &workspace : catalog.workspaces) {
        const QUuid id(workspace.id);
        const QString name = workspace.name.trimmed().toCaseFolded();
        if (id.isNull() || id.toString(QUuid::WithoutBraces) != workspace.id || ids.contains(workspace.id) || !validText(workspace.name)
            || !validText(workspace.shortLabel) || names.contains(name)) {
            return false;
        }
        ids.insert(workspace.id);
        names.insert(name);
    }
    return catalog.activeId.isEmpty() || ids.contains(catalog.activeId);
}

bool checkCatalog(const WorkspaceCatalog::Catalog &catalog, QString &error)
{
    if (!validCatalog(catalog)) {
        error = i18nc("@info", "Workspace names must be unique, names and short labels must be valid, and the active workspace must exist.");
        return false;
    }
    error.clear();
    return true;
}

int indexOf(const WorkspaceCatalog::Catalog &catalog, const QString &id)
{
    for (int i = 0; i < catalog.workspaces.size(); ++i) {
        if (catalog.workspaces.at(i).id == id) {
            return i;
        }
    }
    return -1;
}
}

bool WorkspaceCatalog::load(const KConfigGroup &group, Catalog &catalog, QString &error)
{
    if (!group.hasKey("Catalog")) {
        catalog = {};
        error.clear();
        return true;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(group.readEntry("Catalog", QByteArray()), &parseError);
    const QJsonObject root = document.object();
    const QJsonValue version = root.value(QStringLiteral("version"));
    const QJsonValue entries = root.value(QStringLiteral("workspaces"));
    const QJsonValue active = root.value(QStringLiteral("activeId"));
    bool valid = parseError.error == QJsonParseError::NoError && document.isObject() && version.isDouble() && version.toDouble() == 1 && entries.isArray()
        && active.isString();
    Catalog loaded;
    if (valid) {
        loaded.activeId = active.toString();
        for (const auto &entry : entries.toArray()) {
            const QJsonObject object = entry.toObject();
            const QJsonValue id = object.value(QStringLiteral("id"));
            const QJsonValue name = object.value(QStringLiteral("name"));
            const QJsonValue label = object.value(QStringLiteral("shortLabel"));
            if (!entry.isObject() || !id.isString() || !name.isString() || !label.isString()) {
                valid = false;
                break;
            }
            loaded.workspaces.append({id.toString(), name.toString(), label.toString()});
        }
    }
    if (!valid || !validCatalog(loaded)) {
        error = i18nc("@info", "The workspace catalog is invalid and could not be loaded.");
        return false;
    }
    catalog = loaded;
    error.clear();
    return true;
}

bool WorkspaceCatalog::save(KConfigGroup group, const Catalog &catalog, QString &error)
{
    if (!checkCatalog(catalog, error)) {
        return false;
    }
    QJsonArray entries;
    for (const auto &workspace : catalog.workspaces) {
        entries.append(
            QJsonObject{{QStringLiteral("id"), workspace.id}, {QStringLiteral("name"), workspace.name}, {QStringLiteral("shortLabel"), workspace.shortLabel}});
    }
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("workspaces"), entries}, {QStringLiteral("activeId"), catalog.activeId}};
    const bool hadPrevious = group.hasKey("Catalog");
    const QByteArray previous = group.readEntry("Catalog", QByteArray());
    group.writeEntry("Catalog", QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        if (hadPrevious) {
            group.writeEntry("Catalog", previous);
        } else {
            group.deleteEntry("Catalog");
        }
        error = i18nc("@info", "The workspace catalog could not be saved.");
        return false;
    }
    return true;
}

bool WorkspaceCatalog::create(Catalog &catalog, const QString &name, const QString &shortLabel, QString &error)
{
    Catalog updated = catalog;
    updated.workspaces.append({QUuid::createUuid().toString(QUuid::WithoutBraces), name.trimmed(), shortLabel.trimmed()});
    if (!checkCatalog(updated, error)) {
        return false;
    }
    catalog = updated;
    return true;
}

bool WorkspaceCatalog::rename(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error)
{
    const int index = indexOf(catalog, id);
    if (index < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    Catalog updated = catalog;
    updated.workspaces[index].name = name.trimmed();
    updated.workspaces[index].shortLabel = shortLabel.trimmed();
    if (!checkCatalog(updated, error)) {
        return false;
    }
    catalog = updated;
    return true;
}

bool WorkspaceCatalog::activate(Catalog &catalog, const QString &id, QString &error)
{
    if (!checkCatalog(catalog, error)) {
        return false;
    }
    if (indexOf(catalog, id) < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    catalog.activeId = id;
    return true;
}

bool WorkspaceCatalog::remove(Catalog &catalog, const QString &id, QString &error)
{
    if (!checkCatalog(catalog, error)) {
        return false;
    }
    const int index = indexOf(catalog, id);
    if (index < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    catalog.workspaces.removeAt(index);
    if (catalog.activeId == id) {
        catalog.activeId.clear();
    }
    return true;
}
