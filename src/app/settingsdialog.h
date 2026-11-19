// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigDialog>
#include <KFilePlacesModel>

#include <functional>

class KPageWidgetItem;
class PreviewSizeSpinBox;
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
    void applySidebarStyle(int size, bool below);

    QWidget *createSettingsDialogPage();
    QWidget *createFoldersAndTabsPage();
    QWidget *createPreviewsPage();
    QWidget *createOpeningPage();
    QWidget *createGalleryInformationPage();
    QWidget *createSlideshowPage();
    QWidget *createImageConversionPage();
    QWidget *createConfirmationsPage();
    QWidget *createPlacesPage();
    QWidget *createStatusBarPage();
    QWidget *createViewGeneralPage();
    QWidget *createDetailsPage();
    QWidget *createIconsPage();
    QWidget *createContextMenuActionsPage();
    QWidget *createServiceMenusPage();
    QWidget *createPanelAppearancePage();
    QWidget *createTabAppearancePage();
    QWidget *createArchivesPage();
    QWidget *createFileRulesPage();

    KFilePlacesModel *m_places = nullptr;
    PreviewSizeSpinBox *m_previewLimit = nullptr;
    qulonglong m_savedPreviewLimit = 0;
    /// Each sidebar page and its icon name, for switching to symbolic icons.
    QList<std::pair<KPageWidgetItem *, QString>> m_pageIcons;
    /// A checkbox for a setting kept outside Settings, in config that KIO
    /// shares with Dolphin.
    struct SharedToggle {
        QCheckBox *check;
        std::function<bool()> read;
        std::function<void(bool)> write;
        bool defaultValue;
    };
    /**
     * Creates a checkbox that the dialog loads, saves and resets through
     * @p read and @p write instead of Settings.
     * @param name the checkbox's object name
     * @param text the checkbox label
     * @param read returns the stored value
     * @param write stores a value
     * @param defaultValue the value Restore Defaults sets
     * @returns the checkbox, owned by the caller's layout
     */
    QCheckBox *addSharedToggle(const QString &name, const QString &text, std::function<bool()> read, std::function<void(bool)> write, bool defaultValue);
    QList<SharedToggle> m_sharedToggles;
};
