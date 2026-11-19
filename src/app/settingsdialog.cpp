// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "density.h"
#include "servicemenus.h"
#include "settings.h"
#include "settingswidgets.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPageWidget>
#include <KSharedConfig>
#include <KUrlRequester>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QMetaEnum>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
/// Largest sidebar icon size, in pixels, that uses symbolic page icons.
constexpr int SymbolicIconMaxSize = 24;

QComboBox *comboBox(const QString &entry, const QStringList &choices)
{
    auto *combo = new QComboBox;
    combo->setObjectName(QStringLiteral("kcfg_") + entry);
    combo->addItems(choices);
    return combo;
}

QCheckBox *checkBox(const QString &entry, const QString &text)
{
    auto *check = new QCheckBox(text);
    check->setObjectName(QStringLiteral("kcfg_") + entry);
    return check;
}

QSpinBox *pixelSpinBox(const QString &entry, int minimum, int maximum)
{
    auto *spin = new QSpinBox;
    spin->setObjectName(QStringLiteral("kcfg_") + entry);
    spin->setRange(minimum, maximum);
    spin->setSuffix(i18nc("@item:valuesuffix pixels", " px"));
    return spin;
}

template<typename Editor>
Editor *bound(Editor *editor, const QString &entry, const char *property)
{
    editor->setObjectName(QStringLiteral("kcfg_") + entry);
    editor->setProperty("kcfg_property", QByteArray(property));
    return editor;
}

/// Adds a density choice and its custom space, enabled only for Custom.
void addDensityRows(QFormLayout *form, const QString &densityEntry, const QString &spaceEntry)
{
    QComboBox *density = comboBox(densityEntry,
                                  {i18nc("@item:inlistbox density", "Normal"),
                                   i18nc("@item:inlistbox density", "Compact"),
                                   i18nc("@item:inlistbox density", "Very compact"),
                                   i18nc("@item:inlistbox density", "Custom")});
    QSpinBox *space = pixelSpinBox(spaceEntry, 0, Density::MaxCustomItemSpace);
    const auto updateCustom = [density, space] {
        space->setEnabled(density->currentIndex() == static_cast<int>(Density::Items::Custom));
    };
    QObject::connect(density, &QComboBox::currentIndexChanged, space, updateCustom);
    updateCustom();
    form->addRow(i18nc("@label:listbox", "Density:"), density);
    form->addRow(i18nc("@label:spinbox", "Space above and below items:"), space);
}

KConfigGroup kioConfirmations()
{
    return KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("Confirmations"));
}

KUrlRequester *folderRequester(const QString &entry)
{
    auto *requester = new KUrlRequester;
    requester->setObjectName(QStringLiteral("kcfg_") + entry);
    requester->setMode(KFile::Directory | KFile::ExistingOnly);
    return requester;
}
}

