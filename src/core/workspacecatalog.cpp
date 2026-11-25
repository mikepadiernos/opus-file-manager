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
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace
{
bool validText(const QString &value)
{
    return !value.trimmed().isEmpty() && !value.contains(QChar::Null) && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'));
}

bool validId(const QString &value)
{
    const QUuid id(value);
    return !id.isNull() && id.toString(QUuid::WithoutBraces) == value;
}

bool validPaneSession(const PaneSession &pane);

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
    if (layout.session
        && (layout.session->activePane < 0 || layout.session->activePane > 1 || !validPaneSession(layout.session->left)
            || !validPaneSession(layout.session->right))) {
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
    if (p.toolbarActions && (p.toolbarActions->size() > 256 || !std::all_of(p.toolbarActions->cbegin(), p.toolbarActions->cend(), validText))) {
        return false;
    }
    for (auto it = p.paneToolbars.cbegin(); it != p.paneToolbars.cend(); ++it) {
        if ((it.key() != QLatin1String("Left") && it.key() != QLatin1String("Right")) || it->actions.size() > 256 || it->visible.size() > 256
            || it->hidden.size() > 256 || !std::all_of(it->actions.cbegin(), it->actions.cend(), validText)
            || !std::all_of(it->visible.cbegin(), it->visible.cend(), validText) || !std::all_of(it->hidden.cbegin(), it->hidden.cend(), validText)) {
            return false;
        }
    }
    const bool validPresentation = int(p.panePlacement) >= 0 && p.panePlacement <= WorkspaceCatalog::PanePlacement::Window && int(p.panelLayout) >= 0
        && p.panelLayout <= WorkspaceCatalog::PanelLayout::Separate && int(p.previewPlacement) >= 0
        && p.previewPlacement <= WorkspaceCatalog::PreviewPlacement::Right && int(p.editorPlacement) >= 0
        && p.editorPlacement <= WorkspaceCatalog::EditorPlacement::Window && p.terminalPlacement >= WorkspaceCatalog::TerminalPlacement::Below
        && p.terminalPlacement <= WorkspaceCatalog::TerminalPlacement::Right && p.detachedPaneGeometry.width() >= 0 && p.detachedPaneGeometry.height() >= 0
        && ((p.detachedPaneGeometry.width() == 0) == (p.detachedPaneGeometry.height() == 0)) && (!p.editorVisible || p.editorPresent);
    if (!validPresentation) {
        return false;
    }
    return true;
}

bool validReferenceIds(const QStringList &ids)
{
    QSet<QString> seen;
    for (const QString &id : ids) {
        if (!validId(id) || seen.contains(id)) {
            return false;
        }
        seen.insert(id);
    }
    return true;
}

bool readReferenceIds(const QJsonValue &value, QStringList &ids)
{
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isArray()) {
        return false;
    }
    for (const QJsonValue &entry : value.toArray()) {
        if (!entry.isString()) {
            return false;
        }
        ids.append(entry.toString());
    }
    return validReferenceIds(ids);
}

QUrl sanitizedWorkspaceUrl(const QUrl &url)
{
    QUrl sanitized = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveFragment);
    QUrlQuery query(sanitized);
    static const QRegularExpression credentialParameter(QStringLiteral(R"((?:password|username|credential|secret|token|api[_-]?key|private[_-]?key))"),
                                                        QRegularExpression::CaseInsensitiveOption);
    const auto items = query.queryItems(QUrl::FullyDecoded);
    for (const auto &[key, value] : items) {
        Q_UNUSED(value)
        if (credentialParameter.match(key).hasMatch()) {
            query.removeAllQueryItems(key);
        }
    }
    sanitized.setQuery(query);
    return sanitized;
}

QJsonObject writeBehaviorSelection(const WorkspaceCatalog::BehaviorSelection &selection)
{
    return {{QStringLiteral("mode"), int(selection.mode)},
            {QStringLiteral("locationPresentation"), selection.locationPresentation},
            {QStringLiteral("searchScope"), selection.searchScope},
            {QStringLiteral("layout"), selection.layout},
            {QStringLiteral("paneTabRestoration"), selection.paneTabRestoration},
            {QStringLiteral("appearance"), selection.appearance}};
}

