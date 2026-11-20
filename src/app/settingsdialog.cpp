// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "batchrenamedialog.h"
#include "customcommanddialog.h"
#include "density.h"
#include "localizednumbers.h"
#include "pane.h"
#include "pathfilterpresetsdialog.h"
#include "searchstoredialog.h"
#include "servicemenus.h"
#include "settings.h"
#include "settingsbackup.h"
#include "settingswidgets.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPageWidget>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <KTextEditor/Editor>
#include <KUrlRequester>
#include <KXmlGuiWindow>

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaEnum>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

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

KConfigGroup selectionFilters()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("SelectionFilters"));
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
    addTabbedPage({{createLanguagePage(), i18nc("@title:tab", "Language and Layout")}, {createDateLanguagePage(), i18nc("@title:tab", "Dates")}},
                  i18nc("@title:group", "Translation"),
                  QStringLiteral("preferences-desktop-locale"));
    addTabbedPage({{createSettingsBackupPage(), i18nc("@title:tab", "Backup and Restore")}}, i18nc("@title:group", "Backup"), QStringLiteral("document-save"));
    addTabbedPage({{createToolbarEntriesPage(), i18nc("@title:tab", "Toolbars")},
                   {createFoldersAndTabsPage(), i18nc("@title:tab", "Folders and Tabs")},
                   {createPreviewsPage(), i18nc("@title:tab", "Previews")},
                   {createConfirmationsPage(), i18nc("@title:tab", "Confirmations")},
                   {createPlacesPage(), i18nc("@title:tab", "Panels")},
                   {createPanelAppearancePage(), i18nc("@title:tab", "Panel Appearance")},
                   {createTabAppearancePage(), i18nc("@title:tab", "Tab Appearance")},
                   {createStatusBarPage(), i18nc("@title:tab", "Status and Location Bars")},
                   {createInformationPanelPage(), i18nc("@title:tab", "Information Panel")}},
                  i18nc("@title:group", "Interface"),
                  QStringLiteral("window"));
    addTabbedPage({{createViewGeneralPage(), i18nc("@title:tab", "General")},
                   {createDetailsPage(), i18nc("@title:tab", "Details")},
                   {createIconsPage(), i18nc("@title:tab", "Icons")},
                   {createCompactPage(), i18nc("@title:tab", "Compact")},
                   {createSelectionPage(), i18nc("@title:tab", "Selection")},
                   {createFiltersPage(), i18nc("@title:tab", "Filters")},
                   {createFolderFormatsPage(), i18nc("@title:tab", "Folder Formats")}},
                  i18nc("@title:group", "View"),
                  QStringLiteral("view-list-icons"));
    addTabbedPage({{createContextMenuActionsPage(), i18nc("@title:tab", "Actions")}, {createServiceMenusPage(), i18nc("@title:tab", "Service Menus")}},
                  i18nc("@title:group", "Context Menu"),
                  QStringLiteral("application-menu"));
    addTabbedPage({{createArchivesPage(), i18nc("@title:tab", "Defaults")}}, i18nc("@title:group", "Archives"), QStringLiteral("package-x-generic"));
    addTabbedPage({{createFileRulesPage(), i18nc("@title:tab", "Rules")},
                   {createFileMetadataPage(), i18nc("@title:tab", "Metadata")},
                   {createSelectionFiltersPage(), i18nc("@title:tab", "Selection Filters")}},
                  i18nc("@title:group", "Files"),
                  QStringLiteral("folder"));
    addTabbedPage({{createOpeningPage(), i18nc("@title:tab", "Opening")},
                   {createGalleryInformationPage(), i18nc("@title:tab", "Information")},
                   {createGalleryDisplayPage(), i18nc("@title:tab", "Display")},
                   {createSlideshowPage(), i18nc("@title:tab", "Slideshow")},
                   {createImageConversionPage(), i18nc("@title:tab", "Image Conversion")}},
                  i18nc("@title:group", "Gallery"),
                  QStringLiteral("view-preview"));
    addTabbedPage({{createEditorPage(), i18nc("@title:tab", "Editor")}, {createTextEditorPage(), i18nc("@title:tab", "KTextEditor")}},
                  i18nc("@title:group", "Editor"),
                  QStringLiteral("view-pim-notes"));
    addTabbedPage({{createTerminalPage(), i18nc("@title:tab", "Embedded Terminal")},
                   {createSearchPage(), i18nc("@title:tab", "Search")},
                   {createDuplicateFinderPage(), i18nc("@title:tab", "Duplicate Finder")},
                   {createContentChartPage(), i18nc("@title:tab", "Content Chart")},
                   {createLinkedPanesPage(), i18nc("@title:tab", "Linked Panes")},
                   {createFileDiffPage(), i18nc("@title:tab", "File Diff")},
                   {createPaneComparisonPage(), i18nc("@title:tab", "Pane Comparison")},
                   {createFolderSyncPage(), i18nc("@title:tab", "Folder Sync")},
                   {createBatchRenamePage(), i18nc("@title:tab", "Batch Rename")},
                   {createListingExportPage(), i18nc("@title:tab", "Listing Export")},
                   {createAutomationPage(), i18nc("@title:tab", "Automation")},
                   {createAssociationsPage(), i18nc("@title:tab", "Applications")},
                   {createTransferQueuePage(), i18nc("@title:tab", "Transfer Queue")},
                   {createFileToolDefaultsPage(), i18nc("@title:tab", "File Tools")},
                   {createMetadataEditorPage(), i18nc("@title:tab", "Metadata Editor")}},
                  i18nc("@title:group", "Tools"),
                  QStringLiteral("utilities-terminal"));
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
    auto *numerals = findChild<QLineEdit *>(QStringLiteral("kcfg_CustomNumerals"));
    auto *style = findChild<QComboBox *>(QStringLiteral("kcfg_NumeralStyle"));
    auto *numeralError = findChild<QLabel *>(QStringLiteral("numeralValidationError"));
    const auto validateNumerals = [this, numerals, style, numeralError] {
        const bool custom = style->currentIndex() == Settings::EnumNumeralStyle::CustomDigits;
        const bool valid = !custom || LocalizedNumbers::validCustomNumerals(numerals->text());
        numerals->setEnabled(custom);
        numeralError->setVisible(!valid);
        button(QDialogButtonBox::Ok)->setEnabled(valid);
        if (!valid) {
            button(QDialogButtonBox::Apply)->setEnabled(false);
        }
    };
    connect(this, &KConfigDialog::widgetModified, this, validateNumerals);
    connect(numerals, &QLineEdit::textChanged, this, validateNumerals);
    connect(style, &QComboBox::currentIndexChanged, this, validateNumerals);
    validateNumerals();
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

