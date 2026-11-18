// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigDialog>
#include <KFilePlacesModel>

#include <QMap>

class QCheckBox;

/**
 * The Preferences dialog: one page per feature. Widgets named
 * "kcfg_<Entry>" are bound to the Settings singleton by KConfigDialog.
 */
class SettingsDialog : public KConfigDialog
{
    Q_OBJECT

public:
    /// Name under which KConfigDialog::exists() finds the open dialog.
    static constexpr auto Name = "settings";

    /**
     * @param places the window's places model; the Places page shows and
     *        hides its sections, which KIO shares with Dolphin and file dialogs
     * @param parent the main window
     */
    SettingsDialog(KFilePlacesModel *places, QWidget *parent);

protected:
    void updateSettings() override;
    void updateWidgets() override;
    void updateWidgetsDefault() override;
    bool hasChanged() override;
    bool isDefault() override;

private:
    QWidget *createStartupPage();
    QWidget *createPanesPage();
    QWidget *createViewsPage();
    QWidget *createPreviewsPage();
    QWidget *createPlacesPage();
    QWidget *createStatusBarPage();

    KFilePlacesModel *m_places = nullptr;
    QMap<KFilePlacesModel::GroupType, QCheckBox *> m_placesGroups;
};
