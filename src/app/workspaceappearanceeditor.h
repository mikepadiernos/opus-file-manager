// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

/**
 * Edits the theme profile and appearance overrides attached to a workspace.
 * Saved references that cannot be edited here, such as a removed theme
 * profile or values that are invalid now, are kept unless replaced.
 */
class WorkspaceAppearanceEditor : public QGroupBox
{
    Q_OBJECT

public:
    explicit WorkspaceAppearanceEditor(QWidget *parent = nullptr);

    /// Shows @p appearance, with unchecked options showing the global values.
    void setAppearance(const WorkspaceCatalog::Appearance &appearance);
    /// The edited appearance, keeping saved references that were not replaced.
    WorkspaceCatalog::Appearance appearance() const;

Q_SIGNALS:
    void saveRequested();

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
    QComboBox *m_profile = nullptr;
    QComboBox *m_viewMode = nullptr;
    QComboBox *m_folderRules = nullptr;
    QCheckBox *m_blur = nullptr;
    QComboBox *m_corners = nullptr;
    QSpinBox *m_customCorners = nullptr;
    QComboBox *m_dividers = nullptr;
    QCheckBox *m_revealDividers = nullptr;
    QLabel *m_problems = nullptr;
    QList<Row> m_rows;
};
