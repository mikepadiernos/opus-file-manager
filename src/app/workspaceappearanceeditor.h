// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

/**
 * Edits the theme profile and appearance overrides attached to a workspace:
 * none, the options selected here or all appearance options. Saved
 * references that cannot be edited here, such as a removed theme profile or
 * values that are invalid now, are kept unless replaced.
 */
class WorkspaceAppearanceEditor : public QGroupBox
{
    Q_OBJECT

public:
    /// Which appearance options are saved with the workspace, in the order of the mode list.
    enum class OverrideMode {
        None,
        Selected,
        All,
    };

    explicit WorkspaceAppearanceEditor(QWidget *parent = nullptr);

    /// Shows @p appearance, with options it does not hold showing the global values.
    void setAppearance(const WorkspaceCatalog::Appearance &appearance);
    /// The edited appearance, keeping saved references that were not replaced.
    WorkspaceCatalog::Appearance appearance() const;
    OverrideMode overrideMode() const;
    /// Shows the Preview and Revert buttons when the window can preview appearance.
    void setPreviewAvailable(bool available);
    /// Enables Revert while a preview is shown.
    void setPreviewing(bool previewing);

Q_SIGNALS:
    void saveRequested();
    void previewRequested();
    void revertRequested();

private:
    struct Row {
        QCheckBox *override = nullptr;
        QStringList entries;
        QList<QWidget *> editors;
    };

    void reloadProfiles(const QString &selectedId);
    void showGlobalValues();
    void updateEnabled();
    void updateProblems();

    WorkspaceCatalog::Appearance m_stored;
    /// Values saved in All mode for options without a row: the stored values over the global ones.
    QVariantMap m_snapshot;
    QComboBox *m_profile = nullptr;
    QComboBox *m_mode = nullptr;
    QComboBox *m_viewMode = nullptr;
    QComboBox *m_folderRules = nullptr;
    QCheckBox *m_blur = nullptr;
    QComboBox *m_corners = nullptr;
    QSpinBox *m_customCorners = nullptr;
    QComboBox *m_dividers = nullptr;
    QCheckBox *m_revealDividers = nullptr;
    QLabel *m_problems = nullptr;
    QPushButton *m_preview = nullptr;
    QPushButton *m_revert = nullptr;
    bool m_previewAvailable = false;
    QList<Row> m_rows;
};