SettingsDialog::SettingsDialog(KFilePlacesModel *places, QWidget *parent)
    : KConfigDialog(parent, QLatin1String(Name), Settings::self())
    , m_places(places)
{
    addTabbedPage({{createSettingsDialogPage(), i18nc("@title:tab", "Settings Dialog")}}, i18nc("@title:group", "General"), QStringLiteral("configure"));
    addTabbedPage({{createFoldersAndTabsPage(), i18nc("@title:tab", "Folders and Tabs")},
                   {createPreviewsPage(), i18nc("@title:tab", "Previews")},
                   {createConfirmationsPage(), i18nc("@title:tab", "Confirmations")},
                   {createPlacesPage(), i18nc("@title:tab", "Panels")},
                   {createPanelAppearancePage(), i18nc("@title:tab", "Panel Appearance")},
                   {createTabAppearancePage(), i18nc("@title:tab", "Tab Appearance")},
                   {createStatusBarPage(), i18nc("@title:tab", "Status and Location Bars")}},
                  i18nc("@title:group", "Interface"),
                  QStringLiteral("window"));
    addTabbedPage({{createViewGeneralPage(), i18nc("@title:tab", "General")},
                   {createDetailsPage(), i18nc("@title:tab", "Details")},
                   {createIconsPage(), i18nc("@title:tab", "Icons")}},
                  i18nc("@title:group", "View"),
                  QStringLiteral("view-list-icons"));
    addTabbedPage({{createContextMenuActionsPage(), i18nc("@title:tab", "Actions")}, {createServiceMenusPage(), i18nc("@title:tab", "Service Menus")}},
                  i18nc("@title:group", "Context Menu"),
                  QStringLiteral("application-menu"));
    addTabbedPage({{createArchivesPage(), i18nc("@title:tab", "Defaults")}}, i18nc("@title:group", "Archives"), QStringLiteral("package-x-generic"));
    addTabbedPage({{createFileRulesPage(), i18nc("@title:tab", "Rules")}}, i18nc("@title:group", "Files"), QStringLiteral("folder"));
    addTabbedPage({{createOpeningPage(), i18nc("@title:tab", "Opening")},
                   {createGalleryInformationPage(), i18nc("@title:tab", "Information")},
                   {createSlideshowPage(), i18nc("@title:tab", "Slideshow")},
                   {createImageConversionPage(), i18nc("@title:tab", "Image Conversion")}},
                  i18nc("@title:group", "Gallery"),
                  QStringLiteral("view-preview"));
    applySidebarStyle();
    connect(this, &KConfigDialog::settingsChanged, this, [this] {
        applySidebarStyle();
    });
    updateWidgets();
    m_savedPreviewLimit = m_previewLimit->bytes();
    auto *size = findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"));
    auto *position = findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"));
    const auto preview = [this, size, position] {
        applySidebarStyle(size->value(), position->currentIndex() == Settings::EnumSidebarTitlePosition::Below);
    };
    connect(size, &QSpinBox::valueChanged, this, preview);
    connect(position, &QComboBox::currentIndexChanged, this, preview);
}

void SettingsDialog::addTabbedPage(const QList<std::pair<QWidget *, QString>> &tabs, const QString &title, const QString &iconName)
{
    auto *tabWidget = new QTabWidget;
    for (const auto &[tab, tabTitle] : tabs) {
        tabWidget->addTab(tab, tabTitle);
    }
    m_pageIcons.append({addPage(tabWidget, title), iconName});
}

void SettingsDialog::applySidebarStyle()
{
    applySidebarStyle(Settings::sidebarIconSize(), Settings::sidebarTitlePosition() == Settings::EnumSidebarTitlePosition::Below);
}

void SettingsDialog::applySidebarStyle(int size, bool below)
{
    setFaceType(below ? KPageDialog::List : KPageDialog::FlatList);

    for (const auto &[item, iconName] : std::as_const(m_pageIcons)) {
        item->setIcon(size <= SymbolicIconMaxSize ? QIcon::fromTheme(iconName + QStringLiteral("-symbolic"), QIcon::fromTheme(iconName))
                                                  : QIcon::fromTheme(iconName));
    }
    const auto views = pageWidget()->findChildren<QAbstractItemView *>(Qt::FindDirectChildrenOnly);
    for (QAbstractItemView *view : views) {
        view->setIconSize(QSize(size, size));
    }
}

QWidget *SettingsDialog::createSettingsDialogPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Page titles:"),
                 comboBox(QStringLiteral("SidebarTitlePosition"),
                          {i18nc("@item:inlistbox", "Beside the sidebar icons"), i18nc("@item:inlistbox", "Below the sidebar icons")}));
    form->addRow(i18nc("@label:spinbox", "Sidebar icon size:"), pixelSpinBox(QStringLiteral("SidebarIconSize"), 16, 64));
    return page;
}

