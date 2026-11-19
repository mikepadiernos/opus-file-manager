// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "density.h"
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
#include <QSpinBox>
#include <QTabWidget>

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
                   {createStatusBarPage(), i18nc("@title:tab", "Status and Location Bars")}},
                  i18nc("@title:group", "Interface"),
                  QStringLiteral("window"));
    addTabbedPage({{createViewGeneralPage(), i18nc("@title:tab", "General")},
                   {createDetailsPage(), i18nc("@title:tab", "Details")},
                   {createIconsPage(), i18nc("@title:tab", "Icons")}},
                  i18nc("@title:group", "View"),
                  QStringLiteral("view-list-icons"));
    applySidebarStyle();
    connect(this, &KConfigDialog::settingsChanged, this, &SettingsDialog::applySidebarStyle);
    updateWidgets();
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
    const bool below = Settings::sidebarTitlePosition() == Settings::EnumSidebarTitlePosition::Below;
    setFaceType(below ? KPageDialog::List : KPageDialog::FlatList);

    const int size = Settings::sidebarIconSize();
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
    return page;
}

QWidget *SettingsDialog::createPreviewsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowPreviews"), i18nc("@option:check", "Show previews in new windows")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowRemotePreviews"), i18nc("@option:check", "Show previews in remote folders")));
    form->addRow(i18nc("@label:spinbox", "Skip files larger than:"), bound(new PreviewSizeSpinBox, QStringLiteral("MaximumPreviewSize"), "bytes"));
    form->addRow(i18nc("@label", "Show previews for:"), bound(new PreviewPluginList, QStringLiteral("PreviewPlugins"), "plugins"));
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

QWidget *SettingsDialog::createStatusBarPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowItemCounts"), i18nc("@option:check", "Number of items and selected items")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowSelectionSize"), i18nc("@option:check", "Size of the selected files")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowFreeSpace"), i18nc("@option:check", "Free space")));
    form->addRow(
        i18nc("@label:listbox", "Bar density:"),
        comboBox(QStringLiteral("BarDensity"),
                 {i18nc("@item:inlistbox density", "Normal"), i18nc("@item:inlistbox density", "Compact"), i18nc("@item:inlistbox density", "Very compact")}));
    return page;
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
