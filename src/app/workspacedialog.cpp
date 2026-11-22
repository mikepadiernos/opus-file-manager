// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacedialog.h"
#include "workspacelocations.h"

#include <KConfig>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
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
    resize(620, locations ? 650 : 430);
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(
        i18nc("@info",
              "Choose a workspace, or manage its name and short label. Layouts include pane arrangements, splitter sizes and dock and toolbar positions. "
              "View modes, toolbar contents and terminal placement are not yet saved. After startup, the session layout remains in place until you switch."),
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
    connect(activate, &QPushButton::clicked, this, &WorkspaceDialog::activateWorkspace);
    layoutButtons->addWidget(activate);
    auto *save = new QPushButton(i18nc("@action:button", "Save Current Layout"), this);
    save->setObjectName(QStringLiteral("workspaceSaveLayout"));
    connect(save, &QPushButton::clicked, this, &WorkspaceDialog::saveLayout);
    layoutButtons->addWidget(save);
    layout->addLayout(layoutButtons);
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
        assignmentButtons->addWidget(assign);
        assignmentButtons->addWidget(unassign);
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
                return workspace.name;
            }
        }
        return i18nc("@item:inlistbox location without a workspace", "Unassigned");
    };
    for (const auto &location : m_locations->locations()) {
        const QString state = location.ambiguous ? i18nc("@item:inlistbox", "Ambiguous saved location") : ownerName(location.id);
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox location and workspace", "%1 — %2", location.title, state), m_locationList);
        item->setData(Qt::UserRole, location.id);
        item->setData(Qt::UserRole + 1, location.ambiguous);
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

void WorkspaceDialog::reload(const QString &selected)
{
    QString error;
    if (!WorkspaceCatalog::load(m_group, m_catalog, error)) {
        m_status->setText(error);
        return;
    }
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
    m_status->clear();
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

void WorkspaceDialog::activateWorkspace()
{
    const QString id = selectedId();
    const auto it = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&id](const auto &workspace) {
        return workspace.id == id;
    });
    if (it == m_catalog.workspaces.cend() || !it->layout) {
        m_status->setText(i18nc("@info", "Select a workspace with a saved layout."));
        return;
    }
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Switch Workspace"),
                              i18nc("@info", "Switch to this workspace? Unsaved layout changes will be replaced."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    const auto current = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&id](const auto &workspace) {
        return workspace.id == id;
    });
    if (current == m_catalog.workspaces.cend() || !current->layout) {
        m_status->setText(i18nc("@info", "The selected workspace no longer has a saved layout."));
        return;
    }
    const WorkspaceCatalog::Layout layout = *current->layout;
    WorkspaceCatalog::Layout previous;
    QString error;
    if (!m_capture(previous, error) || !m_apply(layout, error)) {
        m_status->setText(error);
        return;
    }
    auto updated = m_catalog;
    if (!WorkspaceCatalog::activate(updated, id, error) || !commit(updated)) {
        QString rollbackError;
        if (!m_apply(previous, rollbackError)) {
            m_status->setText(i18nc("@info", "The workspace could not be saved and the previous layout could not be restored: %1", rollbackError));
        } else if (!error.isEmpty()) {
            m_status->setText(error);
        }
        return;
    }
    m_currentId = id;
    Q_EMIT currentWorkspaceChanged(id);
    reload(id);
}

void WorkspaceDialog::saveLayout()
{
    const QString id = selectedId();
    if (id.isEmpty()) {
        m_status->setText(i18nc("@info", "Select a workspace first."));
        return;
    }
    if (QMessageBox::question(this,
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