bool readBehaviorSelection(const QJsonValue &value, WorkspaceCatalog::BehaviorSelection &selection)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue mode = object.value(QStringLiteral("mode"));
    const QJsonValue locationPresentation = object.value(QStringLiteral("locationPresentation"));
    const QJsonValue searchScope = object.value(QStringLiteral("searchScope"));
    const QJsonValue layout = object.value(QStringLiteral("layout"));
    const QJsonValue paneTabRestoration = object.value(QStringLiteral("paneTabRestoration"));
    // Catalogs before version 8 have no appearance choice; attached appearance applies by default.
    const QJsonValue appearance = object.value(QStringLiteral("appearance"));
    const double modeNumber = mode.toDouble(-1);
    if (!mode.isDouble() || modeNumber < 0 || modeNumber > int(WorkspaceCatalog::BehaviorMode::None) || int(modeNumber) != modeNumber
        || !locationPresentation.isBool() || !searchScope.isBool() || !layout.isBool() || !paneTabRestoration.isBool()
        || !(appearance.isBool() || appearance.isUndefined())) {
        return false;
    }
    selection.mode = static_cast<WorkspaceCatalog::BehaviorMode>(int(modeNumber));
    selection.locationPresentation = locationPresentation.toBool();
    selection.searchScope = searchScope.toBool();
    selection.layout = layout.toBool();
    selection.paneTabRestoration = paneTabRestoration.toBool();
    selection.appearance = appearance.toBool(true);
    return true;
}

constexpr qsizetype MaximumAppearanceOverrides = 32;
constexpr qsizetype MaximumAppearanceText = 4096;
constexpr qsizetype MaximumAppearanceList = 100;

/**
 * Checks the shape and size of attached appearance. Values are checked
 * against the settings when they are applied, so unknown or currently
 * invalid entries are kept rather than rejecting the catalog.
 */
bool validAppearance(const WorkspaceCatalog::Appearance &appearance)
{
    if ((!appearance.themeProfileId.isEmpty() && !validId(appearance.themeProfileId)) || appearance.overrides.size() > MaximumAppearanceOverrides) {
        return false;
    }
    for (auto it = appearance.overrides.constBegin(); it != appearance.overrides.constEnd(); ++it) {
        if (!validText(it.key()) || it.key().size() > 64) {
            return false;
        }
        const QJsonValue value = it.value();
        if (value.isArray()) {
            const QJsonArray list = value.toArray();
            if (list.size() > MaximumAppearanceList || !std::all_of(list.cbegin(), list.cend(), [](const QJsonValue &entry) {
                    return entry.isString() && entry.toString().size() <= MaximumAppearanceText;
                })) {
                return false;
            }
        } else if (!value.isBool() && !value.isDouble() && !(value.isString() && value.toString().size() <= MaximumAppearanceText)) {
            return false;
        }
    }
    return true;
}

QJsonObject writeAppearance(const WorkspaceCatalog::Appearance &appearance)
{
    QJsonObject object{{QStringLiteral("overrides"), appearance.overrides}};
    if (!appearance.themeProfileId.isEmpty()) {
        object.insert(QStringLiteral("themeProfileId"), appearance.themeProfileId);
    }
    return object;
}

bool readAppearance(const QJsonValue &value, WorkspaceCatalog::Appearance &appearance)
{
    if (value.isUndefined()) {
        return true;
    }
    const QJsonValue profile = value.toObject().value(QStringLiteral("themeProfileId"));
    const QJsonValue overrides = value.toObject().value(QStringLiteral("overrides"));
    if (!value.isObject() || !(profile.isString() || profile.isUndefined()) || !(overrides.isObject() || overrides.isUndefined())) {
        return false;
    }
    appearance.themeProfileId = profile.toString();
    appearance.overrides = overrides.toObject();
    return validAppearance(appearance);
}

