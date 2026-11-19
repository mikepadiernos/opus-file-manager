// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "symbolicicon.h"
#include "viewsettings.h"

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KFileUtils>
#include <KIO/PreviewJob>
#include <KLocalizedString>
#include <KPageWidget>
#include <KPageWidgetItem>
#include <KPluginMetaData>
#include <KService>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace
{
const QList<QPair<QString, QString>> Columns{
    {QStringLiteral("type"), i18nc("@option:check", "Type")},
    {QStringLiteral("size"), i18nc("@option:check", "Size")},
    {QStringLiteral("date"), i18nc("@option:check", "Modified")},
    {QStringLiteral("mode"), i18nc("@option:check", "Permissions")},
    {QStringLiteral("owner"), i18nc("@option:check", "Owner")},
    {QStringLiteral("group"), i18nc("@option:check", "Group")},
    {QStringLiteral("rating"), i18nc("@option:check", "Rating")},
};
const QList<QPair<QString, QString>> IconInformation{
    {QStringLiteral("size"), i18nc("@option:check", "Size")},
    {QStringLiteral("permissions"), i18nc("@option:check", "Permissions")},
    {QStringLiteral("octalPermissions"), i18nc("@option:check", "Permissions (octal)")},
    {QStringLiteral("owner"), i18nc("@option:check", "Owner")},
    {QStringLiteral("ownerAndGroup"), i18nc("@option:check", "Owner and group")},
    {QStringLiteral("creationTime"), i18nc("@option:check", "Created")},
    {QStringLiteral("modificationTime"), i18nc("@option:check", "Modified")},
    {QStringLiteral("accessTime"), i18nc("@option:check", "Accessed")},
    {QStringLiteral("mimeType"), i18nc("@option:check", "MIME type")},
    {QStringLiteral("friendlyMimeType"), i18nc("@option:check", "File type")},
    {QStringLiteral("linkDest"), i18nc("@option:check", "Link target")},
    {QStringLiteral("localPathOrUrl"), i18nc("@option:check", "Path or URL")},
    {QStringLiteral("comment"), i18nc("@option:check", "Comment")},
};
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : KPageDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Configure Opus File Manager"));
    setMinimumSize(680, 520);

    const KConfigGroup sidebarSettings = KSharedConfig::openConfig()->group(QStringLiteral("SettingsDialog"));
    auto *generalTabs = new QTabWidget(this);
    auto *sidebarPage = new QWidget(generalTabs);
    auto *sidebarForm = new QFormLayout(sidebarPage);
    m_sidebarIconSize = new QSpinBox(sidebarPage);
    m_sidebarIconSize->setObjectName(QStringLiteral("sidebar_icon_size"));
    m_sidebarIconSize->setRange(16, 64);
    m_sidebarIconSize->setSuffix(i18nc("@item:valuesuffix pixels", " px"));
    m_sidebarIconSize->setValue(sidebarSettings.readEntry("SidebarIconSize", 24));
    sidebarForm->addRow(i18nc("@label:spinbox", "Sidebar icon size:"), m_sidebarIconSize);
    m_sidebarTitlePosition = new QComboBox(sidebarPage);
    m_sidebarTitlePosition->setObjectName(QStringLiteral("sidebar_title_position"));
    m_sidebarTitlePosition->addItem(i18nc("@item:inlistbox", "To the right of icons"), QStringLiteral("right"));
    m_sidebarTitlePosition->addItem(i18nc("@item:inlistbox", "Below icons"), QStringLiteral("below"));
    const int positionIndex = m_sidebarTitlePosition->findData(sidebarSettings.readEntry("SidebarTitlePosition", QStringLiteral("right")));
    m_sidebarTitlePosition->setCurrentIndex(positionIndex >= 0 ? positionIndex : 0);
    sidebarForm->addRow(i18nc("@label:listbox", "Sidebar titles:"), m_sidebarTitlePosition);
    generalTabs->addTab(sidebarPage, i18nc("@title:tab", "Settings Dialog"));
    addPage(generalTabs, i18nc("@title", "General"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("configure")));

    const ViewSettings settings = ViewSettings::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("View")));
    auto *viewPage = new QWidget(this);
    auto *viewLayout = new QVBoxLayout(viewPage);

    auto *columnsGroup = new QGroupBox(i18nc("@title:group", "Default Visible Columns"), viewPage);
    auto *columnsLayout = new QGridLayout(columnsGroup);
    for (qsizetype i = 0; i < Columns.size(); ++i) {
        const auto &[key, label] = Columns.at(i);
        auto *check = new QCheckBox(label, columnsGroup);
        check->setObjectName(QStringLiteral("column_%1").arg(key));
        check->setChecked(settings.visibleColumns.contains(key));
        m_columnChecks.insert(key, check);
        columnsLayout->addWidget(check, i / 3, i % 3);
    }
    viewLayout->addWidget(columnsGroup);

    auto *generalGroup = new QGroupBox(i18nc("@title:group", "Listing Defaults"), viewPage);
    auto *generalLayout = new QFormLayout(generalGroup);
    m_groupBy = new QComboBox(generalGroup);
    m_groupBy->setObjectName(QStringLiteral("default_group_by"));
    const QList<QPair<QString, QString>> groups{
        {QStringLiteral("none"), i18nc("@item:inlistbox", "No Grouping")},
        {QStringLiteral("name"), i18nc("@item:inlistbox", "Name")},
        {QStringLiteral("type"), i18nc("@item:inlistbox", "Type")},
        {QStringLiteral("size"), i18nc("@item:inlistbox", "Size")},
        {QStringLiteral("date"), i18nc("@item:inlistbox", "Date")},
        {QStringLiteral("ext"), i18nc("@item:inlistbox", "Extension")},
    };
    for (const auto &[key, label] : groups) {
        m_groupBy->addItem(label, key);
    }
    m_groupBy->setCurrentIndex(m_groupBy->findData(settings.groupBy));
    generalLayout->addRow(i18nc("@label", "Group by:"), m_groupBy);

    m_sizeFormat = new QComboBox(generalGroup);
    m_sizeFormat->setObjectName(QStringLiteral("size_format"));
    m_sizeFormat->addItem(i18nc("@item:inlistbox", "Human readable"), QStringLiteral("human"));
    m_sizeFormat->addItem(i18nc("@item:inlistbox", "Bytes"), QStringLiteral("bytes"));
    m_sizeFormat->setCurrentIndex(m_sizeFormat->findData(settings.sizeFormat));
    generalLayout->addRow(i18nc("@label", "Size format:"), m_sizeFormat);

    m_dateFormat = new QComboBox(generalGroup);
    m_dateFormat->setObjectName(QStringLiteral("date_format"));
    m_dateFormat->addItem(i18nc("@item:inlistbox", "Locale"), QStringLiteral("locale"));
    m_dateFormat->addItem(i18nc("@item:inlistbox", "ISO 8601"), QStringLiteral("iso"));
    m_dateFormat->setCurrentIndex(m_dateFormat->findData(settings.dateFormat));
    generalLayout->addRow(i18nc("@label", "Date format:"), m_dateFormat);
    m_showSeconds = new QCheckBox(i18nc("@option:check", "Show seconds"), generalGroup);
    m_showSeconds->setObjectName(QStringLiteral("show_seconds"));
    m_showSeconds->setChecked(settings.showSeconds);
    generalLayout->addRow({}, m_showSeconds);
    viewLayout->addWidget(generalGroup);

    auto *iconsGroup = new QGroupBox(i18nc("@title:group", "Icon View"), viewPage);
    auto *iconsLayout = new QVBoxLayout(iconsGroup);
    auto *linesLayout = new QFormLayout;
    m_iconNameLines = new QSpinBox(iconsGroup);
    m_iconNameLines->setObjectName(QStringLiteral("icon_name_lines"));
    m_iconNameLines->setRange(1, 5);
    m_iconNameLines->setValue(settings.iconNameLines);
    linesLayout->addRow(i18nc("@label", "Name lines:"), m_iconNameLines);
    iconsLayout->addLayout(linesLayout);

    auto *informationGroup = new QGroupBox(i18nc("@title:group", "Metadata Shown"), iconsGroup);
    auto *informationLayout = new QGridLayout(informationGroup);
    for (qsizetype i = 0; i < IconInformation.size(); ++i) {
        const auto &[key, label] = IconInformation.at(i);
        auto *check = new QCheckBox(label, informationGroup);
        check->setObjectName(QStringLiteral("icon_info_%1").arg(key));
        check->setChecked(settings.iconInformation.contains(key));
        m_iconInformationChecks.insert(key, check);
        informationLayout->addWidget(check, i / 3, i % 3);
    }
    iconsLayout->addWidget(informationGroup);
    viewLayout->addWidget(iconsGroup);
    viewLayout->addStretch();
    addPage(viewPage, i18nc("@title", "View"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-list-icons")));

    auto *filesPage = new QWidget(this);
    auto *filesLayout = new QVBoxLayout(filesPage);
    auto *ignoreGroup = new QGroupBox(i18nc("@title:group", "Ignored Names"), filesPage);
    auto *ignoreLayout = new QVBoxLayout(ignoreGroup);
    m_ignoreEntries = new QPlainTextEdit(ignoreGroup);
    m_ignoreEntries->setObjectName(QStringLiteral("ignore_entries"));
    const IgnoreList ignoreList = IgnoreList::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files")));
    m_ignoreEntries->setPlainText(ignoreList.entries().join(QLatin1Char('\n')));
    auto *ignoreHint = new QLabel(i18nc("@info", "One file name or glob pattern per line. Matching is case-insensitive."), ignoreGroup);
    ignoreHint->setWordWrap(true);
    ignoreLayout->addWidget(m_ignoreEntries);
    ignoreLayout->addWidget(ignoreHint);
    filesLayout->addWidget(ignoreGroup);

    auto *colorsGroup = new QGroupBox(i18nc("@title:group", "File Colours by Extension"), filesPage);
    auto *colorsLayout = new QVBoxLayout(colorsGroup);
    m_colorRules = new QTableWidget(colorsGroup);
    m_colorRules->setObjectName(QStringLiteral("extension_color_rules"));
    m_colorRules->setColumnCount(2);
    m_colorRules->setHorizontalHeaderLabels({i18nc("@title:column", "Extension"), i18nc("@title:column", "Colour")});
    m_colorRules->horizontalHeader()->setStretchLastSection(true);
    m_colorRules->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_colorRules->setSelectionMode(QAbstractItemView::SingleSelection);
    m_colorRules->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const FileColorRules colors = FileColorRules::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files")));
    for (const QString &entry : colors.entries()) {
        const qsizetype separator = entry.indexOf(u'=');
        setColorRuleRow(m_colorRules->rowCount(), entry.first(separator), QColor(entry.sliced(separator + 1)));
    }
    colorsLayout->addWidget(m_colorRules);
    auto *colorButtons = new QHBoxLayout;
    auto *addColor = new QPushButton(i18nc("@action:button", "Add…"), colorsGroup);
    auto *editColor = new QPushButton(i18nc("@action:button", "Edit…"), colorsGroup);
    auto *removeColor = new QPushButton(i18nc("@action:button", "Remove"), colorsGroup);
    addColor->setObjectName(QStringLiteral("add_extension_color"));
    editColor->setObjectName(QStringLiteral("edit_extension_color"));
    removeColor->setObjectName(QStringLiteral("remove_extension_color"));
    colorButtons->addWidget(addColor);
    colorButtons->addWidget(editColor);
    colorButtons->addWidget(removeColor);
    colorButtons->addStretch();
    colorsLayout->addLayout(colorButtons);
    connect(addColor, &QPushButton::clicked, this, &SettingsDialog::addColorRule);
    connect(editColor, &QPushButton::clicked, this, &SettingsDialog::editColorRule);
    connect(removeColor, &QPushButton::clicked, this, &SettingsDialog::removeColorRule);
    filesLayout->addWidget(colorsGroup);
    addPage(filesPage, i18nc("@title", "Files"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("folder")));

    const KConfigGroup panelSettings = KSharedConfig::openConfig()->group(QStringLiteral("PanelSettings"));
    auto *panelsPage = new QWidget(this);
    auto *panelsLayout = new QVBoxLayout(panelsPage);
    auto *panelGroup = new QGroupBox(i18nc("@title:group", "Side Panels"), panelsPage);
    auto *panelForm = new QFormLayout(panelGroup);
    m_showPanels = new QCheckBox(i18nc("@option:check", "Show panels"), panelGroup);
    m_showPanels->setObjectName(QStringLiteral("show_panels"));
    m_showPanels->setChecked(panelSettings.readEntry("Visible", true));
    panelForm->addRow({}, m_showPanels);

    m_panelPosition = new QComboBox(panelGroup);
    m_panelPosition->setObjectName(QStringLiteral("panel_position"));
    m_panelPosition->addItem(i18nc("@item:inlistbox", "Left"), QStringLiteral("left"));
    m_panelPosition->addItem(i18nc("@item:inlistbox", "Right"), QStringLiteral("right"));
    const int panelPositionIndex = m_panelPosition->findData(panelSettings.readEntry("Position", QStringLiteral("left")));
    m_panelPosition->setCurrentIndex(panelPositionIndex >= 0 ? panelPositionIndex : 0);
    panelForm->addRow(i18nc("@label", "Position:"), m_panelPosition);

    m_autoFocusFolders = new QCheckBox(i18nc("@option:check", "Follow the active folder in the tree"), panelGroup);
    m_autoFocusFolders->setObjectName(QStringLiteral("follow_active_folder"));
    m_autoFocusFolders->setChecked(panelSettings.readEntry("AutoFocus", true));
    panelForm->addRow({}, m_autoFocusFolders);

    m_showPanelTitles = new QCheckBox(i18nc("@option:check", "Show panel title bars"), panelGroup);
    m_showPanelTitles->setObjectName(QStringLiteral("show_panel_titles"));
    m_showPanelTitles->setChecked(panelSettings.readEntry("TitlesVisible", true));
    panelForm->addRow({}, m_showPanelTitles);

    auto *sectionsGroup = new QGroupBox(i18nc("@title:group", "Combined Panel Sections"), panelsPage);
    auto *sectionsLayout = new QVBoxLayout(sectionsGroup);
    m_showFavorites = new QCheckBox(i18nc("@option:check", "Favorites"), sectionsGroup);
    m_showFavorites->setObjectName(QStringLiteral("show_favorites_section"));
    m_showFavorites->setChecked(panelSettings.readEntry("FavoritesVisible", true));
    sectionsLayout->addWidget(m_showFavorites);
    m_showPlaces = new QCheckBox(i18nc("@option:check", "Places"), sectionsGroup);
    m_showPlaces->setObjectName(QStringLiteral("show_places_section"));
    m_showPlaces->setChecked(panelSettings.readEntry("PlacesVisible", true));
    sectionsLayout->addWidget(m_showPlaces);
    m_showFolderTree = new QCheckBox(i18nc("@option:check", "Folder tree"), sectionsGroup);
    m_showFolderTree->setObjectName(QStringLiteral("show_folder_tree_section"));
    m_showFolderTree->setChecked(panelSettings.readEntry("FoldersVisible", true));
    sectionsLayout->addWidget(m_showFolderTree);
    panelsLayout->addWidget(panelGroup);
    panelsLayout->addWidget(sectionsGroup);
    panelsLayout->addStretch();
    addPage(panelsPage, i18nc("@title", "Panels"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-split-left-right")));

    const KConfigGroup archiveSettings = KSharedConfig::openConfig()->group(QStringLiteral("Archives"));
    auto *archivesPage = new QWidget(this);
    auto *archivesLayout = new QFormLayout(archivesPage);
    m_defaultArchiveFormat = new QComboBox(archivesPage);
    m_defaultArchiveFormat->setObjectName(QStringLiteral("default_archive_format"));
    const QList<QPair<ArchiveWriter::Format, QString>> archiveFormats{
        {ArchiveWriter::Format::Zip, i18nc("@item:inlistbox", "ZIP")},
        {ArchiveWriter::Format::ZipStored, i18nc("@item:inlistbox", "Uncompressed ZIP")},
        {ArchiveWriter::Format::TarGz, i18nc("@item:inlistbox", "tar.gz")},
        {ArchiveWriter::Format::TarXz, i18nc("@item:inlistbox", "tar.xz")},
        {ArchiveWriter::Format::TarZstd, i18nc("@item:inlistbox", "tar.zst")},
    };
    for (const auto &[format, label] : archiveFormats) {
        m_defaultArchiveFormat->addItem(label, static_cast<int>(format));
    }
    const int defaultFormat = archiveSettings.readEntry("DefaultFormat", static_cast<int>(ArchiveWriter::Format::Zip));
    const int defaultFormatIndex = m_defaultArchiveFormat->findData(defaultFormat);
    m_defaultArchiveFormat->setCurrentIndex(defaultFormatIndex >= 0 ? defaultFormatIndex : 0);
    archivesLayout->addRow(i18nc("@label", "Default format:"), m_defaultArchiveFormat);
    addPage(archivesPage, i18nc("@title", "Archives"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("package-x-generic")));

    const KConfigGroup tabSettings = KSharedConfig::openConfig()->group(QStringLiteral("Tabs"));
    auto *tabsPage = new QWidget(this);
    auto *tabsLayout = new QVBoxLayout(tabsPage);
    m_showTabIcons = new QCheckBox(i18nc("@option:check", "Show folder icons on tabs"), tabsPage);
    m_showTabIcons->setObjectName(QStringLiteral("show_tab_icons"));
    m_showTabIcons->setChecked(tabSettings.readEntry("ShowIcons", true));
    tabsLayout->addWidget(m_showTabIcons);
    m_expandTabs = new QCheckBox(i18nc("@option:check", "Expand tabs to fill the available width equally"), tabsPage);
    m_expandTabs->setObjectName(QStringLiteral("expand_tabs"));
    m_expandTabs->setChecked(tabSettings.readEntry("Expand", true));
    tabsLayout->addWidget(m_expandTabs);
    tabsLayout->addStretch();
    addPage(tabsPage, i18nc("@title", "Tabs"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("tab-new")));

    auto addCheck = [this](QFormLayout *form, const QString &group, const QString &key, const QString &label, bool defaultValue) {
        auto *check = new QCheckBox(label, form->parentWidget());
        check->setObjectName(group + u'_' + key);
        check->setChecked(KSharedConfig::openConfig()->group(group).readEntry(key, defaultValue));
        form->addRow({}, check);
        m_m02Controls.insert(group + u'/' + key, check);
        return check;
    };
    auto addCombo = [this](QFormLayout *form,
                           const QString &group,
                           const QString &key,
                           const QString &label,
                           const QList<QPair<QString, QString>> &options,
                           const QString &defaultValue) {
        auto *combo = new QComboBox(form->parentWidget());
        combo->setObjectName(group + u'_' + key);
        for (const auto &[value, text] : options) {
            combo->addItem(text, value);
        }
        const QString value = KSharedConfig::openConfig()->group(group).readEntry(key, defaultValue);
        combo->setCurrentIndex(qMax(0, combo->findData(value)));
        form->addRow(label, combo);
        m_m02Controls.insert(group + u'/' + key, combo);
        return combo;
    };
    auto addSpin = [this](QFormLayout *form,
                          const QString &group,
                          const QString &key,
                          const QString &label,
                          int minimum,
                          int maximum,
                          int defaultValue,
                          const QString &suffix = QString()) {
        auto *spin = new QSpinBox(form->parentWidget());
        spin->setObjectName(group + u'_' + key);
        spin->setRange(minimum, maximum);
        spin->setSuffix(suffix);
        spin->setValue(KSharedConfig::openConfig()->group(group).readEntry(key, defaultValue));
        form->addRow(label, spin);
        m_m02Controls.insert(group + u'/' + key, spin);
        return spin;
    };
    auto addLine = [this](QFormLayout *form, const QString &group, const QString &key, const QString &label, const QString &defaultValue) {
        auto *line = new QLineEdit(form->parentWidget());
        line->setObjectName(group + u'_' + key);
        line->setText(KSharedConfig::openConfig()->group(group).readEntry(key, defaultValue));
        form->addRow(label, line);
        m_m02Controls.insert(group + u'/' + key, line);
        return line;
    };

    auto *interfacePage = new QWidget(this);
    auto *interfaceTabs = new QTabWidget(interfacePage);
    auto *interfaceLayout = new QVBoxLayout(interfacePage);
    interfaceLayout->addWidget(interfaceTabs);

    auto *startupPage = new QWidget(interfaceTabs);
    auto *startupForm = new QFormLayout(startupPage);
    addCombo(startupForm,
             QStringLiteral("Startup"),
             QStringLiteral("StartupMode"),
             i18nc("@label:listbox", "At startup:"),
             {{QStringLiteral("RestoreSession"), i18nc("@item:inlistbox", "Restore previous session")},
              {QStringLiteral("HomeFolder"), i18nc("@item:inlistbox", "Open home folder")},
              {QStringLiteral("FixedFolders"), i18nc("@item:inlistbox", "Open fixed folders")}},
             QStringLiteral("RestoreSession"));
    addLine(startupForm, QStringLiteral("Startup"), QStringLiteral("HomeUrl"), i18nc("@label:textbox", "Home folder URL:"), QDir::homePath());
    addLine(startupForm, QStringLiteral("Startup"), QStringLiteral("LeftStartUrl"), i18nc("@label:textbox", "First pane URL:"), QDir::homePath());
    addLine(startupForm, QStringLiteral("Startup"), QStringLiteral("RightStartUrl"), i18nc("@label:textbox", "Second pane URL:"), QDir::homePath());
    addCombo(startupForm,
             QStringLiteral("Panes"),
             QStringLiteral("DualPaneAtStartup"),
             i18nc("@label:listbox", "Panes at startup:"),
             {{QStringLiteral("AsLastUsed"), i18nc("@item:inlistbox", "Use last session")},
              {QStringLiteral("Always"), i18nc("@item:inlistbox", "Always show two panes")},
              {QStringLiteral("Never"), i18nc("@item:inlistbox", "Always show one pane")}},
             QStringLiteral("AsLastUsed"));
    addCheck(startupForm, QStringLiteral("Panes"), QStringLiteral("AlwaysShowTabBar"), i18nc("@option:check", "Always show tab bars"), false);
    addCheck(startupForm, QStringLiteral("Panes"), QStringLiteral("TabSwitchesPanes"), i18nc("@option:check", "Tab key switches panes"), true);
    addCheck(startupForm,
             QStringLiteral("Panes"),
             QStringLiteral("NewTabsAfterCurrent"),
             i18nc("@option:check", "Open new tabs next to the current tab"),
             false);
    addCheck(startupForm, QStringLiteral("Panes"), QStringLiteral("ShowNavigationBar"), i18nc("@option:check", "Show navigation bars"), true);
    addCheck(startupForm,
             QStringLiteral("Navigation"),
             QStringLiteral("FollowSymlinks"),
             i18nc("@option:check", "Follow symbolic links when navigating"),
             false);
    interfaceTabs->addTab(startupPage, i18nc("@title:tab", "Startup and Navigation"));

    auto *previewPage = new QWidget(interfaceTabs);
    auto *previewLayout = new QVBoxLayout(previewPage);
    auto *previewForm = new QFormLayout;
    addCheck(previewForm, QStringLiteral("Previews"), QStringLiteral("ShowPreviews"), i18nc("@option:check", "Show previews"), true);
    addCheck(previewForm, QStringLiteral("Previews"), QStringLiteral("ShowRemotePreviews"), i18nc("@option:check", "Show previews for remote files"), false);
    auto *previewLimit = addSpin(previewForm,
                                 QStringLiteral("PreviewSettings"),
                                 QStringLiteral("MaximumSizeMiB"),
                                 i18nc("@label:spinbox", "Maximum file size:"),
                                 0,
                                 100000,
                                 0,
                                 i18nc("@item:valuesuffix mebibytes", " MiB"));
    previewLimit->setSpecialValueText(i18nc("@item:inlistbox preview size limit", "No limit"));
    const auto kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
    const KConfigGroup kioPreviewSettings = kioConfig->group(QStringLiteral("PreviewSettings"));
    const qulonglong maximumBytes = kioPreviewSettings.readEntry("MaximumSize", std::numeric_limits<qulonglong>::max());
    if (maximumBytes != std::numeric_limits<qulonglong>::max()) {
        previewLimit->setValue(int(qMin<qulonglong>(100000, qMax<qulonglong>(1, (maximumBytes + 1024 * 1024 - 1) / (1024 * 1024)))));
    }
    previewLayout->addLayout(previewForm);
    auto *pluginGroup = new QGroupBox(i18nc("@title:group", "Preview Plugins"), previewPage);
    auto *pluginLayout = new QVBoxLayout(pluginGroup);
    m_previewPlugins = new QListWidget(pluginGroup);
    m_previewPlugins->setObjectName(QStringLiteral("preview_plugins"));
    auto availablePlugins = KIO::PreviewJob::availableThumbnailerPlugins();
    std::sort(availablePlugins.begin(), availablePlugins.end(), [](const KPluginMetaData &a, const KPluginMetaData &b) {
        return QString::localeAwareCompare(a.name(), b.name()) < 0;
    });
    const KConfigGroup appPreviewSettings = KSharedConfig::openConfig()->group(QStringLiteral("PreviewSettings"));
    const QStringList enabledPlugins = kioPreviewSettings.readEntry("Plugins", appPreviewSettings.readEntry("Plugins", KIO::PreviewJob::defaultPlugins()));
    for (const KPluginMetaData &plugin : std::as_const(availablePlugins)) {
        auto *item = new QListWidgetItem(plugin.name(), m_previewPlugins);
        item->setData(Qt::UserRole, plugin.pluginId());
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(enabledPlugins.contains(plugin.pluginId()) ? Qt::Checked : Qt::Unchecked);
    }
    pluginLayout->addWidget(m_previewPlugins);
    previewLayout->addWidget(pluginGroup);
    interfaceTabs->addTab(previewPage, i18nc("@title:tab", "Previews"));

    auto *confirmationPage = new QWidget(interfaceTabs);
    auto *confirmationForm = new QFormLayout(confirmationPage);
    m_confirmMultipleTabs = new QCheckBox(i18nc("@option:check", "Ask before closing a window with multiple tabs"), confirmationPage);
    m_confirmMultipleTabs->setObjectName(QStringLiteral("confirm_multiple_tabs"));
    m_confirmMultipleTabs->setChecked(KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).readEntry("ConfirmClosingMultipleTabs", true));
    confirmationForm->addRow({}, m_confirmMultipleTabs);
    m_confirmTrash = new QCheckBox(i18nc("@option:check", "Ask before moving files to Trash"), confirmationPage);
    m_confirmDelete = new QCheckBox(i18nc("@option:check", "Ask before permanently deleting files"), confirmationPage);
    const KConfigGroup kioConfirmations = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("Confirmations"));
    m_confirmTrash->setChecked(kioConfirmations.readEntry("ConfirmTrash", true));
    m_confirmDelete->setChecked(kioConfirmations.readEntry("ConfirmDelete", true));
    confirmationForm->addRow({}, m_confirmTrash);
    confirmationForm->addRow({}, m_confirmDelete);
    addCombo(confirmationForm,
             QStringLiteral("Confirmations"),
             QStringLiteral("ExecutableAction"),
             i18nc("@label:listbox", "When opening executables:"),
             {{QStringLiteral("Ask"), i18nc("@item:inlistbox", "Ask what to do")},
              {QStringLiteral("Open"), i18nc("@item:inlistbox", "Open as a document")},
              {QStringLiteral("Run"), i18nc("@item:inlistbox", "Run")},
              {QStringLiteral("RunInTerminal"), i18nc("@item:inlistbox", "Run in a terminal")},
              {QStringLiteral("RunInTerminalKeepOpen"), i18nc("@item:inlistbox", "Run in a terminal and keep it open")}},
             QStringLiteral("Ask"));
    addCombo(confirmationForm,
             QStringLiteral("Confirmations"),
             QStringLiteral("DropAction"),
             i18nc("@label:listbox", "When dropping files:"),
             {{QStringLiteral("Ask"), i18nc("@item:inlistbox", "Ask whether to copy or move")},
              {QStringLiteral("Copy"), i18nc("@item:inlistbox", "Copy")},
              {QStringLiteral("Move"), i18nc("@item:inlistbox", "Move")}},
             QStringLiteral("Ask"));
    interfaceTabs->addTab(confirmationPage, i18nc("@title:tab", "Confirmations"));

    auto *statusPage = new QWidget(interfaceTabs);
    auto *statusForm = new QFormLayout(statusPage);
    addCheck(statusForm, QStringLiteral("StatusBar"), QStringLiteral("ShowItemCounts"), i18nc("@option:check", "Show item counts"), true);
    addCheck(statusForm, QStringLiteral("StatusBar"), QStringLiteral("ShowSelectionSize"), i18nc("@option:check", "Show selected file size"), true);
    addCheck(statusForm, QStringLiteral("StatusBar"), QStringLiteral("ShowFreeSpace"), i18nc("@option:check", "Show disk free space"), true);
    addCombo(statusForm,
             QStringLiteral("Views"),
             QStringLiteral("BarDensity"),
             i18nc("@label:listbox", "Toolbar, location and status bar density:"),
             {{QStringLiteral("Normal"), i18nc("@item:inlistbox", "Normal")},
              {QStringLiteral("Compact"), i18nc("@item:inlistbox", "Compact")},
              {QStringLiteral("VeryCompact"), i18nc("@item:inlistbox", "Very compact")}},
             QStringLiteral("Normal"));
    interfaceTabs->addTab(statusPage, i18nc("@title:tab", "Status Bar"));
    addPage(interfacePage, i18nc("@title", "Interface"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("preferences-desktop")));

    auto *defaultsPage = new QWidget(this);
    auto *defaultsLayout = new QVBoxLayout(defaultsPage);
    auto *defaultsForm = new QFormLayout;
    addCombo(defaultsForm,
             QStringLiteral("Views"),
             QStringLiteral("DefaultViewMode"),
             i18nc("@label:listbox", "Default view:"),
             {{QStringLiteral("Details"), i18nc("@item:inlistbox", "Details")}, {QStringLiteral("Icons"), i18nc("@item:inlistbox", "Icons")}},
             QStringLiteral("Details"));
    addCombo(defaultsForm,
             QStringLiteral("Views"),
             QStringLiteral("SortColumn"),
             i18nc("@label:listbox", "Sort by:"),
             {{QStringLiteral("Name"), i18nc("@item:inlistbox", "Name")},
              {QStringLiteral("Size"), i18nc("@item:inlistbox", "Size")},
              {QStringLiteral("ModifiedTime"), i18nc("@item:inlistbox", "Modified")},
              {QStringLiteral("Type"), i18nc("@item:inlistbox", "Type")},
              {QStringLiteral("Permissions"), i18nc("@item:inlistbox", "Permissions")},
              {QStringLiteral("Owner"), i18nc("@item:inlistbox", "Owner")},
              {QStringLiteral("Group"), i18nc("@item:inlistbox", "Group")}},
             QStringLiteral("Name"));
    addCheck(defaultsForm, QStringLiteral("Views"), QStringLiteral("SortDescending"), i18nc("@option:check", "Sort in descending order"), false);
    addCheck(defaultsForm, QStringLiteral("Views"), QStringLiteral("FoldersFirst"), i18nc("@option:check", "Show folders before files"), true);
    addCheck(defaultsForm, QStringLiteral("Views"), QStringLiteral("ShowHiddenFiles"), i18nc("@option:check", "Show hidden files"), false);
    addCheck(defaultsForm, QStringLiteral("Views"), QStringLiteral("ShowToolTips"), i18nc("@option:check", "Show tooltips for truncated names"), true);
    addSpin(defaultsForm,
            QStringLiteral("Views"),
            QStringLiteral("DetailsIconSize"),
            i18nc("@label:spinbox", "Details icon size:"),
            16,
            64,
            16,
            i18nc("@item:valuesuffix pixels", " px"));
    addSpin(defaultsForm,
            QStringLiteral("Views"),
            QStringLiteral("IconsIconSize"),
            i18nc("@label:spinbox", "Icon view size:"),
            32,
            256,
            64,
            i18nc("@item:valuesuffix pixels", " px"));
    addCombo(defaultsForm,
             QStringLiteral("Views"),
             QStringLiteral("DetailsDensity"),
             i18nc("@label:listbox", "Details density:"),
             {{QStringLiteral("Normal"), i18nc("@item:inlistbox", "Normal")},
              {QStringLiteral("Compact"), i18nc("@item:inlistbox", "Compact")},
              {QStringLiteral("VeryCompact"), i18nc("@item:inlistbox", "Very compact")},
              {QStringLiteral("Custom"), i18nc("@item:inlistbox", "Custom")}},
             QStringLiteral("Normal"));
    addSpin(defaultsForm,
            QStringLiteral("Views"),
            QStringLiteral("DetailsCustomSpace"),
            i18nc("@label:spinbox", "Details custom spacing:"),
            0,
            10,
            2,
            i18nc("@item:valuesuffix pixels", " px"));
    addCombo(defaultsForm,
             QStringLiteral("Views"),
             QStringLiteral("IconsDensity"),
             i18nc("@label:listbox", "Icon density:"),
             {{QStringLiteral("Normal"), i18nc("@item:inlistbox", "Normal")},
              {QStringLiteral("Compact"), i18nc("@item:inlistbox", "Compact")},
              {QStringLiteral("VeryCompact"), i18nc("@item:inlistbox", "Very compact")},
              {QStringLiteral("Custom"), i18nc("@item:inlistbox", "Custom")}},
             QStringLiteral("Normal"));
    addSpin(defaultsForm,
            QStringLiteral("Views"),
            QStringLiteral("IconsCustomSpace"),
            i18nc("@label:spinbox", "Icon custom spacing:"),
            0,
            10,
            2,
            i18nc("@item:valuesuffix pixels", " px"));
    addCheck(defaultsForm,
             QStringLiteral("Views"),
             QStringLiteral("DragFromAnywhere"),
             i18nc("@option:check", "Start dragging from anywhere in a details row"),
             false);
    defaultsLayout->addLayout(defaultsForm);
    defaultsLayout->addStretch();
    addPage(defaultsPage, i18nc("@title", "View Defaults"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-list-details")));

    auto *contextPage = new QWidget(this);
    auto *contextLayout = new QVBoxLayout(contextPage);
    auto *contextForm = new QFormLayout;
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowOpenInNewTab"), i18nc("@option:check", "Open in New Tab"), true);
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowOpenInOtherPane"), i18nc("@option:check", "Open in Other Pane"), true);
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowCopyToOtherPane"), i18nc("@option:check", "Copy to Other Pane"), true);
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowMoveToOtherPane"), i18nc("@option:check", "Move to Other Pane"), true);
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowDelete"), i18nc("@option:check", "Show permanent Delete"), true);
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowOpenTerminal"), i18nc("@option:check", "Open Terminal Here"), true);
    addCheck(contextForm, QStringLiteral("ContextMenu"), QStringLiteral("ShowCopyPath"), i18nc("@option:check", "Copy Path"), true);
    addCheck(contextForm,
             QStringLiteral("ContextMenu"),
             QStringLiteral("ShowCopyMoveToMenus"),
             i18nc("@option:check", "Show Copy To and Move To submenus"),
             false);
    contextLayout->addLayout(contextForm);
    auto *serviceGroup = new QGroupBox(i18nc("@title:group", "Installed Service Menu Actions"), contextPage);
    auto *serviceLayout = new QVBoxLayout(serviceGroup);
    m_serviceMenus = new QListWidget(serviceGroup);
    m_serviceMenus->setObjectName(QStringLiteral("service_menu_actions"));
    struct ServiceEntry {
        QString key;
        QString text;
        bool defaultShown = true;
    };
    QList<ServiceEntry> services;
    QStringList serviceKeys;
    const QStringList serviceFolders =
        QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kio/servicemenus"), QStandardPaths::LocateDirectory);
    const QStringList serviceFiles = KFileUtils::findAllUniqueFiles(serviceFolders, {QStringLiteral("*.desktop")});
    for (const QString &file : serviceFiles) {
        for (const KServiceAction &action : KService(file).actions()) {
            if (!action.noDisplay() && !action.isSeparator() && !serviceKeys.contains(action.name())) {
                serviceKeys.append(action.name());
                services.append({action.name(), action.text(), true});
            }
        }
    }
    for (const KPluginMetaData &plugin : KPluginMetaData::findPlugins(QStringLiteral("kf6/kfileitemaction"))) {
        if (!serviceKeys.contains(plugin.pluginId())) {
            serviceKeys.append(plugin.pluginId());
            services.append({plugin.pluginId(), plugin.name(), plugin.pluginId() != QLatin1String("hidefileitemaction")});
        }
    }
    std::sort(services.begin(), services.end(), [](const ServiceEntry &a, const ServiceEntry &b) {
        return QString::localeAwareCompare(a.text, b.text) < 0;
    });
    const KConfigGroup serviceSettings = KSharedConfig::openConfig(QStringLiteral("kservicemenurc"), KConfig::NoGlobals)->group(QStringLiteral("Show"));
    for (const ServiceEntry &service : std::as_const(services)) {
        auto *item = new QListWidgetItem(service.text, m_serviceMenus);
        item->setData(Qt::UserRole, service.key);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(serviceSettings.readEntry(service.key, service.defaultShown) ? Qt::Checked : Qt::Unchecked);
    }
    serviceLayout->addWidget(m_serviceMenus);
    contextLayout->addWidget(serviceGroup);
    addPage(contextPage, i18nc("@title", "Context Menu"))->setIcon(SymbolicIcon::fromTheme(QStringLiteral("context-menu")));

    connect(m_sidebarIconSize, &QSpinBox::valueChanged, this, &SettingsDialog::applySidebarStyle);
    connect(m_sidebarTitlePosition, &QComboBox::currentIndexChanged, this, &SettingsDialog::applySidebarStyle);
    applySidebarStyle();
}

void SettingsDialog::applySidebarStyle()
{
    KPageWidgetItem *current = currentPage();
    const auto face = m_sidebarTitlePosition->currentData().toString() == QLatin1String("below") ? KPageDialog::List : KPageDialog::FlatList;
    if (pageWidget()->faceType() != static_cast<KPageView::FaceType>(face)) {
        setFaceType(face);
        if (current) {
            setCurrentPage(current);
        }
        QTimer::singleShot(0, this, &SettingsDialog::applySidebarStyle);
    }
    const auto views = pageWidget()->findChildren<QAbstractItemView *>(Qt::FindDirectChildrenOnly);
    for (QAbstractItemView *view : views) {
        if (view->model() == pageWidget()->model()) {
            view->setIconSize(QSize(m_sidebarIconSize->value(), m_sidebarIconSize->value()));
        }
    }
}

void SettingsDialog::accept()
{
    ViewSettings settings;
    settings.visibleColumns.clear();
    for (const auto &[key, label] : Columns) {
        Q_UNUSED(label)
        const QCheckBox *check = m_columnChecks.value(key);
        if (check && check->isChecked()) {
            settings.visibleColumns.append(key);
        }
    }
    for (const auto &[key, label] : IconInformation) {
        Q_UNUSED(label)
        const QCheckBox *check = m_iconInformationChecks.value(key);
        if (check && check->isChecked()) {
            settings.iconInformation.append(key);
        }
    }
    settings.groupBy = m_groupBy->currentData().toString();
    settings.sizeFormat = m_sizeFormat->currentData().toString();
    settings.dateFormat = m_dateFormat->currentData().toString();
    settings.showSeconds = m_showSeconds->isChecked();
    settings.iconNameLines = m_iconNameLines->value();

    const QStringList ignoreEntries = m_ignoreEntries->toPlainText().split(u'\n', Qt::SkipEmptyParts);
    const IgnoreList ignoreList(ignoreEntries);
    const qsizetype nonEmptyEntries = std::count_if(ignoreEntries.cbegin(), ignoreEntries.cend(), [](const QString &entry) {
        return !entry.trimmed().isEmpty();
    });
    if (ignoreList.entries().size() != nonEmptyEntries) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid Ignore Rules"),
                             i18nc("@info", "Each ignore rule must be at most 80 characters, contain no slash, and the list may have at most 100 entries."));
        return;
    }

    KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
    QStringList colorEntries;
    for (int row = 0; row < m_colorRules->rowCount(); ++row) {
        const QString extension = m_colorRules->item(row, 0)->text();
        const QColor color = m_colorRules->item(row, 1)->data(Qt::UserRole).value<QColor>();
        if (!color.isValid()) {
            QMessageBox::warning(this, i18nc("@title:window", "Invalid File Colour"), i18nc("@info", "Choose a valid colour for every file extension."));
            return;
        }
        colorEntries.append(extension + u'=' + color.name(QColor::HexRgb));
    }
    const QStringList sanitizedColors = FileColorRules(colorEntries).entries();
    if (sanitizedColors.size() != m_colorRules->rowCount()) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid File Colour Rules"),
                             i18nc("@info", "Use a unique file extension of up to 32 letters, numbers, dots, underscores, plus signs, or hyphens."));
        return;
    }

    KConfigGroup view = KSharedConfig::openConfig()->group(QStringLiteral("View"));
    settings.writeConfig(view);
    files.writeEntry("IgnoreList", ignoreList.entries());
    files.writeEntry("ExtensionColors", sanitizedColors);

    KConfigGroup sidebarSettings = KSharedConfig::openConfig()->group(QStringLiteral("SettingsDialog"));
    sidebarSettings.writeEntry("SidebarIconSize", m_sidebarIconSize->value());
    sidebarSettings.writeEntry("SidebarTitlePosition", m_sidebarTitlePosition->currentData().toString());

    KConfigGroup panelSettings = KSharedConfig::openConfig()->group(QStringLiteral("PanelSettings"));
    panelSettings.writeEntry("Visible", m_showPanels->isChecked());
    panelSettings.writeEntry("Position", m_panelPosition->currentData().toString());
    panelSettings.writeEntry("AutoFocus", m_autoFocusFolders->isChecked());
    panelSettings.writeEntry("TitlesVisible", m_showPanelTitles->isChecked());
    panelSettings.writeEntry("FavoritesVisible", m_showFavorites->isChecked());
    panelSettings.writeEntry("PlacesVisible", m_showPlaces->isChecked());
    panelSettings.writeEntry("FoldersVisible", m_showFolderTree->isChecked());

    KSharedConfig::openConfig()->group(QStringLiteral("Archives")).writeEntry("DefaultFormat", m_defaultArchiveFormat->currentData().toInt());
    KConfigGroup tabs = KSharedConfig::openConfig()->group(QStringLiteral("Tabs"));
    tabs.writeEntry("ShowIcons", m_showTabIcons->isChecked());
    tabs.writeEntry("Expand", m_expandTabs->isChecked());

    const auto config = KSharedConfig::openConfig();
    for (auto it = m_m02Controls.cbegin(); it != m_m02Controls.cend(); ++it) {
        const qsizetype separator = it.key().indexOf(u'/');
        KConfigGroup group = config->group(it.key().first(separator));
        const QString key = it.key().sliced(separator + 1);
        if (const auto *check = qobject_cast<const QCheckBox *>(it.value())) {
            group.writeEntry(key, check->isChecked());
        } else if (const auto *combo = qobject_cast<const QComboBox *>(it.value())) {
            group.writeEntry(key, combo->currentData().toString());
        } else if (const auto *spin = qobject_cast<const QSpinBox *>(it.value())) {
            group.writeEntry(key, spin->value());
        } else if (const auto *line = qobject_cast<const QLineEdit *>(it.value())) {
            group.writeEntry(key, line->text().trimmed());
        }
    }
    config->group(QStringLiteral("Confirmations")).writeEntry("ConfirmClosingMultipleTabs", m_confirmMultipleTabs->isChecked());
    QStringList previewPlugins;
    for (int row = 0; row < m_previewPlugins->count(); ++row) {
        const QListWidgetItem *item = m_previewPlugins->item(row);
        if (item->checkState() == Qt::Checked) {
            previewPlugins.append(item->data(Qt::UserRole).toString());
        }
    }
    config->group(QStringLiteral("PreviewSettings")).writeEntry("Plugins", previewPlugins);
    auto kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
    KConfigGroup kioConfirmations = kioConfig->group(QStringLiteral("Confirmations"));
    kioConfirmations.writeEntry("ConfirmTrash", m_confirmTrash->isChecked());
    kioConfirmations.writeEntry("ConfirmDelete", m_confirmDelete->isChecked());
    KConfigGroup kioPreviews = kioConfig->group(QStringLiteral("PreviewSettings"));
    kioPreviews.writeEntry("Plugins", previewPlugins);
    const int maximumMiB = config->group(QStringLiteral("PreviewSettings")).readEntry("MaximumSizeMiB", 0);
    const qulonglong maximumBytes = maximumMiB == 0 ? std::numeric_limits<qulonglong>::max() : qulonglong(maximumMiB) * 1024 * 1024;
    kioPreviews.writeEntry("MaximumSize", maximumBytes);
    auto serviceConfig = KSharedConfig::openConfig(QStringLiteral("kservicemenurc"), KConfig::NoGlobals);
    KConfigGroup serviceSettings = serviceConfig->group(QStringLiteral("Show"));
    for (int row = 0; row < m_serviceMenus->count(); ++row) {
        const QListWidgetItem *item = m_serviceMenus->item(row);
        serviceSettings.writeEntry(item->data(Qt::UserRole).toString(), item->checkState() == Qt::Checked);
    }
    config->sync();
    kioConfig->sync();
    serviceConfig->sync();
    KPageDialog::accept();
}

