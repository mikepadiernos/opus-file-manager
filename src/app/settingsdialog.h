// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigDialog>
#include <KFilePlacesModel>

#include <functional>

class AccentColorChooser;
class ColorSchemeCombo;
class KPageWidgetItem;
class PreviewSizeSpinBox;
class QCheckBox;
class QListWidget;
class QTabWidget;
class RuleListEditor;
class StartupWorkspaceCombo;

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
    void showToolbarPage();

public Q_SLOTS:
    /// Reloads workspace names and assignments after the catalog changes.
    void reloadWorkspaces();

Q_SIGNALS:
    void customCommandsChanged();
    /// Asks the main window to open its workspace manager.
    void manageWorkspacesRequested();

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
     * @param tabsOut receives the page's tab widget when provided
     */
    KPageWidgetItem *
    addTabbedPage(const QList<std::pair<QWidget *, QString>> &tabs, const QString &title, const QString &iconName, QTabWidget **tabsOut = nullptr);

    /**
     * Applies the saved sidebar settings: titles beside small icons or below
     * large ones, the icon size, and symbolic page icons at 24 px and below.
     */
    void applySidebarStyle();
    void applySidebarStyle(int size, bool below);

    QWidget *createSettingsDialogPage();
    QWidget *createLanguagePage();
    QWidget *createDateLanguagePage();
    QWidget *createToolbarEntriesPage();
    QWidget *createSettingsBackupPage();
    QWidget *createFoldersAndTabsPage();
    QWidget *createPreviewsPage();
    QWidget *createOpeningPage();
    QWidget *createGalleryInformationPage();
    QWidget *createGalleryDisplayPage();
    QWidget *createSlideshowPage();
    QWidget *createImageConversionPage();
    QWidget *createTerminalPage();
    QWidget *createSearchPage();
    QWidget *createDuplicateFinderPage();
    QWidget *createContentChartPage();
    QWidget *createLinkedPanesPage();
    QWidget *createFileDiffPage();
    QWidget *createPaneComparisonPage();
    QWidget *createFileToolDefaultsPage();
    QWidget *createMetadataEditorPage();
    QWidget *createFolderSyncPage();
    QWidget *createBatchRenamePage();
    QWidget *createListingExportPage();
    QWidget *createAutomationPage();
    QWidget *createAssociationsPage();
    QWidget *createTransferQueuePage();
    QWidget *createEditorPage();
    QWidget *createTextEditorPage();
    QWidget *createConfirmationsPage();
    QWidget *createPlacesPage();
    QWidget *createStatusBarPage();
    QWidget *createInformationPanelPage();
    QWidget *createColorSchemePage();
    QWidget *createCustomColorsPage();
    QWidget *createEffectsPage();
    QWidget *createLinesPage();
    QWidget *createHighlightsPage();
    QWidget *createFolderColorsPage();
    QWidget *createFontsPage();
    QWidget *createSpacingPage();
    QWidget *createViewGeneralPage();
    QWidget *createDetailsPage();
    QWidget *createCompactPage();
    QWidget *createCardsPage();
    QWidget *createSelectionPage();
    QWidget *createFiltersPage();
    QWidget *createFolderFormatsPage();
    QWidget *createIconsPage();
    QWidget *createContextMenuActionsPage();
    QWidget *createServiceMenusPage();
    QWidget *createPanelAppearancePage();
    QWidget *createTabAppearancePage();
    QWidget *createArchivesPage();
    QWidget *createFileRulesPage();
    QWidget *createFileMetadataPage();
    QWidget *createSelectionFiltersPage();
    QWidget *createWorkspaceManagementPage();
    QWidget *createWorkspaceStartupPage();
    QWidget *createWorkspaceSearchPage();
    QWidget *createWorkspaceLayoutsPage();
    QWidget *createWorkspaceSnapshotsPage();
    QWidget *createWorkspaceRecoveryPage();
    QWidget *createWorkspaceIndicatorsPage();
    QWidget *createWorkspaceLocationBarsPage();
    QWidget *createWorkspaceBehaviorsPage();
    QWidget *createWorkspacePortabilityPage();
    void createSettingsBackup();
    void restoreSettingsBackup();
    void exportWorkspaceProfiles();
    void importWorkspaceProfiles();

    KFilePlacesModel *m_places = nullptr;
    PreviewSizeSpinBox *m_previewLimit = nullptr;
    RuleListEditor *m_selectionFilters = nullptr;
    QStringList m_savedSelectionFilters;
    qulonglong m_savedPreviewLimit = 0;
    /// Each sidebar page and its icon name, for switching to symbolic icons.
    QList<std::pair<KPageWidgetItem *, QString>> m_pageIcons;
    KPageWidgetItem *m_toolbarPage = nullptr;
    QTabWidget *m_toolbarTabs = nullptr;
    QListWidget *m_workspaceSummary = nullptr;
    StartupWorkspaceCombo *m_startupWorkspace = nullptr;
    ColorSchemeCombo *m_colorScheme = nullptr;
    AccentColorChooser *m_accentColor = nullptr;
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
