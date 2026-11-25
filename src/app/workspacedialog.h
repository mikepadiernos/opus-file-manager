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
class WorkspaceAppearanceEditor;
class WorkspaceLocations;

class WorkspaceDialog : public QDialog
{
    Q_OBJECT

public:
    using CaptureLayout = std::function<bool(WorkspaceCatalog::Layout &, QString &)>;
    using ApplyLayout = std::function<bool(const WorkspaceCatalog::Layout &, QString &)>;
    /// Lists the locations, providers and panels of a layout that are unavailable.
    using CheckLayout = std::function<QStringList(const WorkspaceCatalog::Layout &)>;
    /// Shows an appearance in the window without saving it, or reverts the preview when null.
    using PreviewAppearance = std::function<void(const WorkspaceCatalog::Appearance *)>;

    /// Manages profiles in group and applies their layouts using the supplied callbacks.
    WorkspaceDialog(KConfigGroup group,
                    CaptureLayout capture,
                    ApplyLayout apply,
                    const QString &currentId,
                    QWidget *parent = nullptr,
                    WorkspaceLocations *locations = nullptr);
    ~WorkspaceDialog() override;
    void selectWorkspace(const QString &id);
    /**
     * Sets how to find unavailable parts of a layout before switching, so the
     * user can make them available and retry.
     * @param check lists the unavailable parts of a layout
     */
    void setAvailabilityCheck(CheckLayout check);
    /**
     * Lets the edited workspace appearance be previewed in the window. The
     * preview is reverted when the dialog closes, the workspace shown changes
     * or the appearance is saved.
     */
    void setAppearancePreview(PreviewAppearance preview);

public Q_SLOTS:
    /// Reloads the catalog after an update from another window.
    void refreshCatalog();
    /**
     * Switches to the selected workspace.
     * @param confirmed true when the user already confirmed replacing the current state
     */
    void activateSelectedWorkspace(bool confirmed = false);

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
    void activateWorkspace(bool confirmed = false);
    /// Whether the window differs from the active workspace's saved layout.
    bool hasUnsavedState() const;
    /// Asks before switching, as set by the ConfirmWorkspaceSwitch setting.
    bool confirmSwitch();
    /// Offers to retry while parts of @p layout are unavailable; false to cancel.
    bool confirmUnavailable(const WorkspaceCatalog::Layout &layout);
    void saveLayout();
    void refreshProfileLinks();
    void saveProfileLinks();
    void refreshBehaviorSelection();
    void saveBehaviorSelection();
    /// Saves the theme profile and appearance options edited for the selected workspace.
    void saveAppearance();
    void previewAppearance();
    void revertAppearancePreview();
    /// Refreshes the list of assignable and unavailable saved locations.
    void refreshLocations();
    /// Assigns the selected saved location to the selected workspace.
    void assignSelectedLocation();
    /// Removes the selected saved location's assignment.
    void unassignSelectedLocation();

    KConfigGroup m_group;
    CaptureLayout m_capture;
    ApplyLayout m_apply;
    CheckLayout m_check;
    PreviewAppearance m_preview;
    bool m_previewing = false;
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
    QCheckBox *m_appearanceBehavior = nullptr;
    WorkspaceAppearanceEditor *m_appearance = nullptr;
};
