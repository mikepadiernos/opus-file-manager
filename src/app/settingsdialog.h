// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "activityassociations.h"

#include <KConfigDialog>
#include <KFilePlacesModel>
#include <QKeySequence>
#include <QMap>

#include <functional>

class AccentColorChooser;
class ActivityAssociationsEditor;
class ActivityController;
class ColorSchemeCombo;
class DesktopTray;
class DesktopWorkspacesEditor;
class EmbeddedMetadataKeyList;
class EmbeddedMetadataMappingTable;
class FieldTemplateEditor;
class HiddenItemsList;
class KKeySequenceWidget;
class KPageWidgetItem;
class KWinIntegration;
class PreviewSizeSpinBox;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QVBoxLayout;
class RemoteProfilesEditor;
class RuleListEditor;
class StartupWorkspaceCombo;
class TagFieldManagerWidget;
class WorkspaceCombo;
class WorkspaceVaultSettings;
class YaziColumnRatiosEditor;

namespace LibraryConfirmations
{
struct Confirmation;
}

namespace KNSCore
{
class Entry;
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
    /// Shows the KDE Plasma › Activities tab, where Activity associations are edited.
    void showActivitiesPage();
    /// Shows the KDE Plasma › KWin tab, where the KWin script and the drop-down window are configured.
    void showKWinPage();
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
    /**
     * Replaces the question asked before applying a Yazi mode key preset
     * that shadows window shortcuts or has conflicts of its own, for tests.
     * @p confirm receives the question and the conflicts and returns whether
     * to apply the preset.
     */
    void setYaziKeyConfirmation(std::function<bool(const QString &, const QStringList &)> confirm);
    /**
     * Sets every Yazi Mode setting in the dialog, that is every entry of the
     * YaziMode settings group, to its default, and has Apply or OK also make
     * open panes drop their own Yazi column choices. Nothing is saved until
     * the settings are applied; saved workspaces, the Yazi mode behavior of
     * new workspaces and other settings are left alone.
     */
    void resetYaziModeToDefaults();
    /// Replaces the yes or no question asked before Restore Yazi Mode Defaults, for tests.
    void setYaziResetConfirmation(std::function<bool(const QString &)> confirm);
    /**
     * Replaces how KDE Plasma › File Manager checks for and restarts
     * xdg-desktop-portal, and the question asked before restarting it, for tests.
     * @p restart returns an error message, or an empty string on success.
     */
    void setPortalSetupHooks(std::function<bool()> available, std::function<QString()> restart, std::function<bool(const QString &)> confirm);
    /// Replaces the yes or no question asked before removing the KWin script, for tests.
    void setKWinRemovalConfirmation(std::function<bool()> confirm);
    /**
     * Replaces how KDE Plasma › KWin reads and assigns the drop-down window's
     * global shortcut, for tests, and loads the shortcut again.
     */
    void setDropDownShortcutHooks(std::function<QKeySequence()> read, std::function<void(const QKeySequence &)> write);
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
    /**
     * Yazi mode defaults restored with Restore Yazi Mode Defaults are being
     * applied, so open panes should drop their own Yazi column choices. The
     * settings are saved right after this signal.
     */
    void yaziDefaultsRestored();

protected:
    void accept() override;
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
     * @param footer a widget shown below the tabs, whichever tab is selected, or nullptr
     */
    KPageWidgetItem *addTabbedPage(const QList<std::pair<QWidget *, QString>> &tabs,
                                   const QString &title,
                                   const QString &iconName,
                                   QTabWidget **tabsOut = nullptr,
                                   QWidget *footer = nullptr);

    /**
     * Applies the saved sidebar settings: titles beside small icons or below
     * large ones, the icon size, and symbolic page icons at 24 px and below.
     */
    void applySidebarStyle();
    void applySidebarStyle(int size, bool below);