QWidget *SettingsDialog::createLanguagePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Interface language:"), bound(new InterfaceLanguageCombo, QStringLiteral("InterfaceLanguage"), "language"));
    form->addRow(i18nc("@label:listbox", "Regional formats:"), bound(new RegionalLocaleCombo, QStringLiteral("RegionalLocale"), "localeName"));
    form->addRow(i18nc("@label:listbox", "Layout direction:"),
                 comboBox(QStringLiteral("TranslationDirection"),
                          {i18nc("@item:inlistbox direction", "Follow interface language"),
                           i18nc("@item:inlistbox direction", "Left to right"),
                           i18nc("@item:inlistbox direction", "Right to left")}));
    form->addRow(i18nc("@label:listbox", "Digit shapes:"),
                 comboBox(QStringLiteral("NumeralStyle"),
                          {i18nc("@item:inlistbox digits", "Follow regional locale"),
                           i18nc("@item:inlistbox digits", "Western"),
                           i18nc("@item:inlistbox digits", "Eastern Arabic-Indic"),
                           i18nc("@item:inlistbox digits", "Custom")}));
    auto *numerals = new QLineEdit;
    numerals->setObjectName(QStringLiteral("kcfg_CustomNumerals"));
    form->addRow(i18nc("@label:textbox", "Custom digits (zero to nine):"), numerals);
    layout->addLayout(form);
    auto *numeralError = new QLabel(i18nc("@info", "Enter ten distinct printable symbols, in order from zero to nine."), page);
    numeralError->setObjectName(QStringLiteral("numeralValidationError"));
    numeralError->setWordWrap(true);
    layout->addWidget(numeralError);
    auto *notice = new QLabel(i18nc("@info",
                                    "Restart Opus to fully apply translation settings. Missing translations fall back to the original text. "
                                    "Custom digit shapes apply to Opus file dates, sizes and selected numeric displays; other numbers, "
                                    "including KDE components and editable controls, may use regional or desktop settings."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createDateLanguagePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Calendar:"),
                 comboBox(QStringLiteral("CalendarOverride"),
                          {i18nc("@item:inlistbox calendar", "Follow regional locale"),
                           i18nc("@item:inlistbox calendar", "Gregorian"),
                           i18nc("@item:inlistbox calendar", "Jalali")}));
    auto *date = new QLineEdit;
    date->setObjectName(QStringLiteral("kcfg_CustomDateFormat"));
    date->setPlaceholderText(i18nc("@info:placeholder", "Use listing default"));
    form->addRow(i18nc("@label:textbox", "Date format:"), date);
    auto *time = new QLineEdit;
    time->setObjectName(QStringLiteral("kcfg_CustomTimeFormat"));
    time->setPlaceholderText(i18nc("@info:placeholder", "Use listing default"));
    form->addRow(i18nc("@label:textbox", "Time format:"), time);
    layout->addLayout(form);
    auto *notice = new QLabel(i18nc("@info",
                                    "Custom formats override the listing date and time defaults. Use Qt date and time format symbols, "
                                    "for example yyyy-MM-dd and HH:mm:ss. A calendar override also takes precedence over the ISO listing default. "
                                    "KDE components may use the desktop's formats."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createToolbarEntriesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *description = new QLabel(
        i18nc("@info",
              "Configure the appearance, entries and order of the main toolbar, pane toolbars and hamburger menu. "
              "The existing editors open after this settings window closes. In the toolbar editor, right-click an entry to choose its icon and button style."),
        page);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *appearance = new QFormLayout;
    appearance->addRow(QString(), checkBox(QStringLiteral("LockToolbars"), i18nc("@option:check", "Lock toolbar positions and editing")));
    appearance->addRow(i18nc("@label:listbox", "Default button style:"),
                       comboBox(QStringLiteral("ToolbarButtonStyle"),
                                {i18nc("@item:inlistbox", "Icon only"),
                                 i18nc("@item:inlistbox", "Text only"),
                                 i18nc("@item:inlistbox", "Text beside icon"),
                                 i18nc("@item:inlistbox", "Text under icon")}));
    auto *iconSize = pixelSpinBox(QStringLiteral("ToolbarIconSize"), 0, 64);
    iconSize->setSpecialValueText(i18nc("@item:valuesuffix", "Default"));
    appearance->addRow(i18nc("@label:spinbox", "Icon size:"), iconSize);
    layout->addLayout(appearance);

    QWidget *window = parentWidget();
    const auto addEditorButton = [this, layout, page, window](const QString &name, const QString &text, QAction *action) {
        auto *button = new QPushButton(text, page);
        button->setObjectName(name);
        button->setEnabled(action && action->isEnabled());
        if (action) {
            const QPointer<QAction> target(action);
            const QPointer<QWidget> parent(window);
            connect(action, &QAction::changed, button, [button, target] {
                button->setEnabled(target && target->isEnabled());
            });
            connect(button, &QPushButton::clicked, this, [this, target, parent] {
                accept();
                if (!target || !parent) {
                    return;
                }
                QTimer::singleShot(0, parent, [target] {
                    if (target && (!target->isCheckable() || !target->isChecked())) {
                        target->trigger();
                    }
                });
            });
        }
        layout->addWidget(button);
    };

    auto *mainWindow = qobject_cast<KXmlGuiWindow *>(window);
    QAction *mainToolbar = mainWindow ? mainWindow->actionCollection()->action(QStringLiteral("customize_main_toolbar")) : nullptr;
    addEditorButton(QStringLiteral("configureMainToolbarEntries"), i18nc("@action:button", "Main Toolbar and Hamburger Menu…"), mainToolbar);
    QAction *resetToolbars = mainWindow ? mainWindow->actionCollection()->action(QStringLiteral("reset_toolbar_customization")) : nullptr;
    addEditorButton(QStringLiteral("resetToolbarCustomization"), i18nc("@action:button", "Restore Default Toolbars…"), resetToolbars);
    QList<Pane *> panes = window ? window->findChildren<Pane *>() : QList<Pane *>{};
    std::sort(panes.begin(), panes.end(), [](const Pane *left, const Pane *right) {
        return left->toolbarConfigId() < right->toolbarConfigId();
    });
    for (Pane *pane : panes) {
        const bool first = pane->toolbarConfigId() == QLatin1String("Left");
        const QString label = first ? i18nc("@action:button", "First Pane Toolbar…") : i18nc("@action:button", "Second Pane Toolbar…");
        const QList<QAction *> toolbarActions = pane->toolBar()->actions();
        QAction *configure = toolbarActions.isEmpty() ? nullptr : toolbarActions.last();
        addEditorButton(first ? QStringLiteral("configureFirstPaneToolbarEntries") : QStringLiteral("configureSecondPaneToolbarEntries"), label, configure);
    }
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createSettingsBackupPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *description = new QLabel(i18nc("@info",
                                         "Back up Opus settings and window and panel layouts. Current tabs and folder locations, credentials, saved custom "
                                         "commands and shared KDE/KIO settings are excluded."),
                                   page);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *create = new QPushButton(i18nc("@action:button", "Create Backup…"), page);
    create->setObjectName(QStringLiteral("createSettingsBackup"));
    connect(create, &QPushButton::clicked, this, &SettingsDialog::createSettingsBackup);
    layout->addWidget(create);

    auto *restore = new QPushButton(i18nc("@action:button", "Restore Backup…"), page);
    restore->setObjectName(QStringLiteral("restoreSettingsBackup"));
    connect(restore, &QPushButton::clicked, this, &SettingsDialog::restoreSettingsBackup);
    layout->addWidget(restore);
    layout->addStretch();
    return page;
}

void SettingsDialog::createSettingsBackup()
{
    const QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("opus-settings.tar.gz"));
    QString path = QFileDialog::getSaveFileName(this,
                                                i18nc("@title:window", "Create Settings Backup"),
                                                defaultPath,
                                                i18nc("@item:inlistbox", "Opus settings backup (*.tar.gz)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QLatin1String(".tar.gz"), Qt::CaseInsensitive)) {
        path.append(QStringLiteral(".tar.gz"));
    }
    const QString error = SettingsBackup::create(path, KSharedConfig::openConfig(), KSharedConfig::openStateConfig());
    if (!error.isEmpty()) {
        QMessageBox::critical(this, i18nc("@title:window", "Could Not Create Backup"), error);
        return;
    }
    QMessageBox::information(this,
                             i18nc("@title:window", "Backup Created"),
                             i18nc("@info", "The Opus settings backup was saved to %1.", QDir::toNativeSeparators(path)));
}

void SettingsDialog::restoreSettingsBackup()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      i18nc("@title:window", "Restore Settings Backup"),
                                                      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      i18nc("@item:inlistbox", "Opus settings backup (*.tar.gz)"));
    if (path.isEmpty()) {
        return;
    }
    SettingsBackup::Preview preview;
    const QString inspectionError = SettingsBackup::inspect(path, &preview);
    if (!inspectionError.isEmpty()) {
        QMessageBox::critical(this, i18nc("@title:window", "Invalid Settings Backup"), inspectionError);
        return;
    }

    const QString summary =
        i18nc("@info",
              "This backup contains:\n%1\n\nIt excludes:\n%2\n\nRestoring replaces Opus settings and window and panel layouts on the next launch. "
              "The current configuration will remain untouched until you restart Opus. Continue?",
              preview.included.join(QLatin1Char('\n')),
              preview.excluded.join(QLatin1Char('\n')));
    if (QMessageBox::question(this, i18nc("@title:window", "Preview Settings Backup"), summary, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }
    const QString error = SettingsBackup::stageRestore(path, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!error.isEmpty()) {
        QMessageBox::critical(this, i18nc("@title:window", "Could Not Stage Restore"), error);
        return;
    }

    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Information);
    prompt.setWindowTitle(i18nc("@title:window", "Restore Staged"));
    prompt.setText(i18nc("@info", "The backup will be applied when Opus starts again."));
    auto *quit = prompt.addButton(i18nc("@action:button", "Quit and Apply on Next Launch"), QMessageBox::AcceptRole);
    prompt.addButton(i18nc("@action:button", "Later"), QMessageBox::RejectRole);
    prompt.exec();
    if (prompt.clickedButton() == quit) {
        QCoreApplication::quit();
    }
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

QWidget *SettingsDialog::createTerminalPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Terminal placement:"),
                 comboBox(QStringLiteral("TerminalPlacement"),
                          {i18nc("@item:inlistbox", "Automatic (right for stacked panes)"),
                           i18nc("@item:inlistbox", "Below both panes"),
                           i18nc("@item:inlistbox", "Right of both panes")}));
    form->addRow(checkBox(QStringLiteral("TerminalFollowActiveFolder"), i18nc("@option:check", "Follow the active pane's local folder")));
    auto *notice = new QLabel(i18nc("@info",
                                    "Following switches to a terminal session in each folder, creating one when needed. "
                                    "Existing sessions and their running commands remain open in tabs until you close them."),
                              page);
    notice->setWordWrap(true);
    form->addRow(notice);
    return page;
}

