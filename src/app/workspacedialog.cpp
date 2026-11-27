// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacedialog.h"
#include "foldersyncprofiles.h"
#include "savedsearches.h"
#include "settings.h"
#include "workspaceappearanceeditor.h"
#include "workspacelocations.h"
#include "workspacerestore.h"

#include <KConfig>
#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

WorkspaceDialog::WorkspaceDialog(KConfigGroup group,
                                 CaptureLayout capture,
                                 ApplyLayout apply,
                                 const QString &currentId,
                                 QWidget *parent,
                                 WorkspaceLocations *locations)
    : QDialog(parent)
    , m_group(group)
    , m_capture(std::move(capture))
    , m_apply(std::move(apply))
    , m_currentId(currentId)
    , m_locations(locations)
{
    setObjectName(QStringLiteral("workspaceDialog"));
    setWindowTitle(i18nc("@title:window", "Workspaces"));
    resize(760, locations ? 720 : 520);
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(
        i18nc("@info",
              "Choose a workspace, or manage its name, short label and behavior selection. Saved layouts include pane and tab sessions, location and "
              "view modes, panel placement, terminal orientation and detached-pane geometry. After startup, the session layout remains in place until you "
              "switch."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("workspaceList"));
    m_list->setAccessibleName(i18nc("@label", "Workspaces"));
    layout->addWidget(m_list);
    auto *form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("workspaceName"));
    m_label = new QLineEdit(this);
    m_label->setObjectName(QStringLiteral("workspaceShortLabel"));
    form->addRow(i18nc("@label:textbox", "Name:"), m_name);
    form->addRow(i18nc("@label:textbox", "Short label:"), m_label);
    layout->addLayout(form);
    auto *buttons = new QHBoxLayout;
    const auto addButton = [this, buttons](const QString &text, const QString &name, auto slot) {
        auto *button = new QPushButton(text, this);
        button->setObjectName(name);
        connect(button, &QPushButton::clicked, this, slot);
        buttons->addWidget(button);
    };
    addButton(i18nc("@action:button", "Create"), QStringLiteral("workspaceCreate"), &WorkspaceDialog::createWorkspace);
    addButton(i18nc("@action:button", "Rename"), QStringLiteral("workspaceRename"), &WorkspaceDialog::renameWorkspace);
    addButton(i18nc("@action:button", "Duplicate"), QStringLiteral("workspaceDuplicate"), &WorkspaceDialog::duplicateWorkspace);
    addButton(i18nc("@action:button", "Delete"), QStringLiteral("workspaceDelete"), &WorkspaceDialog::deleteWorkspace);
    layout->addLayout(buttons);
    auto *layoutButtons = new QHBoxLayout;
    auto *activate = new QPushButton(i18nc("@action:button", "Switch to Workspace"), this);
    activate->setObjectName(QStringLiteral("workspaceActivate"));
    connect(activate, &QPushButton::clicked, this, [this] {
        activateWorkspace();
    });
    layoutButtons->addWidget(activate);
    auto *save = new QPushButton(i18nc("@action:button", "Save Current Layout"), this);
    save->setObjectName(QStringLiteral("workspaceSaveLayout"));
    connect(save, &QPushButton::clicked, this, &WorkspaceDialog::saveLayout);
    layoutButtons->addWidget(save);
    auto *saveLinks = new QPushButton(i18nc("@action:button", "Save Profile Links"), this);
    saveLinks->setObjectName(QStringLiteral("workspaceSaveProfileLinks"));
    connect(saveLinks, &QPushButton::clicked, this, &WorkspaceDialog::saveProfileLinks);
    layoutButtons->addWidget(saveLinks);
    layout->addLayout(layoutButtons);
    auto *behaviorGroup = new QGroupBox(i18nc("@title:group", "Workspace Behaviors"), this);
    auto *behaviorLayout = new QVBoxLayout(behaviorGroup);
    auto *behaviorDescription =
        new QLabel(i18nc("@info", "Choose whether workspace switching applies all behaviors, only checked behaviors, or none. Unselected state is preserved."),
                   behaviorGroup);
    behaviorDescription->setWordWrap(true);
    behaviorLayout->addWidget(behaviorDescription);
    m_behaviorMode = new QComboBox(behaviorGroup);
    m_behaviorMode->setObjectName(QStringLiteral("workspaceBehaviorMode"));
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "All"), int(WorkspaceCatalog::BehaviorMode::All));
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "Selected"), int(WorkspaceCatalog::BehaviorMode::Selected));
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "None"), int(WorkspaceCatalog::BehaviorMode::None));
    behaviorLayout->addWidget(m_behaviorMode);
    const auto addBehavior = [behaviorGroup, behaviorLayout](const QString &name, const QString &text) {
        auto *check = new QCheckBox(text, behaviorGroup);
        check->setObjectName(name);
        behaviorLayout->addWidget(check);
        return check;
    };
    m_locationPresentation = addBehavior(QStringLiteral("workspaceLocationPresentation"), i18nc("@option:check", "Location presentation"));
    m_searchScope = addBehavior(QStringLiteral("workspaceSearchScope"), i18nc("@option:check", "Search scope"));
    m_layoutBehavior = addBehavior(QStringLiteral("workspaceLayoutBehavior"), i18nc("@option:check", "Window and panel layout"));
    m_paneTabRestoration = addBehavior(QStringLiteral("workspacePaneTabRestoration"), i18nc("@option:check", "Pane and tab restoration"));
    m_appearanceBehavior = addBehavior(QStringLiteral("workspaceAppearanceBehavior"), i18nc("@option:check", "Theme profile and appearance"));
    m_yaziBehavior = addBehavior(QStringLiteral("workspaceYaziModeBehavior"), i18nc("@option:check", "Yazi mode"));
    m_yaziBehavior->setAccessibleName(i18nc("@option:check", "Yazi mode"));
    m_yaziBehavior->setAccessibleDescription(
        i18nc("@info:accessibility", "Whether switching restores each pane's Yazi mode, with its preview, parent column and column widths"));
    m_yaziBehavior->setToolTip(i18nc("@info:tooltip",
                                     "Restore each pane's Yazi mode, with its preview, parent column and column widths, as chosen in "
                                     "Preferences › Yazi Mode › Workspace Layouts. When unchecked, switching keeps each pane's current "
                                     "Yazi mode and columns."));
    auto *saveBehaviors = new QPushButton(i18nc("@action:button", "Save Workspace Behaviors"), behaviorGroup);
    saveBehaviors->setObjectName(QStringLiteral("workspaceSaveBehaviors"));
    connect(saveBehaviors, &QPushButton::clicked, this, &WorkspaceDialog::saveBehaviorSelection);
    behaviorLayout->addWidget(saveBehaviors);
    behaviorLayout->addStretch();
    m_appearance = new WorkspaceAppearanceEditor(this);
    connect(m_appearance, &WorkspaceAppearanceEditor::saveRequested, this, &WorkspaceDialog::saveAppearance);
    connect(m_appearance, &WorkspaceAppearanceEditor::previewRequested, this, &WorkspaceDialog::previewAppearance);
    connect(m_appearance, &WorkspaceAppearanceEditor::revertRequested, this, &WorkspaceDialog::revertAppearancePreview);
    connect(this, &QDialog::finished, this, &WorkspaceDialog::revertAppearancePreview);
    auto *behaviorRow = new QHBoxLayout;
    behaviorRow->addWidget(behaviorGroup);
    behaviorRow->addWidget(m_appearance, 1);
    layout->addLayout(behaviorRow);
    auto *profileLists = new QHBoxLayout;
    auto *searchGroup = new QGroupBox(i18nc("@title:group", "Saved Searches"), this);
    auto *searchLayout = new QVBoxLayout(searchGroup);
    m_searchProfiles = new QListWidget(searchGroup);
    m_searchProfiles->setObjectName(QStringLiteral("workspaceSavedSearches"));
    m_searchProfiles->setAccessibleName(i18nc("@label", "Saved searches linked to this workspace"));
    searchLayout->addWidget(m_searchProfiles);
    auto *syncGroup = new QGroupBox(i18nc("@title:group", "Folder Comparison and Sync"), this);
    auto *syncLayout = new QVBoxLayout(syncGroup);
    m_folderSyncProfiles = new QListWidget(syncGroup);
    m_folderSyncProfiles->setObjectName(QStringLiteral("workspaceFolderSyncProfiles"));
    m_folderSyncProfiles->setAccessibleName(i18nc("@label", "Folder comparison and sync profiles linked to this workspace"));
    syncLayout->addWidget(m_folderSyncProfiles);
    profileLists->addWidget(searchGroup);
    profileLists->addWidget(syncGroup);
    layout->addLayout(profileLists);
    if (m_locations) {
        auto *locationLabel = new QLabel(i18nc("@label:listbox", "Saved locations and their workspaces:"), this);
        m_locationList = new QListWidget(this);
        m_locationList->setObjectName(QStringLiteral("workspaceLocationList"));
        m_locationList->setAccessibleName(locationLabel->text());
        locationLabel->setBuddy(m_locationList);
        layout->addWidget(locationLabel);
        layout->addWidget(m_locationList);
        auto *assignmentButtons = new QHBoxLayout;
        auto *assign = new QPushButton(i18nc("@action:button", "Assign to Selected Workspace"), this);
        assign->setObjectName(QStringLiteral("workspaceAssignLocation"));
        connect(assign, &QPushButton::clicked, this, &WorkspaceDialog::assignSelectedLocation);
        auto *unassign = new QPushButton(i18nc("@action:button", "Unassign"), this);
        unassign->setObjectName(QStringLiteral("workspaceUnassignLocation"));
        connect(unassign, &QPushButton::clicked, this, &WorkspaceDialog::unassignSelectedLocation);
        auto *vault = new QPushButton(i18nc("@action:button", "Use as Editor Vault"), this);
        vault->setObjectName(QStringLiteral("workspaceUseAsVault"));
        vault->setToolTip(
            i18nc("@info:tooltip", "Use the selected local location as the built-in editor's Markdown vault while its workspace is active, or stop using it"));
        connect(vault, &QPushButton::clicked, this, &WorkspaceDialog::toggleSelectedVault);
        assignmentButtons->addWidget(assign);
        assignmentButtons->addWidget(unassign);
        assignmentButtons->addWidget(vault);
        layout->addLayout(assignmentButtons);
        connect(m_locations, &WorkspaceLocations::changed, this, &WorkspaceDialog::refreshLocations);
    }
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("workspaceStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(close);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_catalog.workspaces.size()) {
            const auto &workspace = m_catalog.workspaces.at(row);
            m_name->setText(workspace.name);
            m_label->setText(workspace.shortLabel);
            refreshProfileLinks();
            refreshBehaviorSelection();
        }
    });
    reload();
}