    QWidget *createSettingsDialogPage();
    /// The KDE Plasma › System Tray tab.
    QWidget *createSystemTrayPage();
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
    QWidget *createYaziColumnsPage();
    QWidget *createYaziKeyboardPage();
    /// Lists the keys and conflicts of the key preset chosen in the Keyboard tab.
    void updateYaziKeyPreview();
    /// The conflicts of key preset @p preset, as shown to the user: its own, then the window shortcuts it shadows.
    QStringList yaziKeyConflicts(int preset) const;
    /**
     * Asks before applying a key preset with conflicts; when that is
     * declined, chooses the applied preset again.
     * @returns whether the chosen preset may be applied
     */
    bool confirmYaziKeyPreset();
    QWidget *createYaziPreviewPage();
    QWidget *createYaziWorkspaceLayoutsPage();
    /// Describes, on the Workspace Layouts tab, whether new workspaces include the Yazi mode behavior as chosen under Workspaces › Behaviors.
    void updateYaziWorkspaceDefault();
    /// The Restore Yazi Mode Defaults button and its note, shown below the Yazi Mode tabs.
    QWidget *createYaziResetRow();
    /**
     * Shows @p values, by entry name, in the entries' widgets without saving
     * them. Entries without a widget are skipped.
     */
    void setEditorValues(const QVariantMap &values);
    QWidget *createIconsPage();
    QWidget *createContextMenuActionsPage();
    QWidget *createServiceMenusPage();
    /**
     * Lists the installed service menus again, after downloads, keeping
     * unsaved changes to those still installed.
     */
    void refreshServiceMenus();
    /// Summarizes what the download window installed, removed or failed to install.
    void reportServiceMenuChanges(const QList<KNSCore::Entry> &changed);
    HiddenItemsList *m_serviceMenuList = nullptr;
    QVBoxLayout *m_serviceMenuToggles = nullptr;
    QLabel *m_serviceMenuStatus = nullptr;
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
    /// The KDE Plasma › File Manager tab: the default file manager and the system file chooser.
    QWidget *createFileManagerIntegrationPage();
    QGroupBox *createDefaultFileManagerGroup();
    QGroupBox *createFileChooserGroup();
    /// The KDE Plasma › Notifications tab: which operations report to Plasma.
    QWidget *createNotificationsIntegrationPage();
    /// The KDE Plasma › Shortcuts tab: the global shortcut that shows or hides Plasma Opus.
    QWidget *createShortcutsIntegrationPage();
    /// The main window's tray, which owns the global shortcut, if the dialog has one.
    DesktopTray *desktopTray() const;
    /**
     * The KDE Plasma › Activities tab: whether Activity associations are used
     * and offered, each Activity's association, and Activity usage reports.
     */
    QWidget *createActivitiesIntegrationPage();
    /// The KDE Plasma › KDE Connect tab: whether File › Send to Device… is offered and remembers the device used last.
    QWidget *createKdeConnectIntegrationPage();
    /**
     * The KDE Plasma › KRunner tab: whether the KRunner plugin offers
     * anything, and which saved workspaces and Favorites it offers.
     */
    QWidget *createKRunnerIntegrationPage();
    /// Lists the Favorites again in the KRunner tab after they change.
    void reloadRunnerFavorites();
    /**
     * The KDE Plasma › Online Accounts tab: whether Plasma Opus shows KDE
     * Online Accounts at all, and which of the accounts with file storage.
     */
    QWidget *createOnlineAccountsIntegrationPage();
    /**
     * The KDE Plasma › Mount Manager tab: which file systems the Mount Manager
     * and Go › Mount Points list, and whether Mount Points is on the toolbar.
     */
    QWidget *createMountManagerIntegrationPage();
    /**
     * The KDE Plasma › KWin tab: installs, updates, enables, disables or
     * removes the Plasma Opus KWin script on request, and chooses what it
     * does with Plasma Opus windows and which workspace each virtual desktop
     * applies.
     */
    QWidget *createKWinIntegrationPage();
    /// The Drop-Down Window group of the KWin tab: how the main window slides in from a screen edge.
    QGroupBox *createDropDownGroup();
    /// The main window's KWin integration, which manages the KWin script, if the dialog has one.
    KWinIntegration *kwinIntegration() const;
    /**
     * The KDE Plasma › Baloo tab: which indexed search extras the search
     * panel offers, with the state of KDE's shared index, and KDE's File
     * Search settings on request; and which file-type categories Places lists.
     */
    QWidget *createBalooIntegrationPage();
    QGroupBox *createBalooSearchGroup();
    /// The KDE Plasma › Baloo categories group: which file-type categories Places lists, and the large-file size.
    QGroupBox *createFileTypeCategoriesGroup();
    /// The Applications › Terminal tab.
    QWidget *createTerminalIntegrationPage();
    /// The Provider group of the Terminal tab: the terminals for Open Terminal Here and the embedded terminal.
    QGroupBox *createTerminalProviderGroup();
    /// The Atuin group of the Terminal tab: the history search action and the session it opens in.
    /// @p embedded is the embedded terminal choice, whose component the search needs.
    QGroupBox *createAtuinHistoryGroup(QComboBox *embedded);
    /// The Applications › Editor tab.
    QWidget *createEditorIntegrationPage();
    /// The Provider group of the Editor tab: the editor that Edit opens text files in, and where terminal editors run.
    QGroupBox *createEditorProviderGroup();
    /// The Markdown group of the Editor tab: the vault folder and how the built-in editor previews and links Markdown notes.
    QGroupBox *createEditorMarkdownGroup();
    /// The Workspace vault group of the Editor tab: each workspace's vault location and its own Markdown options.
    QGroupBox *createEditorWorkspaceVaultGroup();
    /// The Preview Editing group of the Editor tab: whether and how text previews offer editing.
    QGroupBox *createEditorPreviewGroup();
    /// The Applications › Office tab.
    QWidget *createOfficeIntegrationPage();
    /// The Editing group of the Office tab: the office application that Edit opens documents in.
    QGroupBox *createOfficeEditorGroup();
    /// The Previews group of the Office tab: whether the preview panel shows office documents, and the viewer parts it uses.
    QGroupBox *createOfficePreviewGroup();
    /// The Applications › Media tab.
    QWidget *createMediaIntegrationPage();
    /// The Playback group of the Media tab: the backend that plays audio and video in the preview panel and gallery.
    QGroupBox *createMediaBackendGroup();
    /// The Applications › Ebooks and Fonts tab.
    QWidget *createEbookFontIntegrationPage();
    /// The Document Previews group of the Ebooks and Fonts tab: which ebook, comic and help file formats the preview panel shows.
    QGroupBox *createEbookPreviewGroup();
    /// The Fonts group of the Ebooks and Fonts tab: whether the preview panel shows fonts, and their sample text.
    QGroupBox *createFontPreviewGroup();
    /// The Applications › Packages tab.
    QWidget *createPackagesIntegrationPage();
    /// The Actions group of the Packages tab: which package families Application Package offers actions for, and the tools they need.
    QGroupBox *createPackageActionsGroup();
    /// The AppImage Menu Entries group of the Packages tab: whether and how AppImages are added to the application menu.
    QGroupBox *createAppImageIntegrationGroup(QCheckBox *appImages);
    /// The Applications › Mail tab.
    QWidget *createMailIntegrationPage();
    /// The Client group of the Mail tab: the default mail client, and whether Send by Email is offered.
    QGroupBox *createMailClientGroup();
    /// The Remote Files group of the Mail tab: whether Send by Email asks before preparing local copies of remote files or skips them.
    QGroupBox *createMailRemoteFilesGroup();
    /// The Mail File Preview group of the Mail tab: whether the preview panel shows mail files as messages.
    QGroupBox *createMailPreviewGroup();
    /// The Remote and Cloud › Connections tab.
    QWidget *createRemoteConnectionsIntegrationPage();
    /// The Saved Connections group of the Connections tab: @p editor, which saves changes immediately.
    QGroupBox *createRemoteProfilesGroup(RemoteProfilesEditor *editor);
    /// The Defaults for New Connections group of the Connections tab, which @p editor's new connections start with.
    QGroupBox *createRemoteDefaultsGroup(RemoteProfilesEditor *editor);
    /// The Local Network group of the Connections tab, which shows or hides Go › Network.
    QGroupBox *createRemoteDiscoveryGroup();
    /**
     * The Remote and Cloud › rclone tab: whether Go › Cloud Storage offers
     * rclone's remotes, which of them, and whether it mounts or serves them.
     */
    QWidget *createRcloneIntegrationPage();
    /**
     * Creates the Remote and Cloud › Remote Drives tab, which chooses how saved
     * connections are mounted by default, where, and when they are mounted
     * and unmounted automatically.
     */
    QWidget *createRemoteDrivesIntegrationPage();
    /// Shows the Editor › Editor tab, with the built-in editor's settings.
    void showBuiltInEditorSettings();
    /// The main window's Activity controller, which reads the Activities, if the dialog has one.
    ActivityController *activityController() const;
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
    KPageWidgetItem *m_workspacePage = nullptr;
    QTabWidget *m_workspaceTabs = nullptr;
    KPageWidgetItem *m_editorPage = nullptr;
    QTabWidget *m_editorTabs = nullptr;
    KPageWidgetItem *m_toolsPage = nullptr;
    QTabWidget *m_toolsTabs = nullptr;
    KPageWidgetItem *m_plasmaPage = nullptr;
    QTabWidget *m_plasmaTabs = nullptr;
    ActivityAssociationsEditor *m_activityAssociations = nullptr;
    HiddenItemsList *m_runnerWorkspaces = nullptr;
    HiddenItemsList *m_runnerFavorites = nullptr;
    DesktopWorkspacesEditor *m_desktopWorkspaces = nullptr;
    /// The virtual desktop workspaces when they were last loaded or saved, to tell whether the dialog changed them.
    QMap<QString, QString> m_savedDesktopWorkspaces;
    std::function<bool()> m_confirmKWinRemoval;
    WorkspaceCombo *m_dropDownWorkspace = nullptr;
    WorkspaceVaultSettings *m_workspaceVaultSettings = nullptr;
    KKeySequenceWidget *m_dropDownShortcut = nullptr;
    /// The drop-down window's global shortcut when it was last loaded or assigned, to tell whether the dialog changed it.
    QKeySequence m_savedDropDownShortcut;
    /// Read and assign the drop-down window's global shortcut; empty without a main window.
    std::function<QKeySequence()> m_readDropDownShortcut;
    std::function<void(const QKeySequence &)> m_writeDropDownShortcut;
    /// Enables the drop-down shortcut and shows whether it can work.
    std::function<void()> m_refreshDropDownShortcut;
    /// The Activity associations when they were last loaded or saved, to tell whether the dialog changed them.
    QList<ActivityAssociations::Association> m_savedActivityAssociations;
    QLabel *m_yaziWorkspaceDefault = nullptr;
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
    YaziColumnRatiosEditor *m_yaziColumnRatios = nullptr;
    /// The YaziColumnRatios setting when it was last saved, to tell whether the dialog changed it.
    QList<int> m_savedYaziColumnRatios;
    QComboBox *m_yaziKeyPreset = nullptr;
    QTreeWidget *m_yaziKeyBindings = nullptr;
    QLabel *m_yaziKeyCounts = nullptr;
    QLabel *m_yaziKeyConflictSummary = nullptr;
    QListWidget *m_yaziKeyConflicts = nullptr;
    /// The YaziKeyPreset setting when it was last saved, to tell whether the dialog changed it.
    int m_savedYaziKeyPreset = 0;
    /// The preset whose conflicts were accepted, so that OK asks only once; -1 when none.
    int m_confirmedYaziKeyPreset = -1;
    std::function<bool(const QString &, const QStringList &)> m_confirmYaziKeys;
    std::function<bool(const QString &)> m_confirmYaziReset;
    std::function<bool()> m_portalAvailable;
    std::function<QString()> m_restartPortal;
    std::function<bool(const QString &)> m_confirmPortalRestart;
    KKeySequenceWidget *m_globalShortcutKey = nullptr;
    /// Shows whether the applied global shortcut works.
    std::function<void()> m_refreshGlobalShortcut;
    /// Whether Yazi mode defaults were restored since the settings were last applied or reloaded.
    bool m_yaziResetPending = false;
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
