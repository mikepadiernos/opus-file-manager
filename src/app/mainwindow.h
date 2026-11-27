// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "activityassociations.h"
#include "ark.h"
#include "editorpanel.h"
#include "editorproviders.h"
#include "edittools.h"
#include "fileencryption.h"
#include "metadataeditdialog.h"
#include "officeapps.h"
#include "previewpanel.h"
#include "rclone.h"
#include "remoteprofiles.h"
#include "tagpanel.h"
#include "workspacecatalog.h"
#include "yazimode.h"

#include <KConfigWatcher>
#include <KFileItem>
#include <KXmlGuiWindow>

#include <QHash>
#include <QList>
#include <QPointer>
#include <QUrl>
#include <memory>

class ActivityController;
class CloudActivityDialog;
class CloudSyncDialog;
namespace CloudActivity
{
class Monitor;
struct Limits;
}
namespace CloudSync
{
class Scheduler;
}
class KdeConnectClient;
class PlasmaVaultMenu;
class VaultClient;
class MountBackend;
class RcloneMounts;
class RemoteMountManager;
class DropDownWindow;
class KWinBackend;
class DiskImageBackend;
class PackageTools;
class IndexBackend;
class KWinIntegration;
class CombinedPanel;
class DesktopTray;
class FavoritesModel;
class WorkspaceLocations;
class QFileSystemWatcher;
class FolderTree;
class InformationPanel;
class KFileCopyToMenu;
class KConfig;
class KFileItemActions;
class KFilePlacesModel;
class KFilePlacesView;
class KNewFileMenu;
class KSelectAction;
class KToggleAction;
class Pane;
class PreviewPanel;
class SearchPanel;
class TaggingController;
class TabColors;
class TerminalPanel;
class ToolbarCustomizer;
class QDockWidget;
class QDialog;
class QLabel;
class QHBoxLayout;
class QMenu;
class QSplitter;
class QTabWidget;
class QTimer;
class QToolButton;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Opens each URL in a new tab of the active pane; restores the
    /// session (or the home folder) when the list is empty.
    void openUrls(const QList<QUrl> &urls);

    /// Opens each folder in a new tab of the active pane, restoring the
    /// session first if needed, as the FileManager1 service requests.
    void showFolders(const QList<QUrl> &folders);
    /// Opens the folder of each item in a new tab of the active pane, one tab
    /// per folder, and selects the requested items in it.
    void showItems(const QList<QUrl> &items);
    /// Shows the KDE properties dialog for the items.
    void showItemProperties(const QList<QUrl> &items);

    Pane *activePane() const;
    Pane *inactivePane() const;

    ActivityController *activityController() const;
    /// Opens Preferences › KDE Plasma › Activities, where Plasma Activities are associated with workspaces and default folders.
    void showActivityAssociations();
    /**
     * Applies an Activity association the user chose to apply: switches to its
     * workspace, asking first as a workspace switch does, and opens its
     * default folders in new tabs of the first and second pane.
     */
    void applyActivityPlan(const ActivityAssociations::Plan &plan);
    /**
     * Switches to the saved workspace with @p id, asking first as any
     * workspace switch does, or reports in the status bar that it no longer
     * exists. KRunner and `--workspace` open workspaces this way.
     */
    void openWorkspace(const QString &id);

    /**
     * Splits the panes top/bottom instead of side by side. When stacked,
     * the top pane's tabs run along its bottom edge and the bottom pane's
     * tabs along its top edge, so the two tab bars meet in the middle.
     * @param stacked true for top/bottom, false for side by side
     */
    void setStackedLayout(bool stacked);
    bool isStackedLayout() const;

    /// Which side panels the window shows.
    enum class PanelLayout {
        /// One panel with Favorites, the Places sections and a folder tree.
        Combined,
        /// Separate Places and Folders panels.
        Separate,
    };

    /**
     * Shows either the combined panel or the separate Places and Folders
     * panels, and remembers the choice for new windows.
     * @param layout the panels to show
     */
    void setPanelLayout(PanelLayout layout);
    PanelLayout panelLayout() const;

    FavoritesModel *favorites() const;
    TabColors *tabColors() const;

    bool isDualLayout() const;

    /// The system tray icon and the global shortcut that shows or hides the
    /// window.
    DesktopTray *desktopTray() const;

    /// Runs the two-way cloud synchronization pairs.
    CloudSync::Scheduler *cloudSync() const;
    /// Opens the Cloud Sync dialog, or raises it when it is open.
    void showCloudSync();
    /// Keeps transfer speeds and recent errors for the Cloud Activity window.
    CloudActivity::Monitor *cloudActivity() const;
    /// Opens the Cloud Activity window, or raises it when it is open.
    void showCloudActivity();
    /// Applies rclone transfer limits to sync runs and later mounts.
    void applyCloudLimits(const CloudActivity::Limits &limits);
    /**
     * Waits for files written to @p remotes, or to every mounted rclone remote
     * when it is empty, to upload, and for cloud sync runs when @p syncRuns is
     * set. Returns false when the user cancels.
     */
    bool waitForCloudUploads(const QString &text, const QStringList &remotes, bool syncRuns);

    /// The KDE Connect daemon the Send to Device action uses, created when first needed.
    KdeConnectClient *kdeConnectClient();
    /// Replaces the KDE Connect client and takes ownership of it.
    void setKdeConnectClient(KdeConnectClient *client);
    /// The Go › Plasma Vaults menu's controller.
    PlasmaVaultMenu *plasmaVaultMenu() const;
    /// Replaces the Plasma Vault client and takes ownership of it.
    void setVaultClient(VaultClient *client);
    /// Shows the dialog that sends the selected files to a KDE Connect device.
    void sendToDevice();
    /// Shows the dialog that opens a new message with the selected files attached.
    void sendByEmail();
    /// Shares the selected local items, or the current folder, on the local network.
    void shareOnNetwork();
    /**
     * Switches the active pane between a local folder and the same folder
     * through KIO's admin: worker, after warning about the risks. Reports a
     * missing kio-admin package and unsupported locations.
     */
    void setActingAsAdministrator(bool enabled);

    /// The file systems the mount manager and the Mount Points menu list, created when first needed.
    MountBackend *mountBackend();
    /// Replaces the mount backend and takes ownership of it.
    void setMountBackend(MountBackend *backend);
    /// Shows the mount manager.
    void showMountManager();
    /// Fills the Mount Points menu with each file system and its state.
    void populateMountPoints();
    /// Fills the Remote Connections menu with each saved connection.
    void populateRemoteConnections();
    /// Shows the dialog that manages saved remote connections.
    void showRemoteConnections();
    /// Fills the Online Accounts menu with the folders of each KDE Online Account.
    void populateOnlineAccounts();
    /// Opens @p profile in the active pane, or in the other pane when @p otherPane is set.
    void openRemoteProfile(const RemoteProfiles::Profile &profile, bool otherPane);
    /// Opens an SSH session for @p profile in the terminal panel.
    void openSshTerminal(const RemoteProfiles::Profile &profile);
    /// The rclone mounts this window started, created on first use.
    RcloneMounts *rcloneMounts();
    /// Fills the Cloud Storage menu from the last list of rclone remotes.
    void populateCloudStorage();
    /// Mounts an rclone remote if needed and opens it in the active pane.
    void openRcloneRemote(const QString &remote);
    /// Leaves the remote's folder in either pane and stops its mount.
    void unmountRcloneRemote(const QString &remote);
    /// The remote connections this window mounted as folders, created on first use.
    RemoteMountManager *remoteMounts();
    /// Mounts @p profile with its saved mount options, opening it in the active pane when @p open is set.
    void mountRemoteProfile(const RemoteProfiles::Profile &profile, bool open);
    /// Leaves the mounted folder in either pane and unmounts it.
    void unmountRemoteProfile(const QString &id);
    /// Shows how @p profile is mounted as a folder.
    void showRemoteMountOptions(const RemoteProfiles::Profile &profile);
    /// Removes Places entries left by earlier mounts and mounts the connections set to mount at startup.
    void mountRemoteProfilesAtStartup();

    /// The optional Plasma Opus KWin script and its D-Bus interface.
    KWinIntegration *kwinIntegration() const;
    /// Replaces the KWin backend, for tests.
    void setKWinBackend(std::unique_ptr<KWinBackend> backend);
    /// Shows Preferences › KDE Plasma › KWin.
    void showKWinIntegration();
    /// Shows the drop-down window, or hides it when it is shown and active.
    void toggleDropDown();
    DropDownWindow *dropDownWindow() const;
    /// Replaces the detected Ark backends, for tests.
    void setArkBackends(const Ark::Backends &backends);
    /// Replaces the disk image backend, for tests.
    void setDiskImageBackend(std::unique_ptr<DiskImageBackend> backend);
    /// Replaces the package tools the Application Package dialog uses, for tests.
    void setPackageTools(PackageTools *tools);
    /// Opens the Application Package dialog for the selected AppImage, Flatpak or snap package.
    void showApplicationPackage();
    /// Replaces the file indexer the Indexing Status dialog and the search panel report on, for tests.
    void setIndexBackend(IndexBackend *backend);
    /// Opens the Indexing Status dialog for the active pane's folder.
    void showIndexStatus();
    /**
     * Opens a file-type place from Places, such as Ebooks under Search For.
     * Shows the category's Baloo search in @p pane when the index can answer
     * it, confirming first when the index may be incomplete. Otherwise
     * explains why and offers a live search of a chosen folder.
     */
    void openFileTypePlace(Pane *pane, const QUrl &url, bool newTab);
    /**
     * Searches @p folder live for the files of the file-type place @p url in
     * the search panel.
     * @returns whether the search started
     */
    bool searchFileTypeLive(const QUrl &url, const QUrl &folder);
    /**
     * Adds the selected local items to @p archivePath with Ark, after a
     * confirmation, since Ark edits the archive in place. Refuses formats the
     * installed backends cannot write and an archive inside the selection.
     * @returns whether Ark was asked to add the items
     */
    bool addSelectionToArchive(const QString &archivePath);

    /**
     * Applies the current Settings to the panes, actions and status bar.
     * Called on start and whenever the settings dialog saves.
     */
    void applySettings();

    /**
     * Resets the global appearance options to Plasma's defaults, saves them
     * and applies them, as requested with --reset-appearance.
     */
    void resetAppearance();