void WorkspaceDialog::refreshLocations()
{
    if (!m_locationList) {
        return;
    }
    const QString selected = m_locationList->currentItem() ? m_locationList->currentItem()->data(Qt::UserRole).toString() : QString();
    m_locationList->clear();
    QSet<QString> seen;
    const auto ownerName = [this](const QString &id) {
        const QString owner = m_catalog.locationAssignments.value(id);
        for (const auto &workspace : m_catalog.workspaces) {
            if (workspace.id == owner) {
                if (workspace.vaultLocationId != id) {
                    return workspace.name;
                }
                // Preferences › Applications › Editor can turn the vault off and keep it for later.
                return workspace.vaultEnabled ? i18nc("@item:inlistbox workspace name", "%1, editor vault", workspace.name)
                                              : i18nc("@item:inlistbox workspace name", "%1, editor vault turned off", workspace.name);
            }
        }
        return i18nc("@item:inlistbox location without a workspace", "Unassigned");
    };
    for (const auto &location : m_locations->locations()) {
        const QString state = location.ambiguous ? i18nc("@item:inlistbox", "Ambiguous saved location") : ownerName(location.id);
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox location and workspace", "%1 — %2", location.title, state), m_locationList);
        item->setData(Qt::UserRole, location.id);
        item->setData(Qt::UserRole + 1, location.ambiguous);
        item->setData(Qt::UserRole + 3, location.url.isLocalFile());
        item->setToolTip(location.url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment).toDisplayString(QUrl::PreferLocalFile));
        seen.insert(location.id);
        if (location.id == selected) {
            m_locationList->setCurrentItem(item);
        }
    }
    for (auto it = m_catalog.locationAssignments.cbegin(); it != m_catalog.locationAssignments.cend(); ++it) {
        if (seen.contains(it.key())) {
            continue;
        }
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox unavailable location and workspace", "Unavailable saved location — %1", ownerName(it.key())),
                                         m_locationList);
        item->setData(Qt::UserRole, it.key());
        item->setData(Qt::UserRole + 2, true);
        if (it.key() == selected) {
            m_locationList->setCurrentItem(item);
        }
    }
}