QWidget *SettingsDialog::createOpeningPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    auto *destination = comboBox(QStringLiteral("FileActivationDestination"),
                                 {i18nc("@item:inlistbox", "Default application"),
                                  i18nc("@item:inlistbox", "Gallery for images and videos"),
                                  i18nc("@item:inlistbox", "Preview panel")});
    destination->setToolTip(i18nc("@info:tooltip", "Other files open in their default application when Gallery is selected."));
    form->addRow(i18nc("@label:listbox", "Open files from panes in:"), destination);
    return page;
}

QWidget *SettingsDialog::createGalleryInformationPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(checkBox(QStringLiteral("GalleryShowInformation"), i18nc("@option:check", "Show image information in new gallery windows")));
    return page;
}

QWidget *SettingsDialog::createSlideshowPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    auto *interval = new QSpinBox;
    interval->setObjectName(QStringLiteral("kcfg_GallerySlideshowInterval"));
    interval->setRange(1, 3600);
    interval->setSuffix(i18nc("@item:valuesuffix seconds", " s"));
    form->addRow(i18nc("@label:spinbox", "Interval in new galleries:"), interval);
    form->addRow(checkBox(QStringLiteral("GallerySlideshowLoop"), i18nc("@option:check", "Loop slideshow")));
    return page;
}

QWidget *SettingsDialog::createImageConversionPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(
        i18nc("@label:listbox", "Default copy format:"),
        comboBox(QStringLiteral("GalleryCopyFormat"), {i18nc("@item:inlistbox", "PNG"), i18nc("@item:inlistbox", "JPEG"), i18nc("@item:inlistbox", "WebP")}));
    auto *quality = new QSpinBox;
    quality->setObjectName(QStringLiteral("kcfg_GalleryCopyQuality"));
    quality->setRange(0, 100);
    form->addRow(i18nc("@label:spinbox", "Default JPEG and WebP quality:"), quality);
    return page;
}

