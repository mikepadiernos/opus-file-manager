// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QList>
#include <QPair>

class KKeySequenceWidget;
class KWinIntegration;
class QAction;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QSpinBox;

/**
 * Settings › Configure KWin Integration: the state of the optional Plasma
 * Opus KWin script with explicit actions to install, update, enable, disable
 * or remove it, the window-management behaviours it applies, the workspaces
 * assigned to virtual desktops and the drop-down window.
 */
class KWinIntegrationDialog : public QDialog
{
    Q_OBJECT

public:
    /// @p workspaces are the saved workspaces' ids and names.
    KWinIntegrationDialog(KWinIntegration *kwin, const QList<QPair<QString, QString>> &workspaces, QAction *dropDownAction, QWidget *parent = nullptr);

    /// Saves the options; the script operations take effect immediately.
    void apply();

private:
    void updateState();
    void populateDesktops();
    void updateDropDownControls();
    QComboBox *workspaceCombo(const QString &selected);

    KWinIntegration *m_kwin;
    QList<QPair<QString, QString>> m_workspaces;
    QAction *m_dropDownAction;
    QLabel *m_status = nullptr;
    QPushButton *m_install = nullptr;
    QCheckBox *m_enabled = nullptr;
    QPushButton *m_remove = nullptr;
    QCheckBox *m_restoreWindows = nullptr;
    QCheckBox *m_tileCompanions = nullptr;
    QCheckBox *m_followDesktops = nullptr;
    QFormLayout *m_desktopLayout = nullptr;
    QLabel *m_noDesktops = nullptr;
    QList<QPair<QString, QComboBox *>> m_desktopCombos;
    QCheckBox *m_dropDownEnabled = nullptr;
    QComboBox *m_edge = nullptr;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QComboBox *m_screen = nullptr;
    QCheckBox *m_slide = nullptr;
    QCheckBox *m_hideOnFocusLoss = nullptr;
    QCheckBox *m_screenEdge = nullptr;
    QCheckBox *m_touchEdge = nullptr;
    QComboBox *m_dropDownWorkspace = nullptr;
    KKeySequenceWidget *m_shortcut = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};