void WorkspaceDialog::refreshCatalog()
{
    reload(selectedId());
}

void WorkspaceDialog::selectWorkspace(const QString &id)
{
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toString() == id) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

void WorkspaceDialog::setAvailabilityCheck(CheckLayout check)
{
    m_check = std::move(check);
}

WorkspaceDialog::~WorkspaceDialog()
{
    revertAppearancePreview();
}

void WorkspaceDialog::setAppearancePreview(PreviewAppearance preview)
{
    revertAppearancePreview();
    m_preview = std::move(preview);
    m_appearance->setPreviewAvailable(bool(m_preview));
}

void WorkspaceDialog::previewAppearance()
{
    if (!m_preview || selectedId().isEmpty()) {
        return;
    }
    const WorkspaceCatalog::Appearance appearance = m_appearance->appearance();
    m_preview(&appearance);
    m_previewing = true;
    m_appearance->setPreviewing(true);
    m_status->setText(i18nc("@info", "Previewing the workspace appearance. It is not saved, and it is reverted when this dialog closes."));
}

void WorkspaceDialog::revertAppearancePreview()
{
    if (!m_previewing) {
        return;
    }
    m_previewing = false;
    if (m_preview) {
        m_preview(nullptr);
    }
    m_appearance->setPreviewing(false);
}

