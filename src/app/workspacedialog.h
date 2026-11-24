// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QDialog>
#include <functional>

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class WorkspaceLocations;

class WorkspaceDialog : public QDialog
{
    Q_OBJECT

public:
    using CaptureLayout = std::function<bool(WorkspaceCatalog::Layout &, QString &)>;
    using ApplyLayout = std::function<bool(const WorkspaceCatalog::Layout &, QString &)>;

    /// Manages profiles in group and applies their layouts using the supplied callbacks.
    WorkspaceDialog(KConfigGroup group,
                    CaptureLayout capture,
                    ApplyLayout apply,
                    const QString &currentId,
                    QWidget *parent = nullptr,
                    WorkspaceLocations *locations = nullptr);
    void selectWorkspace(const QString &id);

public Q_SLOTS:
    /// Reloads the catalog after an update from another window.
    void refreshCatalog();
    void activateSelectedWorkspace();

Q_SIGNALS:
    void currentWorkspaceChanged(const QString &id);
    void catalogChanged();

private:
    void reload(const QString &selectedId = {});
    QString selectedId() const;
    bool commit(const WorkspaceCatalog::Catalog &updated);
    void createWorkspace();
    void renameWorkspace();
    void duplicateWorkspace();
    void deleteWorkspace();
    void activateWorkspace();
    /// Whether the window differs from the active workspace's saved layout.
    bool hasUnsavedState() const;
    /// Asks before switching, as set by the ConfirmWorkspaceSwitch setting.
    bool confirmSwitch();
    void saveLayout();
    void refreshProfileLinks();
    void saveProfileLinks();
    void refreshBehaviorSelection();
    void saveBehaviorSelection();
    /// Refreshes the list of assignable and unavailable saved locations.
    void refreshLocations();
    /// Assigns the selected saved location to the selected workspace.
    void assignSelectedLocation();
    /// Removes the selected saved location's assignment.
    void unassignSelectedLocation();

    KConfigGroup m_group;
    CaptureLayout m_capture;
    ApplyLayout m_apply;
    WorkspaceCatalog::Catalog m_catalog;
    QString m_currentId;
    QListWidget *m_list = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_label = nullptr;
    QLabel *m_status = nullptr;
    WorkspaceLocations *m_locations = nullptr;
    QListWidget *m_locationList = nullptr;
    QListWidget *m_searchProfiles = nullptr;
    QListWidget *m_folderSyncProfiles = nullptr;
    QComboBox *m_behaviorMode = nullptr;
    QCheckBox *m_locationPresentation = nullptr;
    QCheckBox *m_searchScope = nullptr;
    QCheckBox *m_layoutBehavior = nullptr;
    QCheckBox *m_paneTabRestoration = nullptr;
};