QWidget *SettingsDialog::createSearchPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(checkBox(QStringLiteral("SearchIndexedByDefault"), i18nc("@option:check", "Search indexed locations by default")));
    layout->addWidget(checkBox(QStringLiteral("SearchContentsByDefault"), i18nc("@option:check", "Search file contents by default")));
    auto *manageFilters = new QPushButton(i18nc("@action:button", "Manage Shared Path Filters…"), page);
    manageFilters->setObjectName(QStringLiteral("managePathFilters"));
    layout->addWidget(manageFilters);
    connect(manageFilters, &QPushButton::clicked, this, [this] {
        PathFilterPresetsDialog dialog(this);
        dialog.exec();
    });
    auto *manageSearches = new QPushButton(i18nc("@action:button", "Manage Saved Searches and Collections…"), page);
    manageSearches->setObjectName(QStringLiteral("manageSavedSearches"));
    layout->addWidget(manageSearches);
    connect(manageSearches, &QPushButton::clicked, this, [this] {
        SearchStoreDialog dialog(this);
        dialog.exec();
    });
    auto *description =
        new QLabel(i18nc("@info", "KDE File Search controls indexed folders and exclusions for all applications. Changes there affect the shared Baloo index."),
                   page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *open = new QPushButton(i18nc("@action:button", "Open KDE File Search Settings"), page);
    open->setObjectName(QStringLiteral("openFileSearchSettings"));
    layout->addWidget(open);
    auto *status = new QLabel(page);
    status->setObjectName(QStringLiteral("fileSearchSettingsStatus"));
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    layout->addWidget(status);
    layout->addStretch();
    connect(open, &QPushButton::clicked, page, [status] {
        const auto plugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_baloofile"));
        const auto widgetPlugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings_qwidgets"), QStringLiteral("kcm_baloofile"));
        if (!plugin.isValid() && !widgetPlugin.isValid()) {
            status->setText(i18nc("@info", "KDE File Search settings are not installed."));
            return;
        }
        const QString launcher = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
        if (launcher.isEmpty() || !QProcess::startDetached(launcher, {QStringLiteral("kcm_baloofile")})) {
            status->setText(i18nc("@info", "Could not launch KDE File Search settings."));
            return;
        }
        status->setText(i18nc("@info", "Requested KDE File Search settings. Changes affect the shared Baloo index."));
    });
    return page;
}