void WorkspaceDialog::activateSelectedWorkspace(bool confirmed, const std::optional<WorkspaceCatalog::BehaviorSelection> &behaviors)
{
    activateWorkspace(confirmed, behaviors);
}

void WorkspaceDialog::assignSelectedLocation()
{
    const auto *item = m_locationList->currentItem();
    const QString workspaceId = selectedId();
    if (!item || workspaceId.isEmpty()) {
        m_status->setText(i18nc("@info", "Select a saved location and a workspace first."));
        return;
    }
    if (item->data(Qt::UserRole + 1).toBool() || item->data(Qt::UserRole + 2).toBool()) {
        m_status->setText(i18nc("@info", "This saved location is unavailable or cannot be distinguished from another location."));
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    const QString owner = m_catalog.locationAssignments.value(id);
    auto updated = m_catalog;
    QString error;
    if (!owner.isEmpty() && owner != workspaceId) {
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Reassign Saved Location"),
                                  i18nc("@info", "Move this location's assignment to the selected workspace? No files will be moved."),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        if (!WorkspaceCatalog::unassignLocation(updated, id, error)) {
            m_status->setText(error);
            return;
        }
    }
    if (!WorkspaceCatalog::assignLocation(updated, id, workspaceId, error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        reload(workspaceId);
    }
}

void WorkspaceDialog::unassignSelectedLocation()
{
    const auto *item = m_locationList->currentItem();
    if (!item) {
        m_status->setText(i18nc("@info", "Select a saved location first."));
        return;
    }
    auto updated = m_catalog;
    QString error;
    if (!WorkspaceCatalog::unassignLocation(updated, item->data(Qt::UserRole).toString(), error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        reload(selectedId());
    }
}

void WorkspaceDialog::toggleSelectedVault()
{
    const auto *item = m_locationList->currentItem();
    if (!item) {
        m_status->setText(i18nc("@info", "Select a saved location first."));
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    const QString owner = m_catalog.locationAssignments.value(id);
    if (owner.isEmpty()) {
        m_status->setText(i18nc("@info", "Assign this location to a workspace before using it as the workspace's editor vault."));
        return;
    }
    const auto workspace = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&owner](const auto &entry) {
        return entry.id == owner;
    });
    // Using a vault that is turned off turns it on again.
    const bool current = workspace != m_catalog.workspaces.cend() && workspace->vaultLocationId == id && workspace->vaultEnabled;
    if (!current && (!item->data(Qt::UserRole + 3).toBool() || item->data(Qt::UserRole + 1).toBool())) {
        m_status->setText(i18nc("@info", "Only an available, unambiguous local folder can be an editor vault."));
        return;
    }
    auto updated = m_catalog;
    QString error;
    if (!WorkspaceCatalog::setVault(updated, owner, current ? QString() : id, error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        reload(selectedId());
        m_status->setText(current ? i18nc("@info", "The location is no longer the workspace's editor vault. No files were changed.")
                                  : i18nc("@info", "The location is now the workspace's editor vault. No files were changed."));
    }
}

void WorkspaceDialog::reload(const QString &selected)
{
    QString error;
    if (!WorkspaceCatalog::load(m_group, m_catalog, error)) {
        m_status->setText(error);
        return;
    }
    m_status->clear();
    if (!m_currentId.isEmpty() && std::none_of(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [this](const auto &workspace) {
            return workspace.id == m_currentId;
        })) {
        m_currentId.clear();
        Q_EMIT currentWorkspaceChanged(m_currentId);
    }
    m_list->clear();
    for (const auto &workspace : m_catalog.workspaces) {
        const QString text = workspace.id == m_currentId
            ? i18nc("@item:inlistbox current workspace name and short label", "%1 (%2) — current", workspace.name, workspace.shortLabel)
            : i18nc("@item:inlistbox workspace name and short label", "%1 (%2)", workspace.name, workspace.shortLabel);
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, workspace.id);
        if (workspace.id == selected) {
            m_list->setCurrentItem(item);
        }
    }
    refreshLocations();
    refreshProfileLinks();
    refreshBehaviorSelection();
}

QString WorkspaceDialog::selectedId() const
{
    return m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
}

bool WorkspaceDialog::commit(const WorkspaceCatalog::Catalog &updated)
{
    QString error;
    if (!WorkspaceCatalog::saveIfUnchanged(m_group, m_catalog, updated, error)) {
        reload(selectedId());
        m_status->setText(error);
        return false;
    }
    Q_EMIT catalogChanged();
    return true;
}

void WorkspaceDialog::refreshBehaviorSelection()
{
    revertAppearancePreview();
    const QString id = selectedId();
    const auto workspace = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&id](const auto &entry) {
        return entry.id == id;
    });
    const WorkspaceCatalog::BehaviorSelection selection =
        workspace == m_catalog.workspaces.cend() ? WorkspaceCatalog::BehaviorSelection{} : workspace->behaviorSelection;
    const QSignalBlocker modeBlocker(m_behaviorMode);
    const QSignalBlocker locationBlocker(m_locationPresentation);
    const QSignalBlocker searchBlocker(m_searchScope);
    const QSignalBlocker layoutBlocker(m_layoutBehavior);
    const QSignalBlocker panesBlocker(m_paneTabRestoration);
    const QSignalBlocker appearanceBlocker(m_appearanceBehavior);
    const QSignalBlocker yaziBlocker(m_yaziBehavior);
    m_behaviorMode->setCurrentIndex(m_behaviorMode->findData(int(selection.mode)));
    m_locationPresentation->setChecked(selection.locationPresentation);
    m_searchScope->setChecked(selection.searchScope);
    m_layoutBehavior->setChecked(selection.layout);
    m_paneTabRestoration->setChecked(selection.paneTabRestoration);
    m_appearanceBehavior->setChecked(selection.appearance);
    m_yaziBehaviorShown = selection.yaziMode.value_or(selection.layout);
    m_yaziBehavior->setChecked(m_yaziBehaviorShown);
    m_appearance->setEnabled(workspace != m_catalog.workspaces.cend());
    m_appearance->setAppearance(workspace == m_catalog.workspaces.cend() ? WorkspaceCatalog::Appearance{} : workspace->appearance);
}