QWidget *SettingsDialog::createFoldersAndTabsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    QComboBox *mode = comboBox(QStringLiteral("StartupMode"),
                               {i18nc("@item:inlistbox", "Restore the last session"),
                                i18nc("@item:inlistbox", "Open the home folder"),
                                i18nc("@item:inlistbox", "Open fixed folders")});
    KUrlRequester *left = folderRequester(QStringLiteral("LeftStartUrl"));
    KUrlRequester *right = folderRequester(QStringLiteral("RightStartUrl"));
    const auto updateFixed = [mode, left, right] {
        const bool fixed = mode->currentIndex() == Settings::EnumStartupMode::FixedFolders;
        left->setEnabled(fixed);
        right->setEnabled(fixed);
    };
    connect(mode, &QComboBox::currentIndexChanged, page, updateFixed);
    updateFixed();

    form->addRow(i18nc("@label:listbox", "On startup:"), mode);
    form->addRow(i18nc("@label:chooser", "First pane:"), left);
    form->addRow(i18nc("@label:chooser", "Second pane:"), right);
    form->addRow(i18nc("@label:chooser", "Home folder:"), folderRequester(QStringLiteral("HomeUrl")));
    form->addRow(i18nc("@label:listbox", "Start with two panes:"),
                 comboBox(QStringLiteral("DualPaneAtStartup"),
                          {i18nc("@item:inlistbox", "As last used"), i18nc("@item:inlistbox", "Always"), i18nc("@item:inlistbox", "Never")}));
    form->addRow(i18nc("@title:group", "Tabs:"), checkBox(QStringLiteral("AlwaysShowTabBar"), i18nc("@option:check", "Always show the tab bars")));
    form->addRow(QString(), checkBox(QStringLiteral("NewTabsAfterCurrent"), i18nc("@option:check", "Open new tabs next to the current tab")));
    form->addRow(i18nc("@title:group", "Panes:"), checkBox(QStringLiteral("TabSwitchesPanes"), i18nc("@option:check", "Tab key switches between the panes")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowNavigationBar"), i18nc("@option:check", "Show the navigation bar of each pane")));
    form->addRow(i18nc("@title:group", "Navigation:"),
                 checkBox(QStringLiteral("FollowSymlinks"), i18nc("@option:check", "Always follow symbolic links to their target")));
    return page;
}

QWidget *SettingsDialog::createViewGeneralPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "View mode in new windows:"),
                 comboBox(QStringLiteral("DefaultViewMode"), {i18nc("@item:inlistbox", "Details"), i18nc("@item:inlistbox", "Icons")}));
    form->addRow(i18nc("@label:listbox", "Sort by in new windows:"),
                 comboBox(QStringLiteral("SortColumn"),
                          {i18nc("@item:inlistbox", "Name"),
                           i18nc("@item:inlistbox", "Size"),
                           i18nc("@item:inlistbox", "Modified"),
                           i18nc("@item:inlistbox", "Permissions"),
                           i18nc("@item:inlistbox", "Owner"),
                           i18nc("@item:inlistbox", "Group"),
                           i18nc("@item:inlistbox", "Type")}));
    form->addRow(QString(), checkBox(QStringLiteral("SortDescending"), i18nc("@option:check", "Descending")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowHiddenFiles"), i18nc("@option:check", "Show hidden files in new windows")));
    form->addRow(i18nc("@title:group", "Sorting:"), checkBox(QStringLiteral("FoldersFirst"), i18nc("@option:check", "Folders before files")));
    form->addRow(i18nc("@title:group", "Tooltips:"), checkBox(QStringLiteral("ShowToolTips"), i18nc("@option:check", "Show the full name of shortened items")));
    form->addRow(i18nc("@label:listbox", "Default grouping:"),
                 comboBox(QStringLiteral("GroupBy"),
                          {i18nc("@item:inlistbox", "No Grouping"),
                           i18nc("@item:inlistbox", "Name"),
                           i18nc("@item:inlistbox", "Type"),
                           i18nc("@item:inlistbox", "Size"),
                           i18nc("@item:inlistbox", "Date"),
                           i18nc("@item:inlistbox", "Extension")}));
    form->addRow(i18nc("@label:listbox", "Size format:"),
                 comboBox(QStringLiteral("SizeFormat"), {i18nc("@item:inlistbox", "Human readable"), i18nc("@item:inlistbox", "Bytes")}));
    form->addRow(i18nc("@label:listbox", "Date format:"),
                 comboBox(QStringLiteral("DateFormat"), {i18nc("@item:inlistbox", "Locale"), i18nc("@item:inlistbox", "ISO 8601")}));
    form->addRow({}, checkBox(QStringLiteral("ShowSeconds"), i18nc("@option:check", "Show seconds")));
    return page;
}

QWidget *SettingsDialog::createDetailsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:spinbox", "Icon size:"), pixelSpinBox(QStringLiteral("DetailsIconSize"), 16, 64));
    form->addRow(i18nc("@title:group", "Columns:"), checkBox(QStringLiteral("ShowSizeColumn"), i18nc("@option:check", "Size")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowModifiedColumn"), i18nc("@option:check", "Modified")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowTypeColumn"), i18nc("@option:check", "Type")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowPermissionsColumn"), i18nc("@option:check", "Permissions")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowOwnerColumn"), i18nc("@option:check", "Owner")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowGroupColumn"), i18nc("@option:check", "Group")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowRatingColumn"), i18nc("@option:check", "Rating (replaces Group)")));
    addDensityRows(form, QStringLiteral("DetailsDensity"), QStringLiteral("DetailsCustomSpace"));
    form->addRow(i18nc("@title:group", "Dragging:"),
                 checkBox(QStringLiteral("DragFromAnywhere"), i18nc("@option:check", "Drag files and folders from anywhere in their row")));
    return page;
}

