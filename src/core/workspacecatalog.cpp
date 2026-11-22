// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacecatalog.h"

#include <KConfig>
#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace
{
bool validText(const QString &value)
{
    return !value.trimmed().isEmpty() && !value.contains(QChar::Null) && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'));
}

bool validLayout(const WorkspaceCatalog::Layout &layout)
{
    const auto validSizes = [](const QList<int> &sizes) {
        return std::all_of(sizes.cbegin(), sizes.cend(), [](int size) {
            return size >= 0;
        });
    };
    if (!validSizes(layout.paneSizes) || !validSizes(layout.workspaceSizes)) {
        return false;
    }
    if (!layout.presentation) {
        return true;
    }
    const auto &p = *layout.presentation;
    QSet<QString> objectNames;
    for (const QString &name : p.windowObjects) {
        if (!validText(name) || objectNames.contains(name)) {
            return false;
        }
        objectNames.insert(name);
    }
    return int(p.panePlacement) >= 0 && p.panePlacement <= WorkspaceCatalog::PanePlacement::Window && int(p.panelLayout) >= 0
        && p.panelLayout <= WorkspaceCatalog::PanelLayout::Separate && int(p.previewPlacement) >= 0
        && p.previewPlacement <= WorkspaceCatalog::PreviewPlacement::Right && int(p.editorPlacement) >= 0
        && p.editorPlacement <= WorkspaceCatalog::EditorPlacement::Window && (!p.editorVisible || p.editorPresent);
}

QJsonArray writeSizes(const QList<int> &sizes)
{
    QJsonArray result;
    for (int size : sizes) {
        result.append(size);
    }
    return result;
}

bool readSizes(const QJsonValue &value, QList<int> &sizes)
{
    if (!value.isArray()) {
        return false;
    }
    for (const auto &entry : value.toArray()) {
        const double number = entry.toDouble(-1);
        if (!entry.isDouble() || number < 0 || number > std::numeric_limits<int>::max() || double(int(number)) != number) {
            return false;
        }
        sizes.append(int(number));
    }
    return true;
}

QJsonObject writeLayout(const WorkspaceCatalog::Layout &layout)
{
    QJsonObject object{{QStringLiteral("windowState"), QString::fromLatin1(layout.windowState.toBase64())},
                       {QStringLiteral("paneSizes"), writeSizes(layout.paneSizes)},
                       {QStringLiteral("workspaceSizes"), writeSizes(layout.workspaceSizes)},
                       {QStringLiteral("dual"), layout.dual},
                       {QStringLiteral("stacked"), layout.stacked}};
    if (layout.presentation) {
        const auto &p = *layout.presentation;
        object.insert(QStringLiteral("presentation"),
                      QJsonObject{{QStringLiteral("panePlacement"), int(p.panePlacement)},
                                  {QStringLiteral("panelLayout"), int(p.panelLayout)},
                                  {QStringLiteral("previewPlacement"), int(p.previewPlacement)},
                                  {QStringLiteral("editorPlacement"), int(p.editorPlacement)},
                                  {QStringLiteral("previewVisible"), p.previewVisible},
                                  {QStringLiteral("editorVisible"), p.editorVisible},
                                  {QStringLiteral("editorPresent"), p.editorPresent},
                                  {QStringLiteral("terminalVisible"), p.terminalVisible},
                                  {QStringLiteral("windowObjects"), QJsonArray::fromStringList(p.windowObjects)}});
    }
    return object;
}

template<typename Enum>
bool readEnum(const QJsonValue &value, Enum maximum, Enum &result)
{
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 0 || number > int(maximum) || int(number) != number) {
        return false;
    }
    result = static_cast<Enum>(int(number));
    return true;
}

bool readPresentation(const QJsonValue &value, WorkspaceCatalog::Presentation &p)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const auto previewVisible = object.value(QStringLiteral("previewVisible"));
    const auto editorVisible = object.value(QStringLiteral("editorVisible"));
    const auto editorPresent = object.value(QStringLiteral("editorPresent"));
    const auto terminalVisible = object.value(QStringLiteral("terminalVisible"));
    const auto windowObjects = object.value(QStringLiteral("windowObjects"));
    if (!readEnum(object.value(QStringLiteral("panePlacement")), WorkspaceCatalog::PanePlacement::Window, p.panePlacement)
        || !readEnum(object.value(QStringLiteral("panelLayout")), WorkspaceCatalog::PanelLayout::Separate, p.panelLayout)
        || !readEnum(object.value(QStringLiteral("previewPlacement")), WorkspaceCatalog::PreviewPlacement::Right, p.previewPlacement)
        || !readEnum(object.value(QStringLiteral("editorPlacement")), WorkspaceCatalog::EditorPlacement::Window, p.editorPlacement) || !previewVisible.isBool()
        || !editorVisible.isBool() || !editorPresent.isBool() || !terminalVisible.isBool() || !windowObjects.isArray()) {
        return false;
    }
    for (const auto &name : windowObjects.toArray()) {
        if (!name.isString() || !validText(name.toString()) || p.windowObjects.contains(name.toString())) {
            return false;
        }
        p.windowObjects.append(name.toString());
    }
    p.previewVisible = previewVisible.toBool();
    p.editorVisible = editorVisible.toBool();
    p.editorPresent = editorPresent.toBool();
    p.terminalVisible = terminalVisible.toBool();
    return !p.editorVisible || p.editorPresent;
}