void WorkspaceDialog::saveBehaviorSelection()
{
    const QString id = selectedId();
    auto updated = m_catalog;
    auto workspace = std::find_if(updated.workspaces.begin(), updated.workspaces.end(), [&id](const auto &entry) {
        return entry.id == id;
    });
    if (workspace == updated.workspaces.end()) {
        m_status->setText(i18nc("@info", "Select a workspace first."));
        return;
    }
    // A workspace saved before the Yazi mode choice keeps restoring it with the views until the choice changes.
    const std::optional<bool> yaziMode = !workspace->behaviorSelection.yaziMode && m_yaziBehavior->isChecked() == m_yaziBehaviorShown
        ? std::nullopt
        : std::optional<bool>(m_yaziBehavior->isChecked());
    workspace->behaviorSelection = {static_cast<WorkspaceCatalog::BehaviorMode>(m_behaviorMode->currentData().toInt()),
                                    m_locationPresentation->isChecked(),
                                    m_searchScope->isChecked(),
                                    m_layoutBehavior->isChecked(),
                                    m_paneTabRestoration->isChecked(),
                                    m_appearanceBehavior->isChecked(),
                                    yaziMode};
    if (commit(updated)) {
        reload(id);
        m_status->setText(i18nc("@info", "Workspace behaviors saved."));
    }
}