QJsonArray writeSizes(const QList<int> &sizes)
{
    QJsonArray result;
    for (int size : sizes) {
        result.append(size);
    }
    return result;
}

QJsonArray writeRect(const QRect &rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}

bool readRect(const QJsonValue &value, QRect &rect)
{
    if (!value.isArray() || value.toArray().size() != 4) {
        return false;
    }
    QList<int> values;
    for (const QJsonValue &entry : value.toArray()) {
        const double number = entry.toDouble(std::numeric_limits<double>::quiet_NaN());
        if (!entry.isDouble() || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max() || int(number) != number) {
            return false;
        }
        values.append(int(number));
    }
    if (values.at(2) < 0 || values.at(3) < 0 || ((values.at(2) == 0) != (values.at(3) == 0))) {
        return false;
    }
    rect = QRect(values.at(0), values.at(1), values.at(2), values.at(3));
    return true;
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

QJsonObject writePaneSession(const PaneSession &pane)
{
    QJsonArray tabs;
    for (const QUrl &url : pane.tabs) {
        tabs.append(sanitizedWorkspaceUrl(url).toString(QUrl::FullyEncoded));
    }
    QJsonArray lockedTabs;
    for (int index : pane.lockedTabs) {
        lockedTabs.append(index);
    }
    return {{QStringLiteral("tabs"), tabs},
            {QStringLiteral("currentTab"), pane.currentTab},
            {QStringLiteral("pinnedCount"), pane.pinnedCount},
            {QStringLiteral("lockedTabs"), lockedTabs},
            {QStringLiteral("names"), QJsonArray::fromStringList(pane.names)},
            {QStringLiteral("filterText"), pane.filterText},
            {QStringLiteral("filterScope"), pane.filterScope},
            {QStringLiteral("locationPresentation"), pane.locationPresentation},
            {QStringLiteral("viewMode"), pane.viewMode}};
}

bool readPaneSession(const QJsonValue &value, PaneSession &pane, bool *credentialsScrubbed = nullptr)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue tabs = object.value(QStringLiteral("tabs"));
    const QJsonValue currentTab = object.value(QStringLiteral("currentTab"));
    const QJsonValue pinnedCount = object.value(QStringLiteral("pinnedCount"));
    const QJsonValue lockedTabs = object.value(QStringLiteral("lockedTabs"));
    const QJsonValue names = object.value(QStringLiteral("names"));
    const QJsonValue filterText = object.value(QStringLiteral("filterText"));
    const QJsonValue filterScope = object.value(QStringLiteral("filterScope"));
    const QJsonValue locationPresentation = object.value(QStringLiteral("locationPresentation"));
    const QJsonValue viewMode = object.value(QStringLiteral("viewMode"));
    const auto validIndex = [](const QJsonValue &entry, int maximum) {
        const double number = entry.toDouble(-1);
        return entry.isDouble() && number >= 0 && number <= maximum && int(number) == number;
    };
    if (!tabs.isArray() || !currentTab.isDouble() || !pinnedCount.isDouble() || !lockedTabs.isArray() || !names.isArray() || !filterText.isString()
        || !filterScope.isDouble() || !locationPresentation.isDouble() || !viewMode.isDouble()) {
        return false;
    }
    for (const QJsonValue &entry : tabs.toArray()) {
        if (!entry.isString()) {
            return false;
        }
        const QUrl url(entry.toString(), QUrl::StrictMode);
        if (!url.isValid() || url.isEmpty()) {
            return false;
        }
        const QUrl sanitized = sanitizedWorkspaceUrl(url);
        if (credentialsScrubbed && sanitized != url) {
            *credentialsScrubbed = true;
        }
        pane.tabs.append(sanitized);
    }
    if (pane.tabs.isEmpty() || !validIndex(currentTab, pane.tabs.size() - 1) || !validIndex(pinnedCount, pane.tabs.size())
        || names.toArray().size() > pane.tabs.size()) {
        return false;
    }
    for (const QJsonValue &entry : lockedTabs.toArray()) {
        if (!validIndex(entry, pane.tabs.size() - 1) || pane.lockedTabs.contains(entry.toInt())) {
            return false;
        }
        pane.lockedTabs.append(entry.toInt());
    }
    for (const QJsonValue &entry : names.toArray()) {
        if (!entry.isString()) {
            return false;
        }
        pane.names.append(entry.toString());
    }
    const double scope = filterScope.toDouble(-2);
    const double presentation = locationPresentation.toDouble(-2);
    const double mode = viewMode.toDouble(-1);
    if (scope < -1 || scope > 2 || int(scope) != scope || presentation < -1 || presentation > 2 || int(presentation) != presentation || mode < 0
        || mode > PaneSession::MaximumViewMode || int(mode) != mode) {
        return false;
    }
    pane.currentTab = currentTab.toInt();
    pane.pinnedCount = pinnedCount.toInt();
    pane.filterText = filterText.toString();
    pane.filterScope = int(scope);
    pane.locationPresentation = int(presentation);
    pane.viewMode = int(mode);
    return true;
}

