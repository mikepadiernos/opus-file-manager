// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "appearancedefaults.h"
#include "batchrenamedialog.h"
#include "colorschemes.h"
#include "customcommanddialog.h"
#include "density.h"
#include "doubleshift.h"
#include "embeddedmetadatasettings.h"
#include "libraryarchive.h"
#include "libraryconfirmations.h"
#include "libraryregistry.h"
#include "localizednumbers.h"
#include "pane.h"
#include "pathfilterpresetsdialog.h"
#include "searchhistory.h"
#include "searchstoredialog.h"
#include "servicemenus.h"
#include "settings.h"
#include "settingsbackup.h"
#include "settingswidgets.h"
#include "startuppresentation.h"
#include "tagfieldmanagerwidget.h"
#include "taggingsettings.h"
#include "taglibrary.h"
#include "themeprofiles.h"
#include "windoweffects.h"
#include "workspacecatalog.h"
#include "yazimode.h"

#include <KActionCollection>
#include <KColorButton>
#include <KConfigGroup>
#include <KEditListWidget>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KPageWidget>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <KTextEditor/Editor>
#include <KToolBar>
#include <KUrlRequester>
#include <KXmlGuiWindow>

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaEnum>
#include <QMetaProperty>
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
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
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

/// A label that redraws its preview when its palette or style changes.
class PreviewLabel : public QLabel
{
public:
    using QLabel::QLabel;
    std::function<void()> refresh;

protected:
    void changeEvent(QEvent *event) override
    {
        QLabel::changeEvent(event);
        if (refresh && (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)) {
            refresh();
        }
    }
};
}