void SettingsDialog::addColorRule()
{
    bool accepted = false;
    QString extension = QInputDialog::getText(this,
                                              i18nc("@title:window", "Add File Colour"),
                                              i18nc("@label:textbox", "Extension (for example: cpp or tar.gz):"),
                                              QLineEdit::Normal,
                                              QString(),
                                              &accepted)
                            .trimmed();
    if (!accepted || extension.isEmpty()) {
        return;
    }
    if (extension.startsWith(u'.')) {
        extension.remove(0, 1);
    }
    const QStringList normalized = FileColorRules({extension + QStringLiteral("=#010101")}).entries();
    if (normalized.isEmpty()) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid Extension"),
                             i18nc("@info", "Enter a file extension with up to 32 letters, numbers, dots, underscores, plus signs, or hyphens."));
        return;
    }
    extension = normalized.first().section(u'=', 0, 0);
    for (int row = 0; row < m_colorRules->rowCount(); ++row) {
        if (m_colorRules->item(row, 0)->text().compare(extension, Qt::CaseInsensitive) == 0) {
            m_colorRules->selectRow(row);
            editColorRule();
            return;
        }
    }
    const QColor color = QColorDialog::getColor(Qt::yellow, this, i18nc("@title:window", "Choose File Colour"));
    if (color.isValid()) {
        setColorRuleRow(m_colorRules->rowCount(), extension, color);
    }
}