QWidget *SettingsDialog::createIconsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:spinbox", "Icon and preview size:"), pixelSpinBox(QStringLiteral("IconsIconSize"), 32, 256));
    addDensityRows(form, QStringLiteral("IconsDensity"), QStringLiteral("IconsCustomSpace"));
    auto *lines = new QSpinBox;
    lines->setObjectName(QStringLiteral("kcfg_IconNameLines"));
    lines->setRange(1, 5);
    form->addRow(i18nc("@label:spinbox", "Name lines:"), lines);
    form->addRow(i18nc("@label", "Metadata shown:"), bound(new IconInformationList, QStringLiteral("IconInformation"), "information"));
    return page;
}

QWidget *SettingsDialog::createPreviewsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowPreviews"), i18nc("@option:check", "Show previews in new windows")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowRemotePreviews"), i18nc("@option:check", "Show previews in remote folders")));
    m_previewLimit = bound(new PreviewSizeSpinBox, QStringLiteral("MaximumPreviewSize"), "bytes");
    form->addRow(i18nc("@label:spinbox", "Skip files larger than:"), m_previewLimit);
    form->addRow(i18nc("@label", "Show previews for:"), bound(new PreviewPluginList, QStringLiteral("PreviewPlugins"), "plugins"));
    auto *textLimit = new QSpinBox;
    textLimit->setObjectName(QStringLiteral("kcfg_TextPreviewLimitKiB"));
    textLimit->setRange(1, 16384);
    textLimit->setSuffix(i18nc("@item:valuesuffix kibibytes", " KiB"));
    form->addRow(i18nc("@label:spinbox", "Maximum text in preview panel:"), textLimit);
    return page;
}

QWidget *SettingsDialog::createPlacesPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowPlaces"), i18nc("@option:check", "Show the Places panel")));

    const QList<std::pair<KFilePlacesModel::GroupType, QString>> groups{
        {KFilePlacesModel::PlacesType, i18nc("@option:check places section", "Places")},
        {KFilePlacesModel::RemoteType, i18nc("@option:check places section", "Remote")},
        {KFilePlacesModel::RecentlySavedType, i18nc("@option:check places section", "Recent")},
        {KFilePlacesModel::SearchForType, i18nc("@option:check places section", "Search For")},
        {KFilePlacesModel::DevicesType, i18nc("@option:check places section", "Devices")},
        {KFilePlacesModel::RemovableDevicesType, i18nc("@option:check places section", "Removable Devices")},
        {KFilePlacesModel::TagsType, i18nc("@option:check places section", "Tags")},
    };
    const QMetaEnum groupType = QMetaEnum::fromType<KFilePlacesModel::GroupType>();
    QString label = i18nc("@title:group", "Sections (shared with Dolphin):");
    for (const auto &[type, text] : groups) {
        QCheckBox *check = addSharedToggle(
            QStringLiteral("placesGroup_") + QLatin1String(groupType.valueToKey(type)),
            text,
            [this, type] {
                return !m_places->isGroupHidden(type);
            },
            [this, type](bool shown) {
                m_places->setGroupHidden(type, !shown);
            },
            true);
        form->addRow(label, check);
        label.clear();
    }
    return page;
}

QWidget *SettingsDialog::createPanelAppearancePage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("PanelsVisible"), i18nc("@option:check", "Show panels")));
    form->addRow(i18nc("@label:listbox", "Position:"),
                 comboBox(QStringLiteral("PanelPosition"), {i18nc("@item:inlistbox", "Left"), i18nc("@item:inlistbox", "Right")}));
    form->addRow({}, checkBox(QStringLiteral("PanelAutoFocus"), i18nc("@option:check", "Follow the active folder in the tree")));
    form->addRow({},
                 checkBox(QStringLiteral("AutoFocusPaneAfterPanelNavigation"), i18nc("@option:check", "Focus the active pane after navigating from a panel")));
    form->addRow({}, checkBox(QStringLiteral("PanelTitlesVisible"), i18nc("@option:check", "Show panel title bars")));
    form->addRow(i18nc("@title:group", "Combined panel sections:"), checkBox(QStringLiteral("FavoritesVisible"), i18nc("@option:check", "Favorites")));
    form->addRow({}, checkBox(QStringLiteral("PlacesVisible"), i18nc("@option:check", "Places")));
    form->addRow({}, checkBox(QStringLiteral("FoldersVisible"), i18nc("@option:check", "Folder tree")));
    return page;
}