QWidget *SettingsDialog::createDuplicateFinderPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    auto *description = new QLabel(i18nc("@info", "Duplicate files are always compared by size and SHA-256 contents."), page);
    description->setWordWrap(true);
    form->addRow(description);
    form->addRow(checkBox(QStringLiteral("DuplicateRequireSameFileName"), i18nc("@option:check", "Also require identical file names")));
    form->addRow(
        i18nc("@label:listbox", "Default candidate selection:"),
        comboBox(QStringLiteral("DuplicateSelectionPolicy"),
                 {i18nc("@item:inlistbox", "Keep newest"), i18nc("@item:inlistbox", "Keep oldest"), i18nc("@item:inlistbox", "Select files in folder")}));
    auto *notice =
        new QLabel(i18nc("@info", "Select Candidates must be invoked explicitly and always leaves at least one file in each group unchecked."), page);
    notice->setWordWrap(true);
    form->addRow(notice);
    return page;
}

QWidget *SettingsDialog::createContentChartPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(checkBox(QStringLiteral("ContentChartShowTreemap"), i18nc("@option:check", "Show the treemap initially")));
    form->addRow(
        i18nc("@label:listbox", "Default table sort:"),
        comboBox(
            QStringLiteral("ContentChartSort"),
            {i18nc("@item:inlistbox", "As listed"), i18nc("@item:inlistbox", "Name"), i18nc("@item:inlistbox", "Size"), i18nc("@item:inlistbox", "Share")}));
    form->addRow(checkBox(QStringLiteral("ContentChartSortDescending"), i18nc("@option:check", "Sort descending")));
    return page;
}

QWidget *SettingsDialog::createLinkedPanesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(checkBox(QStringLiteral("LinkPanesAtStartup"), i18nc("@option:check", "Link the initial pane folders at startup")));
    auto *notice = new QLabel(i18nc("@info", "Both panes must be visible and their initial folders must be existing local folders."), page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createFileDiffPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(checkBox(QStringLiteral("FileDiffWrapLines"), i18nc("@option:check", "Wrap comparison output lines by default")));
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createPaneComparisonPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(i18nc("@info", "Initially show:"), page));
    layout->addWidget(checkBox(QStringLiteral("PaneComparisonShowLeftOnly"), i18nc("@option:check", "Items only in the first pane")));
    layout->addWidget(checkBox(QStringLiteral("PaneComparisonShowRightOnly"), i18nc("@option:check", "Items only in the second pane")));
    layout->addWidget(checkBox(QStringLiteral("PaneComparisonShowDifferent"), i18nc("@option:check", "Different items")));
    layout->addWidget(checkBox(QStringLiteral("PaneComparisonShowMatching"), i18nc("@option:check", "Matching items")));
    auto *notice = new QLabel(i18nc("@info", "Incomplete comparisons are always shown. Selecting differences in the panes requires an explicit action."), page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createFileToolDefaultsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    auto *suffix = new QLineEdit(page);
    suffix->setObjectName(QStringLiteral("kcfg_SymlinkSuffix"));
    suffix->setPlaceholderText(i18nc("@info:placeholder", "Leave empty for the translated default"));
    form->addRow(i18nc("@label:textbox", "Symbolic link name suffix:"), suffix);
    auto *manifest = new QLineEdit(page);
    manifest->setObjectName(QStringLiteral("kcfg_ChecksumFileName"));
    form->addRow(i18nc("@label:textbox", "Checksum filename:"), manifest);
    auto *partSize = new QSpinBox(page);
    partSize->setObjectName(QStringLiteral("kcfg_SplitPartSizeMiB"));
    partSize->setRange(1, std::numeric_limits<int>::max());
    partSize->setSuffix(i18nc("@item:valuesuffix", " MiB"));
    form->addRow(i18nc("@label:spinbox", "Split part size:"), partSize);
    return page;
}

