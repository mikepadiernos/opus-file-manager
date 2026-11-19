// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigDialog>
#include <KFilePlacesModel>

#include <QMap>

class KPageWidgetItem;
class QCheckBox;

/**
 * The Preferences dialog: a sidebar of pages, each holding one tab per
 * feature. Widgets named "kcfg_<Entry>" are bound to the Settings singleton
 * by KConfigDialog.
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
    /**
     * Adds a sidebar page whose features are tabs.
     * @param tabs each tab's widget and title
     * @param title the page title
     * @param iconName the page's theme icon, without the "-symbolic" suffix
     */
    void addTabbedPage(const QList<std::pair<QWidget *, QString>> &tabs, const QString &title, const QString &iconName);

    /**
     * Applies the saved sidebar settings: titles beside small icons or below
     * large ones, the icon size, and symbolic page icons at 24 px and below.
     */
    void applySidebarStyle();

    QWidget *createSettingsDialogPage();
    QWidget *createFoldersAndTabsPage();
    QWidget *createPreviewsPage();
    QWidget *createPlacesPage();
    QWidget *createStatusBarPage();
    QWidget *createViewGeneralPage();
    QWidget *createDetailsPage();
    QWidget *createIconsPage();

    KFilePlacesModel *m_places = nullptr;
    /// Each sidebar page and its icon name, for switching to symbolic icons.
    QList<std::pair<KPageWidgetItem *, QString>> m_pageIcons;
    QMap<KFilePlacesModel::GroupType, QCheckBox *> m_placesGroups;
};