QWidget *SettingsDialog::createTabAppearancePage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("TabIcons"), i18nc("@option:check", "Show folder icons on tabs")));
    form->addRow({}, checkBox(QStringLiteral("ExpandTabs"), i18nc("@option:check", "Expand tabs to fill the available width equally")));
    return page;
}

QWidget *SettingsDialog::createArchivesPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Default format:"),
                 comboBox(QStringLiteral("DefaultArchiveFormat"),
                          {i18nc("@item:inlistbox", "ZIP"),
                           i18nc("@item:inlistbox", "Uncompressed ZIP"),
                           i18nc("@item:inlistbox", "tar.gz"),
                           i18nc("@item:inlistbox", "tar.xz"),
                           i18nc("@item:inlistbox", "tar.zst")}));
    return page;
}

QWidget *SettingsDialog::createFileRulesPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label", "Ignored names:"), bound(new RuleListEditor(RuleListEditor::Kind::Ignore), QStringLiteral("IgnoreList"), "rules"));
    form->addRow(i18nc("@label", "File colours by extension:"),
                 bound(new RuleListEditor(RuleListEditor::Kind::Color), QStringLiteral("ExtensionColors"), "rules"));
    return page;
}

QWidget *SettingsDialog::createStatusBarPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowItemCounts"), i18nc("@option:check", "Number of items and selected items")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowSelectionSize"), i18nc("@option:check", "Size of the selected files")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowFreeSpace"), i18nc("@option:check", "Free space")));
    auto *density = comboBox(QStringLiteral("BarDensity"),
                             {i18nc("@item:inlistbox density", "Normal"),
                              i18nc("@item:inlistbox density", "Compact"),
                              i18nc("@item:inlistbox density", "Very compact"),
                              i18nc("@item:inlistbox density", "Custom")});
    auto *spacing = pixelSpinBox(QStringLiteral("BarCustomSpacing"), 0, 32);
    connect(density, &QComboBox::currentIndexChanged, page, [density, spacing] {
        spacing->setEnabled(density->currentIndex() == Settings::EnumBarDensity::Custom);
    });
    spacing->setEnabled(density->currentIndex() == Settings::EnumBarDensity::Custom);
    form->addRow(i18nc("@label:listbox", "Bar density:"), density);
    form->addRow(i18nc("@label:spinbox", "Custom bar spacing:"), spacing);
    return page;
}