void WorkspaceDialog::saveAppearance()
{
    const QString id = selectedId();
    auto updated = m_catalog;
    QString error;
    if (id.isEmpty()) {
        m_status->setText(i18nc("@info", "Select a workspace first."));
        return;
    }
    if (!WorkspaceCatalog::setAppearance(updated, id, m_appearance->appearance(), error)) {
        m_status->setText(error);
        return;
    }
    // The saved appearance replaces the preview once the window reloads the catalog.
    revertAppearancePreview();
    if (commit(updated)) {
        reload(id);
        m_status->setText(id == m_currentId ? i18nc("@info", "Workspace appearance saved and applied.")
                                            : i18nc("@info", "Workspace appearance saved. It applies when you switch to the workspace."));
    }
}

void WorkspaceDialog::createWorkspace()
{
    auto updated = m_catalog;
    QString error;
    WorkspaceCatalog::Layout layout;
    if (!WorkspaceCatalog::create(updated, m_name->text(), m_label->text(), error) || !m_capture(layout, error)
        || !WorkspaceCatalog::setLayout(updated, updated.workspaces.last().id, layout, error)) {
        m_status->setText(error);
        return;
    }
    const QString id = updated.workspaces.last().id;
    updated.workspaces.last().behaviorSelection = WorkspaceRestore::defaultBehaviorSelection();
    if (commit(updated)) {
        reload(id);
    }
}