QWidget *SettingsDialog::createMetadataEditorPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(i18nc("@label", "Initially displayed audio fields:"), page));
    layout->addWidget(checkBox(QStringLiteral("MetadataAudioTitle"), i18nc("@option:check", "Title")));
    layout->addWidget(checkBox(QStringLiteral("MetadataAudioArtist"), i18nc("@option:check", "Artist")));
    layout->addWidget(checkBox(QStringLiteral("MetadataAudioAlbum"), i18nc("@option:check", "Album")));
    layout->addWidget(new QLabel(i18nc("@label", "Initially displayed image fields:"), page));
    layout->addWidget(checkBox(QStringLiteral("MetadataImageDescription"), i18nc("@option:check", "Description")));
    layout->addWidget(checkBox(QStringLiteral("MetadataImageArtist"), i18nc("@option:check", "Artist")));
    layout->addWidget(checkBox(QStringLiteral("MetadataImageCopyright"), i18nc("@option:check", "Copyright")));
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createFolderSyncPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(checkBox(QStringLiteral("FolderSyncRecursive"), i18nc("@option:check", "Compare subfolders by default")));
    layout->addWidget(checkBox(QStringLiteral("FolderSyncCompareContents"), i18nc("@option:check", "Compare file contents by default")));
    layout->addWidget(checkBox(QStringLiteral("FolderSyncCompareLinks"), i18nc("@option:check", "Compare symbolic link targets by default")));
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Default copy policy:"),
                 comboBox(QStringLiteral("FolderSyncCopyPolicy"),
                          {i18nc("@item:inlistbox", "Copy all eligible files"),
                           i18nc("@item:inlistbox", "Skip existing files"),
                           i18nc("@item:inlistbox", "Only replace older files")}));
    auto *speed = new QSpinBox(page);
    speed->setObjectName(QStringLiteral("kcfg_FolderSyncSpeedLimitKiB"));
    speed->setRange(0, 1048576);
    speed->setSpecialValueText(i18nc("@item:inlistbox", "Unlimited"));
    speed->setSuffix(i18nc("@item:valuesuffix", " KiB/s"));
    form->addRow(i18nc("@label:spinbox", "Copy speed limit:"), speed);
    layout->addLayout(form);
    auto *notice = new QLabel(i18nc("@info", "Save, load and delete comparison profiles in the folder synchronization dialog."), page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createBatchRenamePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *notice = new QLabel(i18nc("@info",
                                    "Manage named rename rules and choose a default. Changes are saved immediately, even if Preferences is canceled. "
                                    "Renaming always requires a preview."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *manage = new QPushButton(i18nc("@action:button", "Manage Rename Presets…"), page);
    manage->setObjectName(QStringLiteral("manageBatchRenamePresets"));
    layout->addWidget(manage);
    connect(manage, &QPushButton::clicked, this, [this] {
        BatchRenameDialog dialog({}, this, {}, true);
        dialog.exec();
    });
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createListingExportPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Default format:"),
                 comboBox(QStringLiteral("ListingExportFormat"), {i18nc("@item:inlistbox", "CSV"), i18nc("@item:inlistbox", "PDF")}));
    auto *custom = checkBox(QStringLiteral("ListingExportCustomColumns"), i18nc("@option:check", "Use export-specific columns"));
    form->addRow(custom);
    auto *columns = new QWidget(page);
    auto *columnLayout = new QVBoxLayout(columns);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->addWidget(new QLabel(i18nc("@info", "Name is always exported. Other columns follow the current listing order."), columns));
    columnLayout->addWidget(checkBox(QStringLiteral("ListingExportType"), i18nc("@option:check", "Type")));
    columnLayout->addWidget(checkBox(QStringLiteral("ListingExportSize"), i18nc("@option:check", "Size")));
    columnLayout->addWidget(checkBox(QStringLiteral("ListingExportModified"), i18nc("@option:check", "Modified")));
    columnLayout->addWidget(checkBox(QStringLiteral("ListingExportPermissions"), i18nc("@option:check", "Permissions")));
    columnLayout->addWidget(checkBox(QStringLiteral("ListingExportOwner"), i18nc("@option:check", "Owner")));
    columnLayout->addWidget(checkBox(QStringLiteral("ListingExportGroup"), i18nc("@option:check", "Group or rating, as shown in the listing")));
    columns->setEnabled(custom->isChecked());
    connect(custom, &QCheckBox::toggled, columns, &QWidget::setEnabled);
    form->addRow(columns);
    auto *notice = new QLabel(i18nc("@info", "Without export-specific columns, the current listing columns are exported."), page);
    notice->setWordWrap(true);
    form->addRow(notice);
    return page;
}

QWidget *SettingsDialog::createAutomationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *notice = new QLabel(i18nc("@info",
                                    "Manage scripts and custom command presets. Add saved commands using Configure Toolbars. "
                                    "Changes are saved immediately, even if Preferences is canceled. "
                                    "Commands run with your permissions and require a preview and confirmation."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *manage = new QPushButton(i18nc("@action:button", "Manage Commands…"), page);
    manage->setObjectName(QStringLiteral("manageCommands"));
    layout->addWidget(manage);
    connect(manage, &QPushButton::clicked, this, [this] {
        CustomCommandDialog dialog({}, {}, this, {}, true);
        connect(&dialog, &CustomCommandDialog::presetsChanged, this, &SettingsDialog::customCommandsChanged);
        dialog.exec();
    });
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createAssociationsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *notice = new QLabel(i18nc("@info",
                                    "KDE File Associations manages filename patterns and default applications for the desktop. "
                                    "Changes affect other applications and are not reverted when Preferences is canceled."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *open = new QPushButton(i18nc("@action:button", "Open KDE File Associations…"), page);
    open->setObjectName(QStringLiteral("openFileAssociations"));
    layout->addWidget(open);
    auto *status = new QLabel(page);
    status->setObjectName(QStringLiteral("fileAssociationsStatus"));
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);
    layout->addStretch();
    connect(open, &QPushButton::clicked, page, [status] {
        const auto plugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_filetypes"));
        const auto widgetPlugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings_qwidgets"), QStringLiteral("kcm_filetypes"));
        if (!plugin.isValid() && !widgetPlugin.isValid()) {
            status->setText(i18nc("@info", "KDE File Associations settings are not installed."));
            return;
        }
        const QString launcher = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
        if (launcher.isEmpty() || !QProcess::startDetached(launcher, {QStringLiteral("kcm_filetypes")})) {
            status->setText(i18nc("@info", "Could not launch KDE File Associations settings."));
            return;
        }
        status->setText(i18nc("@info", "Requested KDE File Associations. Changes affect shared desktop defaults."));
    });
    return page;
}

QWidget *SettingsDialog::createTransferQueuePage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    auto *queue = checkBox(QStringLiteral("TransferQueueEnabled"), i18nc("@option:check", "Limit concurrent copy and move transfers"));
    form->addRow(queue);
    auto *concurrency = new QSpinBox(page);
    concurrency->setObjectName(QStringLiteral("kcfg_TransferQueueConcurrency"));
    concurrency->setRange(1, 16);
    concurrency->setEnabled(queue->isChecked());
    connect(queue, &QCheckBox::toggled, concurrency, &QWidget::setEnabled);
    form->addRow(i18nc("@label:spinbox", "Maximum concurrent transfers:"), concurrency);
    form->addRow(i18nc("@label:listbox", "Existing destination:"),
                 comboBox(QStringLiteral("TransferConflictPolicy"),
                          {i18nc("@item:inlistbox", "Ask"),
                           i18nc("@item:inlistbox", "Skip"),
                           i18nc("@item:inlistbox", "Rename automatically"),
                           i18nc("@item:inlistbox", "Overwrite automatically")}));
    auto *notice = new QLabel(i18nc("@info",
                                    "Automatic overwrite replaces existing files and merges folders without asking. "
                                    "These defaults apply to new copy and move transfers; paste and drag-and-drop are not queued. "
                                    "Concurrency changes take effect when the queue is next opened or a transfer is added."),
                              page);
    notice->setWordWrap(true);
    form->addRow(notice);
    return page;
}

QWidget *SettingsDialog::createEditorPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    auto *opening = comboBox(QStringLiteral("EditorOpening"),
                             {i18nc("@item:inlistbox", "Side panel"),
                              i18nc("@item:inlistbox", "Separate embedded area"),
                              i18nc("@item:inlistbox", "Maximized window"),
                              i18nc("@item:inlistbox", "Separate window")});
    auto *placement =
        comboBox(QStringLiteral("EditorEmbeddedPlacement"), {i18nc("@item:inlistbox", "Below panes"), i18nc("@item:inlistbox", "Right of panes")});
    connect(opening, &QComboBox::currentIndexChanged, placement, [placement](int index) {
        placement->setEnabled(index == Settings::EnumEditorOpening::Embedded);
    });
    form->addRow(i18nc("@label:listbox", "Open editor in:"), opening);
    form->addRow(i18nc("@label:listbox", "Embedded placement:"), placement);
    auto *font = new QSpinBox;
    font->setObjectName(QStringLiteral("kcfg_EditorFontSize"));
    font->setRange(6, 72);
    font->setSuffix(i18nc("@item:valuesuffix points", " pt"));
    form->addRow(i18nc("@label:spinbox", "Font size:"), font);
    form->addRow(QString(), checkBox(QStringLiteral("EditorWordWrap"), i18nc("@option:check", "Wrap long lines")));
    form->addRow(i18nc("@label:listbox", "Default Markdown mode:"),
                 comboBox(QStringLiteral("EditorMarkdownMode"), {i18nc("@item:inlistbox", "Source"), i18nc("@item:inlistbox", "Visual")}));
    form->addRow(QString(), checkBox(QStringLiteral("EditorDraftsEnabled"), i18nc("@option:check", "Save drafts automatically")));
    auto *interval = new QSpinBox;
    interval->setObjectName(QStringLiteral("kcfg_EditorDraftIntervalMs"));
    interval->setRange(100, 600000);
    interval->setSuffix(i18nc("@item:valuesuffix milliseconds", " ms"));
    form->addRow(i18nc("@label:spinbox", "Draft autosave delay:"), interval);
    auto *retention = new QSpinBox;
    retention->setObjectName(QStringLiteral("kcfg_EditorDraftRetentionDays"));
    retention->setRange(0, 3650);
    retention->setSpecialValueText(i18nc("@item:inlistbox", "Keep indefinitely"));
    retention->setSuffix(i18nc("@item:valuesuffix days", " days"));
    retention->setToolTip(i18nc("@info:tooltip", "Expired drafts are deleted when the editor opens. Drafts in use and legacy drafts are kept."));
    form->addRow(i18nc("@label:spinbox", "Keep drafts for:"), retention);
    form->addRow(
        i18nc("@label:listbox", "PDF page size:"),
        comboBox(QStringLiteral("EditorPdfPageSize"), {i18nc("@item:inlistbox", "A4"), i18nc("@item:inlistbox", "Letter"), i18nc("@item:inlistbox", "Legal")}));
    const auto margin = [](const QString &entry) {
        auto *spin = new QSpinBox;
        spin->setObjectName(QStringLiteral("kcfg_") + entry);
        spin->setRange(0, 50);
        spin->setSuffix(i18nc("@item:valuesuffix millimetres", " mm"));
        return spin;
    };
    form->addRow(i18nc("@label:spinbox", "PDF left margin:"), margin(QStringLiteral("EditorPdfMarginLeft")));
    form->addRow(i18nc("@label:spinbox", "PDF top margin:"), margin(QStringLiteral("EditorPdfMarginTop")));
    form->addRow(i18nc("@label:spinbox", "PDF right margin:"), margin(QStringLiteral("EditorPdfMarginRight")));
    form->addRow(i18nc("@label:spinbox", "PDF bottom margin:"), margin(QStringLiteral("EditorPdfMarginBottom")));
    placement->setEnabled(opening->currentIndex() == Settings::EnumEditorOpening::Embedded);
    return page;
}

QWidget *SettingsDialog::createTextEditorPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *description = new QLabel(i18nc("@info",
                                         "KTextEditor settings are applied and saved in its own configuration dialog. "
                                         "Opus font size and word wrap settings take precedence in Opus editor views."),
                                   page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *configure = new QPushButton(i18nc("@action:button", "Configure KTextEditor…"), page);
    configure->setObjectName(QStringLiteral("configureTextEditor"));
    connect(configure, &QPushButton::clicked, this, [this] {
        KTextEditor::Editor::instance()->configDialog(this);
    });
    layout->addWidget(configure);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createGalleryInformationPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(checkBox(QStringLiteral("GalleryShowInformation"), i18nc("@option:check", "Show image information in new gallery windows")));
    return page;
}

QWidget *SettingsDialog::createGalleryDisplayPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Initial image zoom:"),
                 comboBox(QStringLiteral("GalleryInitialZoom"), {i18nc("@item:inlistbox", "Fit"), i18nc("@item:inlistbox", "Actual size")}));
    form->addRow(i18nc("@label:listbox", "Preview and gallery image background:"),
                 comboBox(QStringLiteral("ImageBackground"),
                          {i18nc("@item:inlistbox", "Follow theme"),
                           i18nc("@item:inlistbox", "Light"),
                           i18nc("@item:inlistbox", "Dark"),
                           i18nc("@item:inlistbox", "Checkerboard")}));
    form->addRow(checkBox(QStringLiteral("GalleryShowThumbnails"), i18nc("@option:check", "Show thumbnails in new galleries")));
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
    form->addRow(checkBox(QStringLiteral("GalleryBatchResize"), i18nc("@option:check", "Resize batch conversions by default")));
    auto *width = new QSpinBox;
    width->setObjectName(QStringLiteral("kcfg_GalleryBatchWidth"));
    width->setRange(1, 20000);
    auto *height = new QSpinBox;
    height->setObjectName(QStringLiteral("kcfg_GalleryBatchHeight"));
    height->setRange(1, 20000);
    form->addRow(i18nc("@label:spinbox", "Batch maximum width:"), width);
    form->addRow(i18nc("@label:spinbox", "Batch maximum height:"), height);
    form->addRow(
        i18nc("@label:listbox", "Default batch format:"),
        comboBox(QStringLiteral("GalleryBatchFormat"), {i18nc("@item:inlistbox", "PNG"), i18nc("@item:inlistbox", "JPEG"), i18nc("@item:inlistbox", "WebP")}));
    auto *batchQuality = new QSpinBox;
    batchQuality->setObjectName(QStringLiteral("kcfg_GalleryBatchQuality"));
    batchQuality->setRange(0, 100);
    form->addRow(i18nc("@label:spinbox", "Default batch JPEG and WebP quality:"), batchQuality);
    auto *batchDestination = new QLineEdit;
    batchDestination->setObjectName(QStringLiteral("kcfg_GalleryBatchDestination"));
    batchDestination->setPlaceholderText(i18nc("@info:placeholder", "Source folder"));
    auto *browse = new QPushButton(i18nc("@action:button", "Browse…"), page);
    connect(browse, &QPushButton::clicked, page, [page, batchDestination] {
        const QString path = QFileDialog::getExistingDirectory(page, i18nc("@title:window", "Choose Default Batch Destination"), batchDestination->text());
        if (!path.isEmpty()) {
            batchDestination->setText(path);
        }
    });
    auto *destination = new QHBoxLayout;
    destination->addWidget(batchDestination, 1);
    destination->addWidget(browse);
    form->addRow(i18nc("@label:textbox", "Default batch destination:"), destination);
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
    auto *dimInactive = checkBox(QStringLiteral("DimInactivePane"), i18nc("@option:check", "Dim the inactive pane"));
    auto *inactiveOpacity = new QSpinBox;
    inactiveOpacity->setObjectName(QStringLiteral("kcfg_InactivePaneOpacity"));
    inactiveOpacity->setRange(20, 100);
    inactiveOpacity->setSuffix(i18nc("@item:valuesuffix percent", "%"));
    connect(dimInactive, &QCheckBox::toggled, inactiveOpacity, &QWidget::setEnabled);
    inactiveOpacity->setEnabled(dimInactive->isChecked());
    form->addRow(QString(), dimInactive);
    form->addRow(i18nc("@label:spinbox", "Inactive pane opacity:"), inactiveOpacity);
    form->addRow(i18nc("@label:listbox", "Default second-pane destination:"),
                 comboBox(QStringLiteral("PaneMoveDestination"), {i18nc("@item:inlistbox", "Tab"), i18nc("@item:inlistbox", "Separate window")}));
    form->addRow(
        i18nc("@label:listbox", "Display style when moving:"),
        comboBox(QStringLiteral("PaneMoveDisplayStyle"), {i18nc("@item:inlistbox", "Keep current view"), i18nc("@item:inlistbox", "Reset to default view")}));
    form->addRow(i18nc("@label:listbox", "Default pane swap:"),
                 comboBox(QStringLiteral("PaneSwapMode"), {i18nc("@item:inlistbox", "Swap current tabs"), i18nc("@item:inlistbox", "Swap whole pane sides")}));
    form->addRow(i18nc("@title:group", "Navigation:"),
                 checkBox(QStringLiteral("FollowSymlinks"), i18nc("@option:check", "Always follow symbolic links to their target")));
    form->addRow(i18nc("@title:group", "History:"),
                 checkBox(QStringLiteral("RestoreHistorySelection"), i18nc("@option:check", "Restore selection when navigating back or forward")));
    form->addRow({}, checkBox(QStringLiteral("RestoreHistoryScroll"), i18nc("@option:check", "Restore scroll position when navigating back or forward")));
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
    auto *showMinimap = checkBox(QStringLiteral("ShowFileListMinimap"), i18nc("@option:check", "Show a minimap in each pane"));
    auto *minimapWidth = pixelSpinBox(QStringLiteral("FileListMinimapWidth"), 8, 64);
    connect(showMinimap, &QCheckBox::toggled, minimapWidth, &QWidget::setEnabled);
    minimapWidth->setEnabled(showMinimap->isChecked());
    form->addRow(i18nc("@title:group", "File list:"), showMinimap);
    form->addRow(i18nc("@label:spinbox", "Minimap width:"), minimapWidth);
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
    auto *sizeRefresh = new QSpinBox;
    sizeRefresh->setObjectName(QStringLiteral("kcfg_FolderSizeRefreshInterval"));
    sizeRefresh->setRange(0, 86400);
    sizeRefresh->setSpecialValueText(i18nc("@item:inrange", "Off"));
    sizeRefresh->setSuffix(i18nc("@item:valuesuffix seconds", " s"));
    sizeRefresh->setToolTip(i18nc("@info:tooltip", "Periodically recalculate displayed folder sizes. This may be expensive, especially for remote folders."));
    form->addRow(i18nc("@label:spinbox", "Refresh calculated folder sizes:"), sizeRefresh);
    form->addRow(i18nc("@label:listbox", "Date format:"),
                 comboBox(QStringLiteral("DateFormat"), {i18nc("@item:inlistbox", "Locale"), i18nc("@item:inlistbox", "ISO 8601")}));
    form->addRow({}, checkBox(QStringLiteral("ShowSeconds"), i18nc("@option:check", "Show seconds")));
    return page;
}