QWidget *SettingsDialog::createContextMenuActionsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@title:group", "Show in the context menu:"), checkBox(QStringLiteral("ShowOpenInNewTab"), i18nc("@option:check", "Open in New Tab")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowOpenInOtherPane"), i18nc("@option:check", "Open in Other Pane")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowCopyToOtherPane"), i18nc("@option:check", "Copy to Other Pane")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowMoveToOtherPane"), i18nc("@option:check", "Move to Other Pane")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowCopyMoveToMenus"), i18nc("@option:check", "Additional KDE Copy To and Move To submenus")));
    form->addRow(QString(), checkBox(QStringLiteral("CopyToSubmenu"), i18nc("@option:check", "Replace Copy to Other Pane with a Copy To menu")));
    form->addRow(QString(), checkBox(QStringLiteral("MoveToSubmenu"), i18nc("@option:check", "Replace Move to Other Pane with a Move To menu")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowDelete"), i18nc("@option:check", "Delete Permanently")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowOpenTerminal"), i18nc("@option:check", "Open Terminal Here")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowCopyPath"), i18nc("@option:check", "Copy Path")));
    auto *list = new HiddenItemsList;
    const QList<std::pair<QString, QString>> entries{
        {QStringLiteral("open"), i18nc("@item:inlistbox", "Open")},
        {QStringLiteral("open_in_tab"), i18nc("@item:inlistbox", "Open in New Tab")},
        {QStringLiteral("open_in_other_pane"), i18nc("@item:inlistbox", "Open in Other Pane")},
        {QStringLiteral("open_with"), i18nc("@item:inlistbox", "Open With")},
        {QStringLiteral("new_menu"), i18nc("@item:inlistbox", "Create New")},
        {QStringLiteral("new_tab"), i18nc("@item:inlistbox", "New Tab")},
        {QStringLiteral("cut"), i18nc("@item:inlistbox", "Cut")},
        {QStringLiteral("copy"), i18nc("@item:inlistbox", "Copy")},
        {QStringLiteral("paste"), i18nc("@item:inlistbox", "Paste")},
        {QStringLiteral("copy_to"), i18nc("@item:inlistbox", "Opus Copy to Other Pane / Copy To")},
        {QStringLiteral("move_to"), i18nc("@item:inlistbox", "Opus Move to Other Pane / Move To")},
        {QStringLiteral("rename"), i18nc("@item:inlistbox", "Rename")},
        {QStringLiteral("trash"), i18nc("@item:inlistbox", "Move to Trash")},
        {QStringLiteral("delete"), i18nc("@item:inlistbox", "Delete Permanently")},
        {QStringLiteral("refresh"), i18nc("@item:inlistbox", "Reload")},
        {QStringLiteral("terminal"), i18nc("@item:inlistbox", "Open Terminal Here")},
        {QStringLiteral("copy_path"), i18nc("@item:inlistbox", "Copy Path")},
        {QStringLiteral("properties"), i18nc("@item:inlistbox", "Properties")},
    };
    for (const auto &[id, label] : entries) {
        list->addItem(id, label);
    }
    form->addRow(i18nc("@label", "Show these actions:"), bound(list, QStringLiteral("HiddenContextActions"), "hiddenItems"));
    return page;
}

QWidget *SettingsDialog::createServiceMenusPage()
{
    auto *list = new QWidget;
    auto *column = new QVBoxLayout(list);
    const QList<ServiceMenuEntry> entries = ServiceMenus::installed();
    auto *localActions = new HiddenItemsList;
    for (const ServiceMenuEntry &entry : entries) {
        localActions->addItem(entry.key, entry.submenu.isEmpty() ? entry.text : i18nc("@item:inlistbox", "%1: %2", entry.submenu, entry.text));
    }
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label", "Show in Opus:"), bound(localActions, QStringLiteral("HiddenServiceActions"), "hiddenItems"));
    column->addLayout(form);
    for (const ServiceMenuEntry &entry : entries) {
        const QString key = entry.key;
        const bool shownByDefault = entry.shownByDefault;
        const QString text = entry.submenu.isEmpty() ? entry.text : i18nc("@option:check submenu: action", "%1: %2", entry.submenu, entry.text);
        QCheckBox *check = addSharedToggle(
            QStringLiteral("serviceMenu_") + key,
            text,
            [key, shownByDefault] {
                return KConfig(QLatin1String(ServiceMenus::ConfigFile), KConfig::NoGlobals)
                    .group(QLatin1String(ServiceMenus::ShowGroup))
                    .readEntry(key, shownByDefault);
            },
            [key](bool shown) {
                KConfig config(QLatin1String(ServiceMenus::ConfigFile), KConfig::NoGlobals);
                config.group(QLatin1String(ServiceMenus::ShowGroup)).writeEntry(key, shown);
                config.sync();
            },
            shownByDefault);
        check->setIcon(QIcon::fromTheme(entry.iconName));
        column->addWidget(check);
    }
    column->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(list);
    return scroll;
}