Q_SIGNALS:
    void workspaceCatalogChanged();

public Q_SLOTS:
    void configureToolbars() override;

protected:
    bool queryClose() override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    void childEvent(QChildEvent *event) override;
    void readProperties(const KConfigGroup &group) override;
    void saveProperties(KConfigGroup &group) override;
    void saveNewToolbarConfig() override;

private:
    /// Opens the workspace selector and management dialog.
    void showWorkspaces();
    void selectWorkspace(int index, bool confirmed = false, const std::optional<WorkspaceCatalog::BehaviorSelection> &behaviors = std::nullopt);
    /// The behaviours that apply to @p workspace: an applied Activity association's, or its own.
    WorkspaceCatalog::BehaviorSelection workspaceBehaviors(const WorkspaceCatalog::Workspace &workspace) const;
    /// Asks, if enabled, and returns to the active workspace's saved layout.
    void restoreActiveWorkspaceLayout();
    /// Applies the configured colour scheme and reports when it cannot be used.
    void applyColorScheme();
    /// Reapplies colours and effects that depend on the high contrast preference.
    void applyContrastDependentAppearance();
    /// Applies the configured tint and translucency of the pane backgrounds.
    void applyPaneBackgrounds();
    /// Applies background blur, combining the setting with the terminal's request.
    void applyWindowEffects();
    /// Makes the window and docked panel backgrounds as translucent as configured.
    void applyBackgroundOpacity();
    void applyPanelOpacity(QDockWidget *dock);
    /// Applies the app-local corner radius, dividers and other style rules.
    void applyAppearanceStyle();
    /// Applies the custom stylesheet after every other appearance option, or removes it.
    void applyCustomStyleSheet();
    /// Redraws folder colours on icons, names, tabs and breadcrumbs after they or their style change.
    void refreshFolderColors();
    /// Applies the app-local fonts of file lists, the folder trees, tabs and status bars.
    void applyFonts();
    /// Applies the appearance options in effect, including the active workspace's, without other settings.
    void applyAppearance();
    /// Scales the margins of the toolbars, navigation bars and status bars.
    void applyBarMargins();
    /**
     * Layers the theme profile and appearance options attached to the active
     * workspace over the global settings, and reports what is unavailable.
     */
    void updateWorkspaceAppearanceLayer();
    /// Applies the appearance layer in effect when it differs from the one last applied.
    void applyAppearanceLayer();
    /// Applies the active workspace's appearance live when it or the workspace changes.
    void applyWorkspaceAppearance();
    /**
     * Previews @p appearance over the global settings without saving it, or
     * restores the active workspace's appearance when @p appearance is null.
     * Open panes keep their view mode.
     */
    void previewWorkspaceAppearance(const WorkspaceCatalog::Appearance *appearance);
    /// Places the workspace selector in the View menu, the status bar or both.
    void applyWorkspaceSelectorPlacement();
    /// Gives each pane its configured location bar, replacing the current choice only when the setting changes.
    void applyLocationPresentationDefaults();
    void refreshWorkspaceSelector();
    void scheduleWorkspaceSelectorRefresh();
    /// Loads workspace assignments and updates their presentation.
    void reloadWorkspaceCatalog();
    /// Applies current workspace visibility to the navigation panels.
    void refreshWorkspaceLocations();
    /// The active workspace's editor vault: its local folder, or why it can't be found, and its options.
    EditorPanel::WorkspaceVault workspaceVault() const;
    /// Watches the workspace configuration for changes from other windows.
    void watchWorkspaceCatalog();
    bool captureWorkspaceLayout(WorkspaceCatalog::Layout &layout, QString &error) const;
    bool applyWorkspaceLayout(const WorkspaceCatalog::Layout &layout, QString &error);
    bool workspaceLayoutAvailable(const WorkspaceCatalog::Layout &layout, QStringList &unavailable, QString &error) const;
    bool restoreWorkspaceLayout(const WorkspaceCatalog::Layout &layout, QString &error, bool &changed);
    void restoreWorkspacePanePlacement(WorkspaceCatalog::PanePlacement placement);
    void restoreWorkspaceEditor(const WorkspaceCatalog::Presentation &presentation);
    void setupActions();
    void setupPanes();
    void updateTerminalPlacement();
    /// Keeps the search panel beside the terminal when their orientations match, otherwise in the outer search splitter.
    void updateSearchPlacement();
    void setupPlaces();
    /**
     * Adds the shown file-type places to Places on first use, then keeps the
     * FileTypeCategoriesHidden setting in step with the entries removed from
     * or added to Places, so that removed entries stay removed.
     */
    void addFileTypePlaces();
    /// Adds the shown file-type places missing from Places and removes the hidden ones.
    void syncFileTypePlaces();
    /// Records the file-type places missing from Places as hidden.
    void rememberFileTypePlaces();
    void setupPanels();
    void setupStatusBar();
    void applyPanelSettings();
    void applyEmbeddedPanelTitles();
    /// Creates or removes the double Shift search trigger to follow its setting.
    void updateDoubleShift();

    void setActivePane(Pane *pane);
    void connectPane(Pane *pane);
    void updateActions();
    void updateFolderGateActions();
    void updateStatusBarIndicatorOrder();
    void updateStatusBar();
    void updateFreeSpace(Pane *pane = nullptr);
    void updateWindowTitle();
    void updateTabBars();
    void showStatusMessage(const QString &message);
    void recordFolderVisit(Pane *pane, const QUrl &url);
    void savePopularFolderVisits();
    void populatePopularFolders();
    void populateSelectionFilters();
    void applySelectionFilter(const QString &pattern);
    void selectByPattern(bool select);
    void selectByPathFilter(bool select);
    void selectByQuery(bool select);
    void saveSelectionFilter();
    void calculateOccupiedSpace();
    void jumpBack();
    void testArchiveIntegrity();
    void testArchiveFile(const QString &fileName, const QString &password = QString());

    void restoreSession();
    void saveSession();

    void showContextMenu(Pane *pane, const QPoint &globalPos, const KFileItemList &items);
    /**
     * Runs a Yazi mode file command of @p pane through the window's own
     * action, so its confirmations, conflict dialogs, transfer queue and undo
     * apply. Never runs an action that deletes permanently.
     */
    void runYaziFileCommand(Pane *pane, YaziMode::Command command);
    /// Where a menu that a Yazi mode key opens should appear: below @p pane's focused item.
    QPoint yaziMenuPosition(Pane *pane) const;
    void addTransferMenu(QMenu *menu, const KFileItemList &items, bool move);
    void activateItem(const KFileItem &item);
    void activatePaneItem(Pane *pane, const KFileItem &item);
    void updatePreviewPanel();
    void updateInformationPanel();
    void openGallery();
    void openEditor();
    /**
     * Shows the editor panel where the user's opening preference places it,
     * first opening @p url when it isn't empty. Returns false if the file
     * couldn't be opened or the user kept the editor's unsaved changes.
     */
    bool showEditor(const QUrl &url);
    void editFile();
    void editWithEditor(EditorProviders::Editor editor);
    void openInOfficeApp(OfficeApps::App app);
    void updateTerminalPanelVisibility();
    void compareFiles();
    void createSymlink();
    void exportListing(bool pdf);
    void showContentChart();
    void showSearch();
    void showSearchForPane(Pane *pane);
    /**
     * Shows the search panel for a search of @p pane's folder, as Yazi mode's
     * search keys do, and reports the search's progress and outcome in the
     * pane's Yazi status row.
     * @param contents true to search file contents, false to search file names
     */
    void showYaziSearch(Pane *pane, bool contents);
    /// Shows @p status in the Yazi status row of the pane that started the current search from Yazi mode.
    void reportYaziSearch(const QString &status, bool running);
    /// Searches the active pane's folder from a toolbar search field.
    void searchFromToolbar(const QString &text);
    /// Shows the search panel for a toolbar search when Preferences ask for it, or reports the outcome in the status bar.
    void presentToolbarSearch(bool started);
    void showDuplicateFinder();
    void createChecksumFile();
    void verifyChecksumFile();
    void comparePaneFolders();
    void compareFoldersFiltered();
    void syncLinkedPane(Pane *pane, const QUrl &url);
    void swapPanesAccordingToPreference();
    void swapCurrentTabs();
    void swapPaneSides();
    void restoreSecondPanePlacement();
    void moveSecondPaneToPreferredDestination();
    void moveSecondPaneToTab();
    void popOutSecondPane();
    void moveSecondPaneToDual();
    void resetMovedPaneDisplayStyle();
    void saveSplitterSizes();
    void restoreSplitterSizes(bool panes = true, bool workspace = true, bool defaultsIfMissing = false);
    void resetSplitterSizes();
    /// Moves the divider between the file panes towards the end (1) or start (-1).
    void movePaneDivider(int direction);
    void restoreEditorDockSize();
    void savePanelDockSize(QDockWidget *dock, const QString &name);
    void restorePanelDockSize(QDockWidget *dock, const QString &name);
    void updateTabBarPlacement();
    void splitFile();
    void combineFiles();
    /// Opens the encryption dialog for the selected files that @p operation applies to.
    void encryptSelection(FileEncryption::Operation operation);
    /// Hands the selected files to Kleopatra's sign-and-encrypt or decrypt-and-verify action.
    void openSelectionInKleopatra(bool decrypt);
    /// Enables the Encryption menu's entries for the selection and the installed tools.
    void updateEncryptionActions();
    void setupEditor();
    void setEditorPlacement(EditorPanel::Placement placement);
    void setPreviewPlacement(PreviewPanel::Placement placement);
    void setMetadataPlacement(MetadataEditDialog *dialog, QDockWidget *dock, MetadataEditDialog::Placement placement);
    void setTagPanelPlacement(TagPanel::Placement placement);
    /// Stores the tag panel placement in the TagPanelPlacement setting.
    void saveTagPanelPlacement(bool below);
    /// Applies Preferences › Tagging › Tags to the tag panel and open tag managers.
    void applyTagSettings();
    /// Shows the active pane's folder and selection in the tag panel while it is shown.
    void updateTagPanel();
    /// Shows the current state of the tag library at @p root after it was created, removed, moved or refreshed.
    void reloadLibraryViews(const QString &root);

    void copyToOtherPane();
    void moveToOtherPane();
    void cut();
    void copy();
    void paste();
    void rename();
    void batchRename();
    void editMetadata();
    /// Opens the embedded metadata editor for the file at @p path, which previews and confirms each write.
    void openMetadataEditor(const QString &path);
    void showCustomCommand();
    void showCustomCommandPreset(const QString &name);
    void refreshCustomCommandActions();
    void showTransferQueue();
    void trash();
    void deletePermanently();
    void showProperties();
    void openTerminal();
    void newFolder();
    void showNewMenu();
    void toggleLayout(bool dual);
    void showSettings();
    void showToolbarSettings();
    /// Applies the settings that only take effect in a new window.
    void applyViewDefaults();
    /// Stores whether the user left the Places panel shown.
    void rememberPlacesShown(bool shown);
    void toggleViewMode();
    void setShowHiddenFiles(bool show);
    void setPreviewsEnabled(bool enabled);
    /// Reveals the active pane's folder in the trees and matches its hidden files setting.
    void syncFolderTrees();
    /// Shows the active pane's folder flat or not, and remembers the choice for that folder.
    void setFlat(bool flat);
    /// Shows the context menu for a folder in a tree.
    void showFolderTreeMenu(const QUrl &url, const QPoint &globalPos);
    /// Adds the selected folders, or the current folder, to Favorites.
    void addToFavorites();
    /**
     * Extracts each selected archive into a new folder named after it in
     * the other pane's folder, never overwriting an existing one.
     */
    void extractToOtherPane();
    void extractProtectedToOtherPane();
    /**
     * Asks how to compress the selected local items, then writes the archive
     * in the background, never replacing an existing file.
     */
    void compress();
    void setSelectionLabel(const QString &label);
    void setSelectionColor(const QString &color);
    void setSelectionRating(int rating);
    void setSelectionNote();
    void toggleSelectionPin();
    void editFolderLook(const QUrl &folder);
    /**
     * Copies the top-level entries of an archive into a new folder.
     * @param archive the archive's browse URL from Archives::browseUrl()
     * @param target the folder to create; it must not exist
     */
    void extractArchive(const QUrl &archive, const QUrl &target);
    /// Opens the selected archive in Ark, which can add, rename and remove entries.
    void openInArk();
    /// Asks for an archive Ark can write and adds the selection to it.
    void addToArchive();
    /// What the installed Ark backends support, read once.
    const Ark::Backends &arkBackends();
    /// Whether @p item is a local archive that Ark reads but KIO cannot browse, such as RAR.
    bool isArkOnlyArchive(const KFileItem &item);
    /// Starts Ark with @p arguments, reporting a missing Ark or a failed start.
    void launchArk(const QStringList &arguments);
    /// The selected item when it is exactly one local disk image, otherwise a null item.
    KFileItem selectedDiskImage() const;
    /**
     * Mounts or unmounts the selected disk image through Dolphin's Mount ISO
     * plugin, then reports the result Solid shows.
     */
    void setDiskImageMounted(bool mount);
    /// The selected item when it is exactly one local AppImage, Flatpak or snap package, otherwise a null item.
    KFileItem selectedPackage() const;

    std::optional<Ark::Backends> m_arkBackends;
    std::unique_ptr<DiskImageBackend> m_diskImages;
    PackageTools *m_packageTools = nullptr;
    IndexBackend *m_indexBackend = nullptr;

    KFilePlacesModel *m_placesModel = nullptr;
    /// Set while syncFileTypePlaces() edits Places, so that its own changes aren't recorded one at a time.
    bool m_syncingFileTypePlaces = false;
    DesktopTray *m_desktopTray = nullptr;
    CloudSync::Scheduler *m_cloudSync = nullptr;
    QPointer<CloudSyncDialog> m_cloudSyncDialog;
    CloudActivity::Monitor *m_cloudActivity = nullptr;
    QPointer<CloudActivityDialog> m_cloudActivityDialog;
    ActivityController *m_activityController = nullptr;
    KdeConnectClient *m_kdeConnect = nullptr;
    MountBackend *m_mountBackend = nullptr;
    KWinIntegration *m_kwin = nullptr;
    DropDownWindow *m_dropDown = nullptr;
    /// The file system the Mount Points menu mounts and then opens.
    QString m_openAfterMount;
    RcloneMounts *m_rcloneMounts = nullptr;
    QMenu *m_cloudStorageMenu = nullptr;
    QMenu *m_onlineAccountsMenu = nullptr;
    PlasmaVaultMenu *m_plasmaVaultMenu = nullptr;
    QList<Rclone::Remote> m_rcloneRemotes;
    QString m_rcloneError;
    bool m_rcloneListed = false;
    QString m_openAfterRcloneMount;
    RemoteMountManager *m_remoteMounts = nullptr;
    /// The connection to open once mounted.
    QString m_openAfterRemoteMount;
    /// The names and folders of mounted connections, by profile identifier.
    QHash<QString, QString> m_remoteMountNames;
    QHash<QString, QString> m_remoteMountPaths;
    /// Behaviours an applied Activity association chose for its workspace, until another workspace is chosen.
    std::optional<WorkspaceCatalog::BehaviorSelection> m_activityBehaviors;
    QString m_activityBehaviorsWorkspaceId;
    bool m_applyingActivity = false;
    std::unique_ptr<KConfig> m_workspaceConfig;
    QString m_currentWorkspaceId;
    WorkspaceCatalog::Catalog m_workspaceCatalog;
    /// The workspace whose appearance this window last applied, and what was attached to it.
    QString m_appearanceWorkspaceId;
    WorkspaceCatalog::Appearance m_workspaceAppearance;
    bool m_workspaceAppearanceApplies = false;
    QStringList m_workspaceAppearanceProblems;
    /// The layered appearance values this window last applied.
    QVariantMap m_appliedAppearanceLayer;
    WorkspaceLocations *m_workspaceLocations = nullptr;
    QFileSystemWatcher *m_workspaceWatcher = nullptr;
    QString m_workspaceConfigPath;
    /// The last query used to select or deselect items.
    QString m_selectionQuery;
    qint64 m_workspaceConfigModified = -1;
    QTimer *m_workspaceIndicatorTimer = nullptr;
    /// Watches Plasma's colours, so its accent can follow over an app-local scheme.
    KConfigWatcher::Ptr m_plasmaColorsWatcher;
    KSelectAction *m_workspaceSelectorAction = nullptr;
    QToolButton *m_workspaceStatusButton = nullptr;
    KFilePlacesView *m_placesView = nullptr;
    FavoritesModel *m_favorites = nullptr;
    TabColors *m_tabColors = nullptr;
    QDockWidget *m_placesDock = nullptr;
    QDockWidget *m_foldersDock = nullptr;
    QDockWidget *m_combinedDock = nullptr;
    QDockWidget *m_informationDock = nullptr;
    FolderTree *m_folderTree = nullptr;
    CombinedPanel *m_combinedPanel = nullptr;
    KSelectAction *m_panelLayoutAction = nullptr;
    KToggleAction *m_flatAction = nullptr;
    /// Folders shown flat, as URL strings; kept in the state config.
    QStringList m_flatLocations;
    PanelLayout m_panelLayout = PanelLayout::Combined;
    KFileItemActions *m_fileItemActions = nullptr;
    TaggingController *m_tagging = nullptr;
    TagPanel *m_tagPanel = nullptr;
    QDockWidget *m_tagDock = nullptr;
    KFileCopyToMenu *m_copyToMenu = nullptr;
    KNewFileMenu *m_newFileMenu = nullptr;
    QSplitter *m_splitter = nullptr;
    QSplitter *m_workspaceSplitter = nullptr;
    /// Holds the workspace splitter and, when it is placed separately from the terminal, the search panel.
    QSplitter *m_searchSplitter = nullptr;
    QTabWidget *m_paneTabs = nullptr;
    QWidget *m_rightPlaceholder = nullptr;
    QPointer<QMainWindow> m_rightWindow;
    TerminalPanel *m_terminalPanel = nullptr;
    SearchPanel *m_searchPanel = nullptr;
    /// Opens search on two Shift taps, while enabled and available.
    QObject *m_doubleShift = nullptr;
    /// Whether a terminal profile asks for blur behind the window.
    bool m_terminalBlur = false;
    EditorPanel *m_editor = nullptr;
    QDockWidget *m_editorDock = nullptr;
    QDialog *m_editorWindow = nullptr;
    EditorPanel::Placement m_editorPlacement = EditorPanel::Placement::Panel;
    bool m_movingEditor = false;
    Pane *m_left = nullptr;
    Pane *m_right = nullptr;
    Pane *m_active = nullptr;
    PreviewPanel *m_previewPanel = nullptr;
    QDockWidget *m_previewDock = nullptr;
    QList<QPointer<QDockWidget>> m_metadataDocks;
    int m_metadataDockSerial = 0;
    PreviewPanel::Placement m_previewPlacement = PreviewPanel::Placement::Pane;
    bool m_movingPreview = false;
    InformationPanel *m_informationPanel = nullptr;
    ToolbarCustomizer *m_toolbarCustomizer = nullptr;
    Pane *m_hoveredInformationPane = nullptr;
    KFileItem m_hoveredInformationItem;
    QLabel *m_itemsLabel = nullptr;
    QLabel *m_messageLabel = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;
    QHBoxLayout *m_statusBarLayout = nullptr;
    QHash<Pane *, QLabel *> m_paneItemsLabels;
    QHash<Pane *, QLabel *> m_paneFreeSpaceLabels;
    KToggleAction *m_dualLayoutAction = nullptr;
    KToggleAction *m_yaziModeAction = nullptr;
    KToggleAction *m_yaziPreviewAction = nullptr;
    KToggleAction *m_yaziPreviewInformationAction = nullptr;
    KToggleAction *m_yaziParentColumnAction = nullptr;
    QToolButton *m_yaziModeStatusButton = nullptr;
    KToggleAction *m_yaziVimKeysAction = nullptr;
    QList<QAction *> m_yaziColumnActions;
    KToggleAction *m_stackedLayoutAction = nullptr;
    KToggleAction *m_hiddenFilesAction = nullptr;
    KToggleAction *m_previewsAction = nullptr;
    KToggleAction *m_previewPanelAction = nullptr;
    KToggleAction *m_terminalPanelAction = nullptr;
    KToggleAction *m_searchPanelAction = nullptr;
    KToggleAction *m_linkedPanesAction = nullptr;
    QUrl m_linkedLeftRoot;
    QUrl m_linkedRightRoot;
    bool m_syncingLinkedPanes = false;
    bool m_secondPaneInTab = false;
    bool m_movingSecondPane = false;
    bool m_restoringSearchPanel = false;
    /// Whether a search started from the toolbar field reports its outcome in the status bar while the panel is hidden.
    bool m_reportToolbarSearch = false;
    /// The pane whose Yazi search keys opened the search panel; its Yazi status row follows the search.
    QPointer<Pane> m_yaziSearchPane;
    QAction *m_viewModeAction = nullptr;
    QAction *m_selectionModeAction = nullptr;
    QAction *m_selectionMarkersAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_jumpBackAction = nullptr;
    QMenu *m_selectionFiltersMenu = nullptr;
    bool m_automaticLocalSizeDefault = false;
    bool m_automaticRemoteSizeDefault = false;
    QMenu *m_popularFoldersMenu = nullptr;
    QMenu *m_mountPointsMenu = nullptr;
    QMenu *m_remoteConnectionsMenu = nullptr;
    QHash<QString, int> m_popularFolderVisits;
    QHash<QString, QAction *> m_customCommandActions;
    QHash<Pane *, QUrl> m_lastFolderByPane;
    QHash<Pane *, QUrl> m_jumpBackPoints;
    bool m_jumpingBack = false;
    bool m_autoFocusFolders = true;
    bool m_sessionRestored = false;
    bool m_startupLinkAttempted = false;
};