QWidget *SettingsDialog::createDetailsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("InlineFolderExpansion"), i18nc("@option:check", "Expand folders inline")));
    form->addRow(i18nc("@label:listbox", "Default column widths:"),
                 comboBox(QStringLiteral("DetailsColumnWidth"),
                          {i18nc("@item:inlistbox", "Stretch name"), i18nc("@item:inlistbox", "Automatic"), i18nc("@item:inlistbox", "Manual")}));
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

QWidget *SettingsDialog::createSelectionPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("DefaultSelectionMarkers"), i18nc("@option:check", "Show selection markers by default")));
    form->addRow({}, checkBox(QStringLiteral("DefaultSelectionMode"), i18nc("@option:check", "Start panes in selection mode")));
    return page;
}

QWidget *SettingsDialog::createCompactPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:spinbox", "Icon size:"), pixelSpinBox(QStringLiteral("CompactIconSize"), 16, 64));
    form->addRow(i18nc("@label:spinbox", "Space between items:"), pixelSpinBox(QStringLiteral("CompactItemSpacing"), 0, 20));
    form->addRow(i18nc("@label", "Additional information:"), bound(new IconInformationList, QStringLiteral("CompactInformation"), "information"));
    return page;
}

QWidget *SettingsDialog::createFiltersPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Default filter scope:"),
                 comboBox(QStringLiteral("DefaultFilterScope"),
                          {i18nc("@item:inlistbox", "Files"), i18nc("@item:inlistbox", "Folders"), i18nc("@item:inlistbox", "Files and folders")}));
    form->addRow({}, checkBox(QStringLiteral("FilterTypeAhead"), i18nc("@option:check", "Open the filter when typing in a listing")));
    form->addRow({}, checkBox(QStringLiteral("RetainFilterOnNavigation"), i18nc("@option:check", "Keep filters when changing folders")));
    form->addRow({}, checkBox(QStringLiteral("HighlightFilterMatches"), i18nc("@option:check", "Highlight matching file names")));
    return page;
}