SettingsDialog::SettingsDialog(KFilePlacesModel *places, QWidget *parent)
    : KConfigDialog(parent, QLatin1String(Name), Settings::self())
    , m_places(places)
{
    m_confirm = [this](const QString &question) {
        return QMessageBox::question(this, i18nc("@title:window", "Reset Appearance"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes;
    };
    m_confirmLibrary = [this](const LibraryConfirmations::Confirmation &confirmation) {
        return LibraryConfirmations::ask(this, confirmation);
    };
    m_chooseLibraryFolder = [this] {
        return QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose a Library Folder"), QDir::homePath());
    };
    addTabbedPage({{createSettingsDialogPage(), i18nc("@title:tab", "Settings Dialog")}}, i18nc("@title:group", "General"), QStringLiteral("configure"));
    addTabbedPage({{createLanguagePage(), i18nc("@title:tab", "Language and Layout")}, {createDateLanguagePage(), i18nc("@title:tab", "Dates")}},
                  i18nc("@title:group", "Translation"),
                  QStringLiteral("preferences-desktop-locale"));
    addTabbedPage({{createSettingsBackupPage(), i18nc("@title:tab", "Backup and Restore")}}, i18nc("@title:group", "Backup"), QStringLiteral("document-save"));
    m_toolbarPage = addTabbedPage({{createToolbarEntriesPage(), i18nc("@title:tab", "Toolbars")},
                                   {createFoldersAndTabsPage(), i18nc("@title:tab", "Folders and Tabs")},
                                   {createPreviewsPage(), i18nc("@title:tab", "Previews")},
                                   {createConfirmationsPage(), i18nc("@title:tab", "Confirmations")},
                                   {createPlacesPage(), i18nc("@title:tab", "Panels")},
                                   {createPanelAppearancePage(), i18nc("@title:tab", "Panel Appearance")},
                                   {createTabAppearancePage(), i18nc("@title:tab", "Tab Appearance")},
                                   {createStatusBarPage(), i18nc("@title:tab", "Status and Location Bars")},
                                   {createInformationPanelPage(), i18nc("@title:tab", "Information Panel")}},
                                  i18nc("@title:group", "Interface"),
                                  QStringLiteral("window"),
                                  &m_toolbarTabs);
    addTabbedPage({{createColorSchemePage(), i18nc("@title:tab", "Colour Scheme")},
                   {createCustomColorsPage(), i18nc("@title:tab", "Custom Colours")},
                   {createEffectsPage(), i18nc("@title:tab", "Effects")},
                   {createLinesPage(), i18nc("@title:tab", "Lines")},
                   {createHighlightsPage(), i18nc("@title:tab", "Highlights")},
                   {createFolderColorsPage(), i18nc("@title:tab", "Folder Colours")},
                   {createFontsPage(), i18nc("@title:tab", "Fonts")},
                   {createSpacingPage(), i18nc("@title:tab", "Spacing")},
                   {createBarStylePage(), i18nc("@title:tab", "Tabs and Status Bars")},
                   {createMotionPage(), i18nc("@title:tab", "Motion and Accessibility")},
                   {createStyleSheetPage(), i18nc("@title:tab", "Stylesheet")},
                   {createThemeProfilesPage(), i18nc("@title:tab", "Profiles")}},
                  i18nc("@title:group", "Themes"),
                  QStringLiteral("preferences-desktop-theme"));
    addTabbedPage({{createBrandingPresentationPage(), i18nc("@title:tab", "Presentation")}},
                  i18nc("@title:group", "Branding"),
                  QStringLiteral("org.kde.opusfilemanager"));
    addTabbedPage({{createViewGeneralPage(), i18nc("@title:tab", "General")},
                   {createDetailsPage(), i18nc("@title:tab", "Details")},
                   {createIconsPage(), i18nc("@title:tab", "Icons")},
                   {createCompactPage(), i18nc("@title:tab", "Compact")},
                   {createCardsPage(), i18nc("@title:tab", "Cards")},
                   {createSelectionPage(), i18nc("@title:tab", "Selection")},
                   {createFiltersPage(), i18nc("@title:tab", "Filters")},
                   {createFolderFormatsPage(), i18nc("@title:tab", "Folder Formats")}},
                  i18nc("@title:group", "View"),
                  QStringLiteral("view-list-icons"));
    addTabbedPage({{createYaziActivationPage(), i18nc("@title:tab", "Activation")}, {createYaziColumnsPage(), i18nc("@title:tab", "Columns")}},
                  i18nc("@title:group", "Yazi Mode"),
                  QStringLiteral("view-file-columns"));
    addTabbedPage({{createContextMenuActionsPage(), i18nc("@title:tab", "Actions")}, {createServiceMenusPage(), i18nc("@title:tab", "Service Menus")}},
                  i18nc("@title:group", "Context Menu"),
                  QStringLiteral("application-menu"));
    addTabbedPage({{createArchivesPage(), i18nc("@title:tab", "Defaults")}}, i18nc("@title:group", "Archives"), QStringLiteral("package-x-generic"));
    addTabbedPage({{createFileRulesPage(), i18nc("@title:tab", "Rules")},
                   {createFileMetadataPage(), i18nc("@title:tab", "Metadata")},
                   {createSelectionFiltersPage(), i18nc("@title:tab", "Selection Filters")}},
                  i18nc("@title:group", "Files"),
                  QStringLiteral("folder"));
    addTabbedPage({{createTaggingPrivacyPage(), i18nc("@title:tab", "Privacy")},
                   {createTaggingLibrariesPage(), i18nc("@title:tab", "Libraries")},
                   {createTaggingTagsPage(), i18nc("@title:tab", "Tags")},
                   {createTaggingSearchPage(), i18nc("@title:tab", "Search")},
                   {createTaggingFieldsPage(), i18nc("@title:tab", "Fields")},
                   {createTaggingEmbeddedMetadataPage(), i18nc("@title:tab", "Embedded Metadata")}},
                  i18nc("@title:group", "Tagging"),
                  QStringLiteral("tag"));
    addTabbedPage({{createWorkspaceManagementPage(), i18nc("@title:tab", "Management")},
                   {createWorkspaceStartupPage(), i18nc("@title:tab", "Startup")},
                   {createWorkspaceSearchPage(), i18nc("@title:tab", "Search")},
                   {createWorkspaceLayoutsPage(), i18nc("@title:tab", "Layouts")},
                   {createWorkspaceSnapshotsPage(), i18nc("@title:tab", "Snapshots")},
                   {createWorkspaceRecoveryPage(), i18nc("@title:tab", "Recovery")},
                   {createWorkspaceIndicatorsPage(), i18nc("@title:tab", "Indicators")},
                   {createWorkspaceLocationBarsPage(), i18nc("@title:tab", "Location Bars")},
                   {createWorkspaceBehaviorsPage(), i18nc("@title:tab", "Behaviors")},
                   {createWorkspacePortabilityPage(), i18nc("@title:tab", "Portability")}},
                  i18nc("@title:group", "Workspaces"),
                  QStringLiteral("view-grid"));
    reloadWorkspaces();
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

KPageWidgetItem *
SettingsDialog::addTabbedPage(const QList<std::pair<QWidget *, QString>> &tabs, const QString &title, const QString &iconName, QTabWidget **tabsOut)
{
    auto *tabWidget = new QTabWidget;
    if (tabsOut) {
        *tabsOut = tabWidget;
    }
    for (const auto &[tab, tabTitle] : tabs) {
        tabWidget->addTab(tab, tabTitle);
    }
    auto *page = addPage(tabWidget, title);
    m_pageIcons.append({page, iconName});
    return page;
}

void SettingsDialog::showToolbarPage()
{
    setCurrentPage(m_toolbarPage);
    m_toolbarTabs->setCurrentIndex(0);
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
                                    "Restart Plasma Opus to fully apply translation settings. Missing translations fall back to the original text. "
                                    "Custom digit shapes apply to Plasma Opus file dates, sizes and selected numeric displays; other numbers, "
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
    auto *description =
        new QLabel(i18nc("@info",
                         "Back up Plasma Opus settings and window and panel layouts. Current tabs and folder locations, credentials, saved custom "
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
    auto *workspaces = new QLabel(i18nc("@info", "Export or import individual workspace profiles under Workspaces, Portability."), page);
    workspaces->setWordWrap(true);
    layout->addWidget(workspaces);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspacePortabilityPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("workspacePortabilityPage"));
    auto *layout = new QVBoxLayout(page);
    auto *description = new QLabel(i18nc("@info",
                                         "Export selected workspace profiles to a settings backup archive, or import selected profiles from one. "
                                         "Before importing, review which profiles share a name or identity with existing workspaces and choose "
                                         "whether to replace or keep them. Profiles exclude credentials and secrets in remote locations; "
                                         "importing changes workspace settings only and never runs commands or modifies managed files."),
                                   page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *exportWorkspaces = new QPushButton(i18nc("@action:button", "Export Workspace Profiles…"), page);
    exportWorkspaces->setObjectName(QStringLiteral("exportWorkspaceProfiles"));
    connect(exportWorkspaces, &QPushButton::clicked, this, &SettingsDialog::exportWorkspaceProfiles);
    layout->addWidget(exportWorkspaces);

    auto *importWorkspaces = new QPushButton(i18nc("@action:button", "Import Workspace Profiles…"), page);
    importWorkspaces->setObjectName(QStringLiteral("importWorkspaceProfiles"));
    connect(importWorkspaces, &QPushButton::clicked, this, &SettingsDialog::importWorkspaceProfiles);
    layout->addWidget(importWorkspaces);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceManagementPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *description =
        new QLabel(i18nc("@info",
                         "Create, switch, rename, duplicate and delete workspaces, and manage their full names, short labels and assigned locations, in the "
                         "workspace manager. Each location belongs to at most one workspace; unassigned locations stay available in every workspace. "
                         "Duplicating a workspace copies its layout without reassigning locations. Deleting a workspace asks for confirmation and keeps "
                         "its files and locations."),
                   page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *security = new QLabel(i18nc("@info",
                                      "Workspace configuration is stored locally in your Plasma Opus settings without credentials. Workspaces organize "
                                      "what is shown; they are not a security boundary and do not restrict access to files."),
                                page);
    security->setObjectName(QStringLiteral("workspaceSecurityNotice"));
    security->setWordWrap(true);
    layout->addWidget(security);
    m_workspaceSummary = new QListWidget(page);
    m_workspaceSummary->setObjectName(QStringLiteral("workspaceSummary"));
    m_workspaceSummary->setAccessibleName(i18nc("@label", "Workspaces"));
    m_workspaceSummary->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_workspaceSummary);
    auto *manage = new QPushButton(i18nc("@action:button", "Manage Workspaces…"), page);
    manage->setObjectName(QStringLiteral("manageWorkspaces"));
    connect(manage, &QPushButton::clicked, this, &SettingsDialog::manageWorkspacesRequested);
    layout->addWidget(manage, 0, Qt::AlignLeft);
    return page;
}

QWidget *SettingsDialog::createWorkspaceStartupPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    m_startupWorkspace = bound(new StartupWorkspaceCombo, QStringLiteral("StartupWorkspace"), "workspace");
    form->addRow(i18nc("@label:listbox", "Startup workspace:"), m_startupWorkspace);
    layout->addLayout(form);
    auto *notice = new QLabel(i18nc("@info",
                                    "The startup workspace becomes active when Plasma Opus starts. Its saved layout is not applied, so the startup "
                                    "folder and session choices under Interface still decide which folders and tabs open. Switch workspaces to "
                                    "restore a saved layout."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceSearchPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(checkBox(QStringLiteral("WorkspaceSearchByDefault"), i18nc("@option:check", "Limit new searches to the active workspace's locations")));
    auto *notice = new QLabel(i18nc("@info",
                                    "Applies when the active workspace includes search scope in its behaviors. Clear the option in the search "
                                    "window at any time to search the current folder or indexed locations outside the workspace. Other search "
                                    "defaults are under Tools, Search."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceLayoutsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *restore = new QGroupBox(i18nc("@title:group", "Restore When Switching Workspaces"), page);
    auto *restoreLayout = new QVBoxLayout(restore);
    restoreLayout->addWidget(checkBox(QStringLiteral("WorkspaceRestorePaneArrangement"),
                                      i18nc("@option:check", "Pane arrangement: single or dual panes, stacking, sizes and second-pane placement")));
    auto *views = checkBox(QStringLiteral("WorkspaceRestoreViews"), i18nc("@option:check", "Each pane's view mode"));
    views->setToolTip(i18nc("@info:tooltip",
                            "Each pane's Yazi mode has its own workspace behavior. Workspaces saved before that behavior existed restore Yazi "
                            "mode with the view mode."));
    restoreLayout->addWidget(views);
    restoreLayout->addWidget(checkBox(QStringLiteral("WorkspaceRestoreToolbars"), i18nc("@option:check", "Main and pane toolbar entries")));
    restoreLayout->addWidget(checkBox(QStringLiteral("WorkspaceRestorePanels"),
                                      i18nc("@option:check", "Panels: dock and toolbar positions, and the preview, editor and terminal panels")));
    layout->addWidget(restore);
    auto *notice = new QLabel(i18nc("@info",
                                    "These options apply to workspaces whose behaviors include layouts. Parts that are not restored keep their "
                                    "current state. Panels are restored only when their integrations are available."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addWidget(checkBox(QStringLiteral("ConfirmWorkspaceLayoutOverwrite"), i18nc("@option:check", "Ask before replacing a workspace's saved layout")));
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceSnapshotsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *restore = new QGroupBox(i18nc("@title:group", "Restore When Switching Workspaces"), page);
    auto *restoreLayout = new QVBoxLayout(restore);
    restoreLayout->addWidget(checkBox(QStringLiteral("WorkspaceRestoreTabs"), i18nc("@option:check", "Each pane's tabs, with their pins, locks and names")));
    restoreLayout->addWidget(
        checkBox(QStringLiteral("WorkspaceRestoreActiveTabs"), i18nc("@option:check", "The current tab of each pane and the active pane")));
    restoreLayout->addWidget(checkBox(QStringLiteral("WorkspaceRestoreFilters"), i18nc("@option:check", "Each pane's name filter")));
    layout->addWidget(restore);
    auto *notice = new QLabel(i18nc("@info",
                                    "These options apply to workspaces whose behaviors include pane and tab restoration. Snapshots are restored "
                                    "only when you switch workspaces; the startup folder and session choices under Interface still decide what "
                                    "opens at startup. Running file transfers continue when you switch."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Ask before switching:"),
                 comboBox(QStringLiteral("ConfirmWorkspaceSwitch"),
                          {i18nc("@item:inlistbox ask before switching workspaces", "Always"),
                           i18nc("@item:inlistbox ask before switching workspaces", "When the current state is not saved in a workspace"),
                           i18nc("@item:inlistbox ask before switching workspaces", "Never")}));
    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceRecoveryPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(
        checkBox(QStringLiteral("WorkspaceRecoveryPrompt"), i18nc("@option:check", "Offer to retry when locations, providers or panels are unavailable")));
    auto *notice = new QLabel(i18nc("@info",
                                    "When switching workspaces, Plasma Opus lists the folders, remote location providers and panels that cannot be "
                                    "restored, so you can mount a drive or start a service and retry. When the option is cleared, the workspace "
                                    "opens without them and the workspace manager lists them. Workspaces keep their saved references until you save "
                                    "a new layout."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceIndicatorsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Show the workspace selector in:"),
                 comboBox(QStringLiteral("WorkspaceSelectorPlacement"),
                          {i18nc("@item:inlistbox workspace selector placement", "View menu"),
                           i18nc("@item:inlistbox workspace selector placement", "Status bar"),
                           i18nc("@item:inlistbox workspace selector placement", "View menu and status bar")}));
    layout->addLayout(form);
    layout->addWidget(checkBox(QStringLiteral("ShowWorkspaceUnsavedIndicator"),
                               i18nc("@option:check", "Mark the active workspace with * when the window differs from its saved layout")));
    layout->addWidget(checkBox(QStringLiteral("ConfirmWorkspaceLayoutRestore"), i18nc("@option:check", "Ask before returning to the saved workspace layout")));
    auto *notice = new QLabel(i18nc("@info",
                                    "The selector shows the active workspace. View, Workspaces… always stays available, so workspaces can be "
                                    "managed and switched wherever the selector is placed."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceLocationBarsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    const QStringList choices{i18nc("@item:inlistbox location bar", "Full breadcrumbs"),
                              i18nc("@item:inlistbox location bar", "Abbreviated breadcrumbs"),
                              i18nc("@item:inlistbox location bar", "Editable full path")};
    form->addRow(i18nc("@label:listbox", "Left or top pane:"), comboBox(QStringLiteral("LeftLocationPresentation"), choices));
    form->addRow(i18nc("@label:listbox", "Right or bottom pane:"), comboBox(QStringLiteral("RightLocationPresentation"), choices));
    layout->addLayout(form);
    auto *notice = new QLabel(i18nc("@info",
                                    "Changing a choice applies it to that pane now. The Path button in each pane changes its location bar until "
                                    "the next change, and the session and workspace layouts remember it. Switching to a workspace that includes "
                                    "location presentation in its behaviors restores the location bars saved with its layout."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createWorkspaceBehaviorsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    auto *mode = comboBox(QStringLiteral("WorkspaceDefaultBehaviorMode"),
                          {i18nc("@item:inlistbox workspace behaviors", "All"),
                           i18nc("@item:inlistbox workspace behaviors", "Selected"),
                           i18nc("@item:inlistbox workspace behaviors", "None")});
    form->addRow(i18nc("@label:listbox", "New workspaces apply:"), mode);
    layout->addLayout(form);
    auto *selected = new QGroupBox(i18nc("@title:group", "Selected Behaviors"), page);
    auto *selectedLayout = new QVBoxLayout(selected);
    selectedLayout->addWidget(checkBox(QStringLiteral("WorkspaceDefaultLocationPresentation"), i18nc("@option:check", "Location presentation")));
    selectedLayout->addWidget(checkBox(QStringLiteral("WorkspaceDefaultSearchScope"), i18nc("@option:check", "Search scope")));
    selectedLayout->addWidget(checkBox(QStringLiteral("WorkspaceDefaultLayout"), i18nc("@option:check", "Window and panel layout")));
    selectedLayout->addWidget(checkBox(QStringLiteral("WorkspaceDefaultPaneTabRestoration"), i18nc("@option:check", "Pane and tab restoration")));
    selectedLayout->addWidget(checkBox(QStringLiteral("WorkspaceDefaultAppearance"), i18nc("@option:check", "Theme profile and appearance")));
    selectedLayout->addWidget(checkBox(QStringLiteral("WorkspaceDefaultYaziMode"), i18nc("@option:check", "Yazi mode")));
    layout->addWidget(selected);
    const auto updateSelected = [mode, selected] {
        selected->setEnabled(mode->currentIndex() == Settings::EnumWorkspaceDefaultBehaviorMode::Selected);
    };
    connect(mode, &QComboBox::currentIndexChanged, selected, updateSelected);
    updateSelected();
    auto *notice = new QLabel(i18nc("@info",
                                    "New workspaces start with these behaviors. Duplicated workspaces keep the behaviors of the workspace they "
                                    "copy, and existing workspaces keep their own; change them in the workspace manager. Behaviors that are not "
                                    "applied leave the current state unchanged when switching, and the options under Search, Layouts, Snapshots "
                                    "and Location Bars still decide what each applied behavior restores."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

void SettingsDialog::reloadWorkspaces()
{
    if (!m_workspaceSummary || !m_startupWorkspace) {
        return;
    }
    m_workspaceSummary->clear();
    const auto shared = KSharedConfig::openConfig();
    const KConfig config(shared->name(), shared->openFlags(), shared->locationType());
    WorkspaceCatalog::Catalog catalog;
    QString error;
    if (!WorkspaceCatalog::load(config.group(QStringLiteral("Workspaces")), catalog, error)) {
        m_workspaceSummary->addItem(error);
        return;
    }
    if (catalog.workspaces.isEmpty()) {
        m_workspaceSummary->addItem(i18nc("@item:inlistbox", "No workspaces"));
    }
    QList<std::pair<QString, QString>> names;
    QMap<QString, int> assigned;
    for (const QString &workspaceId : std::as_const(catalog.locationAssignments)) {
        ++assigned[workspaceId];
    }
    for (const auto &workspace : std::as_const(catalog.workspaces)) {
        QString text = i18ncp("@item:inlistbox workspace name, short label and assigned location count",
                              "%2 (%3): %1 assigned location",
                              "%2 (%3): %1 assigned locations",
                              assigned.value(workspace.id),
                              workspace.name,
                              workspace.shortLabel);
        if (workspace.id == catalog.activeId) {
            text = i18nc("@item:inlistbox active workspace", "%1 — active", text);
        }
        auto *item = new QListWidgetItem(text, m_workspaceSummary);
        item->setData(Qt::UserRole, workspace.id);
        names.append({workspace.id, i18nc("@item:inlistbox workspace name and short label", "%1 (%2)", workspace.name, workspace.shortLabel)});
    }
    m_startupWorkspace->setWorkspaces(names);
}

void SettingsDialog::createSettingsBackup()
{
    const QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("opus-settings.tar.gz"));
    QString path = QFileDialog::getSaveFileName(this,
                                                i18nc("@title:window", "Create Settings Backup"),
                                                defaultPath,
                                                i18nc("@item:inlistbox", "Plasma Opus settings backup (*.tar.gz)"));
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
                             i18nc("@info", "The Plasma Opus settings backup was saved to %1.", QDir::toNativeSeparators(path)));
}

void SettingsDialog::restoreSettingsBackup()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      i18nc("@title:window", "Restore Settings Backup"),
                                                      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      i18nc("@item:inlistbox", "Plasma Opus settings backup (*.tar.gz)"));
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
              "This backup contains:\n%1\n\nIt excludes:\n%2\n\nRestoring replaces Plasma Opus settings and window and panel layouts on the next launch. "
              "The current configuration will remain untouched until you restart Plasma Opus. Continue?",
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
    prompt.setText(i18nc("@info", "The backup will be applied when Plasma Opus starts again."));
    auto *quit = prompt.addButton(i18nc("@action:button", "Quit and Apply on Next Launch"), QMessageBox::AcceptRole);
    prompt.addButton(i18nc("@action:button", "Later"), QMessageBox::RejectRole);
    prompt.exec();
    if (prompt.clickedButton() == quit) {
        QCoreApplication::quit();
    }
}

void SettingsDialog::exportWorkspaceProfiles()
{
    const auto config = KSharedConfig::openConfig();
    WorkspaceCatalog::Catalog catalog;
    QString error;
    if (!WorkspaceCatalog::load(config->group(QStringLiteral("Workspaces")), catalog, error)) {
        QMessageBox::critical(this, i18nc("@title:window", "Could Not Read Workspaces"), error);
        return;
    }
    if (catalog.workspaces.isEmpty()) {
        QMessageBox::information(this, i18nc("@title:window", "No Workspaces"), i18nc("@info", "Create a workspace before exporting profiles."));
        return;
    }

    QDialog select(this);
    select.setWindowTitle(i18nc("@title:window", "Select Workspace Profiles"));
    auto *layout = new QVBoxLayout(&select);
    auto *profiles = new QListWidget(&select);
    profiles->setSelectionMode(QAbstractItemView::MultiSelection);
    profiles->setAccessibleName(i18nc("@label", "Workspace profiles to export"));
    for (const auto &workspace : catalog.workspaces) {
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox workspace profile", "%1 (%2)", workspace.name, workspace.shortLabel), profiles);
        item->setData(Qt::UserRole, workspace.id);
    }
    layout->addWidget(profiles);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &select);
    connect(buttons, &QDialogButtonBox::accepted, &select, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &select, &QDialog::reject);
    layout->addWidget(buttons);
    if (select.exec() != QDialog::Accepted || profiles->selectedItems().isEmpty()) {
        return;
    }

    QStringList ids;
    for (const QListWidgetItem *item : profiles->selectedItems()) {
        ids.append(item->data(Qt::UserRole).toString());
    }
    const QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("opus-workspaces.tar.gz"));
    QString path = QFileDialog::getSaveFileName(this,
                                                i18nc("@title:window", "Export Workspace Profiles"),
                                                defaultPath,
                                                i18nc("@item:inlistbox", "Plasma Opus settings backup (*.tar.gz)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QLatin1String(".tar.gz"), Qt::CaseInsensitive)) {
        path.append(QStringLiteral(".tar.gz"));
    }
    error = SettingsBackup::createWorkspaceProfiles(path, config, KSharedConfig::openStateConfig(), ids);
    if (!error.isEmpty()) {
        QMessageBox::critical(this, i18nc("@title:window", "Could Not Export Workspace Profiles"), error);
        return;
    }
    QMessageBox::information(this,
                             i18nc("@title:window", "Workspace Profiles Exported"),
                             i18nc("@info", "The selected workspace profiles were saved to %1.", QDir::toNativeSeparators(path)));
}

void SettingsDialog::importWorkspaceProfiles()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      i18nc("@title:window", "Import Workspace Profiles"),
                                                      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      i18nc("@item:inlistbox", "Plasma Opus settings backup (*.tar.gz)"));
    if (path.isEmpty()) {
        return;
    }
    const auto config = KSharedConfig::openConfig();
    SettingsBackup::WorkspaceProfilesPreview preview;
    QString error = SettingsBackup::inspectWorkspaceProfiles(path, config, &preview);
    if (!error.isEmpty()) {
        QMessageBox::critical(this, i18nc("@title:window", "Invalid Workspace Export"), error);
        return;
    }
    if (preview.profiles.isEmpty()) {
        QMessageBox::information(this, i18nc("@title:window", "No Workspace Profiles"), i18nc("@info", "The backup contains no workspace profiles."));
        return;
    }
    QDialog select(this);
    select.setObjectName(QStringLiteral("workspaceImportSelection"));
    select.setWindowTitle(i18nc("@title:window", "Preview Workspace Import"));
    auto *layout = new QVBoxLayout(&select);
    auto *intro = new QLabel(i18nc("@info",
                                   "Choose the workspace profiles to import. Profiles marked as conflicts share a name or identity with an "
                                   "existing workspace. Replacing keeps the current workspace identity when possible; keeping existing "
                                   "workspaces skips conflicting profiles. Import changes workspace settings only; it does not run commands "
                                   "or modify managed files."),
                             &select);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *profiles = new QListWidget(&select);
    profiles->setObjectName(QStringLiteral("workspaceImportProfiles"));
    profiles->setAccessibleName(i18nc("@label", "Workspace profiles to import"));
    for (int i = 0; i < preview.profiles.size(); ++i) {
        const bool conflict = preview.conflictIds.contains(preview.ids.at(i));
        auto *item =
            new QListWidgetItem(conflict ? i18nc("@item:inlistbox workspace profile", "%1 (conflict)", preview.profiles.at(i)) : preview.profiles.at(i),
                                profiles);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, preview.ids.at(i));
    }
    layout->addWidget(profiles);
    auto *buttons = new QDialogButtonBox(&select);
    auto *replace = buttons->addButton(i18nc("@action:button", "Replace Conflicts"), QDialogButtonBox::AcceptRole);
    auto *keep = buttons->addButton(i18nc("@action:button", "Keep Existing"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    replace->setObjectName(QStringLiteral("workspaceImportReplace"));
    keep->setObjectName(QStringLiteral("workspaceImportKeep"));
    QAbstractButton *chosen = nullptr;
    connect(buttons, &QDialogButtonBox::clicked, &select, [&chosen, &select, buttons](QAbstractButton *button) {
        if (buttons->buttonRole(button) == QDialogButtonBox::AcceptRole) {
            chosen = button;
            select.accept();
        } else {
            select.reject();
        }
    });
    const auto updateButtons = [profiles, replace, keep] {
        bool any = false;
        for (int i = 0; i < profiles->count(); ++i) {
            any = any || profiles->item(i)->checkState() == Qt::Checked;
        }
        replace->setEnabled(any);
        keep->setEnabled(any);
    };
    connect(profiles, &QListWidget::itemChanged, &select, updateButtons);
    layout->addWidget(buttons);
    if (select.exec() != QDialog::Accepted || !chosen) {
        return;
    }
    QStringList ids;
    for (int i = 0; i < profiles->count(); ++i) {
        if (profiles->item(i)->checkState() == Qt::Checked) {
            ids.append(profiles->item(i)->data(Qt::UserRole).toString());
        }
    }
    QStringList imported;
    error = SettingsBackup::importWorkspaceProfiles(path, config, chosen == replace, &imported, &ids);
    if (!error.isEmpty()) {
        QMessageBox::critical(this, i18nc("@title:window", "Could Not Import Workspace Profiles"), error);
        return;
    }
    QMessageBox::information(this,
                             i18nc("@title:window", "Workspace Profiles Imported"),
                             imported.isEmpty() ? i18nc("@info", "No profiles were imported because existing profiles were kept.")
                                                : i18nc("@info", "Imported profiles:\n%1", imported.join(QLatin1Char('\n'))));
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
    auto *defaultsLabel = new QLabel(i18nc("@info",
                                           "The toolbar search field and the search panel start with these criteria. Criteria changed in an "
                                           "open search panel are kept when these defaults change."),
                                     page);
    defaultsLabel->setWordWrap(true);
    layout->addWidget(defaultsLabel);
    auto *defaults = new QFormLayout;
    defaults->addRow(i18nc("@label:listbox", "Default scope:"),
                     comboBox(QStringLiteral("DefaultSearchScope"),
                              {i18nc("@item:inlistbox", "Current folder (live)"),
                               i18nc("@item:inlistbox", "Indexed locations (Baloo)"),
                               i18nc("@item:inlistbox", "Search folders (live)")}));
    defaults->addRow(i18nc("@label:listbox", "Default search in:"),
                     comboBox(QStringLiteral("DefaultSearchField"),
                              {i18nc("@item:inlistbox", "File name"), i18nc("@item:inlistbox", "File contents"), i18nc("@item:inlistbox", "Advanced query")}));
    auto *defaultFilter = bound(new SavedPathFilterCombo(page), QStringLiteral("DefaultSearchPathFilter"), "filterName");
    defaults->addRow(i18nc("@label:listbox", "Default path filter:"), defaultFilter);
    layout->addLayout(defaults);
    auto *panel = new QFormLayout;
    panel->addRow(i18nc("@label:listbox", "Search panel placement:"),
                  comboBox(QStringLiteral("SearchPanelPlacement"),
                           {i18nc("@item:inlistbox", "With the embedded terminal"),
                            i18nc("@item:inlistbox", "Below the panes"),
                            i18nc("@item:inlistbox", "Right of the panes")}));
    panel->addRow(checkBox(QStringLiteral("RestoreSearchPanel"), i18nc("@option:check", "Show the search panel at startup if it was open when last used")));
    panel->addRow(
        i18nc("@label:listbox", "Default result presentation:"),
        comboBox(QStringLiteral("DefaultSearchResultPresentation"), {i18nc("@item:inlistbox", "Full paths"), i18nc("@item:inlistbox", "Names and folders")}));
    auto *historyRow = new QHBoxLayout;
    historyRow->addWidget(checkBox(QStringLiteral("RetainSearchHistory"), i18nc("@option:check", "Remember recent search queries")));
    historyRow->addStretch(1);
    auto *clearHistory = new QPushButton(i18nc("@action:button", "Clear Search History"), page);
    clearHistory->setObjectName(QStringLiteral("clearSearchHistory"));
    clearHistory->setToolTip(i18nc("@info:tooltip", "Forget recent search queries now, in every Plasma Opus window"));
    historyRow->addWidget(clearHistory);
    panel->addRow(historyRow);
    const auto history = [] {
        return KSharedConfig::openStateConfig()->group(QStringLiteral("SearchHistory"));
    };
    clearHistory->setEnabled(!SearchHistory::load(history()).isEmpty());
    connect(clearHistory, &QPushButton::clicked, this, [clearHistory, history] {
        KConfigGroup group = history();
        SearchHistory::save(group, {});
        group.sync();
        clearHistory->setEnabled(false);
    });
    layout->addLayout(panel);
    auto *toolbar = new QFormLayout;
    auto *mainWindow = qobject_cast<KXmlGuiWindow *>(parentWidget());
    QPointer<QAction> fieldAction = mainWindow ? mainWindow->actionCollection()->action(QStringLiteral("search_toolbar")) : nullptr;
    const QPointer<KXmlGuiWindow> window(mainWindow);
    QCheckBox *showField = addSharedToggle(
        QStringLiteral("searchToolbarField"),
        i18nc("@option:check", "Show the search field on the main toolbar"),
        [window, fieldAction]() -> bool {
            return window && fieldAction && window->toolBar()->actions().contains(fieldAction.data());
        },
        [window, fieldAction](bool shown) {
            if (!window || !fieldAction) {
                return;
            }
            QToolBar *bar = window->toolBar();
            const QList<QAction *> entries = bar->actions();
            if (entries.contains(fieldAction.data()) == shown) {
                return;
            }
            if (!shown) {
                bar->removeAction(fieldAction);
                return;
            }
            // Place the field next to the Search button, or before the hamburger menu.
            QAction *before = nullptr;
            for (const QString &name : {QStringLiteral("search_folder"), QStringLiteral("hamburger_menu")}) {
                QAction *candidate = window->actionCollection()->action(name);
                if (candidate && entries.contains(candidate)) {
                    before = candidate;
                    break;
                }
            }
            bar->insertAction(before, fieldAction);
        },
        false);
    showField->setEnabled(!fieldAction.isNull());
    showField->setToolTip(i18nc("@info:tooltip", "Adds or removes the Search Field entry, like the main toolbar editor on the Toolbars page"));
    toolbar->addRow(i18nc("@title:group", "Toolbar search field:"), showField);
    toolbar->addRow(QString(),
                    checkBox(QStringLiteral("ToolbarSearchOpensPanel"), i18nc("@option:check", "Show the search panel when a search starts from the field")));
    layout->addLayout(toolbar);
    auto *foldersLabel = new QLabel(i18nc("@label", "Search folders, searched together by the \"Search folders\" scope:"), page);
    foldersLabel->setWordWrap(true);
    layout->addWidget(foldersLabel);
    auto *folders = bound(new FolderListEditor(i18nc("@title:window", "Choose a Search Folder"), page), QStringLiteral("SearchFolders"), "folders");
    foldersLabel->setBuddy(folders);
    layout->addWidget(folders);
    auto *exclusionsLabel = new QLabel(i18nc("@label", "Excluded folders, skipped by every Plasma Opus search:"), page);
    exclusionsLabel->setWordWrap(true);
    layout->addWidget(exclusionsLabel);
    auto *exclusions = bound(new FolderListEditor(i18nc("@title:window", "Choose a Folder to Exclude"), page), QStringLiteral("SearchExclusions"), "folders");
    exclusionsLabel->setBuddy(exclusions);
    layout->addWidget(exclusions);
    layout->addWidget(checkBox(QStringLiteral("DoubleShiftSearch"), i18nc("@option:check", "Open search by pressing Shift twice")));
    auto *doubleShiftStatus = new QLabel(page);
    doubleShiftStatus->setObjectName(QStringLiteral("doubleShiftStatus"));
    doubleShiftStatus->setWordWrap(true);
    doubleShiftStatus->setTextFormat(Qt::PlainText);
    const QString unavailable = DoubleShiftDetector::unavailableReason();
    doubleShiftStatus->setText(unavailable.isEmpty() ? i18nc("@info",
                                                             "Press Shift twice quickly in a Plasma Opus window to search the active folder. Change the "
                                                             "Search shortcut under Configure Keyboard Shortcuts.")
                                                     : i18nc("@info", "Pressing Shift twice is unavailable: %1", unavailable));
    layout->addWidget(doubleShiftStatus);
    auto *manageFilters = new QPushButton(i18nc("@action:button", "Manage Shared Path Filters…"), page);
    manageFilters->setObjectName(QStringLiteral("managePathFilters"));
    layout->addWidget(manageFilters);
    connect(manageFilters, &QPushButton::clicked, this, [this, defaultFilter] {
        PathFilterPresetsDialog dialog(this);
        dialog.exec();
        defaultFilter->reload();
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
    auto *placement = new QFormLayout;
    placement->addRow(
        i18nc("@label:listbox", "Open metadata editor in:"),
        comboBox(QStringLiteral("MetadataPlacement"),
                 {i18nc("@item:inlistbox", "Separate window"), i18nc("@item:inlistbox", "Below panes"), i18nc("@item:inlistbox", "Right of panes")}));
    layout->addLayout(placement);
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
                                         "Plasma Opus font size and word wrap settings take precedence in Plasma Opus editor views."),
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

QWidget *SettingsDialog::createColorSchemePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Colour scheme:"), bound(m_colorScheme = new ColorSchemeCombo, QStringLiteral("ColorScheme"), "scheme"));
    m_accentColor = bound(new AccentColorChooser, QStringLiteral("AccentColor"), "accent");
    m_accentColor->setScheme(m_colorScheme->scheme());
    connect(m_colorScheme, &ColorSchemeCombo::schemeChanged, m_accentColor, [this] {
        m_accentColor->setScheme(m_colorScheme->scheme());
    });
    form->addRow(i18nc("@label", "Accent colour:"), m_accentColor);
    auto *tint = comboBox(QStringLiteral("PaneTint"),
                          {i18nc("@item:inlistbox pane tint", "No tint"),
                           i18nc("@item:inlistbox pane tint", "All panes"),
                           i18nc("@item:inlistbox pane tint", "Active pane"),
                           i18nc("@item:inlistbox pane tint", "Inactive pane")});
    auto *tintColor = new KColorButton;
    tintColor->setObjectName(QStringLiteral("kcfg_PaneTintColor"));
    tintColor->setAccessibleName(i18nc("@label", "Tint colour"));
    auto *tintStrength = new QSpinBox;
    tintStrength->setObjectName(QStringLiteral("kcfg_PaneTintStrength"));
    tintStrength->setRange(1, 50);
    tintStrength->setSuffix(i18nc("@item:valuesuffix percent", "%"));
    tintStrength->setToolTip(i18nc("@info:tooltip", "The tint is weakened when needed to keep text readable."));
    const auto updateTint = [tint, tintColor, tintStrength] {
        tintColor->setEnabled(tint->currentIndex() > 0);
        tintStrength->setEnabled(tint->currentIndex() > 0);
    };
    connect(tint, &QComboBox::currentIndexChanged, page, updateTint);
    updateTint();
    form->addRow(i18nc("@label:listbox", "Tint pane background:"), tint);
    form->addRow(i18nc("@label", "Tint colour:"), tintColor);
    form->addRow(i18nc("@label:spinbox", "Tint strength:"), tintStrength);
    auto *tintNotice = createContrastNotice(QStringLiteral("paneTintContrastNotice"), true, page);
    form->addRow(QString(), tintNotice);
    const auto updateTintNotice = [this, tint, tintColor, tintStrength, tintNotice] {
        const int requested = tintStrength->value();
        const int effective = tint->currentIndex() > 0
            ? ColorSchemes::paneTintStrength(ColorSchemes::palette(m_colorScheme->scheme()), tintColor->color(), requested)
            : requested;
        if (effective >= requested) {
            tintNotice->hide();
            return;
        }
        tintNotice->setText(effective > 0 ? i18nc("@info",
                                                  "With the selected colour scheme, this tint is weakened to %1% so that text in file lists "
                                                  "stays readable.",
                                                  effective)
                                          : i18nc("@info",
                                                  "With the selected colour scheme, this tint is not shown, because any amount of it would "
                                                  "make text in file lists hard to read."));
        tintNotice->show();
    };
    connect(tint, &QComboBox::currentIndexChanged, page, updateTintNotice);
    connect(tintColor, &KColorButton::changed, page, updateTintNotice);
    connect(tintStrength, &QSpinBox::valueChanged, page, updateTintNotice);
    connect(m_colorScheme, &ColorSchemeCombo::schemeChanged, page, updateTintNotice);
    updateTintNotice();
    layout->addLayout(form);
    auto *notice = new QLabel(i18nc("@info",
                                    "The colour scheme applies only to Plasma Opus. Plasma's global colour scheme, widget style and window "
                                    "decoration theme are not changed. Follow the Plasma colour scheme to use the desktop's colours again."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createCustomColorsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *notice = new QLabel(i18nc("@info",
                                    "Override palette roles of the selected colour scheme. Unchecked roles use the scheme's colour. The preview "
                                    "updates as you edit; Revert or Cancel discards changes that have not been applied."),
                              page);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *editor = bound(new ColorPaletteEditor, QStringLiteral("CustomColors"), "colors");
    editor->setScheme(m_colorScheme->scheme());
    editor->setAccent(m_accentColor->accent());
    connect(m_colorScheme, &ColorSchemeCombo::schemeChanged, editor, [this, editor] {
        editor->setScheme(m_colorScheme->scheme());
    });
    connect(m_accentColor, &AccentColorChooser::accentChanged, editor, [this, editor] {
        editor->setAccent(m_accentColor->accent());
    });
    layout->addWidget(editor, 1);
    return page;
}

QWidget *SettingsDialog::createEffectsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    auto *blur = checkBox(QStringLiteral("BackgroundBlur"), i18nc("@option:check", "Blur the desktop behind translucent backgrounds"));
    blur->setToolTip(i18nc("@info:tooltip", "Uses KWin's blur effect; its strength follows the Blur settings in Desktop Effects."));
    form->addRow(i18nc("@title:group", "Background:"), blur);
    const auto opacity = [](const QString &entry) {
        auto *spin = new QSpinBox;
        spin->setObjectName(QStringLiteral("kcfg_") + entry);
        spin->setRange(20, 100);
        spin->setSuffix(i18nc("@item:valuesuffix percent", "%"));
        return spin;
    };
    auto *windowOpacity = opacity(QStringLiteral("WindowOpacity"));
    auto *paneOpacity = opacity(QStringLiteral("PaneOpacity"));
    auto *panelOpacity = opacity(QStringLiteral("PanelOpacity"));
    form->addRow(i18nc("@label:spinbox", "Window opacity:"), windowOpacity);
    form->addRow(i18nc("@label:spinbox", "Pane opacity:"), paneOpacity);
    form->addRow(i18nc("@label:spinbox", "Panel opacity:"), panelOpacity);
    auto *corners = comboBox(QStringLiteral("CornerRadius"),
                             {i18nc("@item:inlistbox corner radius", "Follow widget style"),
                              i18nc("@item:inlistbox corner radius", "Square"),
                              i18nc("@item:inlistbox corner radius", "Small"),
                              i18nc("@item:inlistbox corner radius", "Medium"),
                              i18nc("@item:inlistbox corner radius", "Large"),
                              i18nc("@item:inlistbox corner radius", "Custom")});
    corners->setToolTip(i18nc("@info:tooltip",
                              "Rounds item highlights, cards, input fields, tabs and tooltips. Buttons, combo boxes and panels keep the "
                              "widget style's corners."));
    m_cornerRadius = corners;
    auto *customCorners = new QSpinBox;
    customCorners->setObjectName(QStringLiteral("kcfg_CustomCornerRadius"));
    customCorners->setRange(0, 24);
    customCorners->setSuffix(i18nc("@item:valuesuffix logical pixels", " px"));
    customCorners->setAccessibleName(i18nc("@label:spinbox", "Custom corner radius"));
    const auto updateCorners = [corners, customCorners] {
        customCorners->setEnabled(corners->currentIndex() == Settings::EnumCornerRadius::Custom);
    };
    connect(corners, &QComboBox::currentIndexChanged, page, updateCorners);
    updateCorners();
    auto *cornerRow = new QHBoxLayout;
    cornerRow->addWidget(corners);
    cornerRow->addWidget(customCorners);
    cornerRow->addStretch();
    form->addRow(i18nc("@label:listbox", "Corner radius:"), cornerRow);
    layout->addLayout(form);
    auto *explanation = new QLabel(i18nc("@info",
                                         "Only backgrounds become translucent; text, icons and selections stay opaque. Panes and panels are "
                                         "drawn over the window background, so they are never more transparent than the window."),
                                   page);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    auto *notice = new KMessageWidget(page);
    notice->setObjectName(QStringLiteral("windowEffectsNotice"));
    notice->setMessageType(KMessageWidget::Information);
    notice->setCloseButtonVisible(false);
    notice->setWordWrap(true);
    notice->hide();
    layout->addWidget(notice);
    const auto updateNotice = [this, blur, windowOpacity, paneOpacity, panelOpacity, notice] {
        const bool translucency = windowOpacity->value() < 100 || paneOpacity->value() < 100 || panelOpacity->value() < 100;
        const QString reason = WindowEffects::unavailableReason(parentWidget(), blur->isChecked(), translucency);
        notice->setText(reason);
        notice->setVisible(!reason.isEmpty());
    };
    connect(blur, &QCheckBox::toggled, page, updateNotice);
    for (QSpinBox *spin : {windowOpacity, paneOpacity, panelOpacity}) {
        connect(spin, &QSpinBox::valueChanged, page, updateNotice);
    }
    updateNotice();
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createLinesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    auto *dividers = comboBox(
        QStringLiteral("Dividers"),
        {i18nc("@item:inlistbox dividers", "Follow widget style"), i18nc("@item:inlistbox dividers", "Thin"), i18nc("@item:inlistbox dividers", "Hidden")});
    dividers->setToolTip(
        i18nc("@info:tooltip", "Applies to splitter handles, panel and toolbar separators, header and grid lines and the frames of panes and panels."));
    form->addRow(i18nc("@label:listbox", "Dividers:"), dividers);
    auto *reveal = checkBox(QStringLiteral("RevealDividersOnHover"), i18nc("@option:check", "Show hidden dividers under the pointer"));
    form->addRow(QString(), reveal);
    const auto updateReveal = [dividers, reveal] {
        reveal->setEnabled(dividers->currentIndex() == Settings::EnumDividers::Hidden);
    };
    connect(dividers, &QComboBox::currentIndexChanged, page, updateReveal);
    updateReveal();
    auto *treeLines = comboBox(QStringLiteral("TreeLines"),
                               {i18nc("@item:inlistbox tree lines", "Follow widget style"),
                                i18nc("@item:inlistbox tree lines", "Hidden"),
                                i18nc("@item:inlistbox tree lines", "Solid"),
                                i18nc("@item:inlistbox tree lines", "Dotted")});
    form->addRow(i18nc("@label:listbox", "Tree lines:"), treeLines);
    auto *lineColors = comboBox(QStringLiteral("TreeLineColors"),
                                {i18nc("@item:inlistbox tree line colours", "Neutral"),
                                 i18nc("@item:inlistbox tree line colours", "Single colour"),
                                 i18nc("@item:inlistbox tree line colours", "By nesting depth"),
                                 i18nc("@item:inlistbox tree line colours", "By folder colour")});
    lineColors->setToolTip(i18nc("@info:tooltip", "Applies to solid and dotted tree lines. Folders without a colour use the neutral line colour."));
    form->addRow(i18nc("@label:listbox", "Tree line colours:"), lineColors);
    auto *lineColor = new KColorButton;
    lineColor->setObjectName(QStringLiteral("kcfg_TreeLineColor"));
    lineColor->setAccessibleName(i18nc("@label", "Tree line colour"));
    form->addRow(i18nc("@label", "Line colour:"), lineColor);
    auto *linePalette = bound(new ColorListEditor, QStringLiteral("TreeLinePalette"), "colors");
    form->addRow(i18nc("@label", "Depth colours:"), linePalette);
    auto *lineNotice = createContrastNotice(QStringLiteral("treeLineContrastNotice"), true, page);
    form->addRow(QString(), lineNotice);
    const auto updateLineColors = [this, treeLines, lineColors, lineColor, linePalette, lineNotice] {
        // Colours apply only to the lines Plasma Opus draws itself.
        const bool drawn = treeLines->currentIndex() == Settings::EnumTreeLines::Solid || treeLines->currentIndex() == Settings::EnumTreeLines::Dotted;
        lineColors->setEnabled(drawn);
        const bool single = drawn && lineColors->currentIndex() == Settings::EnumTreeLineColors::Single;
        lineColor->setEnabled(single);
        linePalette->setEnabled(drawn && lineColors->currentIndex() == Settings::EnumTreeLineColors::Depth);
        const QColor background = ColorSchemes::palette(m_colorScheme->scheme()).color(QPalette::Active, QPalette::Base);
        linePalette->setBackground(background);
        lineNotice->setText(i18nc("@info", "This line colour is darkened or lightened to stay visible against the view background."));
        lineNotice->setVisible(single && ColorSchemes::contrastRatio(lineColor->color(), background) < 3.0);
    };
    connect(treeLines, &QComboBox::currentIndexChanged, page, updateLineColors);
    connect(lineColors, &QComboBox::currentIndexChanged, page, updateLineColors);
    connect(lineColor, &KColorButton::changed, page, updateLineColors);
    connect(m_colorScheme, &ColorSchemeCombo::schemeChanged, page, updateLineColors);
    updateLineColors();
    layout->addLayout(form);
    auto *explanation = new QLabel(i18nc("@info",
                                         "Hidden splitters can still be dragged, change the pointer shape and can be moved with the Move Pane "
                                         "Divider actions. Reset Pane and Panel Sizes restores their default positions."),
                                   page);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    auto *colorNote = new QLabel(i18nc("@info",
                                       "Tree line positions show nesting; colours only supplement them. Colours that are too close to the view or "
                                       "selection background are darkened or lightened to stay visible."),
                                 page);
    colorNote->setWordWrap(true);
    layout->addWidget(colorNote);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createHighlightsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Selection:"),
                 comboBox(QStringLiteral("SelectionStyle"),
                          {i18nc("@item:inlistbox selection style", "Follow widget style"),
                           i18nc("@item:inlistbox selection style", "Filled"),
                           i18nc("@item:inlistbox selection style", "Outlined"),
                           i18nc("@item:inlistbox selection style", "Rounded"),
                           i18nc("@item:inlistbox selection style", "Accent bar")}));
    form->addRow(i18nc("@title:group", "Highlight:"), checkBox(QStringLiteral("HoverHighlight"), i18nc("@option:check", "Items under the pointer")));
    form->addRow(i18nc("@title:group", "Details view:"), checkBox(QStringLiteral("StripeDetailsRows"), i18nc("@option:check", "Alternate row backgrounds")));
    layout->addLayout(form);
    auto *note = new QLabel(i18nc("@info",
                                  "With a selection style other than the widget style's, the current item has a dashed outline, so keyboard "
                                  "focus stays visible with and without a selection. Outlined and accent bar selections keep the normal text "
                                  "colour; views whose text would not be readable on them use a filled selection instead."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createFolderColorsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Show folder colours as:"),
                 comboBox(QStringLiteral("FolderColorStyle"),
                          {i18nc("@item:inlistbox folder colour style", "Coloured icons"),
                           i18nc("@item:inlistbox folder colour style", "Badges on icons"),
                           i18nc("@item:inlistbox folder colour style", "Coloured names"),
                           i18nc("@item:inlistbox folder colour style", "Hidden")}));
    layout->addLayout(form);
    auto *note = new QLabel(i18nc("@info",
                                  "Folder colours are set with Folder Appearance and shown in file lists, the folder tree, Places, tabs and the "
                                  "abbreviated location bar. Coloured icons use the icon theme's coloured folders where it has them and tint "
                                  "other icons. Coloured names are darkened or lightened to stay readable."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *rulesLabel = new QLabel(i18nc("@label", "Rules for folders without their own colour, checked in order:"), page);
    rulesLabel->setWordWrap(true);
    layout->addWidget(rulesLabel);
    auto *rules = bound(new FolderColorRuleEditor(page), QStringLiteral("FolderColorRules"), "rules");
    rulesLabel->setBuddy(rules);
    layout->addWidget(rules, 1);
    auto *rulesNote = new QLabel(i18nc("@info",
                                       "A folder's own colour takes precedence over rules, and the first matching rule wins. Rules only change how "
                                       "folders are shown; they never change the folders or their contents."),
                                 page);
    rulesNote->setWordWrap(true);
    layout->addWidget(rulesNote);
    return page;
}

QWidget *SettingsDialog::createFontsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    const QList<std::tuple<QString, QString, QString>> areas{
        {QStringLiteral("FileListFont"), i18nc("@label", "File lists:"), i18nc("@label", "File list font")},
        {QStringLiteral("FolderTreeFont"), i18nc("@label", "Folder tree:"), i18nc("@label", "Folder tree font")},
        {QStringLiteral("TabFont"), i18nc("@label", "Tabs:"), i18nc("@label", "Tab font")},
        {QStringLiteral("StatusBarFont"), i18nc("@label", "Status bars:"), i18nc("@label", "Status bar font")},
    };
    for (const auto &[entry, label, name] : areas) {
        form->addRow(label, bound(new FontOverrideChooser(name), entry, "fontSetting"));
    }
    layout->addLayout(form);
    auto *note = new QLabel(i18nc("@info",
                                  "Areas without a custom font follow Plasma's General font. Custom fonts are stored in points, so they "
                                  "scale with the display like Plasma's fonts. File lists include their column headers."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createSpacingPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    auto *scale = new QSpinBox;
    scale->setObjectName(QStringLiteral("kcfg_SpacingScale"));
    scale->setRange(50, 200);
    scale->setSingleStep(10);
    scale->setSuffix(i18nc("@item:valuesuffix percent", "%"));
    scale->setToolTip(i18nc("@info:tooltip", "100% follows the spacing of Plasma's widget style."));
    form->addRow(i18nc("@label:spinbox", "Spacing scale:"), scale);
    layout->addLayout(form);
    auto *note = new QLabel(i18nc("@info",
                                  "Scales the padding and spacing of toolbars, tabs, panels and status bars. Bars with a Compact, Very "
                                  "compact or Custom bar density keep that spacing, and file lists keep their item density."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createBarStylePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    auto *shape = comboBox(QStringLiteral("TabShape"),
                           {i18nc("@item:inlistbox tab shape", "Follow widget style"),
                            i18nc("@item:inlistbox tab shape", "Rounded"),
                            i18nc("@item:inlistbox tab shape", "Square"),
                            i18nc("@item:inlistbox tab shape", "Flat")});
    auto *indicator = comboBox(QStringLiteral("TabIndicator"),
                               {i18nc("@item:inlistbox active tab indicator", "Line"),
                                i18nc("@item:inlistbox active tab indicator", "Thick line"),
                                i18nc("@item:inlistbox active tab indicator", "None")});
    // Tabs that follow the widget style are drawn rounded, with the indicator,
    // when the corner radius does not follow the widget style.
    QComboBox *corners = m_cornerRadius;
    const auto updateIndicator = [shape, indicator, corners] {
        indicator->setEnabled(shape->currentIndex() != Settings::EnumTabShape::FollowStyle
                              || (corners && corners->currentIndex() != Settings::EnumCornerRadius::FollowStyle));
    };
    connect(shape, &QComboBox::currentIndexChanged, page, updateIndicator);
    if (corners) {
        connect(corners, &QComboBox::currentIndexChanged, page, updateIndicator);
    }
    updateIndicator();
    form->addRow(i18nc("@label:listbox", "Tab shape:"), shape);
    form->addRow(i18nc("@label:listbox", "Active tab indicator:"), indicator);
    form->addRow(i18nc("@label:listbox", "Tab colours:"),
                 comboBox(QStringLiteral("TabColorStrength"),
                          {i18nc("@item:inlistbox tab colour strength", "Coloured text"),
                           i18nc("@item:inlistbox tab colour strength", "Light tint"),
                           i18nc("@item:inlistbox tab colour strength", "Strong fill")}));
    form->addRow(i18nc("@label:listbox", "Status bars:"),
                 comboBox(QStringLiteral("StatusBarStyle"),
                          {i18nc("@item:inlistbox status bar style", "Follow widget style"),
                           i18nc("@item:inlistbox status bar style", "Separated"),
                           i18nc("@item:inlistbox status bar style", "Shaded")}));
    layout->addLayout(form);
    auto *note = new QLabel(i18nc("@info",
                                  "Rounded tabs use the corner radius from Effects, and tabs that follow the widget style are drawn "
                                  "rounded when that corner radius does not. Flat tabs always keep an indicator on the active tab. "
                                  "Tab colours are chosen from a tab's context menu and shown when folder-based tab colours are enabled "
                                  "in Interface settings; text on tinted and filled tabs is adjusted to stay readable."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createBrandingPresentationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    QCheckBox *enabled =
        checkBox(QStringLiteral("StartupPresentation"), i18nc("@option:check", "Show a brief presentation of the logo and version at startup"));
    QComboBox *variant = comboBox(QStringLiteral("LogoVariant"),
                                  {i18nc("@item:inlistbox logo variant", "Match the colour scheme"),
                                   i18nc("@item:inlistbox logo variant", "Dark text on a light card"),
                                   i18nc("@item:inlistbox logo variant", "Light text on a dark card")});
    form->addRow(i18nc("@title:group", "Startup:"), enabled);
    form->addRow(i18nc("@label:listbox", "Logo:"), variant);
    layout->addLayout(form);

    auto *preview = new PreviewLabel(page);
    preview->setObjectName(QStringLiteral("brandingPreview"));
    preview->setAlignment(Qt::AlignCenter);
    layout->addWidget(preview);

    auto *note = new QLabel(i18nc("@info",
                                  "The presentation appears over the main window after it opens and hides itself after a moment. It never "
                                  "delays or blocks startup: the window can be used while it shows, and any key, click or scroll dismisses it. "
                                  "It fades only when animations are on, and uses the colour scheme instead of the logo choice while Plasma "
                                  "asks for high contrast. The application name, icon and About dialog stay the same."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();

    const auto update = [enabled, variant, preview] {
        variant->setEnabled(enabled->isChecked());
        preview->setEnabled(enabled->isChecked());
        const auto logoVariant = static_cast<Branding::LogoVariant>(std::max(variant->currentIndex(), 0));
        constexpr int PreviewWidth = 320;
        const QPixmap card =
            StartupPresentation::preview(logoVariant, preview->palette(), ColorSchemes::highContrastPreferred(), PreviewWidth, preview->devicePixelRatioF());
        preview->setPixmap(card);
        preview->setAccessibleName(i18nc("@info accessible name", "Presentation preview: %1", variant->currentText()));
    };
    connect(enabled, &QCheckBox::toggled, page, update);
    connect(variant, &QComboBox::currentIndexChanged, page, update);
    preview->refresh = update;
    update();
    return page;
}

QWidget *SettingsDialog::createMotionPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Animations:"),
                 comboBox(QStringLiteral("Animations"),
                          {i18nc("@item:inlistbox animation speed", "Follow Plasma"),
                           i18nc("@item:inlistbox animation speed", "Reduced"),
                           i18nc("@item:inlistbox animation speed", "Off")}));
    form->addRow(i18nc("@title:group", "High contrast:"),
                 checkBox(QStringLiteral("SuspendEffectsInHighContrast"),
                          i18nc("@option:check", "Suspend blur, translucency, tints, custom corners and the stylesheet")));
    layout->addLayout(form);
    auto *note = new QLabel(i18nc("@info",
                                  "Follow Plasma uses the animation speed from System Settings. Reduced halves transitions such as "
                                  "places appearing and disappearing, and moves panels without animation. Off turns these transitions off. "
                                  "Animations drawn by the widget style itself, such as button hover fades, follow Plasma."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *contrastNote =
        new QLabel(i18nc("@info",
                         "When Plasma asks for high contrast, Plasma Opus always follows the Plasma colour scheme. With the "
                         "option above, it also makes backgrounds opaque, removes blur and pane tints, uses the widget style's "
                         "corners, shows tab colours as text only and removes the custom stylesheet, until high contrast is turned off again."),
                   page);
    contrastNote->setWordWrap(true);
    layout->addWidget(contrastNote);
    if (ColorSchemes::highContrastPreferred()) {
        auto *active = new KMessageWidget(i18nc("@info", "Plasma currently asks for high contrast."), page);
        active->setObjectName(QStringLiteral("highContrastActive"));
        active->setMessageType(KMessageWidget::Information);
        active->setCloseButtonVisible(false);
        layout->addWidget(active);
    }
    auto *resetRow = new QHBoxLayout;
    auto *reset = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-reset")), i18nc("@action:button", "Reset Appearance to Plasma Defaults"), page);
    reset->setObjectName(QStringLiteral("resetAppearance"));
    reset->setToolTip(i18nc("@info:tooltip",
                            "Sets every Themes option to follow Plasma. Folder colour rules are kept. Apply or OK saves the change; "
                            "Cancel discards it."));
    connect(reset, &QPushButton::clicked, this, [this] {
        if (m_confirm(i18nc("@info",
                            "Reset every Themes option to follow Plasma? Folder colour rules and saved theme profiles are kept, "
                            "and Cancel in this dialog still discards the change."))) {
            resetAppearanceToDefaults();
        }
    });
    resetRow->addWidget(reset);
    resetRow->addStretch();
    layout->addLayout(resetRow);
    auto *resetNote = new QLabel(i18nc("@info",
                                       "You can also reset the appearance by starting Plasma Opus with --reset-appearance, for example when "
                                       "its window is hard to use. Start it with --no-workspace-appearance to ignore appearance options "
                                       "attached to workspaces for that session."),
                                 page);
    resetNote->setWordWrap(true);
    layout->addWidget(resetNote);
    layout->addStretch();
    return page;
}

void SettingsDialog::resetAppearanceToDefaults()
{
    QVariantMap values;
    const QStringList kept = AppearanceDefaults::keptEntries();
    const KConfigSkeletonItem::List items = Settings::self()->items();
    for (const KConfigSkeletonItem *item : items) {
        if (item->group() == QLatin1String(AppearanceDefaults::Group) && !kept.contains(item->name())) {
            values.insert(item->name(), item->getDefault());
        }
    }
    setAppearanceValues(values);
}

void SettingsDialog::setConfirmation(std::function<bool(const QString &)> confirm)
{
    m_confirm = std::move(confirm);
}

void SettingsDialog::setLibraryConfirmation(std::function<bool(const LibraryConfirmations::Confirmation &)> confirm)
{
    m_confirmLibrary = std::move(confirm);
}

void SettingsDialog::setLibraryFolderChooser(std::function<QString()> choose)
{
    m_chooseLibraryFolder = std::move(choose);
}

QVariantMap SettingsDialog::appearanceValues() const
{
    QVariantMap values;
    for (const QString &name : ThemeProfiles::entries(Settings::self())) {
        const auto *editor = findChild<QWidget *>(QStringLiteral("kcfg_") + name);
        if (!editor) {
            values.insert(name, Settings::self()->findItem(name)->property());
        } else if (const QByteArray property = editor->property("kcfg_property").toByteArray(); !property.isEmpty()) {
            values.insert(name, editor->property(property.constData()));
        } else if (const auto *combo = qobject_cast<const QComboBox *>(editor)) {
            values.insert(name, combo->currentIndex());
        } else if (const QMetaProperty user = editor->metaObject()->userProperty(); user.isValid()) {
            values.insert(name, user.read(editor));
        }
    }
    return values;
}

void SettingsDialog::setAppearanceValues(const QVariantMap &values)
{
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        auto *editor = findChild<QWidget *>(QStringLiteral("kcfg_") + it.key());
        if (!editor) {
            continue;
        }
        if (const QByteArray property = editor->property("kcfg_property").toByteArray(); !property.isEmpty()) {
            editor->setProperty(property.constData(), it.value());
        } else if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            combo->setCurrentIndex(it.value().toInt());
        } else if (const QMetaProperty user = editor->metaObject()->userProperty(); user.isValid()) {
            user.write(editor, it.value());
        }
    }
    updateButtons();
}

QWidget *SettingsDialog::createStyleSheetPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(checkBox(QStringLiteral("UseCustomStyleSheet"), i18nc("@option:check", "Apply a custom Qt stylesheet")));
    auto *note = new QLabel(i18nc("@info",
                                  "Qt Style Sheets are a CSS-like subset for Qt widgets, not full web CSS. The stylesheet applies to Plasma "
                                  "Opus windows and their dialogs after every other Themes option, and is refused if Qt cannot read it. "
                                  "For safety, only images built into Plasma Opus (url(:/…)) can be used, and @import and qproperty- "
                                  "are refused. If a window becomes hard to use, start Plasma Opus with --reset-appearance."),
                            page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addWidget(bound(new StyleSheetEditor(page), QStringLiteral("CustomStyleSheet"), "sheet"), 1);
    return page;
}

QWidget *SettingsDialog::createThemeProfilesPage()
{
    auto *manager = new ThemeProfileManager({[this] {
                                                 return appearanceValues();
                                             },
                                             [this](const QVariantMap &values) {
                                                 setAppearanceValues(values);
                                             },
                                             [this] {
                                                 resetAppearanceToDefaults();
                                             }});
    manager->setObjectName(QStringLiteral("themeProfileManager"));
    return manager;
}

QWidget *SettingsDialog::createViewGeneralPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(
        i18nc("@label:listbox", "View mode in new windows:"),
        comboBox(
            QStringLiteral("DefaultViewMode"),
            {i18nc("@item:inlistbox", "Details"), i18nc("@item:inlistbox", "Icons"), i18nc("@item:inlistbox", "Compact"), i18nc("@item:inlistbox", "Cards")}));
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

QWidget *SettingsDialog::createCardsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:spinbox", "Preview size:"), pixelSpinBox(QStringLiteral("CardPreviewSize"), 48, 256));
    auto *columns = new QSpinBox;
    columns->setObjectName(QStringLiteral("kcfg_CardColumns"));
    columns->setRange(0, 12);
    columns->setSpecialValueText(i18nc("@item:inrange card columns", "Automatic"));
    columns->setToolTip(i18nc("@info:tooltip", "Automatic fits as many cards as the pane is wide."));
    form->addRow(i18nc("@label:spinbox", "Columns:"), columns);
    addDensityRows(form, QStringLiteral("CardsDensity"), QStringLiteral("CardsCustomSpace"));
    form->addRow(i18nc("@label", "Metadata shown:"), bound(new IconInformationList(nullptr, true), QStringLiteral("CardInformation"), "information"));
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

QWidget *SettingsDialog::createYaziActivationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("yaziActivationPage"));
    auto *layout = new QVBoxLayout(page);
    const auto addPane = [page, layout](const QString &title, const QString &side) {
        auto *group = new QGroupBox(title, page);
        auto *groupLayout = new QVBoxLayout(group);
        groupLayout->addWidget(checkBox(QStringLiteral("Yazi%1PaneDefault").arg(side), i18nc("@option:check", "Use Yazi mode by default")));
        groupLayout->addWidget(checkBox(QStringLiteral("Yazi%1PaneRestore").arg(side), i18nc("@option:check", "Restore the mode from the last session")));
        layout->addWidget(group);
    };
    addPane(i18nc("@title:group", "First Pane"), QStringLiteral("Left"));
    addPane(i18nc("@title:group", "Second Pane"), QStringLiteral("Right"));
    auto *notice = new QLabel(i18nc("@info",
                                    "When a window opens, a pane that restores its mode starts in Yazi mode if it was in Yazi mode when the last "
                                    "session was saved. Otherwise, or when Interface › Folders and Tabs › On startup doesn't restore the last "
                                    "session, the pane starts with its default. Switching to a workspace or restoring its layout still applies "
                                    "the workspace's Yazi mode behavior. Open panes keep their mode; use View › Yazi Mode to change it."),
                              page);
    notice->setObjectName(QStringLiteral("yaziActivationNotice"));
    notice->setWordWrap(true);
    layout->addWidget(notice);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createYaziColumnsPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("yaziColumnsPage"));
    auto *layout = new QVBoxLayout(page);
    const auto notice = [page](const QString &name, const QString &text) {
        auto *label = new QLabel(text, page);
        label->setObjectName(name);
        label->setWordWrap(true);
        return label;
    };

    auto *shown = new QGroupBox(i18nc("@title:group", "Shown Columns"), page);
    auto *shownLayout = new QVBoxLayout(shown);
    shownLayout->addWidget(checkBox(QStringLiteral("YaziParentColumnVisible"), i18nc("@option:check", "Show the parent column")));
    shownLayout->addWidget(checkBox(QStringLiteral("YaziPreviewVisible"), i18nc("@option:check", "Show the preview column")));
    shownLayout->addWidget(notice(QStringLiteral("yaziShownColumnsNotice"),
                                  i18nc("@info",
                                        "The current folder's column is always shown. A pane whose parent or preview column was shown or hidden "
                                        "with View › Yazi Parent Column or View › Yazi Preview keeps its own choice.")));
    layout->addWidget(shown);

    auto *widths = new QGroupBox(i18nc("@title:group", "Column Widths"), page);
    auto *widthsLayout = new QVBoxLayout(widths);
    m_yaziColumnRatios = bound(new YaziColumnRatiosEditor, QStringLiteral("YaziColumnRatios"), "ratios");
    widthsLayout->addWidget(m_yaziColumnRatios);
    widthsLayout->addWidget(notice(QStringLiteral("yaziColumnWidthsNotice"),
                                   i18nc("@info",
                                         "Only the proportions of the widths matter. Dragging the edge between two columns in a pane gives that "
                                         "pane its own widths, in pixels, and half a second later makes them the widths shown here. Other open "
                                         "panes use these widths when they are applied, and new windows start with them.")));
    layout->addWidget(widths);

    auto *narrow = new QGroupBox(i18nc("@title:group", "Narrow Panes"), page);
    auto *narrowLayout = new QVBoxLayout(narrow);
    narrowLayout->addWidget(notice(QStringLiteral("yaziNarrowPanesNotice"),
                                   i18nc("@info",
                                         "The columns adapt to each pane's own width, so the other file pane is never resized or hidden. In "
                                         "a pane narrower than about %1 pixels, the preview column is hidden first; below about %2 pixels, "
                                         "the parent column is hidden too, so the current column stays at least %3 pixels wide. Hidden "
                                         "columns return when the pane is widened.",
                                         YaziMode::ThreeColumnWidth,
                                         YaziMode::TwoColumnWidth,
                                         YaziMode::MinimumCurrentWidth)));
    layout->addWidget(narrow);
    layout->addStretch();

    // Dragging a column's edge saves new widths while the dialog may be open; show them unless the widths were changed here.
    m_savedYaziColumnRatios = Settings::yaziColumnRatios();
    connect(Settings::self(), &Settings::configChanged, m_yaziColumnRatios, [this] {
        const QList<int> saved = Settings::yaziColumnRatios();
        if (m_yaziColumnRatios->ratios() == m_savedYaziColumnRatios) {
            m_yaziColumnRatios->setRatios(saved);
        }
        m_savedYaziColumnRatios = saved;
    });
    return page;
}

QWidget *SettingsDialog::createPreviewsPage()
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->addRow(i18nc("@label:listbox", "Preview panel placement:"),
                 comboBox(QStringLiteral("PreviewPlacement"),
                          {i18nc("@item:inlistbox", "Pane"), i18nc("@item:inlistbox", "Below panes"), i18nc("@item:inlistbox", "Right of panes")}));
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
    form->addRow({}, checkBox(QStringLiteral("PanelTitlesVisible"), i18nc("@option:check", "Show navigation panel title bars")));
    auto *embeddedTitles = checkBox(QStringLiteral("EmbeddedPanelTitlesVisible"), i18nc("@option:check", "Show embedded panel titles"));
    embeddedTitles->setToolTip(i18nc("@info:tooltip",
                                     "Show titles for the terminal, preview, editor, metadata editor and information panels. "
                                     "Controls and document information remain available when titles are hidden."));
    form->addRow({}, embeddedTitles);
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

QWidget *SettingsDialog::createTaggingPrivacyPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("taggingPrivacyPage"));
    auto *layout = new QVBoxLayout(page);
    auto *explanation = new QLabel(
        i18nc("@info",
              "A library's metadata holds its tag names, aliases, shorthand, colours and groups; its field definitions and values; the paths of "
              "indexed items relative to the library folder, with their size, modification time and file system identity numbers; and which items "
              "have which tags. The list of library folders is kept in Plasma Opus's configuration, and the copy of a library's metadata made before "
              "each restore is kept in the backup folder. Nothing is sent over the network. Files themselves are only changed when you write "
              "embedded metadata."),
        page);
    explanation->setObjectName(QStringLiteral("taggingPrivacyExplanation"));
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto *form = new QFormLayout;
    QComboBox *storage = comboBox(QStringLiteral("NewLibraryStorage"),
                                  {i18nc("@item:inlistbox library metadata location", "In the library folder"),
                                   i18nc("@item:inlistbox library metadata location", "Privately, in Plasma Opus's data folder")});
    storage->setToolTip(i18nc("@info:tooltip",
                              "Private storage writes nothing to the library folder, which suits shared drives and read-only folders. An existing "
                              "library document is always used where it is."));
    form->addRow(i18nc("@label:listbox", "Store metadata of new libraries:"), storage);

    KUrlRequester *backups = folderRequester(QStringLiteral("LibraryBackupFolder"));
    backups->setPlaceholderText(LibraryArchive::backupDirectory());
    backups->setToolTip(i18nc("@info:tooltip", "Where a copy of a library's metadata is kept before a restore replaces it"));
    form->addRow(i18nc("@label:chooser", "Backup folder:"), backups);

    auto *limit = new QSpinBox;
    limit->setObjectName(QStringLiteral("kcfg_LibraryBackupLimit"));
    limit->setRange(0, 1000);
    limit->setSpecialValueText(i18nc("@item:inrange keep all backups", "All"));
    limit->setSuffix(i18nc("@item:valuesuffix", " backups per library"));
    limit->setToolTip(i18nc("@info:tooltip", "After a restore, the oldest backups of that library beyond this number are deleted"));
    form->addRow(i18nc("@label:spinbox", "Keep at most:"), limit);
    layout->addLayout(form);

    layout->addWidget(
        checkBox(QStringLiteral("AllowEmbeddedMetadataWrites"), i18nc("@option:check", "Allow writing library values into files' embedded metadata")));
    auto *consent = new QLabel(i18nc("@info", "Plasma Opus always previews the changes and asks before changing a file."), page);
    consent->setObjectName(QStringLiteral("taggingPrivacyWriteNote"));
    consent->setWordWrap(true);
    layout->addWidget(consent);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createTaggingLibrariesPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("taggingLibrariesPage"));
    auto *layout = new QVBoxLayout(page);

    auto *libraries = new QGroupBox(i18nc("@title:group", "Libraries"), page);
    auto *librariesLayout = new QVBoxLayout(libraries);
    m_libraries = new QTreeWidget(libraries);
    m_libraries->setObjectName(QStringLiteral("taggingLibraries"));
    m_libraries->setHeaderLabels({i18nc("@title:column", "Folder"), i18nc("@title:column", "Metadata"), i18nc("@title:column", "State")});
    m_libraries->setRootIsDecorated(false);
    m_libraries->setUniformRowHeights(true);
    m_libraries->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraries->setMinimumHeight(140);
    m_libraries->header()->setStretchLastSection(false);
    m_libraries->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_libraries->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_libraries->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    librariesLayout->addWidget(m_libraries);

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add Library…"), libraries);
    add->setObjectName(QStringLiteral("taggingAddLibrary"));
    add->setToolTip(i18nc("@info:tooltip", "Index a local folder as a tag library without moving or changing its files"));
    m_removeLibrary = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove…"), libraries);
    m_removeLibrary->setObjectName(QStringLiteral("taggingRemoveLibrary"));
    m_removeLibrary->setToolTip(i18nc("@info:tooltip", "Stop using the selected folder as a library; its metadata is kept and no files are deleted"));
    m_moveLibraryDocument = new QPushButton(libraries);
    m_moveLibraryDocument->setObjectName(QStringLiteral("taggingMoveLibraryMetadata"));
    buttons->addWidget(add);
    buttons->addWidget(m_removeLibrary);
    buttons->addWidget(m_moveLibraryDocument);
    buttons->addStretch();
    librariesLayout->addLayout(buttons);

    auto *note = new QLabel(i18nc("@info",
                                  "Adding, removing and moving libraries takes effect immediately, even if you cancel Preferences. Removing a library "
                                  "keeps its metadata, and no files are deleted."),
                            libraries);
    note->setObjectName(QStringLiteral("taggingLibrariesNote"));
    note->setWordWrap(true);
    librariesLayout->addWidget(note);
    layout->addWidget(libraries);

    connect(add, &QPushButton::clicked, this, &SettingsDialog::addLibrary);
    connect(m_removeLibrary, &QPushButton::clicked, this, &SettingsDialog::removeLibrary);
    connect(m_moveLibraryDocument, &QPushButton::clicked, this, &SettingsDialog::moveLibraryDocument);
    connect(m_libraries, &QTreeWidget::itemSelectionChanged, this, &SettingsDialog::updateLibraryButtons);

    auto *indexing = new QGroupBox(i18nc("@title:group", "Indexing"), page);
    auto *indexingForm = new QFormLayout(indexing);
    auto *exclusions = bound(new KEditListWidget(indexing), QStringLiteral("LibraryExclusions"), "items");
    exclusions->setButtons(KEditListWidget::Add | KEditListWidget::Remove);
    exclusions->setToolTip(i18nc("@info:tooltip", "Items that library refreshes don't index or look inside"));
    indexingForm->addRow(i18nc("@label", "Exclude:"), exclusions);
    auto *exclusionHelp = new QLabel(i18nc("@info",
                                           "One wildcard pattern per entry, where * matches any characters and ? one character; case matters. A "
                                           "pattern without / matches names at any depth, such as node_modules, *.tmp or .git. A pattern with / "
                                           "matches the path below the library folder, such as Archive/Old or Exports/*. Excluded items that are "
                                           "already indexed keep their tags and are not marked missing."),
                                     indexing);
    exclusionHelp->setObjectName(QStringLiteral("taggingExclusionsHelp"));
    exclusionHelp->setWordWrap(true);
    indexingForm->addRow({}, exclusionHelp);
    indexingForm->addRow({}, checkBox(QStringLiteral("IndexHiddenItems"), i18nc("@option:check", "Index hidden files and folders")));

    auto *limit = new QSpinBox(indexing);
    limit->setObjectName(QStringLiteral("kcfg_LibraryScanLimit"));
    limit->setRange(0, 10000000);
    limit->setSingleStep(1000);
    limit->setSpecialValueText(i18nc("@item:inrange no limit", "No limit"));
    limit->setSuffix(i18nc("@item:valuesuffix", " items"));
    limit->setToolTip(i18nc("@info:tooltip", "A refresh that reaches this number stops without marking any entry missing"));
    indexingForm->addRow(i18nc("@label:spinbox", "Index at most:"), limit);

    auto *depth = new QSpinBox(indexing);
    depth->setObjectName(QStringLiteral("kcfg_LibraryScanDepth"));
    depth->setRange(0, 1000);
    depth->setSpecialValueText(i18nc("@item:inrange no limit", "No limit"));
    depth->setSuffix(i18nc("@item:valuesuffix", " levels"));
    depth->setToolTip(i18nc("@info:tooltip", "How many folder levels below the library folder are indexed; 1 indexes only its direct contents"));
    indexingForm->addRow(i18nc("@label:spinbox", "Folder depth:"), depth);
    layout->addWidget(indexing);

    auto *automatic = new QGroupBox(i18nc("@title:group", "Automatic Refresh"), page);
    auto *automaticForm = new QFormLayout(automatic);
    QComboBox *when = comboBox(QStringLiteral("AutomaticLibraryRefresh"),
                               {i18nc("@item:inlistbox refresh libraries automatically", "Never"),
                                i18nc("@item:inlistbox refresh libraries automatically", "When Plasma Opus starts"),
                                i18nc("@item:inlistbox refresh libraries automatically", "When a folder in the library is opened")});
    when->setToolTip(i18nc("@info:tooltip", "Automatic refreshes run quietly in the background and report changes in the status bar"));
    automaticForm->addRow(i18nc("@label:listbox", "Refresh libraries automatically:"), when);
    auto *interval = new QSpinBox(automatic);
    interval->setObjectName(QStringLiteral("kcfg_LibraryRefreshInterval"));
    interval->setRange(1, 10080);
    interval->setSuffix(i18nc("@item:valuesuffix", " minutes"));
    interval->setToolTip(i18nc("@info:tooltip", "A library refreshed more recently than this, also by a quick restart, is not refreshed again automatically"));
    automaticForm->addRow(i18nc("@label:spinbox", "At most once every:"), interval);
    const auto updateInterval = [when, interval] {
        interval->setEnabled(when->currentIndex() != Settings::EnumAutomaticLibraryRefresh::Never);
    };
    connect(when, &QComboBox::currentIndexChanged, interval, updateInterval);
    updateInterval();
    layout->addWidget(automatic);
    layout->addStretch();

    reloadLibraries();

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("taggingLibrariesScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

QWidget *SettingsDialog::createTaggingTagsPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("taggingTagsPage"));
    auto *layout = new QVBoxLayout(page);

    auto *panel = new QGroupBox(i18nc("@title:group", "Tag Panel"), page);
    auto *panelForm = new QFormLayout(panel);
    QComboBox *placement =
        comboBox(QStringLiteral("TagPanelPlacement"),
                 {i18nc("@item:inlistbox tag panel placement", "On the right"), i18nc("@item:inlistbox tag panel placement", "Below the panes")});
    placement->setToolTip(i18nc("@info:tooltip", "The tag panel's own placement menu and dragging the panel also change this"));
    panelForm->addRow(i18nc("@label:listbox", "Show the tag panel:"), placement);
    layout->addWidget(panel);

    auto *colors = new QGroupBox(i18nc("@title:group", "Colours"), page);
    auto *colorsLayout = new QVBoxLayout(colors);
    QCheckBox *showColors = checkBox(QStringLiteral("ShowTagColors"), i18nc("@option:check", "Show tag and group colours"));
    showColors->setToolTip(i18nc("@info:tooltip", "When unchecked, tags and groups show a plain tag icon; their colours are kept"));
    colorsLayout->addWidget(showColors);
    auto *defaultColor = bound(new TagColorChooser(colors), QStringLiteral("DefaultTagColor"), "color");
    defaultColor->setToolTip(i18nc("@info:tooltip", "The colour of tags created in the tag panel, the tag manager and metadata imports"));
    colorsLayout->addWidget(defaultColor);
    layout->addWidget(colors);

    auto *information = new QGroupBox(i18nc("@title:group", "Information Panel"), page);
    auto *informationLayout = new QVBoxLayout(information);
    QCheckBox *showTags = checkBox(QStringLiteral("ShowTagsInInformationPanel"), i18nc("@option:check", "Show the tags of the selected item"));
    informationLayout->addWidget(showTags);
    QCheckBox *showInherited = checkBox(QStringLiteral("ShowInheritedTags"), i18nc("@option:check", "List tags inherited from parent tags"));
    showInherited->setToolTip(i18nc("@info:tooltip",
                                    "Inherited tags follow the item's own tags, marked as inherited. When unchecked, they are listed in the "
                                    "tooltip of the tags."));
    informationLayout->addWidget(showInherited);
    connect(showTags, &QCheckBox::toggled, showInherited, &QWidget::setEnabled);
    showInherited->setEnabled(showTags->isChecked());
    layout->addWidget(information);

    auto *searching = new QGroupBox(i18nc("@title:group", "Searching"), page);
    auto *searchingLayout = new QVBoxLayout(searching);
    searchingLayout->addWidget(checkBox(QStringLiteral("TagMatchesChildTags"), i18nc("@option:check", "Match child tags in tag: searches")));
    auto *searchingHelp = new QLabel(i18nc("@info",
                                           "When checked, tag:Animals also finds items tagged only with a child tag of Animals, such as Cats. "
                                           "tag=Animals always finds only items tagged Animals. This applies to searches and selection queries."),
                                     searching);
    searchingHelp->setObjectName(QStringLiteral("taggingChildTagsHelp"));
    searchingHelp->setWordWrap(true);
    searchingLayout->addWidget(searchingHelp);
    layout->addWidget(searching);
    layout->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("taggingTagsScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

QWidget *SettingsDialog::createTaggingSearchPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("taggingSearchPage"));
    auto *layout = new QVBoxLayout(page);

    auto *scope = new QGroupBox(i18nc("@title:group", "Scope"), page);
    auto *scopeForm = new QFormLayout(scope);
    QComboBox *where = comboBox(
        QStringLiteral("TagSearchScope"),
        {i18nc("@item:inlistbox where tag searches start", "The current folder"), i18nc("@item:inlistbox where tag searches start", "The whole library")});
    where->setToolTip(i18nc("@info:tooltip", "Where a search of the current folder starts when its query has tag criteria"));
    scopeForm->addRow(i18nc("@label:listbox", "Search tags in:"), where);
    auto *scopeHelp = new QLabel(i18nc("@info",
                                       "With the whole library, a search of the current folder whose query has tag:, tag= or tagged: criteria "
                                       "starts in the folder of the library that contains the current folder, and its status says so. Search "
                                       "folders, workspace searches and Select by Query are not affected."),
                                 scope);
    scopeHelp->setObjectName(QStringLiteral("taggingSearchScopeHelp"));
    scopeHelp->setWordWrap(true);
    scopeForm->addRow({}, scopeHelp);
    layout->addWidget(scope);

    auto *matching = new QGroupBox(i18nc("@title:group", "Matching"), page);
    auto *matchingLayout = new QVBoxLayout(matching);
    QCheckBox *aliases = checkBox(QStringLiteral("TagMatchAliases"), i18nc("@option:check", "Match tag aliases and shorthands"));
    aliases->setToolTip(i18nc("@info:tooltip", "When unchecked, tag:Kitty only finds items with a tag named Kitty, not with a tag that has Kitty as an alias"));
    matchingLayout->addWidget(aliases);
    QCheckBox *outside =
        checkBox(QStringLiteral("UntaggedIncludesOutsideLibraries"), i18nc("@option:check", "Include items outside libraries in tagged:no searches"));
    outside->setToolTip(i18nc("@info:tooltip", "tagged:yes and tag: criteria never match items outside libraries"));
    matchingLayout->addWidget(outside);
    auto *matchingHelp = new QLabel(i18nc("@info",
                                          "Matching applies to searches and Select by Query. Searches already running keep the settings they "
                                          "started with."),
                                    matching);
    matchingHelp->setObjectName(QStringLiteral("taggingSearchMatchingHelp"));
    matchingHelp->setWordWrap(true);
    matchingLayout->addWidget(matchingHelp);
    layout->addWidget(matching);
    layout->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("taggingSearchScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

QWidget *SettingsDialog::createTaggingFieldsPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("taggingFieldsPage"));
    auto *layout = new QVBoxLayout(page);

    auto *libraryFields = new QGroupBox(i18nc("@title:group", "Library Fields"), page);
    auto *libraryLayout = new QVBoxLayout(libraryFields);
    auto *pickerForm = new QFormLayout;
    m_fieldLibrary = new QComboBox(libraryFields);
    m_fieldLibrary->setObjectName(QStringLiteral("taggingFieldLibrary"));
    m_fieldLibrary->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_fieldLibrary->setMinimumContentsLength(30);
    pickerForm->addRow(i18nc("@label:listbox", "Library:"), m_fieldLibrary);
    libraryLayout->addLayout(pickerForm);
    m_noFieldLibraries =
        new QLabel(i18nc("@info", "There are no libraries yet. Add one in the Libraries tab or with Tags › Create Library from Folder."), libraryFields);
    m_noFieldLibraries->setObjectName(QStringLiteral("taggingNoFieldLibraries"));
    m_noFieldLibraries->setWordWrap(true);
    libraryLayout->addWidget(m_noFieldLibraries);
    m_fieldManager = new TagFieldManagerWidget(libraryFields);
    m_fieldManager->setObjectName(QStringLiteral("taggingFieldManager"));
    m_fieldManager->setMinimumHeight(200);
    libraryLayout->addWidget(m_fieldManager, 1);
    auto *note = new QLabel(i18nc("@info",
                                  "Changes to a library's fields are saved to its metadata immediately, even if you cancel Preferences. Deleting a "
                                  "field removes its values from every item; no files are changed."),
                            libraryFields);
    note->setObjectName(QStringLiteral("taggingFieldsNote"));
    note->setWordWrap(true);
    libraryLayout->addWidget(note);
    layout->addWidget(libraryFields, 1);

    connect(m_fieldLibrary, &QComboBox::currentIndexChanged, this, [this] {
        m_fieldManager->setLibrary(m_fieldLibrary->currentData().toString());
    });
    connect(m_fieldManager, &TagFieldManagerWidget::libraryChanged, this, &SettingsDialog::libraryChanged);
    connect(m_fieldManager, &TagFieldManagerWidget::libraryChanged, this, &SettingsDialog::reloadEmbeddedMetadataFields);

    auto *newLibraries = new QGroupBox(i18nc("@title:group", "Fields for New Libraries"), page);
    auto *newLayout = new QVBoxLayout(newLibraries);
    auto *templates = bound(new FieldTemplateEditor(newLibraries), QStringLiteral("NewLibraryFields"), "templates");
    m_fieldTemplates = templates;
    connect(templates, &FieldTemplateEditor::templatesChanged, this, &SettingsDialog::reloadEmbeddedMetadataFields);
    templates->setMinimumHeight(140);
    newLayout->addWidget(templates);
    auto *templatesHelp = new QLabel(i18nc("@info",
                                           "A library created from now on starts with these fields, in this order. Existing libraries, and "
                                           "library metadata that is adopted when a folder is added again, aren't changed."),
                                     newLibraries);
    templatesHelp->setObjectName(QStringLiteral("taggingFieldTemplatesHelp"));
    templatesHelp->setWordWrap(true);
    newLayout->addWidget(templatesHelp);
    layout->addWidget(newLibraries);

    reloadFieldLibraries();

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("taggingFieldsScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

QWidget *SettingsDialog::createTaggingEmbeddedMetadataPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("taggingEmbeddedMetadataPage"));
    auto *layout = new QVBoxLayout(page);

    auto *read = new QGroupBox(i18nc("@title:group", "Properties to Read"), page);
    auto *readLayout = new QVBoxLayout(read);
    auto *keys = bound(new EmbeddedMetadataKeyList(read), QStringLiteral("DisabledEmbeddedMetadataKeys"), "disabledKeys");
    readLayout->addWidget(keys);
    auto *readHelp = new QLabel(i18nc("@info",
                                      "EXIF properties are read from images and audio tags such as ID3 from audio files. Unchecked properties "
                                      "aren't read or shown: metadata: search criteria and Select by Query ignore them, the preview panel and "
                                      "gallery don't show them, and importing doesn't offer them. Files are never changed."),
                                read);
    readHelp->setObjectName(QStringLiteral("taggingEmbeddedMetadataReadHelp"));
    readHelp->setWordWrap(true);
    readLayout->addWidget(readHelp);
    layout->addWidget(read);

    auto *import = new QGroupBox(i18nc("@title:group", "Importing into Libraries"), page);
    auto *importLayout = new QVBoxLayout(import);
    m_metadataMappings = bound(new EmbeddedMetadataMappingTable(import), QStringLiteral("EmbeddedMetadataMappings"), "mappings");
    importLayout->addWidget(m_metadataMappings);
    auto *mappingHelp = new QLabel(i18nc("@info",
                                         "Tags › Import Embedded Metadata starts with these choices and saves the ones you import with. A "
                                         "field is found by name in each library, ignoring case; libraries without a suitable field of that "
                                         "name skip the property. Date fields only take dates. The fields offered are those of your libraries "
                                         "and of new libraries."),
                                   import);
    mappingHelp->setObjectName(QStringLiteral("taggingEmbeddedMetadataMappingHelp"));
    mappingHelp->setWordWrap(true);
    importLayout->addWidget(mappingHelp);
    auto *importForm = new QFormLayout;
    QComboBox *conflicts = comboBox(QStringLiteral("EmbeddedMetadataConflicts"),
                                    {i18nc("@item:inlistbox embedded metadata conflicts", "Keep existing values"),
                                     i18nc("@item:inlistbox embedded metadata conflicts", "Replace existing values")});
    conflicts->setToolTip(i18nc("@info:tooltip", "What happens when a field already has a different value; the import dialog can change it each time"));
    importForm->addRow(i18nc("@label:listbox", "Different field values:"), conflicts);
    importLayout->addLayout(importForm);
    QCheckBox *onRefresh =
        checkBox(QStringLiteral("ImportEmbeddedMetadataOnRefresh"), i18nc("@option:check", "Import the metadata of new items when a library is refreshed"));
    onRefresh->setToolTip(i18nc("@info:tooltip", "Manual and automatic refreshes import the files they add, without a preview"));
    importLayout->addWidget(onRefresh);
    auto *refreshHint = new QLabel(i18nc("@info", "Choose how at least one property is imported to import on refresh."), import);
    refreshHint->setObjectName(QStringLiteral("taggingEmbeddedMetadataRefreshHint"));
    refreshHint->setWordWrap(true);
    importLayout->addWidget(refreshHint);
    layout->addWidget(import);
    layout->addStretch();

    const auto update = [keys, mappings = m_metadataMappings, onRefresh, refreshHint] {
        mappings->setVisibleKeys(keys->enabledKeys());
        const bool mapped = mappings->hasVisibleMappings();
        onRefresh->setEnabled(mapped);
        refreshHint->setHidden(mapped);
    };
    connect(keys, &EmbeddedMetadataKeyList::disabledKeysChanged, this, update);
    connect(m_metadataMappings, &EmbeddedMetadataMappingTable::mappingsChanged, this, update);
    update();
    reloadEmbeddedMetadataFields();

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("taggingEmbeddedMetadataScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}

void SettingsDialog::reloadEmbeddedMetadataFields()
{
    if (!m_metadataMappings) {
        return;
    }
    QList<TagLibrary::Field> fields;
    for (const QString &root : LibraryRegistry::roots()) {
        const TagLibrary::Library library = TagLibrary::Library::load(root);
        fields.append(library.fields());
    }
    if (m_fieldTemplates) {
        fields.append(TagLibrary::parseFieldTemplates(m_fieldTemplates->templates()));
    }
    m_metadataMappings->setFields(fields);
}

void SettingsDialog::reloadFieldLibraries()
{
    if (!m_fieldLibrary) {
        return;
    }
    const QString current = m_fieldLibrary->currentData().toString();
    const QStringList roots = LibraryRegistry::roots();
    {
        const QSignalBlocker blocker(m_fieldLibrary);
        m_fieldLibrary->clear();
        for (const QString &root : roots) {
            m_fieldLibrary->addItem(QIcon::fromTheme(QStringLiteral("folder")), root, root);
            m_fieldLibrary->setItemData(m_fieldLibrary->count() - 1, root, Qt::ToolTipRole);
        }
        m_fieldLibrary->setCurrentIndex(std::max(0, int(m_fieldLibrary->findData(current))));
    }
    m_fieldLibrary->setEnabled(!roots.isEmpty());
    m_noFieldLibraries->setVisible(roots.isEmpty());
    m_fieldManager->setVisible(!roots.isEmpty());
    m_fieldManager->setLibrary(m_fieldLibrary->currentData().toString());
}

void SettingsDialog::reloadLibraries(const QString &select)
{
    if (!m_libraries) {
        return;
    }
    const QString selected = select.isEmpty() ? selectedLibrary() : select;
    m_libraries->clear();
    QTreeWidgetItem *current = nullptr;
    for (const QString &root : LibraryRegistry::roots()) {
        bool readOnly = false;
        QString state = TagLibrary::checkLocation(root, &readOnly);
        if (state.isEmpty()) {
            QString error;
            const TagLibrary::Library library = TagLibrary::Library::load(root, &error);
            if (library.isNull()) {
                state = error;
            } else if (readOnly || library.isReadOnly()) {
                state = i18nc("@item library state", "Read-only");
            } else {
                state = i18nc("@item library state", "Ready");
            }
        }
        auto *item = new QTreeWidgetItem(
            m_libraries,
            {root,
             TagLibrary::isPrivate(root) ? i18nc("@item library metadata location", "Private") : i18nc("@item library metadata location", "Library folder"),
             state});
        item->setData(0, Qt::UserRole, root);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("folder")));
        item->setToolTip(0, root);
        item->setToolTip(1, TagLibrary::documentPath(root));
        item->setToolTip(2, state);
        if (root == selected) {
            current = item;
        }
    }
    if (current) {
        m_libraries->setCurrentItem(current);
    }
    updateLibraryButtons();
    reloadFieldLibraries();
    reloadEmbeddedMetadataFields();
}

QString SettingsDialog::selectedLibrary() const
{
    const QList<QTreeWidgetItem *> items = m_libraries ? m_libraries->selectedItems() : QList<QTreeWidgetItem *>();
    return items.isEmpty() ? QString() : items.constFirst()->data(0, Qt::UserRole).toString();
}

void SettingsDialog::updateLibraryButtons()
{
    const QString root = selectedLibrary();
    m_removeLibrary->setEnabled(!root.isEmpty());
    const bool makePrivate = root.isEmpty() || !TagLibrary::isPrivate(root);
    m_moveLibraryDocument->setText(makePrivate ? i18nc("@action:button", "Keep Metadata Privately")
                                               : i18nc("@action:button", "Keep Metadata in Library Folder"));
    m_moveLibraryDocument->setIcon(QIcon::fromTheme(makePrivate ? QStringLiteral("security-high") : QStringLiteral("folder")));
    m_moveLibraryDocument->setToolTip(makePrivate ? i18nc("@info:tooltip", "Move the selected library's metadata to Plasma Opus's data folder")
                                                  : i18nc("@info:tooltip", "Move the selected library's metadata into the library folder"));
    m_moveLibraryDocument->setEnabled(!root.isEmpty());
}

void SettingsDialog::addLibrary()
{
    const QString folder = m_chooseLibraryFolder ? m_chooseLibraryFolder() : QString();
    if (folder.isEmpty()) {
        return;
    }
    const QString root = QDir::cleanPath(folder);
    if (LibraryRegistry::contains(root)) {
        reloadLibraries(root);
        return;
    }
    // The Privacy and Fields tabs' choices apply here even before they are saved, as the user sees them.
    TagLibrary::Storage preferred = TaggingSettings::newLibraryStorage();
    if (const auto *storage = findChild<QComboBox *>(QStringLiteral("kcfg_NewLibraryStorage"))) {
        preferred =
            storage->currentIndex() == Settings::EnumNewLibraryStorage::PrivateFolder ? TagLibrary::Storage::PrivateFolder : TagLibrary::Storage::LibraryFolder;
    }
    const TagLibrary::Storage storage = LibraryRegistry::storageFor(root, preferred);
    QList<TagLibrary::Field> fields = TaggingSettings::newLibraryFields();
    if (const auto *templates = findChild<FieldTemplateEditor *>(QStringLiteral("kcfg_NewLibraryFields"))) {
        fields = TagLibrary::parseFieldTemplates(templates->templates());
    }
    const LibraryConfirmations::Confirmation confirmation = LibraryConfirmations::create(root, storage, preferred);
    if (!m_confirmLibrary(confirmation)) {
        return;
    }
    if (const QString error = LibraryRegistry::create(root, storage, fields); !error.isEmpty()) {
        KMessageBox::error(this, error, confirmation.title);
        return;
    }
    reloadLibraries(root);
    Q_EMIT libraryChanged(root);
    Q_EMIT libraryAdded(root);
}

void SettingsDialog::removeLibrary()
{
    const QString root = selectedLibrary();
    if (root.isEmpty() || !m_confirmLibrary(LibraryConfirmations::remove(root))) {
        return;
    }
    LibraryRegistry::remove(root);
    reloadLibraries();
    Q_EMIT libraryChanged(root);
}

void SettingsDialog::moveLibraryDocument()
{
    const QString root = selectedLibrary();
    if (root.isEmpty()) {
        return;
    }
    const TagLibrary::Storage target = TagLibrary::isPrivate(root) ? TagLibrary::Storage::LibraryFolder : TagLibrary::Storage::PrivateFolder;
    const LibraryConfirmations::Confirmation confirmation = LibraryConfirmations::moveDocument(root, target);
    if (!m_confirmLibrary(confirmation)) {
        return;
    }
    if (const QString error = TagLibrary::moveDocument(root, target); !error.isEmpty()) {
        KMessageBox::error(this, error, confirmation.title);
    }
    reloadLibraries(root);
    Q_EMIT libraryChanged(root);
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
        {QStringLiteral("copy_to"), i18nc("@item:inlistbox", "Plasma Opus Copy to Other Pane / Copy To")},
        {QStringLiteral("move_to"), i18nc("@item:inlistbox", "Plasma Opus Move to Other Pane / Move To")},
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
    form->addRow(i18nc("@label", "Show in Plasma Opus:"), bound(localActions, QStringLiteral("HiddenServiceActions"), "hiddenItems"));
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
    // Libraries may have been added or removed from the Tags menu since the dialog was shown.
    reloadLibraries();
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