void SettingsDialog::editColorRule()
{
    const int row = m_colorRules->currentRow();
    if (row < 0) {
        return;
    }
    const QString currentExtension = m_colorRules->item(row, 0)->text();
    bool accepted = false;
    QString extension = QInputDialog::getText(this,
                                              i18nc("@title:window", "Edit File Colour"),
                                              i18nc("@label:textbox", "Extension:"),
                                              QLineEdit::Normal,
                                              currentExtension,
                                              &accepted)
                            .trimmed();
    if (!accepted || extension.isEmpty()) {
        return;
    }
    if (extension.startsWith(u'.')) {
        extension.remove(0, 1);
    }
    const QStringList normalized = FileColorRules({extension + QStringLiteral("=#010101")}).entries();
    if (normalized.isEmpty()) {
        QMessageBox::warning(this,
                             i18nc("@title:window", "Invalid Extension"),
                             i18nc("@info", "Enter a file extension with up to 32 letters, numbers, dots, underscores, plus signs, or hyphens."));
        return;
    }
    extension = normalized.first().section(u'=', 0, 0);
    for (int other = 0; other < m_colorRules->rowCount(); ++other) {
        if (other != row && m_colorRules->item(other, 0)->text() == extension) {
            QMessageBox::warning(this, i18nc("@title:window", "Duplicate Extension"), i18nc("@info", "There is already a colour rule for this extension."));
            return;
        }
    }
    const QColor initial = m_colorRules->item(row, 1)->data(Qt::UserRole).value<QColor>();
    const QColor color = QColorDialog::getColor(initial, this, i18nc("@title:window", "Choose File Colour"));
    if (color.isValid()) {
        setColorRuleRow(row, extension, color);
    }
}

void SettingsDialog::removeColorRule()
{
    const int row = m_colorRules->currentRow();
    if (row >= 0) {
        m_colorRules->removeRow(row);
    }
}

void SettingsDialog::setColorRuleRow(int row, const QString &extension, const QColor &color)
{
    if (row == m_colorRules->rowCount()) {
        m_colorRules->insertRow(row);
    }
    auto *extensionItem = new QTableWidgetItem(extension);
    auto *colorItem = new QTableWidgetItem(color.name(QColor::HexRgb));
    colorItem->setData(Qt::UserRole, color);
    colorItem->setBackground(color);
    m_colorRules->setItem(row, 0, extensionItem);
    m_colorRules->setItem(row, 1, colorItem);
}