bool readLayout(const QJsonValue &value, WorkspaceCatalog::Layout &layout)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue state = object.value(QStringLiteral("windowState"));
    const QJsonValue dual = object.value(QStringLiteral("dual"));
    const QJsonValue stacked = object.value(QStringLiteral("stacked"));
    if (!state.isString() || !dual.isBool() || !stacked.isBool() || !readSizes(object.value(QStringLiteral("paneSizes")), layout.paneSizes)
        || !readSizes(object.value(QStringLiteral("workspaceSizes")), layout.workspaceSizes)) {
        return false;
    }
    const auto decoded = QByteArray::fromBase64Encoding(state.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) {
        return false;
    }
    layout.windowState = decoded.decoded;
    layout.dual = dual.toBool();
    layout.stacked = stacked.toBool();
    const auto presentation = object.value(QStringLiteral("presentation"));
    if (!presentation.isUndefined()) {
        WorkspaceCatalog::Presentation parsed;
        if (!readPresentation(presentation, parsed)) {
            return false;
        }
        layout.presentation = parsed;
    }
    return true;
}

bool validCatalog(const WorkspaceCatalog::Catalog &catalog)
{
    QSet<QString> ids;
    QSet<QString> names;
    for (const auto &workspace : catalog.workspaces) {
        const QUuid id(workspace.id);
        const QString name = workspace.name.trimmed().toCaseFolded();
        if (id.isNull() || id.toString(QUuid::WithoutBraces) != workspace.id || ids.contains(workspace.id) || !validText(workspace.name)
            || !validText(workspace.shortLabel) || names.contains(name) || (workspace.layout && !validLayout(*workspace.layout))) {
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
    const double versionNumber = version.toDouble();
    bool valid = parseError.error == QJsonParseError::NoError && document.isObject() && version.isDouble()
        && (versionNumber == 1 || versionNumber == 2 || versionNumber == 3) && entries.isArray() && active.isString();
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
            Workspace workspace{id.toString(), name.toString(), label.toString(), std::nullopt};
            const QJsonValue layout = object.value(QStringLiteral("layout"));
            if (!layout.isUndefined()) {
                Layout decoded;
                if (versionNumber == 1 || !readLayout(layout, decoded) || (versionNumber == 2 && decoded.presentation)) {
                    valid = false;
                    break;
                }
                workspace.layout = decoded;
            }
            loaded.workspaces.append(workspace);
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
        QJsonObject entry{{QStringLiteral("id"), workspace.id}, {QStringLiteral("name"), workspace.name}, {QStringLiteral("shortLabel"), workspace.shortLabel}};
        if (workspace.layout) {
            entry.insert(QStringLiteral("layout"), writeLayout(*workspace.layout));
        }
        entries.append(entry);
    }
    const QJsonObject root{{QStringLiteral("version"), 3}, {QStringLiteral("workspaces"), entries}, {QStringLiteral("activeId"), catalog.activeId}};
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

bool WorkspaceCatalog::saveIfUnchanged(KConfigGroup group, const Catalog &expected, const Catalog &updated, QString &error)
{
    if (!checkCatalog(updated, error)) {
        return false;
    }
    KConfig *config = group.config();
    if (config->isDirty()) {
        error = i18nc("@info", "The workspace catalog cannot be updated while configuration changes are pending.");
        return false;
    }
    QString path = config->name();
    if (path.isEmpty()) {
        error = i18nc("@info", "The workspace catalog has no configuration file.");
        return false;
    }
    if (QDir::isRelativePath(path)) {
        path = QDir(QStandardPaths::writableLocation(config->locationType())).filePath(path);
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        error = i18nc("@info", "The workspace configuration directory could not be created.");
        return false;
    }
    QLockFile lock(path + QStringLiteral(".workspaces.lock"));
    if (!lock.tryLock(1000)) {
        error = i18nc("@info", "The workspace catalog is being updated by another process or cannot be locked.");
        return false;
    }
    config->reparseConfiguration();
    Catalog current;
    if (!load(group, current, error)) {
        return false;
    }
    if (current != expected) {
        error = i18nc("@info", "The workspace catalog changed in another window. Reload it and try again.");
        return false;
    }
    if (!save(group, updated, error)) {
        group.markAsClean();
        return false;
    }
    return true;
}

bool WorkspaceCatalog::create(Catalog &catalog, const QString &name, const QString &shortLabel, QString &error)
{
    Catalog updated = catalog;
    updated.workspaces.append({QUuid::createUuid().toString(QUuid::WithoutBraces), name.trimmed(), shortLabel.trimmed(), std::nullopt});
    if (!checkCatalog(updated, error)) {
        return false;
    }
    catalog = updated;
    return true;
}

bool WorkspaceCatalog::duplicate(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error)
{
    if (!checkCatalog(catalog, error)) {
        return false;
    }
    const int index = indexOf(catalog, id);
    if (index < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    Catalog updated = catalog;
    Workspace copy = updated.workspaces.at(index);
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name = name.trimmed();
    copy.shortLabel = shortLabel.trimmed();
    updated.workspaces.append(copy);
    if (!checkCatalog(updated, error)) {
        return false;
    }
    catalog = updated;
    return true;
}

bool WorkspaceCatalog::setLayout(Catalog &catalog, const QString &id, const Layout &layout, QString &error)
{
    const int index = indexOf(catalog, id);
    if (index < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    Catalog updated = catalog;
    updated.workspaces[index].layout = layout;
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