QWidget *SettingsDialog::createFolderFormatsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("AutomaticLocalFolderSizes"), i18nc("@option:check", "Automatically calculate local folder sizes")));
    form->addRow({}, checkBox(QStringLiteral("AutomaticRemoteFolderSizes"), i18nc("@option:check", "Automatically calculate remote folder sizes")));
    auto *description = new QLabel(i18nc("@info", "Manage the current folder's format and inheritance in the View menu."), page);
    description->setWordWrap(true);
    form->addRow(description);
    return page;
}

QWidget *SettingsDialog::createPreviewsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("ShowPreviews"), i18nc("@option:check", "Show previews in new windows")));
    form->addRow(QString(), checkBox(QStringLiteral("ShowRemotePreviews"), i18nc("@option:check", "Show previews in remote folders")));
    form->addRow(QString(), checkBox(QStringLiteral("PreviewMediaAutoplay"), i18nc("@option:check", "Automatically play audio and video previews")));
    form->addRow(QString(), checkBox(QStringLiteral("PreviewFormatted"), i18nc("@option:check", "Show formatted HTML and Markdown by default")));
    form->addRow(QString(), checkBox(QStringLiteral("PreviewLineNumbers"), i18nc("@option:check", "Show line numbers in text previews")));
    form->addRow(QString(), checkBox(QStringLiteral("PreviewSyntaxHighlighting"), i18nc("@option:check", "Highlight syntax in text previews")));
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
    auto *configure = new QPushButton(i18nc("@action:button", "Configure Section Order and Titles…"), page);
    configure->setObjectName(QStringLiteral("configureCombinedPanelSections"));
    QWidget *window = parentWidget();
    auto *mainWindow = qobject_cast<KXmlGuiWindow *>(window);
    QAction *action = mainWindow ? mainWindow->actionCollection()->action(QStringLiteral("customize_combined_panel")) : nullptr;
    configure->setEnabled(action && action->isEnabled());
    if (action) {
        const QPointer<QAction> target(action);
        const QPointer<QWidget> parent(window);
        connect(action, &QAction::changed, configure, [configure, target] {
            configure->setEnabled(target && target->isEnabled());
        });
        connect(configure, &QPushButton::clicked, this, [this, target, parent] {
            accept();
            if (!target || !parent) {
                return;
            }
            QTimer::singleShot(0, parent, [target] {
                if (target) {
                    target->trigger();
                }
            });
        });
    }
    form->addRow(i18nc("@title:group", "Section order and titles:"), configure);
    return page;
}