bool validPaneSession(const PaneSession &pane)
{
    if (pane.tabs.isEmpty() || pane.currentTab < 0 || pane.currentTab >= pane.tabs.size() || pane.pinnedCount < 0 || pane.pinnedCount > pane.tabs.size()
        || pane.names.size() > pane.tabs.size() || pane.filterScope < -1 || pane.filterScope > 2 || pane.viewMode < 0
        || pane.viewMode > PaneSession::MaximumViewMode) {
        return false;
    }
    if (pane.locationPresentation < -1 || pane.locationPresentation > 2) {
        return false;
    }
    for (const QUrl &url : pane.tabs) {
        if (!url.isValid() || url.isEmpty()) {
            return false;
        }
    }
    QSet<int> lockedTabs;
    for (int index : pane.lockedTabs) {
        if (index < 0 || index >= pane.tabs.size() || lockedTabs.contains(index)) {
            return false;
        }
        lockedTabs.insert(index);
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
    if (layout.session) {
        object.insert(QStringLiteral("session"),
                      QJsonObject{{QStringLiteral("left"), writePaneSession(layout.session->left)},
                                  {QStringLiteral("right"), writePaneSession(layout.session->right)},
                                  {QStringLiteral("activePane"), layout.session->activePane}});
    }
    if (layout.presentation) {
        const auto &p = *layout.presentation;
        QJsonObject presentation{{QStringLiteral("panePlacement"), int(p.panePlacement)},
                                 {QStringLiteral("panelLayout"), int(p.panelLayout)},
                                 {QStringLiteral("previewPlacement"), int(p.previewPlacement)},
                                 {QStringLiteral("editorPlacement"), int(p.editorPlacement)},
                                 {QStringLiteral("previewVisible"), p.previewVisible},
                                 {QStringLiteral("editorVisible"), p.editorVisible},
                                 {QStringLiteral("editorPresent"), p.editorPresent},
                                 {QStringLiteral("terminalVisible"), p.terminalVisible},
                                 {QStringLiteral("terminalPlacement"), int(p.terminalPlacement)},
                                 {QStringLiteral("detachedPaneGeometry"), writeRect(p.detachedPaneGeometry)},
                                 {QStringLiteral("windowObjects"), QJsonArray::fromStringList(p.windowObjects)}};
        if (p.toolbarActions) {
            presentation.insert(QStringLiteral("toolbarActions"), QJsonArray::fromStringList(*p.toolbarActions));
        }
        QJsonObject paneToolbars;
        for (auto it = p.paneToolbars.cbegin(); it != p.paneToolbars.cend(); ++it) {
            paneToolbars.insert(it.key(),
                                QJsonObject{{QStringLiteral("actions"), QJsonArray::fromStringList(it->actions)},
                                            {QStringLiteral("visible"), QJsonArray::fromStringList(it->visible)},
                                            {QStringLiteral("hidden"), QJsonArray::fromStringList(it->hidden)}});
        }
        if (!paneToolbars.isEmpty()) {
            presentation.insert(QStringLiteral("paneToolbars"), paneToolbars);
        }
        object.insert(QStringLiteral("presentation"), presentation);
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
    const auto terminalPlacement = object.value(QStringLiteral("terminalPlacement"));
    const auto detachedPaneGeometry = object.value(QStringLiteral("detachedPaneGeometry"));
    const auto windowObjects = object.value(QStringLiteral("windowObjects"));
    const auto toolbarActions = object.value(QStringLiteral("toolbarActions"));
    const auto paneToolbars = object.value(QStringLiteral("paneToolbars"));
    if (!readEnum(object.value(QStringLiteral("panePlacement")), WorkspaceCatalog::PanePlacement::Window, p.panePlacement)
        || !readEnum(object.value(QStringLiteral("panelLayout")), WorkspaceCatalog::PanelLayout::Separate, p.panelLayout)
        || !readEnum(object.value(QStringLiteral("previewPlacement")), WorkspaceCatalog::PreviewPlacement::Right, p.previewPlacement)
        || !readEnum(object.value(QStringLiteral("editorPlacement")), WorkspaceCatalog::EditorPlacement::Window, p.editorPlacement) || !previewVisible.isBool()
        || !editorVisible.isBool() || !editorPresent.isBool() || !terminalVisible.isBool() || !windowObjects.isArray()
        || (!terminalPlacement.isUndefined() && !readEnum(terminalPlacement, WorkspaceCatalog::TerminalPlacement::Right, p.terminalPlacement))
        || (!detachedPaneGeometry.isUndefined() && !readRect(detachedPaneGeometry, p.detachedPaneGeometry))) {
        return false;
    }
    for (const auto &name : windowObjects.toArray()) {
        if (!name.isString() || !validText(name.toString()) || p.windowObjects.contains(name.toString())) {
            return false;
        }
        p.windowObjects.append(name.toString());
    }
    const auto readToolbarList = [](const QJsonValue &value, QStringList &result) {
        if (!value.isArray() || value.toArray().size() > 256) {
            return false;
        }
        for (const QJsonValue &entry : value.toArray()) {
            if (!entry.isString() || !validText(entry.toString())) {
                return false;
            }
            result.append(entry.toString());
        }
        return true;
    };
    if (!toolbarActions.isUndefined()) {
        QStringList actions;
        if (!readToolbarList(toolbarActions, actions)) {
            return false;
        }
        p.toolbarActions = actions;
    }
    if (!paneToolbars.isUndefined()) {
        if (!paneToolbars.isObject()) {
            return false;
        }
        const QJsonObject toolbars = paneToolbars.toObject();
        for (auto it = toolbars.constBegin(); it != toolbars.constEnd(); ++it) {
            if (it.key() != QLatin1String("Left") && it.key() != QLatin1String("Right")) {
                return false;
            }
            const QJsonObject toolbar = it.value().toObject();
            WorkspaceCatalog::PaneToolbarState state;
            if (!it.value().isObject() || !readToolbarList(toolbar.value(QStringLiteral("actions")), state.actions)
                || !readToolbarList(toolbar.value(QStringLiteral("visible")), state.visible)
                || !readToolbarList(toolbar.value(QStringLiteral("hidden")), state.hidden)) {
                return false;
            }
            p.paneToolbars.insert(it.key(), state);
        }
    }
    p.previewVisible = previewVisible.toBool();
    p.editorVisible = editorVisible.toBool();
    p.editorPresent = editorPresent.toBool();
    p.terminalVisible = terminalVisible.toBool();
    return !p.editorVisible || p.editorPresent;
}

bool readLayout(const QJsonValue &value, WorkspaceCatalog::Layout &layout, bool *credentialsScrubbed = nullptr)
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
    const auto session = object.value(QStringLiteral("session"));
    if (!session.isUndefined()) {
        if (!session.isObject()) {
            return false;
        }
        const QJsonObject sessionObject = session.toObject();
        const QJsonValue activePane = sessionObject.value(QStringLiteral("activePane"));
        const double activePaneNumber = activePane.toDouble(-1);
        WindowSession parsed;
        if (!readPaneSession(sessionObject.value(QStringLiteral("left")), parsed.left, credentialsScrubbed)
            || !readPaneSession(sessionObject.value(QStringLiteral("right")), parsed.right, credentialsScrubbed) || !activePane.isDouble()
            || activePaneNumber < 0 || activePaneNumber > 1 || int(activePaneNumber) != activePaneNumber) {
            return false;
        }
        parsed.activePane = int(activePaneNumber);
        parsed.dual = layout.dual;
        parsed.stacked = layout.stacked;
        layout.session = parsed;
    }
    return true;
}

bool validCatalog(const WorkspaceCatalog::Catalog &catalog)
{
    QSet<QString> ids;
    QSet<QString> names;
    for (const auto &workspace : catalog.workspaces) {
        const QString name = workspace.name.trimmed().toCaseFolded();
        if (!validId(workspace.id) || ids.contains(workspace.id) || !validText(workspace.name) || !validText(workspace.shortLabel) || names.contains(name)
            || (workspace.layout && !validLayout(*workspace.layout)) || !validReferenceIds(workspace.savedSearchIds)
            || !validReferenceIds(workspace.folderSyncProfileIds) || workspace.behaviorSelection.mode < WorkspaceCatalog::BehaviorMode::All
            || workspace.behaviorSelection.mode > WorkspaceCatalog::BehaviorMode::None || !validAppearance(workspace.appearance)) {
            return false;
        }
        ids.insert(workspace.id);
        names.insert(name);
    }
    if (!catalog.activeId.isEmpty() && !ids.contains(catalog.activeId)) {
        return false;
    }
    for (auto it = catalog.locationAssignments.cbegin(); it != catalog.locationAssignments.cend(); ++it) {
        if (!validId(it.key()) || !ids.contains(it.value())) {
            return false;
        }
    }
    return true;
}

bool checkCatalog(const WorkspaceCatalog::Catalog &catalog, QString &error)
{
    if (!validCatalog(catalog)) {
        error = i18nc("@info", "Workspace names must be unique, names, short labels and assignments must be valid, and referenced workspaces must exist.");
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
    const QJsonValue assignments = root.value(QStringLiteral("locationAssignments"));
    const double versionNumber = version.toDouble();
    bool valid = parseError.error == QJsonParseError::NoError && document.isObject() && version.isDouble() && (versionNumber >= 1 && versionNumber <= 8)
        && entries.isArray() && active.isString() && (versionNumber >= 4 ? assignments.isObject() : assignments.isUndefined());
    Catalog loaded;
    bool credentialsScrubbed = false;
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
            Workspace workspace{id.toString(), name.toString(), label.toString(), std::nullopt, {}, {}, {}, {}};
            if (!readReferenceIds(object.value(QStringLiteral("savedSearchIds")), workspace.savedSearchIds)
                || !readReferenceIds(object.value(QStringLiteral("folderSyncProfileIds")), workspace.folderSyncProfileIds)) {
                valid = false;
                break;
            }
            if (versionNumber >= 6 && !readBehaviorSelection(object.value(QStringLiteral("behaviorSelection")), workspace.behaviorSelection)) {
                valid = false;
                break;
            }
            if (versionNumber >= 8 && !readAppearance(object.value(QStringLiteral("appearance")), workspace.appearance)) {
                valid = false;
                break;
            }
            const QJsonValue layout = object.value(QStringLiteral("layout"));
            if (!layout.isUndefined()) {
                Layout decoded;
                if (versionNumber == 1 || !readLayout(layout, decoded, &credentialsScrubbed) || (versionNumber == 2 && decoded.presentation)) {
                    valid = false;
                    break;
                }
                workspace.layout = decoded;
            }
            loaded.workspaces.append(workspace);
        }
    }
    if (valid && versionNumber >= 4) {
        const QJsonObject storedAssignments = assignments.toObject();
        for (auto it = storedAssignments.constBegin(); it != storedAssignments.constEnd(); ++it) {
            if (!it.value().isString()) {
                valid = false;
                break;
            }
            loaded.locationAssignments.insert(it.key(), it.value().toString());
        }
    }
    if (!valid || !validCatalog(loaded)) {
        error = i18nc("@info", "The workspace catalog is invalid and could not be loaded.");
        return false;
    }
    if (credentialsScrubbed && !save(group, loaded, error)) {
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
        QJsonObject entry{{QStringLiteral("id"), workspace.id},
                          {QStringLiteral("name"), workspace.name},
                          {QStringLiteral("shortLabel"), workspace.shortLabel},
                          {QStringLiteral("savedSearchIds"), QJsonArray::fromStringList(workspace.savedSearchIds)},
                          {QStringLiteral("folderSyncProfileIds"), QJsonArray::fromStringList(workspace.folderSyncProfileIds)},
                          {QStringLiteral("behaviorSelection"), writeBehaviorSelection(workspace.behaviorSelection)}};
        if (workspace.layout) {
            entry.insert(QStringLiteral("layout"), writeLayout(*workspace.layout));
        }
        if (!workspace.appearance.isEmpty()) {
            entry.insert(QStringLiteral("appearance"), writeAppearance(workspace.appearance));
        }
        entries.append(entry);
    }
    QJsonObject assignments;
    for (auto it = catalog.locationAssignments.cbegin(); it != catalog.locationAssignments.cend(); ++it) {
        assignments.insert(it.key(), it.value());
    }
    const QJsonObject root{{QStringLiteral("version"), 8},
                           {QStringLiteral("workspaces"), entries},
                           {QStringLiteral("activeId"), catalog.activeId},
                           {QStringLiteral("locationAssignments"), assignments}};
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
    updated.workspaces.append({QUuid::createUuid().toString(QUuid::WithoutBraces), name.trimmed(), shortLabel.trimmed(), std::nullopt, {}, {}, {}, {}});
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

bool WorkspaceCatalog::setAppearance(Catalog &catalog, const QString &id, const Appearance &appearance, QString &error)
{
    const int index = indexOf(catalog, id);
    if (index < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    if (!validAppearance(appearance)) {
        error = i18nc("@info", "The workspace appearance options are invalid.");
        return false;
    }
    Catalog updated = catalog;
    updated.workspaces[index].appearance = appearance;
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
    for (auto it = catalog.locationAssignments.begin(); it != catalog.locationAssignments.end();) {
        if (it.value() == id) {
            it = catalog.locationAssignments.erase(it);
        } else {
            ++it;
        }
    }
    if (catalog.activeId == id) {
        catalog.activeId.clear();
    }
    return true;
}

QString WorkspaceCatalog::locationId(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || url.isRelative()) {
        return {};
    }
    const auto normalized = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash);
    static const QUuid locationNamespace(QStringLiteral("b880d3a6-10b1-42b8-a4fa-05b525d0c3f6"));
    return QUuid::createUuidV5(locationNamespace, normalized.toEncoded()).toString(QUuid::WithoutBraces);
}

bool WorkspaceCatalog::assignLocation(Catalog &catalog, const QString &locationId, const QString &workspaceId, QString &error)
{
    if (!checkCatalog(catalog, error)) {
        return false;
    }
    if (!validId(locationId)) {
        error = i18nc("@info", "The saved location identifier is invalid.");
        return false;
    }
    if (indexOf(catalog, workspaceId) < 0) {
        error = i18nc("@info", "The workspace no longer exists.");
        return false;
    }
    const QString owner = catalog.locationAssignments.value(locationId);
    if (!owner.isEmpty() && owner != workspaceId) {
        error = i18nc("@info", "This location is already assigned to another workspace.");
        return false;
    }
    catalog.locationAssignments.insert(locationId, workspaceId);
    error.clear();
    return true;
}

bool WorkspaceCatalog::unassignLocation(Catalog &catalog, const QString &locationId, QString &error)
{
    if (!checkCatalog(catalog, error)) {
        return false;
    }
    if (!validId(locationId) || !catalog.locationAssignments.contains(locationId)) {
        error = i18nc("@info", "The saved location is not assigned to a workspace.");
        return false;
    }
    catalog.locationAssignments.remove(locationId);
    error.clear();
    return true;
}