QWidget *SettingsDialog::createConfirmationsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    const auto kioConfirmation = [this](const QString &name, const QString &text, const char *key, bool defaultValue) {
        return addSharedToggle(
            name,
            text,
            [key, defaultValue] {
                return kioConfirmations().readEntry(key, defaultValue);
            },
            [key](bool confirm) {
                KConfigGroup group = kioConfirmations();
                group.writeEntry(key, confirm);
                group.sync();
            },
            defaultValue);
    };
    form->addRow(i18nc("@title:group", "Ask for confirmation (shared with Dolphin):"),
                 kioConfirmation(QStringLiteral("kioConfirmTrash"), i18nc("@option:check", "When moving to the trash"), "ConfirmTrash", false));
    form->addRow(QString(), kioConfirmation(QStringLiteral("kioConfirmDelete"), i18nc("@option:check", "When deleting permanently"), "ConfirmDelete", true));
    form->addRow(i18nc("@title:group", "Ask for confirmation:"),
                 checkBox(QStringLiteral("ConfirmClosingMultipleTabs"), i18nc("@option:check", "When closing a window with several tabs")));
    form->addRow(i18nc("@label:listbox", "Opening an executable file:"),
                 comboBox(QStringLiteral("ExecutableAction"),
                          {i18nc("@item:inlistbox", "Always ask"),
                           i18nc("@item:inlistbox", "Open it in its application"),
                           i18nc("@item:inlistbox", "Run it"),
                           i18nc("@item:inlistbox", "Run it in a terminal that closes when it ends"),
                           i18nc("@item:inlistbox", "Run it in a terminal that stays open")}));
    form->addRow(i18nc("@label:listbox", "Dropping files without a modifier key:"),
                 comboBox(QStringLiteral("DropAction"),
                          {i18nc("@item:inlistbox", "Ask what to do"), i18nc("@item:inlistbox", "Copy"), i18nc("@item:inlistbox", "Move")}));
    return page;
}

QCheckBox *
SettingsDialog::addSharedToggle(const QString &name, const QString &text, std::function<bool()> read, std::function<void(bool)> write, bool defaultValue)
{
    auto *check = new QCheckBox(text);
    check->setObjectName(name);
    connect(check, &QCheckBox::toggled, this, &SettingsDialog::updateButtons);
    m_sharedToggles.append({check, std::move(read), std::move(write), defaultValue});
    return check;
}

void SettingsDialog::updateSettings()
{
    const qulonglong previewLimit = m_previewLimit->bytes();
    if (previewLimit != m_savedPreviewLimit) {
        KConfigGroup previews = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("PreviewSettings"));
        previews.writeEntry("MaximumSize", previewLimit);
        previews.sync();
        m_savedPreviewLimit = previewLimit;
    }
    for (const SharedToggle &toggle : std::as_const(m_sharedToggles)) {
        toggle.write(toggle.check->isChecked());
    }
}

void SettingsDialog::updateWidgets()
{
    for (const SharedToggle &toggle : std::as_const(m_sharedToggles)) {
        toggle.check->setChecked(toggle.read());
    }
}

void SettingsDialog::updateWidgetsDefault()
{
    for (const SharedToggle &toggle : std::as_const(m_sharedToggles)) {
        toggle.check->setChecked(toggle.defaultValue);
    }
    QMetaObject::invokeMethod(
        this,
        [this] {
            updateButtons();
        },
        Qt::QueuedConnection);
}

bool SettingsDialog::hasChanged()
{
    return std::any_of(m_sharedToggles.cbegin(), m_sharedToggles.cend(), [](const SharedToggle &toggle) {
        return toggle.check->isChecked() != toggle.read();
    });
}

bool SettingsDialog::isDefault()
{
    return std::all_of(m_sharedToggles.cbegin(), m_sharedToggles.cend(), [](const SharedToggle &toggle) {
        return toggle.check->isChecked() == toggle.defaultValue;
    });
}
