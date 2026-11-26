// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigDialog>
#include <KFilePlacesModel>

#include <functional>

class AccentColorChooser;
class ColorSchemeCombo;
class EmbeddedMetadataKeyList;
class EmbeddedMetadataMappingTable;
class FieldTemplateEditor;
class KPageWidgetItem;
class PreviewSizeSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class RuleListEditor;
class StartupWorkspaceCombo;
class TagFieldManagerWidget;

namespace LibraryConfirmations
{
struct Confirmation;
}

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
    /**
     * Sets the appearance options in the dialog to Plasma's defaults, except
     * AppearanceDefaults::keptEntries(). Nothing is saved until they are
     * applied.
     */
    void resetAppearanceToDefaults();
    /// Replaces the yes or no question asked before resetting the appearance, for tests.
    void setConfirmation(std::function<bool(const QString &)> confirm);
    /// Replaces the questions asked before adding, removing or moving a tag library, for tests.
    void setLibraryConfirmation(std::function<bool(const LibraryConfirmations::Confirmation &)> confirm);
    /// Replaces the folder chooser of Add Library, for tests; it returns an empty path when canceled.
    void setLibraryFolderChooser(std::function<QString()> choose);
    /// The theme profile options shown in the dialog, which may not be applied yet.
    QVariantMap appearanceValues() const;
    /// Shows Themes options in the dialog without applying them.
    void setAppearanceValues(const QVariantMap &values);

public Q_SLOTS:
    /// Reloads workspace names and assignments after the catalog changes.
    void reloadWorkspaces();

Q_SIGNALS:
    void customCommandsChanged();
    /// Asks the main window to open its workspace manager.
    void manageWorkspacesRequested();
    /// A tag library was added, removed or had its metadata moved in the Libraries tab, or its fields changed in the Fields tab; this takes effect immediately.
    void libraryChanged(const QString &root);
    /// A tag library was added in the Libraries tab and needs its first refresh.
    void libraryAdded(const QString &root);

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
    QWidget *createBarStylePage();
    QWidget *createMotionPage();
    QWidget *createThemeProfilesPage();
    QWidget *createBrandingPresentationPage();
    QWidget *createStyleSheetPage();
    QWidget *createViewGeneralPage();
    QWidget *createDetailsPage();
    QWidget *createCompactPage();
    QWidget *createCardsPage();
    QWidget *createSelectionPage();
    QWidget *createFiltersPage();
    QWidget *createFolderFormatsPage();
    QWidget *createYaziActivationPage();
    QWidget *createIconsPage();
    QWidget *createContextMenuActionsPage();
    QWidget *createServiceMenusPage();
    QWidget *createPanelAppearancePage();
    QWidget *createTabAppearancePage();
    QWidget *createArchivesPage();
    QWidget *createFileRulesPage();
    QWidget *createFileMetadataPage();
    QWidget *createSelectionFiltersPage();
    QWidget *createTaggingPrivacyPage();
    QWidget *createTaggingLibrariesPage();
    QWidget *createTaggingTagsPage();
    QWidget *createTaggingSearchPage();
    QWidget *createTaggingFieldsPage();
    QWidget *createTaggingEmbeddedMetadataPage();
    /// Offers the fields of the registered libraries and of new libraries as embedded metadata targets.
    void reloadEmbeddedMetadataFields();
    /// Lists the registered libraries in the Fields tab, keeping the chosen one when it is still registered, and reloads its fields.
    void reloadFieldLibraries();
    /// Lists the registered tag libraries with their storage and state, selecting @p select when listed.
    void reloadLibraries(const QString &select = {});
    void updateLibraryButtons();
    QString selectedLibrary() const;
    void addLibrary();
    void removeLibrary();
    void moveLibraryDocument();
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
    QComboBox *m_cornerRadius = nullptr;
    std::function<bool(const QString &)> m_confirm;
    std::function<bool(const LibraryConfirmations::Confirmation &)> m_confirmLibrary;
    std::function<QString()> m_chooseLibraryFolder;
    QTreeWidget *m_libraries = nullptr;
    QPushButton *m_removeLibrary = nullptr;
    QPushButton *m_moveLibraryDocument = nullptr;
    QComboBox *m_fieldLibrary = nullptr;
    QLabel *m_noFieldLibraries = nullptr;
    TagFieldManagerWidget *m_fieldManager = nullptr;
    FieldTemplateEditor *m_fieldTemplates = nullptr;
    EmbeddedMetadataMappingTable *m_metadataMappings = nullptr;
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