QWidget *SettingsDialog::createTabAppearancePage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("TabIcons"), i18nc("@option:check", "Show folder icons on tabs")));
    form->addRow({}, checkBox(QStringLiteral("ExpandTabs"), i18nc("@option:check", "Expand tabs to fill the available width equally")));
    form->addRow({}, checkBox(QStringLiteral("ShowTabColors"), i18nc("@option:check", "Show folder-based tab colours")));
    form->addRow({}, checkBox(QStringLiteral("ShowTabCloseButtons"), i18nc("@option:check", "Show tab close buttons")));
    form->addRow({}, checkBox(QStringLiteral("StackedTabBarsTogether"), i18nc("@option:check", "Keep stacked pane tab bars together")));
    const QStringList positions{i18nc("@item:inlistbox tab bar position", "Above"),
                                i18nc("@item:inlistbox tab bar position", "Below"),
                                i18nc("@item:inlistbox tab bar position", "Left"),
                                i18nc("@item:inlistbox tab bar position", "Right")};
    form->addRow(i18nc("@label:listbox", "Left pane tabs:"), comboBox(QStringLiteral("HorizontalFirstTabPosition"), positions));
    form->addRow(i18nc("@label:listbox", "Right pane tabs:"), comboBox(QStringLiteral("HorizontalSecondTabPosition"), positions));
    form->addRow(i18nc("@label:listbox", "Upper pane tabs:"), comboBox(QStringLiteral("StackedFirstTabPosition"), positions));
    form->addRow(i18nc("@label:listbox", "Lower pane tabs:"), comboBox(QStringLiteral("StackedSecondTabPosition"), positions));
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
    form->addRow({}, checkBox(QStringLiteral("ArchiveOtherPaneByDefault"), i18nc("@option:check", "Create archives in the other pane by default")));
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

QWidget *SettingsDialog::createFileMetadataPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow({}, checkBox(QStringLiteral("ShowFileLabels"), i18nc("@option:check", "Show label colours")));
    form->addRow({}, checkBox(QStringLiteral("ShowFileColors"), i18nc("@option:check", "Show custom file colours")));
    form->addRow({}, checkBox(QStringLiteral("ShowFileRatings"), i18nc("@option:check", "Show ratings in the rating column")));
    form->addRow({}, checkBox(QStringLiteral("ShowFilePins"), i18nc("@option:check", "Show pin indicators")));
    form->addRow({}, checkBox(QStringLiteral("PinnedItemsFirst"), i18nc("@option:check", "Sort pinned items first")));
    return page;
}

QWidget *SettingsDialog::createSelectionFiltersPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    m_selectionFilters = new RuleListEditor(RuleListEditor::Kind::SelectionFilter, page);
    m_selectionFilters->setObjectName(QStringLiteral("selectionFilterRules"));
    connect(m_selectionFilters, &RuleListEditor::rulesChanged, this, [this] {
        updateButtons();
    });
    form->addRow(i18nc("@label", "Saved selection patterns:"), m_selectionFilters);
    return page;
}

QWidget *SettingsDialog::createStatusBarPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Status indicators:"),
                 comboBox(QStringLiteral("StatusBarLayout"), {i18nc("@item:inlistbox", "Shared"), i18nc("@item:inlistbox", "One per pane")}));
    form->addRow(i18nc("@label:listbox", "Indicator order:"),
                 comboBox(QStringLiteral("StatusBarIndicatorOrder"),
                          {i18nc("@item:inlistbox", "Item counts, then free space"), i18nc("@item:inlistbox", "Free space, then item counts")}));
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

QWidget *SettingsDialog::createInformationPanelPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(QString(), checkBox(QStringLiteral("InformationHover"), i18nc("@option:check", "Show information for the item under the pointer")));
    auto *fields = new IconInformationList;
    fields->setMinimumHeight(240);
    form->addRow(i18nc("@label", "Metadata fields, in display order:"), bound(fields, QStringLiteral("InformationFields"), "information"));
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
    if (m_selectionFilters->rules() != m_savedSelectionFilters) {
        KConfigGroup group = selectionFilters();
        const QStringList edited = m_selectionFilters->rules();
        QStringList patterns = group.readEntry("Patterns", QStringList());
        if (patterns == m_savedSelectionFilters) {
            patterns = edited;
        } else {
            for (const QString &previous : std::as_const(m_savedSelectionFilters)) {
                if (!edited.contains(previous)) {
                    patterns.removeAll(previous);
                }
            }
            for (const QString &pattern : edited) {
                if (!m_savedSelectionFilters.contains(pattern) && !patterns.contains(pattern)) {
                    patterns.append(pattern);
                }
            }
        }
        group.writeEntry("Patterns", patterns);
        group.sync();
        const QSignalBlocker blocker(m_selectionFilters);
        m_selectionFilters->setRules(patterns);
        m_savedSelectionFilters = m_selectionFilters->rules();
    }
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
    const QSignalBlocker blocker(m_selectionFilters);
    m_selectionFilters->setRules(selectionFilters().readEntry("Patterns", QStringList()));
    m_savedSelectionFilters = m_selectionFilters->rules();
    for (const SharedToggle &toggle : std::as_const(m_sharedToggles)) {
        toggle.check->setChecked(toggle.read());
    }
}

void SettingsDialog::updateWidgetsDefault()
{
    m_selectionFilters->setRules({});
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
    return m_selectionFilters->rules() != m_savedSelectionFilters
        || std::any_of(m_sharedToggles.cbegin(), m_sharedToggles.cend(), [](const SharedToggle &toggle) {
               return toggle.check->isChecked() != toggle.read();
           });
}

bool SettingsDialog::isDefault()
{
    return m_selectionFilters->rules().isEmpty() && std::all_of(m_sharedToggles.cbegin(), m_sharedToggles.cend(), [](const SharedToggle &toggle) {
               return toggle.check->isChecked() == toggle.defaultValue;
           });
}