void WorkspaceDialog::renameWorkspace()
{
    const QString id = selectedId();
    auto updated = m_catalog;
    QString error;
    if (!WorkspaceCatalog::rename(updated, id, m_name->text(), m_label->text(), error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        reload(id);
    }
}

void WorkspaceDialog::duplicateWorkspace()
{
    const QString id = selectedId();
    auto updated = m_catalog;
    QString error;
    if (!WorkspaceCatalog::duplicate(updated, id, m_name->text(), m_label->text(), error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        reload(updated.workspaces.last().id);
    }
}

void WorkspaceDialog::deleteWorkspace()
{
    const QString id = selectedId();
    if (id.isEmpty()) {
        m_status->setText(i18nc("@info", "Select a workspace first."));
        return;
    }
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Delete Workspace"),
                              i18nc("@info", "Delete this workspace? Its files, locations and tags will not be deleted."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    auto updated = m_catalog;
    QString error;
    if (!WorkspaceCatalog::remove(updated, id, error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        if (id == m_currentId) {
            m_currentId.clear();
            Q_EMIT currentWorkspaceChanged(m_currentId);
        }
        reload();
    }
}

bool WorkspaceDialog::hasUnsavedState() const
{
    const auto active = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [this](const auto &workspace) {
        return workspace.id == m_currentId;
    });
    if (active == m_catalog.workspaces.cend() || !active->layout) {
        return true;
    }
    WorkspaceCatalog::Layout layout;
    QString error;
    return !m_capture(layout, error) || layout != *active->layout;
}

bool WorkspaceDialog::confirmSwitch()
{
    const auto mode = Settings::confirmWorkspaceSwitch();
    if (mode == Settings::EnumConfirmWorkspaceSwitch::Never) {
        return true;
    }
    const bool unsaved = hasUnsavedState();
    if (mode == Settings::EnumConfirmWorkspaceSwitch::WhenUnsaved && !unsaved) {
        return true;
    }
    const QString text = unsaved
        ? i18nc("@info",
                "Switch to this workspace? The current layout, panes and tabs have changes that are not saved in a workspace. The workspace's "
                "selected state will replace them. Running file transfers continue.")
        : i18nc("@info",
                "Switch to this workspace? Its selected layout, pane, tab and location-bar state will replace the current state. Running file "
                "transfers continue.");
    return QMessageBox::question(this, i18nc("@title:window", "Switch Workspace"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes;
}

bool WorkspaceDialog::confirmUnavailable(const WorkspaceCatalog::Layout &layout)
{
    if (!m_check || !Settings::workspaceRecoveryPrompt()) {
        return true;
    }
    for (QStringList unavailable = m_check(layout); !unavailable.isEmpty(); unavailable = m_check(layout)) {
        QMessageBox box(QMessageBox::Warning,
                        i18nc("@title:window", "Workspace Items Unavailable"),
                        i18nc("@info",
                              "Some locations, providers or panels in this workspace are unavailable. Make them available and retry, or "
                              "continue without them. The workspace keeps its saved references either way."),
                        QMessageBox::Retry | QMessageBox::Cancel,
                        this);
        box.setObjectName(QStringLiteral("workspaceUnavailableItems"));
        box.setInformativeText(unavailable.join(QLatin1Char('\n')));
        QPushButton *continueButton = box.addButton(i18nc("@action:button", "Continue Without Them"), QMessageBox::AcceptRole);
        box.setDefaultButton(QMessageBox::Retry);
        box.exec();
        if (box.clickedButton() == continueButton) {
            return true;
        }
        if (box.clickedButton() != box.button(QMessageBox::Retry)) {
            return false;
        }
    }
    return true;
}

void WorkspaceDialog::activateWorkspace(bool confirmed, const std::optional<WorkspaceCatalog::BehaviorSelection> &behaviors)
{
    const QString id = selectedId();
    const auto it = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&id](const auto &workspace) {
        return workspace.id == id;
    });
    if (it == m_catalog.workspaces.cend()) {
        m_status->setText(i18nc("@info", "Select a workspace with a saved layout."));
        return;
    }
    const WorkspaceCatalog::BehaviorSelection selection = behaviors.value_or(it->behaviorSelection);
    const WorkspaceRestore::Options options = WorkspaceRestore::fromSettings();
    // Yazi state lives in the pane snapshot; without one there is nothing of it to restore.
    const bool restoreYazi = WorkspaceRestore::yaziParts(selection, options).any() && it->layout && it->layout->session;
    const bool restoreAnything = selection.applies(selection.layout) || selection.applies(selection.paneTabRestoration)
        || selection.applies(selection.locationPresentation) || restoreYazi;
    if (restoreAnything && !it->layout) {
        m_status->setText(i18nc("@info", "The selected workspace has no saved layout for its enabled behaviors."));
        return;
    }
    if (restoreAnything && !confirmed && !confirmSwitch()) {
        return;
    }
    const auto current = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&id](const auto &workspace) {
        return workspace.id == id;
    });
    if (current == m_catalog.workspaces.cend() || (restoreAnything && !current->layout)) {
        m_status->setText(i18nc("@info", "The selected workspace or its saved layout is no longer available."));
        return;
    }
    WorkspaceCatalog::Layout previous;
    QString error;
    bool layoutApplied = false;
    QString warning;
    if (restoreAnything) {
        if (!m_capture(previous, error)) {
            m_status->setText(error);
            return;
        }
        WorkspaceCatalog::Layout layout;
        if (!WorkspaceRestore::merge(*current->layout, previous, selection, options, layout, error)) {
            m_status->setText(error);
            return;
        }
        if (!confirmUnavailable(layout)) {
            return;
        }
        if (!m_apply(layout, error)) {
            m_status->setText(error);
            return;
        }
        layoutApplied = true;
        warning = error;
    }
    auto updated = m_catalog;
    if (!WorkspaceCatalog::activate(updated, id, error) || !commit(updated)) {
        if (layoutApplied) {
            QString rollbackError;
            if (!m_apply(previous, rollbackError) || !rollbackError.isEmpty()) {
                m_status->setText(i18nc("@info", "The workspace could not be saved and the previous layout could not be restored: %1", rollbackError));
            } else if (!error.isEmpty()) {
                m_status->setText(error);
            }
        }
        return;
    }
    m_currentId = id;
    Q_EMIT currentWorkspaceChanged(id);
    reload(id);
    m_status->setText(warning);
}

void WorkspaceDialog::saveLayout()
{
    const QString id = selectedId();
    if (id.isEmpty()) {
        m_status->setText(i18nc("@info", "Select a workspace first."));
        return;
    }
    if (Settings::confirmWorkspaceLayoutOverwrite()
        && QMessageBox::question(this,
                                 i18nc("@title:window", "Save Workspace Layout"),
                                 i18nc("@info", "Replace the selected workspace's saved layout with the current window layout?"),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No)
            != QMessageBox::Yes) {
        return;
    }
    auto updated = m_catalog;
    WorkspaceCatalog::Layout layout;
    QString error;
    if (!m_capture(layout, error) || !WorkspaceCatalog::setLayout(updated, id, layout, error)) {
        m_status->setText(error);
    } else if (commit(updated)) {
        reload(id);
    }
}

void WorkspaceDialog::refreshProfileLinks()
{
    m_searchProfiles->clear();
    m_folderSyncProfiles->clear();
    auto *saveButton = findChild<QPushButton *>(QStringLiteral("workspaceSaveProfileLinks"));
    saveButton->setEnabled(false);
    const auto workspace = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [this](const auto &entry) {
        return entry.id == selectedId();
    });
    if (workspace == m_catalog.workspaces.cend()) {
        return;
    }

    QString error;
    const auto searches = SavedSearches::load(m_group.config()->group(QStringLiteral("SavedSearches")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    QSet<QString> searchIds;
    for (const auto &search : searches) {
        auto *item = new QListWidgetItem(search.name, m_searchProfiles);
        item->setData(Qt::UserRole, search.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(workspace->savedSearchIds.contains(search.id) ? Qt::Checked : Qt::Unchecked);
        searchIds.insert(search.id);
    }
    for (const QString &id : workspace->savedSearchIds) {
        if (searchIds.contains(id)) {
            continue;
        }
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox missing saved search", "Missing saved search — %1", id), m_searchProfiles);
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }

    const auto profiles = FolderSyncProfiles::load(m_group.config()->group(QStringLiteral("FolderSyncProfiles")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    QSet<QString> profileIds;
    for (const auto &profile : profiles) {
        auto *item = new QListWidgetItem(profile.name, m_folderSyncProfiles);
        item->setData(Qt::UserRole, profile.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(workspace->folderSyncProfileIds.contains(profile.id) ? Qt::Checked : Qt::Unchecked);
        profileIds.insert(profile.id);
    }
    for (const QString &id : workspace->folderSyncProfileIds) {
        if (profileIds.contains(id)) {
            continue;
        }
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox missing folder sync profile", "Missing profile — %1", id), m_folderSyncProfiles);
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    saveButton->setEnabled(true);
}

void WorkspaceDialog::saveProfileLinks()
{
    const QString id = selectedId();
    auto updated = m_catalog;
    auto workspace = std::find_if(updated.workspaces.begin(), updated.workspaces.end(), [&id](const auto &entry) {
        return entry.id == id;
    });
    if (workspace == updated.workspaces.end()) {
        m_status->setText(i18nc("@info", "Select a workspace first."));
        return;
    }
    workspace->savedSearchIds.clear();
    workspace->folderSyncProfileIds.clear();
    for (int row = 0; row < m_searchProfiles->count(); ++row) {
        const QListWidgetItem *item = m_searchProfiles->item(row);
        if (item->checkState() == Qt::Checked) {
            workspace->savedSearchIds.append(item->data(Qt::UserRole).toString());
        }
    }
    for (int row = 0; row < m_folderSyncProfiles->count(); ++row) {
        const QListWidgetItem *item = m_folderSyncProfiles->item(row);
        if (item->checkState() == Qt::Checked) {
            workspace->folderSyncProfileIds.append(item->data(Qt::UserRole).toString());
        }
    }
    if (commit(updated)) {
        reload(id);
        m_status->setText(i18nc("@info", "Workspace profile links saved. Switching workspaces will not run tasks."));
    }
}
