// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "metadataeditdialog.h"

#include "archives.h"
#include "archivewriter.h"
#include "batchrenamedialog.h"
#include "checksumdialog.h"
#include "combinedpanel.h"
#include "compressdialog.h"
#include "contentchartdialog.h"
#include "customcommanddialog.h"
#include "density.h"
#include "duplicatefinderdialog.h"
#include "editorpanel.h"
#include "executables.h"
#include "favoritesmodel.h"
#include "filediffdialog.h"
#include "fileoperations.h"
#include "filesplitdialog.h"
#include "foldercomparisondialog.h"
#include "foldermetadata.h"
#include "foldersyncdialog.h"
#include "foldertree.h"
#include "folderview.h"
#include "gallerywindow.h"
#include "informationpanel.h"
#include "labelcolor.h"
#include "linkednavigation.h"
#include "pane.h"
#include "pathfiltereditor.h"
#include "previewpanel.h"
#include "searchdialog.h"
#include "session.h"
#include "settings.h"
#include "settingsdialog.h"
#include "startup.h"
#include "symbolicicon.h"
#include "tabcolors.h"
#include "tabmodel.h"
#include "terminalpanel.h"
#include "toolbarcustomizer.h"
#include "transferqueue.h"
#include "transferqueuedialog.h"
#include "viewsettings.h"

#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KDirModel>
#include <KFileCopyToMenu>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KFileUtils>
#include <KGuiItem>
#include <KHamburgerMenu>
#include <KIO/CopyJob>
#include <KIO/DirectorySizeJob>
#include <KIO/DropJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/FileUndoManager>
#include <KIO/Global>
#include <KIO/ListJob>
#include <KIO/MkdirJob>
#include <KIO/PreviewJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNewFileMenu>
#include <KSelectAction>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToggleAction>
#include <KToolBar>
#include <Solid/Device>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeDatabase>
#include <QPointer>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

// The XMLGUI rc file is compiled into the static library; make sure the
// linker keeps it even though nothing else references the resource.
static void initResources()
{
    Q_INIT_RESOURCE(opusfm);
}

namespace
{
constexpr auto SessionGroup = "Session";
constexpr auto PanelsGroup = "Panels";
constexpr auto FlatGroup = "Flat";
constexpr auto PopularFoldersGroup = "PopularFolders";
constexpr auto SelectionFiltersGroup = "SelectionFilters";
constexpr auto WindowLayoutGroup = "WindowLayout";
constexpr auto SplitterSizesGroup = "SplitterSizes";

QString flatKey(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash).toString();
}

KFileItemList comparisonFiles(Pane *active, Pane *other, bool dual)
{
    KFileItemList items = active->view()->selectedItems();
    if (items.size() == 1 && dual && !other->isHidden()) {
        items += other->view()->selectedItems();
    }
    if (items.size() != 2 || !std::all_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
            return item.url().isLocalFile() && item.isFile();
        })) {
        return {};
    }
    return items;
}

/// Maps a details or icons density setting, whose choices are the same.
template<typename Choices>
Density::Items itemDensity(typename Choices::type density)
{
    switch (density) {
    case Choices::Compact:
        return Density::Items::Compact;
    case Choices::VeryCompact:
        return Density::Items::VeryCompact;
    case Choices::Custom:
        return Density::Items::Custom;
    default:
        return Density::Items::Normal;
    }
}

Density::Bars barDensity(Settings::EnumBarDensity::type density)
{
    switch (density) {
    case Settings::EnumBarDensity::Compact:
        return Density::Bars::Compact;
    case Settings::EnumBarDensity::VeryCompact:
        return Density::Bars::VeryCompact;
    case Settings::EnumBarDensity::Custom:
        return Density::Bars::Custom;
    default:
        return Density::Bars::Normal;
    }
}

/**
 * Sets the margins and spacing of a bar's layout, remembering the style's
 * own on first use so -1 restores them.
 */
void setBarMargin(QWidget *bar, int margin)
{
    QLayout *layout = bar->layout();
    constexpr auto MarginsKey = "opusfmStyleMargins";
    constexpr auto SpacingKey = "opusfmStyleSpacing";
    if (!bar->property(MarginsKey).isValid()) {
        bar->setProperty(MarginsKey, QVariant::fromValue(layout->contentsMargins()));
        bar->setProperty(SpacingKey, layout->spacing());
    }
    if (margin < 0) {
        layout->setContentsMargins(bar->property(MarginsKey).value<QMargins>());
        layout->setSpacing(bar->property(SpacingKey).toInt());
    } else {
        layout->setContentsMargins(margin, margin, margin, margin);
        layout->setSpacing(margin);
    }
}

/// Places model whose icons use SymbolicIcon at small sizes.
class SymbolicPlacesModel : public KFilePlacesModel
{
public:
    using KFilePlacesModel::KFilePlacesModel;

    QVariant data(const QModelIndex &index, int role) const override
    {
        const QVariant value = KFilePlacesModel::data(index, role);
        if (role != Qt::DecorationRole || value.typeId() != QMetaType::QIcon) {
            return value;
        }
        return SymbolicIcon::wrap(value.value<QIcon>(),
                                  KFilePlacesModel::data(index, IconNameRole).toString(),
                                  isDevice(index) ? deviceForIndex(index).emblems() : QStringList());
    }
};

QString stdName(KStandardAction::StandardAction id)
{
    return KStandardAction::name(id);
}

QString labelText(LabelColor color)
{
    switch (color) {
    case LabelColor::Red:
        return i18nc("@item:inmenu label colour", "Red");
    case LabelColor::Orange:
        return i18nc("@item:inmenu label colour", "Orange");
    case LabelColor::Yellow:
        return i18nc("@item:inmenu label colour", "Yellow");
    case LabelColor::Green:
        return i18nc("@item:inmenu label colour", "Green");
    case LabelColor::Blue:
        return i18nc("@item:inmenu label colour", "Blue");
    case LabelColor::Purple:
        return i18nc("@item:inmenu label colour", "Purple");
    case LabelColor::Pink:
        return i18nc("@item:inmenu label colour", "Pink");
    case LabelColor::Gray:
        return i18nc("@item:inmenu label colour", "Gray");
    case LabelColor::None:
        return i18nc("@item:inmenu label colour", "None");
    }
    return {};
}

QList<QUrl> urlsOf(const KFileItemList &items)
{
    QList<QUrl> urls;
    urls.reserve(items.size());
    for (const KFileItem &item : items) {
        urls.append(item.url());
    }
    return urls;
}

QString previewMimeType(const KFileItem &item, bool determine = false)
{
    if (determine && item.url().isLocalFile()) {
        return item.mimetype();
    }
    return item.isMimeTypeKnown() ? item.currentMimeType().name() : QMimeDatabase().mimeTypeForFile(item.name(), QMimeDatabase::MatchExtension).name();
}
}

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_placesModel(new SymbolicPlacesModel(this))
{
    initResources();
    KConfigGroup legacyView = KSharedConfig::openConfig()->group(QStringLiteral("View"));
    KConfigGroup views = KSharedConfig::openConfig()->group(QStringLiteral("Views"));
    if (ViewSettings::migrateColumns(legacyView, views)) {
        Settings::self()->load();
    }
    setComponentName(QStringLiteral("opus-file-manager"), i18n("Opus File Manager"));
    m_flatLocations = KSharedConfig::openStateConfig()->group(QLatin1String(FlatGroup)).readEntry("Locations", QStringList());
    m_favorites = new FavoritesModel(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")), this);
    m_tabColors = new TabColors(KSharedConfig::openConfig()->group(QStringLiteral("TabColors")), this);
    const KConfigGroup popular = KSharedConfig::openConfig()->group(QLatin1String(PopularFoldersGroup));
    const QStringList popularUrls = popular.readEntry("Urls", QStringList());
    const QList<int> popularVisits = popular.readEntry("Visits", QList<int>());
    for (qsizetype i = 0; i < popularUrls.size(); ++i) {
        const QUrl url(popularUrls.at(i), QUrl::StrictMode);
        if (url.isValid() && !url.isEmpty()) {
            m_popularFolderVisits.insert(url.adjusted(QUrl::StripTrailingSlash).toString(QUrl::FullyEncoded), qMax(0, popularVisits.value(i)));
        }
    }
    setupPanes();
    setupPlaces();
    setupPanels();
    setupStatusBar();
    setupActions();

    setStandardToolBarMenuEnabled(true);
    setupGUI(QSize(1100, 700), Default, QStringLiteral("opus-file-managerui.rc"));
    m_toolbarCustomizer = new ToolbarCustomizer(toolBar(), actionCollection(), this);
    connect(actionCollection()->action(QStringLiteral("customize_main_toolbar")), &QAction::toggled, m_toolbarCustomizer, &ToolbarCustomizer::setEditing);
    connect(m_toolbarCustomizer,
            &ToolbarCustomizer::editingChanged,
            actionCollection()->action(QStringLiteral("customize_main_toolbar")),
            &QAction::setChecked);
    const KConfigGroup panels = KSharedConfig::openStateConfig()->group(QLatin1String(PanelsGroup));
    setPanelLayout(panels.readEntry("Layout", QString()) == QLatin1String("Separate") ? PanelLayout::Separate : PanelLayout::Combined);
    applyPanelSettings();

    connect(KIO::FileUndoManager::self(), &KIO::FileUndoManager::undoAvailable, this, [this](bool available) {
        m_undoAction->setEnabled(available);
    });
    connect(KIO::FileUndoManager::self(), &KIO::FileUndoManager::undoTextChanged, this, [this](const QString &text) {
        m_undoAction->setText(text);
    });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateActions);

    applyViewDefaults();
    setActivePane(m_left);
    applySettings();
    updateActions();
}

MainWindow::~MainWindow()
{
    if (m_editorDock) {
        disconnect(m_editorDock, nullptr, this, nullptr);
    }
}

void MainWindow::setupPanes()
{
    m_workspaceSplitter = new QSplitter(Qt::Vertical, this);
    m_workspaceSplitter->setObjectName(QStringLiteral("workspaceSplitter"));
    m_workspaceSplitter->setChildrenCollapsible(false);
    m_splitter = new QSplitter(Qt::Horizontal, m_workspaceSplitter);
    m_splitter->setObjectName(QStringLiteral("paneSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_left = new Pane(m_placesModel, m_tabColors, m_splitter);
    m_right = new Pane(m_placesModel, m_tabColors, m_splitter);
    m_previewPanel = new PreviewPanel(m_splitter);
    m_splitter->addWidget(m_left);
    m_splitter->addWidget(m_right);
    m_splitter->addWidget(m_previewPanel);
    m_previewPanel->hide();
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_terminalPanel = new TerminalPanel(m_workspaceSplitter);
    m_terminalPanel->hide();
    m_workspaceSplitter->addWidget(m_splitter);
    m_workspaceSplitter->addWidget(m_terminalPanel);
    m_workspaceSplitter->setStretchFactor(0, 3);
    m_workspaceSplitter->setStretchFactor(1, 1);
    setCentralWidget(m_workspaceSplitter);

    connectPane(m_left);
    connectPane(m_right);
}

void MainWindow::connectPane(Pane *pane)
{
    connect(pane, &Pane::activated, this, [this, pane] {
        setActivePane(pane);
    });
    connect(pane, &Pane::urlChanged, this, [this, pane](const QUrl &url) {
        syncLinkedPane(pane, url);
        recordFolderVisit(pane, url);
        pane->view()->setFlat(m_flatLocations.contains(flatKey(url)));
        const bool clearedHoveredInformation = m_hoveredInformationPane == pane;
        if (clearedHoveredInformation) {
            m_hoveredInformationPane = nullptr;
            m_hoveredInformationItem = {};
        }
        if (pane == m_active) {
            if (m_terminalPanelAction && m_terminalPanelAction->isChecked() && Settings::terminalFollowActiveFolder()) {
                m_terminalPanel->follow(url);
            }
            if (m_previewPanelAction && m_previewPanelAction->isChecked()) {
                m_previewPanel->clear();
            }
            updateActions();
            updateStatusBar();
            updateFreeSpace();
            updateWindowTitle();
            updateInformationPanel();
            syncFolderTrees();
        } else if (m_active) {
            if (Settings::statusBarLayout() == Settings::EnumStatusBarLayout::PerPane) {
                updateStatusBar();
                updateFreeSpace(pane);
            }
            updateActions();
        }
        if (clearedHoveredInformation && pane != m_active) {
            updateInformationPanel();
        }
    });
    connect(pane, &Pane::tabCountChanged, this, &MainWindow::updateTabBars);
    connect(pane, &Pane::statusMessage, this, &MainWindow::showStatusMessage);
    connect(pane->view(), &FolderView::itemActivated, this, [this, pane](const KFileItem &item) {
        activatePaneItem(pane, item);
    });
    connect(pane->view(), &FolderView::contextMenuRequested, this, [this, pane](const QPoint &pos, const KFileItemList &items) {
        setActivePane(pane);
        showContextMenu(pane, pos, items);
    });
    connect(pane->view(), &FolderView::selectionChanged, this, [this, pane] {
        updateStatusBar();
        if (pane == m_active) {
            updatePreviewPanel();
            updateInformationPanel();
        }
        if (m_active) {
            updateActions();
        }
    });
    connect(pane->view(), &FolderView::selectionModeChanged, this, [this, pane] {
        if (pane == m_active) {
            updateActions();
        }
    });
    connect(pane->view(), &FolderView::copySelectionRequested, this, [this, pane] {
        setActivePane(pane);
        copy();
    });
    connect(pane->view(), &FolderView::cutSelectionRequested, this, [this, pane] {
        setActivePane(pane);
        cut();
    });
    connect(pane->view(), &FolderView::trashSelectionRequested, this, [this, pane] {
        setActivePane(pane);
        trash();
    });
    connect(pane->view(), &FolderView::itemCountChanged, this, [this, pane] {
        updateStatusBar();
        if (pane == m_active) {
            updatePreviewPanel();
            updateInformationPanel();
        }
    });
    connect(pane->view(), &FolderView::itemHovered, this, [this, pane](const KFileItem &item) {
        if (Settings::informationHover()) {
            m_hoveredInformationPane = pane;
            m_hoveredInformationItem = item;
            updateInformationPanel();
        }
    });
    connect(pane->view(), &FolderView::itemHoverCleared, this, [this, pane] {
        if (m_hoveredInformationPane == pane) {
            m_hoveredInformationPane = nullptr;
            m_hoveredInformationItem = {};
            updateInformationPanel();
        }
    });
    connect(pane->view(), &FolderView::itemsOrderChanged, this, [this, pane] {
        if (pane == m_active) {
            updatePreviewPanel();
        }
    });
    connect(pane->view(), &FolderView::loadingChanged, this, [this, pane](bool loading) {
        if (pane == m_active && !loading) {
            updateFreeSpace();
        }
    });
}

void MainWindow::setupPlaces()
{
    m_placesView = new KFilePlacesView(this);
    m_placesView->setModel(m_placesModel);
    m_placesView->setDropOnPlaceEnabled(true);
    m_placesView->setAutoResizeItemsEnabled(false);

    m_placesDock = new QDockWidget(i18nc("@title:window", "Places"), this);
    m_placesDock->setObjectName(QStringLiteral("placesDock"));
    m_placesDock->setWidget(m_placesView);
    m_placesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_placesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::LeftDockWidgetArea, m_placesDock);

    connect(m_placesView, &KFilePlacesView::urlChanged, this, [this](const QUrl &url) {
        activePane()->setUrl(url);
        if (Settings::autoFocusPaneAfterPanelNavigation()) {
            activePane()->view()->currentView()->setFocus();
        }
    });
    connect(m_placesView, &KFilePlacesView::tabRequested, this, [this](const QUrl &url) {
        activePane()->openInNewTab(url);
    });
    connect(m_placesView, &KFilePlacesView::urlsDropped, this, [this](const QUrl &dest, QDropEvent *event, QWidget *) {
        FileOperations::drop(event, dest, this);
    });
}

void MainWindow::setupPanels()
{
    const auto addPanelDock = [this](const QString &objectName, const QString &title, QWidget *widget) {
        auto *dock = new QDockWidget(title, this);
        dock->setObjectName(objectName);
        dock->setWidget(widget);
        dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::LeftDockWidgetArea, dock);
        return dock;
    };
    const auto navigate = [this](const QUrl &url) {
        activePane()->setUrl(url);
        if (Settings::autoFocusPaneAfterPanelNavigation()) {
            activePane()->view()->currentView()->setFocus();
        }
    };
    const auto openInTab = [this](const QUrl &url) {
        activePane()->openInNewTab(url);
    };
    const KConfigGroup panels = KSharedConfig::openStateConfig()->group(QLatin1String(PanelsGroup));

    m_folderTree = new FolderTree({QUrl::fromLocalFile(QDir::homePath()), QUrl::fromLocalFile(QStringLiteral("/"))});
    m_folderTree->setExpandedUrls(panels.readEntry("FoldersExpanded", QList<QUrl>()));
    connect(m_folderTree, &FolderTree::urlActivated, this, navigate);
    connect(m_folderTree, &FolderTree::tabRequested, this, openInTab);
    connect(m_folderTree, &FolderTree::contextMenuRequested, this, &MainWindow::showFolderTreeMenu);
    connect(m_folderTree, &FolderTree::urlsDropped, this, [this](const QUrl &destination, QDropEvent *event) {
        FileOperations::drop(event, destination, this);
    });
    auto *treeScroll = new QScrollArea;
    treeScroll->setWidgetResizable(true);
    treeScroll->setWidget(m_folderTree);
    m_foldersDock = addPanelDock(QStringLiteral("foldersDock"), i18nc("@title:window", "Folders"), treeScroll);
    splitDockWidget(m_placesDock, m_foldersDock, Qt::Vertical);

    m_combinedPanel = new CombinedPanel(m_placesModel, m_favorites);
    m_combinedPanel->folderTree()->setExpandedUrls(panels.readEntry("CombinedExpanded", QList<QUrl>()));
    connect(m_combinedPanel, &CombinedPanel::urlActivated, this, navigate);
    connect(m_combinedPanel, &CombinedPanel::tabRequested, this, openInTab);
    connect(m_combinedPanel, &CombinedPanel::folderContextMenuRequested, this, &MainWindow::showFolderTreeMenu);
    m_combinedPanel->setCollapsedSections(panels.readEntry("CollapsedSections", QStringList()));
    connect(m_combinedPanel, &CombinedPanel::urlsDropped, this, [this](const QUrl &destination, QDropEvent *event) {
        FileOperations::drop(event, destination, this);
    });
    m_combinedDock = addPanelDock(QStringLiteral("combinedDock"), i18nc("@title:window", "Explorer"), m_combinedPanel);

    m_informationPanel = new InformationPanel;
    m_informationPanel->setFields(Settings::informationFields());
    m_informationDock = addPanelDock(QStringLiteral("informationDock"), i18nc("@title:window", "Information"), m_informationPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_informationDock);
    m_informationDock->hide();
}

void MainWindow::setPanelLayout(PanelLayout layout)
{
    m_panelLayout = layout;
    const bool combined = layout == PanelLayout::Combined;
    applyPanelSettings();
    m_panelLayoutAction->setCurrentItem(combined ? 0 : 1);

    KConfigGroup panels = KSharedConfig::openStateConfig()->group(QLatin1String(PanelsGroup));
    panels.writeEntry("Layout", combined ? QStringLiteral("Combined") : QStringLiteral("Separate"));
    panels.sync();
}

MainWindow::PanelLayout MainWindow::panelLayout() const
{
    return m_panelLayout;
}

FavoritesModel *MainWindow::favorites() const
{
    return m_favorites;
}

TabColors *MainWindow::tabColors() const
{
    return m_tabColors;
}

void MainWindow::compress()
{
    const KFileItemList selected = activePane()->view()->selectedItems();
    QStringList sources;
    for (const KFileItem &item : selected) {
        if (!item.localPath().isEmpty()) {
            sources.append(item.localPath());
        }
    }
    if (sources.isEmpty()) {
        return;
    }
    const QString defaultName = selected.size() == 1 ? (selected.first().isDir() ? selected.first().name() : Archives::baseName(selected.first().name()))
                                                     : activePane()->url().adjusted(QUrl::StripTrailingSlash).fileName();
    CompressDialog dialog(defaultName,
                          inactivePane()->url().isLocalFile(),
                          this,
                          static_cast<ArchiveWriter::Format>(Settings::defaultArchiveFormat()),
                          Settings::archiveOtherPaneByDefault());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QUrl folder = dialog.toOtherPane() ? inactivePane()->url() : activePane()->url();
    if (!folder.isLocalFile()) {
        showStatusMessage(i18nc("@info:status", "Archives can only be created in local folders."));
        return;
    }
    QString fileName = dialog.archiveName() + u'.' + ArchiveWriter::suffix(dialog.format());
    if (QFileInfo::exists(QDir(folder.toLocalFile()).filePath(fileName))) {
        fileName = KFileUtils::suggestName(folder, fileName);
    }
    const QString archivePath = QDir(folder.toLocalFile()).filePath(fileName);

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, fileName] {
        const QString error = watcher->result();
        showStatusMessage(error.isEmpty() ? i18nc("@info:status", "Created %1", fileName) : i18nc("@info:status", "Could not create %1: %2", fileName, error));
        watcher->deleteLater();
    });
    const QString password = dialog.password();
    const ArchiveWriter::Format format = dialog.format();
    watcher->setFuture(QtConcurrent::run([archivePath, format, sources, password] {
        return password.isEmpty() ? ArchiveWriter::write(archivePath, format, sources)
                                  : ArchiveWriter::writeEncryptedZip(archivePath, format, sources, password);
    }));
}

void MainWindow::setSelectionLabel(const QString &label)
{
    const KFileItemList items = activePane()->view()->selectedItems();
    for (const KFileItem &item : items) {
        const QFileInfo info(item.localPath());
        QString error;
        if (!FolderMetadata::setLabel(info.absolutePath(), info.fileName(), label, &error)) {
            showStatusMessage(i18nc("@info:status", "Could not set the label: %1", error));
            return;
        }
    }
    activePane()->view()->reloadMetadata();
}

void MainWindow::setSelectionColor(const QString &color)
{
    const KFileItemList items = activePane()->view()->selectedItems();
    for (const KFileItem &item : items) {
        const QFileInfo info(item.localPath());
        QString error;
        if (!FolderMetadata::setColor(info.absolutePath(), info.fileName(), color, &error)) {
            showStatusMessage(i18nc("@info:status", "Could not set the file colour: %1", error));
            return;
        }
    }
    activePane()->view()->reloadMetadata();
}

void MainWindow::setSelectionRating(int rating)
{
    const KFileItemList items = activePane()->view()->selectedItems();
    for (const KFileItem &item : items) {
        const QFileInfo info(item.localPath());
        QString error;
        if (!FolderMetadata::setRating(info.absolutePath(), info.fileName(), rating, &error)) {
            showStatusMessage(i18nc("@info:status", "Could not set the rating: %1", error));
            return;
        }
    }
    activePane()->view()->reloadMetadata();
}

void MainWindow::setSelectionNote()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    QString currentNote;
    if (items.size() == 1) {
        const QFileInfo info(items.first().localPath());
        QString error;
        const QJsonObject metadata = FolderMetadata::read(info.absolutePath(), &error);
        if (!error.isEmpty()) {
            showStatusMessage(i18nc("@info:status", "Could not read the file note: %1", error));
            return;
        }
        currentNote = metadata.value(QStringLiteral("notes")).toObject().value(info.fileName()).toString();
    }
    bool accepted = false;
    const QString note = QInputDialog::getMultiLineText(this,
                                                        i18nc("@title:window", "Edit Note"),
                                                        i18nc("@label:textbox", "Note (up to 500 characters):"),
                                                        currentNote,
                                                        &accepted);
    if (!accepted) {
        return;
    }
    if (note.size() > 500) {
        showStatusMessage(i18nc("@info:status", "Notes can be at most 500 characters."));
        return;
    }
    for (const KFileItem &item : items) {
        const QFileInfo info(item.localPath());
        QString error;
        if (!FolderMetadata::setNote(info.absolutePath(), info.fileName(), note, &error)) {
            showStatusMessage(i18nc("@info:status", "Could not set the note: %1", error));
            return;
        }
    }
    activePane()->view()->reloadMetadata();
}

void MainWindow::toggleSelectionPin()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    for (const KFileItem &item : items) {
        const QFileInfo info(item.localPath());
        QString error;
        if (!FolderMetadata::togglePin(info.absolutePath(), info.fileName(), &error)) {
            showStatusMessage(i18nc("@info:status", "Could not change the pin: %1", error));
            return;
        }
    }
    activePane()->view()->reloadMetadata();
}

void MainWindow::editFolderLook(const QUrl &folder)
{
    if (!folder.isLocalFile()) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(i18nc("@title:window", "Folder Look"));
    auto *icon = new QComboBox(&dialog);
    const QList<QPair<QString, QString>> icons{
        {i18nc("@item:inlistbox folder icon", "Default"), QString()},
        {i18nc("@item:inlistbox folder icon", "Folder"), QStringLiteral("folder")},
        {i18nc("@item:inlistbox folder icon", "Home"), QStringLiteral("folder-home")},
        {i18nc("@item:inlistbox folder icon", "Documents"), QStringLiteral("folder-documents")},
        {i18nc("@item:inlistbox folder icon", "Downloads"), QStringLiteral("folder-download")},
        {i18nc("@item:inlistbox folder icon", "Pictures"), QStringLiteral("folder-pictures")},
        {i18nc("@item:inlistbox folder icon", "Music"), QStringLiteral("folder-music")},
        {i18nc("@item:inlistbox folder icon", "Videos"), QStringLiteral("folder-videos")},
    };
    for (const auto &[label, name] : icons) {
        icon->addItem(label, name);
    }
    const QString directoryFile = QDir(folder.toLocalFile()).filePath(QStringLiteral(".directory"));
    KConfig directoryConfig(directoryFile, KConfig::SimpleConfig);
    const QString existingIcon = directoryConfig.group(QStringLiteral("Desktop Entry")).readEntry("Icon", QString());
    const int iconIndex = icon->findData(existingIcon);
    if (iconIndex >= 0) {
        icon->setCurrentIndex(iconIndex);
    } else if (!existingIcon.isEmpty()) {
        icon->addItem(existingIcon, existingIcon);
        icon->setCurrentIndex(icon->count() - 1);
    }

    auto *color = new QComboBox(&dialog);
    color->addItem(i18nc("@item:inlistbox folder colour", "Default"), QString());
    for (LabelColor value : LabelColors::all()) {
        color->addItem(labelText(value), LabelColors::rgb(value).name());
    }
    QString metadataError;
    const QJsonObject metadata = FolderMetadata::read(folder.toLocalFile(), &metadataError);
    if (!metadataError.isEmpty()) {
        showStatusMessage(i18nc("@info:status", "Could not read the folder appearance: %1", metadataError));
        return;
    }
    const QString existingColor = metadata.value(QStringLiteral("color")).toString();
    const int colorIndex = color->findData(existingColor);
    if (colorIndex >= 0) {
        color->setCurrentIndex(colorIndex);
    }

    auto *form = new QFormLayout(&dialog);
    form->addRow(i18nc("@label:listbox", "Icon:"), icon);
    form->addRow(i18nc("@label:listbox", "Colour:"), color);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (!directoryConfig.isConfigWritable(false)) {
        showStatusMessage(i18nc("@info:status", "The folder's .directory file is not writable."));
        return;
    }

    QString error;
    if (!FolderMetadata::setFolderColor(folder.toLocalFile(), color->currentData().toString(), &error)) {
        showStatusMessage(i18nc("@info:status", "Could not set the folder colour: %1", error));
        return;
    }
    KConfigGroup desktopEntry = directoryConfig.group(QStringLiteral("Desktop Entry"));
    const QString iconName = icon->currentData().toString();
    if (iconName.isEmpty()) {
        desktopEntry.deleteEntry("Icon");
    } else {
        desktopEntry.writeEntry("Icon", iconName);
    }
    if (!directoryConfig.sync()) {
        showStatusMessage(i18nc("@info:status", "Could not save the folder icon."));
        return;
    }
    const QUrl parent = KIO::upUrl(folder);
    for (Pane *pane : {m_left, m_right}) {
        if (pane->url() == parent || pane->url() == folder) {
            pane->view()->reload();
        }
    }
    m_folderTree->reload();
    m_combinedPanel->folderTree()->reload();
    m_folderTree->reveal(folder);
    m_combinedPanel->folderTree()->reveal(folder);
}

void MainWindow::extractToOtherPane()
{
    const QUrl destination = inactivePane()->url();
    const KFileItemList selected = activePane()->view()->selectedItems();
    for (const KFileItem &item : selected) {
        const QUrl archive = Archives::browseUrl(item.mostLocalUrl(), item.mimetype());
        if (!archive.isValid()) {
            continue;
        }
        QString name = Archives::baseName(item.name());
        if (destination.isLocalFile() && QFileInfo::exists(QDir(destination.toLocalFile()).filePath(name))) {
            name = KFileUtils::suggestName(destination, name);
        }
        QUrl target = destination;
        target.setPath(QDir(destination.path()).filePath(name));
        extractArchive(archive, target);
    }
}

void MainWindow::extractProtectedToOtherPane()
{
    const QUrl destination = inactivePane()->url();
    if (!destination.isLocalFile()) {
        showStatusMessage(i18nc("@info:status archive extraction", "Password-protected archives require a local destination."));
        return;
    }
    for (const KFileItem &item : activePane()->view()->selectedItems()) {
        if (item.localPath().isEmpty() || Archives::browseUrl(item.mostLocalUrl(), item.mimetype()).isEmpty()) {
            continue;
        }
        bool accepted = false;
        const QString password = QInputDialog::getText(this,
                                                       i18nc("@title:window", "Archive Password"),
                                                       i18nc("@label:textbox", "Password for %1:", item.name()),
                                                       QLineEdit::Password,
                                                       QString(),
                                                       &accepted);
        if (!accepted) {
            continue;
        }
        if (password.isEmpty()) {
            showStatusMessage(i18nc("@info:status archive extraction", "An archive password is required."));
            continue;
        }
        QString name = Archives::baseName(item.name());
        if (QFileInfo::exists(QDir(destination.toLocalFile()).filePath(name))) {
            name = KFileUtils::suggestName(destination, name);
        }
        const QString target = QDir(destination.toLocalFile()).filePath(name);
        const QString archivePath = item.localPath();
        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, target] {
            const QString error = watcher->result();
            showStatusMessage(error.isEmpty()
                                  ? i18nc("@info:status archive extraction", "Extracted to %1", target)
                                  : i18nc("@info:status archive extraction", "Extraction failed: %1. A partial folder may remain at %2.", error, target));
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([archivePath, target, password] {
            return Archives::extractProtected(archivePath, target, password);
        }));
    }
}

void MainWindow::extractArchive(const QUrl &archive, const QUrl &target)
{
    auto *list = KIO::listDir(archive, KIO::HideProgressInfo);
    auto entries = std::make_shared<QList<QUrl>>();
    connect(list, &KIO::ListJob::entries, this, [archive, entries](KIO::Job *, const KIO::UDSEntryList &listed) {
        for (const KIO::UDSEntry &entry : listed) {
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name != QLatin1String(".") && name != QLatin1String("..")) {
                QUrl child = archive;
                child.setPath(archive.path() + name);
                entries->append(child);
            }
        }
    });
    connect(list, &KJob::result, this, [this, target, entries](KJob *job) {
        if (job->error()) {
            showStatusMessage(job->errorString());
            return;
        }
        KIO::MkdirJob *mkdir = KIO::mkdir(target);
        KJobWidgets::setWindow(mkdir, this);
        connect(mkdir, &KJob::result, this, [this, target, entries](KJob *made) {
            if (made->error()) {
                showStatusMessage(made->errorString());
                return;
            }
            KIO::CopyJob *copy = KIO::copy(*entries, target);
            KIO::FileUndoManager::self()->recordCopyJob(copy);
            KJobWidgets::setWindow(copy, this);
        });
    });
}

void MainWindow::addToFavorites()
{
    const KFileItemList selected = activePane()->view()->selectedItems();
    bool addedSelection = false;
    for (const KFileItem &item : selected) {
        if (item.isDir()) {
            m_favorites->add(item.targetUrl());
            addedSelection = true;
        }
    }
    if (!addedSelection) {
        m_favorites->add(activePane()->url());
    }
}

void MainWindow::setupStatusBar()
{
    m_itemsLabel = new QLabel(this);
    m_itemsLabel->setObjectName(QStringLiteral("itemsLabel"));
    m_messageLabel = new QLabel(this);
    m_freeSpaceLabel = new QLabel(this);
    m_freeSpaceLabel->setObjectName(QStringLiteral("freeSpaceLabel"));
    statusBar()->addWidget(m_itemsLabel);
    statusBar()->addWidget(m_messageLabel, 1);
    statusBar()->addPermanentWidget(m_freeSpaceLabel);
    for (Pane *pane : {m_left, m_right}) {
        auto *bar = new QWidget(pane);
        auto *layout = new QHBoxLayout(bar);
        layout->setContentsMargins(4, 0, 4, 0);
        layout->setSpacing(8);
        auto *items = new QLabel(bar);
        items->setObjectName(pane == m_left ? QStringLiteral("leftItemsLabel") : QStringLiteral("rightItemsLabel"));
        auto *freeSpace = new QLabel(bar);
        freeSpace->setObjectName(pane == m_left ? QStringLiteral("leftFreeSpaceLabel") : QStringLiteral("rightFreeSpaceLabel"));
        layout->addWidget(items);
        layout->addStretch();
        layout->addWidget(freeSpace);
        pane->setStatusBarWidget(bar);
        m_paneItemsLabels.insert(pane, items);
        m_paneFreeSpaceLabels.insert(pane, freeSpace);
    }
}

void MainWindow::setupActions()
{
    KActionCollection *actions = actionCollection();

    // File
    m_newFileMenu = new KNewFileMenu(this);
    m_newFileMenu->setParentWidget(this);
    m_newFileMenu->setPopupMode(QToolButton::InstantPopup);
    actions->addAction(QStringLiteral("new_menu"), m_newFileMenu);
    actions->setDefaultShortcut(m_newFileMenu, Qt::CTRL | Qt::Key_N);
    connect(m_newFileMenu, &QAction::triggered, this, &MainWindow::showNewMenu);
    connect(m_newFileMenu->menu(), &QMenu::aboutToShow, this, [this] {
        m_newFileMenu->setWorkingDirectory(activePane()->url());
        m_newFileMenu->checkUpToDate();
    });

    QAction *newFolder = actions->addAction(QStringLiteral("new_folder"), this, &MainWindow::newFolder);
    newFolder->setText(i18nc("@action", "New Folder…"));
    newFolder->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    actions->setDefaultShortcut(newFolder, Qt::Key_F7);
    m_newFileMenu->setNewFolderShortcutAction(newFolder);

    QAction *copyTo = actions->addAction(QStringLiteral("copy_to_other_pane"), this, &MainWindow::copyToOtherPane);
    copyTo->setText(i18nc("@action", "Copy to Other Pane"));
    copyTo->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    actions->setDefaultShortcut(copyTo, Qt::Key_F5);

    QAction *moveTo = actions->addAction(QStringLiteral("move_to_other_pane"), this, &MainWindow::moveToOtherPane);
    moveTo->setText(i18nc("@action", "Move to Other Pane"));
    moveTo->setIcon(QIcon::fromTheme(QStringLiteral("edit-move")));
    actions->setDefaultShortcut(moveTo, Qt::Key_F6);

    QAction *renameAction = actions->addAction(QStringLiteral("rename"), this, &MainWindow::rename);
    renameAction->setText(i18nc("@action", "Rename…"));
    renameAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
    actions->setDefaultShortcut(renameAction, Qt::Key_F2);
    QAction *batchRenameAction = actions->addAction(QStringLiteral("batch_rename"), this, &MainWindow::batchRename);
    batchRenameAction->setText(i18nc("@action", "Batch Rename…"));
    batchRenameAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
    actions->setDefaultShortcut(batchRenameAction, Qt::SHIFT | Qt::Key_F2);
    QAction *editMetadata = actions->addAction(QStringLiteral("edit_metadata"), this, &MainWindow::editMetadata);
    editMetadata->setText(i18nc("@action", "Edit Metadata…"));
    editMetadata->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));

    QAction *symlink = actions->addAction(QStringLiteral("create_symlink"), this, &MainWindow::createSymlink);
    symlink->setText(i18nc("@action", "Create Symbolic Link…"));
    symlink->setIcon(QIcon::fromTheme(QStringLiteral("emblem-symbolic-link")));

    QAction *trashAction = actions->addAction(QStringLiteral("move_to_trash"), this, &MainWindow::trash);
    trashAction->setText(i18nc("@action", "Move to Trash"));
    trashAction->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
    actions->setDefaultShortcuts(trashAction, {Qt::Key_F8, Qt::Key_Delete});

    QAction *deleteAction = actions->addAction(QStringLiteral("delete"), this, &MainWindow::deletePermanently);
    deleteAction->setText(i18nc("@action", "Delete Permanently"));
    deleteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    actions->setDefaultShortcut(deleteAction, Qt::SHIFT | Qt::Key_Delete);

    QAction *properties = actions->addAction(QStringLiteral("properties"), this, &MainWindow::showProperties);
    properties->setText(i18nc("@action", "Properties"));
    properties->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    actions->setDefaultShortcut(properties, Qt::ALT | Qt::Key_Return);

    QAction *occupiedSpace = actions->addAction(QStringLiteral("calculate_occupied_space"), this, &MainWindow::calculateOccupiedSpace);
    occupiedSpace->setText(i18nc("@action", "Calculate Occupied Space"));
    occupiedSpace->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));
    QAction *contentChart = actions->addAction(QStringLiteral("content_chart"), this, &MainWindow::showContentChart);
    contentChart->setText(i18nc("@action", "Folder Content Chart"));
    contentChart->setIcon(QIcon::fromTheme(QStringLiteral("view-statistics")));
    QAction *search = actions->addAction(QStringLiteral("search_folder"), this, &MainWindow::showSearch);
    search->setText(i18nc("@action", "Search Current Folder…"));
    search->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    QAction *duplicateFinder = actions->addAction(QStringLiteral("find_duplicates"), this, &MainWindow::showDuplicateFinder);
    duplicateFinder->setText(i18nc("@action", "Find Duplicate Files…"));
    QAction *compareFolders = actions->addAction(QStringLiteral("compare_pane_folders"), this, &MainWindow::comparePaneFolders);
    compareFolders->setText(i18nc("@action", "Compare Pane Folders"));
    QAction *filteredComparison = actions->addAction(QStringLiteral("compare_folders_filtered"), this, &MainWindow::compareFoldersFiltered);
    filteredComparison->setText(i18nc("@action", "Compare and Synchronize Folders…"));
    QAction *createChecksum = actions->addAction(QStringLiteral("create_checksum"), this, &MainWindow::createChecksumFile);
    createChecksum->setText(i18nc("@action", "Create SHA-256 Checksum File…"));
    QAction *verifyChecksum = actions->addAction(QStringLiteral("verify_checksum"), this, &MainWindow::verifyChecksumFile);
    verifyChecksum->setText(i18nc("@action", "Verify SHA-256 Checksum File"));
    QAction *splitFile = actions->addAction(QStringLiteral("split_file"), this, &MainWindow::splitFile);
    splitFile->setText(i18nc("@action", "Split File…"));
    QAction *combineFiles = actions->addAction(QStringLiteral("combine_files"), this, &MainWindow::combineFiles);
    combineFiles->setText(i18nc("@action", "Combine Files…"));

    QAction *terminal = actions->addAction(QStringLiteral("open_terminal"), this, &MainWindow::openTerminal);
    terminal->setText(i18nc("@action", "Open Terminal Here"));
    terminal->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    actions->setDefaultShortcut(terminal, Qt::SHIFT | Qt::Key_F4);
    QAction *customCommand = actions->addAction(QStringLiteral("custom_command"), this, &MainWindow::showCustomCommand);
    customCommand->setText(i18nc("@action", "Scripts and Custom Commands…"));
    customCommand->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    refreshCustomCommandActions();
    QAction *transferQueue = actions->addAction(QStringLiteral("transfer_queue"), this, &MainWindow::showTransferQueue);
    transferQueue->setText(i18nc("@action", "Transfer Queue…"));
    transferQueue->setIcon(QIcon::fromTheme(QStringLiteral("view-list-details")));
    QAction *editor = actions->addAction(QStringLiteral("open_editor"), this, &MainWindow::openEditor);
    editor->setText(i18nc("@action", "Open Editor"));
    editor->setIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));
    QAction *compare = actions->addAction(QStringLiteral("compare_files"), this, &MainWindow::compareFiles);
    compare->setText(i18nc("@action", "Compare Files"));
    compare->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    QAction *exportDefault = actions->add<QAction>(QStringLiteral("export_listing"));
    exportDefault->setText(i18nc("@action", "Export Listing…"));
    connect(exportDefault, &QAction::triggered, this, [this] {
        exportListing(Settings::listingExportFormat() == Settings::EnumListingExportFormat::Pdf);
    });
    QAction *exportCsv = actions->add<QAction>(QStringLiteral("export_listing_csv"));
    exportCsv->setText(i18nc("@action", "Export Listing to CSV…"));
    connect(exportCsv, &QAction::triggered, this, [this] {
        exportListing(false);
    });
    QAction *exportPdf = actions->add<QAction>(QStringLiteral("export_listing_pdf"));
    exportPdf->setText(i18nc("@action", "Export Listing to PDF…"));
    connect(exportPdf, &QAction::triggered, this, [this] {
        exportListing(true);
    });

    KStandardAction::quit(this, &MainWindow::close, actions);

    // Edit
    m_undoAction = KStandardAction::undo(KIO::FileUndoManager::self(), &KIO::FileUndoManager::undo, actions);
    m_undoAction->setEnabled(KIO::FileUndoManager::self()->isUndoAvailable());
    // The platform alternates (Shift+Delete for cut, Backspace for back…)
    // collide with the file-manager bindings below, so pin single keys.
    QAction *cutAction = KStandardAction::cut(this, &MainWindow::cut, actions);
    actions->setDefaultShortcut(cutAction, Qt::CTRL | Qt::Key_X);
    QAction *copyAction = KStandardAction::copy(this, &MainWindow::copy, actions);
    actions->setDefaultShortcut(copyAction, Qt::CTRL | Qt::Key_C);
    m_pasteAction = KStandardAction::paste(this, &MainWindow::paste, actions);
    actions->setDefaultShortcut(m_pasteAction, Qt::CTRL | Qt::Key_V);
    actions->setDefaultShortcut(m_undoAction, Qt::CTRL | Qt::Key_Z);
    KStandardAction::selectAll(
        this,
        [this] {
            activePane()->view()->selectAll();
        },
        actions);

    auto *selectMenu = new QMenu(i18nc("@menu", "Select"), this);
    QAction *selectPattern = selectMenu->addAction(i18nc("@action:inmenu", "Select by Pattern…"));
    connect(selectPattern, &QAction::triggered, this, [this] {
        selectByPattern(true);
    });
    QAction *deselectPattern = selectMenu->addAction(i18nc("@action:inmenu", "Deselect by Pattern…"));
    connect(deselectPattern, &QAction::triggered, this, [this] {
        selectByPattern(false);
    });
    QAction *selectPaths = actions->addAction(QStringLiteral("select_by_path_filter"));
    selectPaths->setText(i18nc("@action:inmenu", "Select by Path Filter…"));
    selectMenu->addAction(selectPaths);
    connect(selectPaths, &QAction::triggered, this, [this] {
        selectByPathFilter(true);
    });
    QAction *deselectPaths = actions->addAction(QStringLiteral("deselect_by_path_filter"));
    deselectPaths->setText(i18nc("@action:inmenu", "Deselect by Path Filter…"));
    selectMenu->addAction(deselectPaths);
    connect(deselectPaths, &QAction::triggered, this, [this] {
        selectByPathFilter(false);
    });
    selectMenu->addSeparator();
    QAction *saveSelection = selectMenu->addAction(i18nc("@action:inmenu", "Save Selection Filter…"));
    connect(saveSelection, &QAction::triggered, this, &MainWindow::saveSelectionFilter);
    m_selectionFiltersMenu = selectMenu->addMenu(i18nc("@menu", "Saved Selection Filters"));
    connect(m_selectionFiltersMenu, &QMenu::aboutToShow, this, &MainWindow::populateSelectionFilters);
    actions->addAction(QStringLiteral("select_menu"), selectMenu->menuAction());

    QAction *copyPath = actions->addAction(QStringLiteral("copy_path"), this, [this] {
        const KFileItemList items = activePane()->view()->selectedItemsOrRoot();
        QStringList paths;
        for (const KFileItem &item : items) {
            paths.append(item.url().toDisplayString(QUrl::PreferLocalFile));
        }
        QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
    });
    copyPath->setText(i18nc("@action", "Copy Path"));
    copyPath->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-path")));

    // View
    m_dualLayoutAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action", "Dual Pane"), this);
    m_dualLayoutAction->setChecked(true);
    actions->addAction(QStringLiteral("dual_layout"), m_dualLayoutAction);
    actions->setDefaultShortcut(m_dualLayoutAction, Qt::CTRL | Qt::SHIFT | Qt::Key_L);
    connect(m_dualLayoutAction, &KToggleAction::toggled, this, &MainWindow::toggleLayout);

    m_stackedLayoutAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")), i18nc("@action", "Stacked Panes"), this);
    actions->addAction(QStringLiteral("stacked_layout"), m_stackedLayoutAction);
    connect(m_stackedLayoutAction, &KToggleAction::toggled, this, &MainWindow::setStackedLayout);

    auto *swapTabs = actions->addAction(QStringLiteral("swap_current_tabs"), this, &MainWindow::swapCurrentTabs);
    swapTabs->setText(i18nc("@action", "Swap Current Tabs"));
    auto *swapSides = actions->addAction(QStringLiteral("swap_pane_sides"), this, &MainWindow::swapPaneSides);
    swapSides->setText(i18nc("@action", "Swap Pane Sides"));
    auto *saveSizes = actions->addAction(QStringLiteral("save_splitter_sizes"), this, &MainWindow::saveSplitterSizes);
    saveSizes->setText(i18nc("@action", "Save Pane and Panel Sizes"));
    auto *resetSizes = actions->addAction(QStringLiteral("reset_splitter_sizes"), this, &MainWindow::resetSplitterSizes);
    resetSizes->setText(i18nc("@action", "Reset Pane and Panel Sizes"));
    auto *customizeCombinedPanel = actions->addAction(QStringLiteral("customize_combined_panel"), m_combinedPanel, &CombinedPanel::configureSections);
    customizeCombinedPanel->setText(i18nc("@action", "Customize Combined Panel…"));
    auto *tabPosition = new KSelectAction(i18nc("@action", "Active Pane Tab Bar Position"), this);
    tabPosition->setItems({i18nc("@item:inmenu tab bar position", "Above"),
                           i18nc("@item:inmenu tab bar position", "Below"),
                           i18nc("@item:inmenu tab bar position", "Left"),
                           i18nc("@item:inmenu tab bar position", "Right")});
    actions->addAction(QStringLiteral("tab_bar_position"), tabPosition);
    auto *tabsTogether = new KToggleAction(i18nc("@action", "Keep Stacked Tab Bars Together"), this);
    tabsTogether->setChecked(Settings::stackedTabBarsTogether());
    actions->addAction(QStringLiteral("stacked_tab_bars_together"), tabsTogether);
    connect(tabsTogether, &KToggleAction::toggled, this, [this](bool together) {
        Settings::setStackedTabBarsTogether(together);
        Settings::self()->save();
        updateTabBarPlacement();
    });
    connect(tabPosition, &KSelectAction::indexTriggered, this, [this, tabsTogether](int position) {
        if (isStackedLayout()) {
            if (tabsTogether->isChecked()) {
                tabsTogether->setChecked(false);
            }
            if (activePane() == m_left) {
                Settings::setStackedFirstTabPosition(position);
            } else {
                Settings::setStackedSecondTabPosition(position);
            }
        } else if (activePane() == m_left) {
            Settings::setHorizontalFirstTabPosition(position);
        } else {
            Settings::setHorizontalSecondTabPosition(position);
        }
        Settings::self()->save();
        updateTabBarPlacement();
    });
    auto *showTabClose = new KToggleAction(i18nc("@action", "Show Tab Close Buttons"), this);
    showTabClose->setChecked(Settings::showTabCloseButtons());
    actions->addAction(QStringLiteral("show_tab_close_buttons"), showTabClose);
    connect(showTabClose, &KToggleAction::toggled, this, [this](bool shown) {
        Settings::setShowTabCloseButtons(shown);
        Settings::self()->save();
        m_left->reloadTabSettings();
        m_right->reloadTabSettings();
    });

    auto *dimInactive = new KToggleAction(i18nc("@action", "Dim Inactive Pane"), this);
    dimInactive->setChecked(Settings::dimInactivePane());
    actions->addAction(QStringLiteral("dim_inactive_pane"), dimInactive);
    connect(dimInactive, &QAction::triggered, this, [this](bool enabled) {
        Settings::setDimInactivePane(enabled);
        Settings::self()->save();
        m_left->setInactiveDimming(enabled);
        m_right->setInactiveDimming(enabled);
    });

    m_terminalPanelAction = new KToggleAction(i18nc("@action", "Show Embedded Terminal"), this);
    actions->addAction(QStringLiteral("show_embedded_terminal"), m_terminalPanelAction);
    connect(m_terminalPanelAction, &KToggleAction::toggled, this, [this](bool shown) {
        if (shown) {
            m_terminalPanel->start(activePane()->url());
        }
        m_terminalPanel->setVisible(shown);
        toggleLayout(m_dualLayoutAction->isChecked());
        restoreSplitterSizes(false, true);
    });

    m_linkedPanesAction = new KToggleAction(i18nc("@action", "Link Pane Navigation"), this);
    actions->addAction(QStringLiteral("link_panes"), m_linkedPanesAction);
    connect(m_linkedPanesAction, &KToggleAction::toggled, this, [this](bool linked) {
        if (!linked) {
            return;
        }
        if (!isDualLayout() || m_left->isHidden() || m_right->isHidden()) {
            m_linkedPanesAction->setChecked(false);
            showStatusMessage(i18nc("@info:status", "Show both panes before linking their navigation."));
            return;
        }
        const auto result = LinkedNavigation::target(m_left->url(), m_right->url(), m_left->url());
        if (!result.error.isEmpty()) {
            m_linkedPanesAction->setChecked(false);
            showStatusMessage(result.error);
            return;
        }
        m_linkedLeftRoot = m_left->url();
        m_linkedRightRoot = m_right->url();
        showStatusMessage(i18nc("@info:status", "Pane navigation linked from the current folders."));
    });

    QAction *showPlaces = m_placesDock->toggleViewAction();
    showPlaces->setText(i18nc("@action", "Show Places Panel"));
    showPlaces->setIcon(QIcon::fromTheme(QStringLiteral("compass")));
    actions->addAction(QStringLiteral("show_places"), showPlaces);
    actions->setDefaultShortcut(showPlaces, Qt::Key_F9);
    connect(showPlaces, &QAction::triggered, this, &MainWindow::rememberPlacesShown);
    m_placesDock->installEventFilter(this);

    m_viewModeAction = actions->addAction(QStringLiteral("toggle_view_mode"), this, &MainWindow::toggleViewMode);
    m_viewModeAction->setText(i18nc("@action", "Icons View"));
    m_viewModeAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    actions->setDefaultShortcut(m_viewModeAction, Qt::CTRL | Qt::SHIFT | Qt::Key_V);
    QAction *compactView = actions->addAction(QStringLiteral("compact_view"), this, [this] {
        activePane()->view()->setViewMode(FolderView::ViewMode::Compact);
        updateActions();
    });
    compactView->setText(i18nc("@action", "Compact View"));
    compactView->setIcon(QIcon::fromTheme(QStringLiteral("view-list-tree")));

    QAction *formatSource = actions->addAction(QStringLiteral("folder_format_source"));
    formatSource->setEnabled(false);
    QAction *openFormatSource = actions->addAction(QStringLiteral("open_folder_format_source"), this, [this] {
        const QString source = activePane()->view()->formatRuleSource();
        if (!source.isEmpty()) {
            activePane()->setUrl(QUrl::fromLocalFile(source));
        }
    });
    openFormatSource->setText(i18nc("@action", "Go to Folder Format Source"));
    QAction *clearFormat = actions->addAction(QStringLiteral("clear_folder_format"), this, [this] {
        if (KMessageBox::warningContinueCancel(
                this,
                i18nc("@info", "Remove this folder's saved format? Other supported metadata is kept, but unsupported fields may be discarded."),
                i18nc("@title:window", "Remove Folder Format"))
            == KMessageBox::Continue) {
            activePane()->view()->clearLocalFolderFormat();
            updateActions();
        }
    });
    clearFormat->setText(i18nc("@action", "Remove This Folder's Format…"));
    QAction *formatLock = actions->addAction(QStringLiteral("lock_folder_format"));
    formatLock->setText(i18nc("@action", "Lock Current Pane Format"));
    formatLock->setCheckable(true);
    connect(formatLock, &QAction::triggered, this, [this](bool locked) {
        activePane()->view()->setFormatLocked(locked);
        updateActions();
    });
    QAction *formatDescendants = actions->addAction(QStringLiteral("inherit_folder_format"));
    formatDescendants->setText(i18nc("@action", "Apply Folder Format to Descendants"));
    formatDescendants->setCheckable(true);
    connect(formatDescendants, &QAction::triggered, this, [this](bool include) {
        activePane()->view()->setFormatIncludesDescendants(include);
        updateActions();
    });
    QAction *folderSizes = actions->addAction(QStringLiteral("calculate_folder_sizes"), this, [this] {
        activePane()->view()->calculateFolderSizes();
    });
    folderSizes->setText(i18nc("@action", "Calculate Folder Sizes"));
    QAction *automaticSizes = actions->addAction(QStringLiteral("automatic_folder_sizes"));
    automaticSizes->setText(i18nc("@action", "Automatically Calculate Folder Sizes for This Format"));
    automaticSizes->setCheckable(true);
    connect(automaticSizes, &QAction::triggered, this, [this](bool enabled) {
        activePane()->view()->setAutomaticFolderSizes(enabled);
        updateActions();
    });
    QAction *defaultSizePolicy = actions->addAction(QStringLiteral("default_folder_size_policy"), this, [this] {
        activePane()->view()->clearFolderSizePolicy();
        updateActions();
    });
    defaultSizePolicy->setText(i18nc("@action", "Use Location Default for This Format's Folder Sizes"));
    const auto addSizeDefault = [this, actions](const QString &name, const QString &text, bool local) {
        QAction *action = actions->addAction(name);
        action->setText(text);
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, local](bool enabled) {
            if (local) {
                Settings::setAutomaticLocalFolderSizes(enabled);
            } else {
                Settings::setAutomaticRemoteFolderSizes(enabled);
            }
            Settings::self()->save();
            m_automaticLocalSizeDefault = Settings::automaticLocalFolderSizes();
            m_automaticRemoteSizeDefault = Settings::automaticRemoteFolderSizes();
            m_left->view()->refreshFolderSizePolicy();
            m_right->view()->refreshFolderSizePolicy();
            updateActions();
        });
    };
    addSizeDefault(QStringLiteral("automatic_local_folder_sizes"), i18nc("@action", "Automatically Calculate Local Folder Sizes by Default"), true);
    addSizeDefault(QStringLiteral("automatic_remote_folder_sizes"), i18nc("@action", "Automatically Calculate Remote Folder Sizes by Default"), false);

    m_selectionModeAction = actions->addAction(QStringLiteral("selection_mode"));
    m_selectionModeAction->setText(i18nc("@action", "Selection Mode"));
    m_selectionModeAction->setCheckable(true);
    connect(m_selectionModeAction, &QAction::triggered, this, [this](bool enabled) {
        activePane()->view()->setSelectionMode(enabled);
    });
    m_selectionMarkersAction = actions->addAction(QStringLiteral("selection_markers"));
    m_selectionMarkersAction->setText(i18nc("@action", "Show Selection Markers"));
    m_selectionMarkersAction->setCheckable(true);
    connect(m_selectionMarkersAction, &QAction::triggered, this, [this](bool enabled) {
        activePane()->view()->setSelectionMarkersVisible(enabled);
    });

    m_hiddenFilesAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-hidden")), i18nc("@action", "Show Hidden Files"), this);
    actions->addAction(QStringLiteral("show_hidden_files"), m_hiddenFilesAction);
    actions->setDefaultShortcut(m_hiddenFilesAction, Qt::CTRL | Qt::Key_H);
    connect(m_hiddenFilesAction, &KToggleAction::toggled, this, &MainWindow::setShowHiddenFiles);

    m_previewsAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-preview")), i18nc("@action", "Previews"), this);
    m_previewsAction->setChecked(true);
    actions->addAction(QStringLiteral("show_previews"), m_previewsAction);
    connect(m_previewsAction, &KToggleAction::toggled, this, &MainWindow::setPreviewsEnabled);

    m_previewPanelAction = new KToggleAction(SymbolicIcon::fromTheme(QStringLiteral("document-preview")), i18nc("@action", "Preview Panel"), this);
    actions->addAction(QStringLiteral("show_preview_panel"), m_previewPanelAction);
    actions->setDefaultShortcut(m_previewPanelAction, Qt::Key_F3);
    connect(m_previewPanelAction, &KToggleAction::toggled, this, [this](bool shown) {
        toggleLayout(m_dualLayoutAction->isChecked());
        if (shown) {
            updatePreviewPanel();
        } else {
            m_previewPanel->clear();
        }
    });
    connect(m_previewPanel, &PreviewPanel::closeRequested, m_previewPanelAction, &QAction::trigger);
    connect(m_previewPanel, &PreviewPanel::galleryRequested, this, &MainWindow::openGallery);
    connect(m_previewPanel, &PreviewPanel::imageRequested, this, [this](const QUrl &url) {
        activePane()->view()->selectUrls({url});
    });
    connect(m_previewPanel, &PreviewPanel::openRequested, this, [this] {
        const KFileItemList items = activePane()->view()->selectedItems();
        if (items.size() == 1) {
            FileOperations::open(items.first(), this);
        }
    });
    QAction *informationAction = actions->addAction(QStringLiteral("show_information_panel"));
    informationAction->setText(i18nc("@action", "Information Panel"));
    informationAction->setCheckable(true);
    connect(informationAction, &QAction::triggered, this, [this] {
        m_informationDock->setVisible(!m_informationDock->isVisible());
    });
    connect(m_informationDock, &QDockWidget::visibilityChanged, informationAction, &QAction::setChecked);
    QAction *customizeToolbar = actions->addAction(QStringLiteral("customize_main_toolbar"));
    customizeToolbar->setText(i18nc("@action", "Customize Main Toolbar"));
    customizeToolbar->setCheckable(true);
    QAction *gallery = actions->addAction(QStringLiteral("open_gallery"), this, &MainWindow::openGallery);
    gallery->setText(i18nc("@action", "Open Gallery"));
    gallery->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-media-preview")));
    actions->setDefaultShortcut(gallery, Qt::Key_Space);

    QAction *reload = KStandardAction::redisplay(
        this,
        [this] {
            activePane()->reload();
        },
        actions);
    // F5 is copy-to-other-pane here, so reload must not keep the F5 default.
    actions->setDefaultShortcut(reload, Qt::CTRL | Qt::Key_R);

    // Go
    QAction *back = KStandardAction::back(
        this,
        [this] {
            activePane()->goBack();
        },
        actions);
    actions->setDefaultShortcut(back, Qt::ALT | Qt::Key_Left);
    QAction *forward = KStandardAction::forward(
        this,
        [this] {
            activePane()->goForward();
        },
        actions);
    actions->setDefaultShortcut(forward, Qt::ALT | Qt::Key_Right);
    QAction *up = KStandardAction::up(
        this,
        [this] {
            activePane()->goUp();
        },
        actions);
    actions->setDefaultShortcuts(up, {Qt::Key_Backspace, Qt::ALT | Qt::Key_Up});
    KStandardAction::home(
        this,
        [this] {
            activePane()->setUrl(Settings::homeUrl());
        },
        actions);

    m_jumpBackAction = actions->addAction(QStringLiteral("jump_back"), this, &MainWindow::jumpBack);
    m_jumpBackAction->setText(i18nc("@action", "Jump Back"));
    m_jumpBackAction->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));
    m_jumpBackAction->setEnabled(false);

    m_popularFoldersMenu = new QMenu(i18nc("@menu", "Popular Folders"), this);
    actions->addAction(QStringLiteral("popular_folders"), m_popularFoldersMenu->menuAction());
    connect(m_popularFoldersMenu, &QMenu::aboutToShow, this, &MainWindow::populatePopularFolders);

    QAction *editUrl = actions->addAction(QStringLiteral("edit_location"), this, [this] {
        activePane()->editUrl();
    });
    editUrl->setText(i18nc("@action", "Edit Location"));
    editUrl->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
    actions->setDefaultShortcut(editUrl, Qt::CTRL | Qt::Key_L);

    QAction *switchPane = actions->addAction(QStringLiteral("switch_pane"), this, [this] {
        setActivePane(inactivePane());
    });
    switchPane->setText(i18nc("@action", "Switch Pane"));
    actions->setDefaultShortcut(switchPane, Qt::Key_Tab);

    // Tabs
    QAction *newTab = actions->addAction(QStringLiteral("new_tab"), this, [this] {
        activePane()->newTab();
    });
    newTab->setText(i18nc("@action", "New Tab"));
    newTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    actions->setDefaultShortcut(newTab, Qt::CTRL | Qt::Key_T);

    QAction *closeTab = actions->addAction(QStringLiteral("close_tab"), this, [this] {
        activePane()->closeCurrentTab();
    });
    closeTab->setText(i18nc("@action", "Close Tab"));
    closeTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    actions->setDefaultShortcut(closeTab, Qt::CTRL | Qt::Key_W);

    QAction *reopenTab = actions->addAction(QStringLiteral("reopen_closed_tab"), this, [this] {
        activePane()->tabs()->reopenClosedTab();
    });
    reopenTab->setText(i18nc("@action", "Reopen Closed Tab"));
    reopenTab->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    actions->setDefaultShortcut(reopenTab, Qt::CTRL | Qt::SHIFT | Qt::Key_T);

    QAction *nextTab = actions->addAction(QStringLiteral("next_tab"), this, [this] {
        activePane()->tabs()->activateNext();
    });
    nextTab->setText(i18nc("@action", "Next Tab"));
    actions->setDefaultShortcut(nextTab, Qt::CTRL | Qt::Key_Tab);

    QAction *previousTab = actions->addAction(QStringLiteral("previous_tab"), this, [this] {
        activePane()->tabs()->activatePrevious();
    });
    previousTab->setText(i18nc("@action", "Previous Tab"));
    actions->setDefaultShortcut(previousTab, Qt::CTRL | Qt::SHIFT | Qt::Key_Tab);

    for (int i = 1; i <= 9; ++i) {
        QAction *tabN = actions->addAction(QStringLiteral("activate_tab_%1").arg(i), this, [this, i] {
            activePane()->tabs()->setCurrentIndex(i - 1);
        });
        tabN->setText(i18nc("@action %1 is a tab number", "Activate Tab %1", i));
        actions->setDefaultShortcut(tabN, QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + i)));
    }

    QAction *compressAction = actions->addAction(QStringLiteral("compress"), this, &MainWindow::compress);
    compressAction->setText(i18nc("@action", "Compress…"));
    compressAction->setIcon(QIcon::fromTheme(QStringLiteral("archive-insert")));

    QAction *extract = actions->addAction(QStringLiteral("extract_to_other_pane"), this, &MainWindow::extractToOtherPane);
    extract->setText(i18nc("@action", "Extract to Other Pane"));
    extract->setIcon(QIcon::fromTheme(QStringLiteral("archive-extract")));

    QAction *extractProtected = actions->addAction(QStringLiteral("extract_protected_archive"), this, &MainWindow::extractProtectedToOtherPane);
    extractProtected->setText(i18nc("@action", "Extract Password-Protected Archive to Other Pane…"));
    extractProtected->setIcon(QIcon::fromTheme(QStringLiteral("archive-extract")));

    QAction *testArchive = actions->addAction(QStringLiteral("test_archive_integrity"), this, &MainWindow::testArchiveIntegrity);
    testArchive->setText(i18nc("@action", "Test Archive Integrity…"));
    testArchive->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));

    m_flatAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-list-text")), i18nc("@action", "Show Flat (No Folders)"), this);
    actions->addAction(QStringLiteral("show_flat"), m_flatAction);
    actions->setDefaultShortcut(m_flatAction, Qt::CTRL | Qt::SHIFT | Qt::Key_F);
    connect(m_flatAction, &KToggleAction::triggered, this, &MainWindow::setFlat);

    QAction *filter = actions->addAction(QStringLiteral("show_filter_bar"), this, [this] {
        activePane()->showFilterBar();
    });
    filter->setText(i18nc("@action", "Filter…"));
    filter->setIcon(QIcon::fromTheme(QStringLiteral("view-filter")));
    actions->setDefaultShortcut(filter, Qt::CTRL | Qt::Key_F);

    QAction *addFavorite = actions->addAction(QStringLiteral("add_to_favorites"), this, &MainWindow::addToFavorites);
    addFavorite->setText(i18nc("@action", "Add to Favorites"));
    addFavorite->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    actions->setDefaultShortcut(addFavorite, Qt::CTRL | Qt::Key_D);

    m_panelLayoutAction = new KSelectAction(QIcon::fromTheme(QStringLiteral("sidebar-show")), i18nc("@action", "Panels"), this);
    m_panelLayoutAction->setItems({i18nc("@item:inmenu panel layout", "Combined Panel"), i18nc("@item:inmenu panel layout", "Separate Places and Folders")});
    actions->addAction(QStringLiteral("panel_layout"), m_panelLayoutAction);
    connect(m_panelLayoutAction, &KSelectAction::indexTriggered, this, [this](int index) {
        setPanelLayout(index == 0 ? PanelLayout::Combined : PanelLayout::Separate);
    });

    // Settings
    KStandardAction::preferences(this, &MainWindow::showSettings, actions);
    QAction *showMenuBar = KStandardAction::showMenubar(menuBar(), &QMenuBar::setVisible, actions);
    KHamburgerMenu *hamburger = KStandardAction::hamburgerMenu(nullptr, nullptr, actions);
    hamburger->setMenuBar(menuBar());
    hamburger->setShowMenuBarAction(showMenuBar);

    m_copyToMenu = new KFileCopyToMenu(this);
    m_fileItemActions = new KFileItemActions(this);
    m_fileItemActions->setParentWidget(this);
}

Pane *MainWindow::activePane() const
{
    return m_active ? m_active : m_left;
}

Pane *MainWindow::inactivePane() const
{
    return activePane() == m_left ? m_right : m_left;
}

void MainWindow::setActivePane(Pane *pane)
{
    if (!pane || pane == m_active) {
        return;
    }
    if (m_active) {
        m_active->setActive(false);
    }
    m_active = pane;
    m_active->setActive(true);
    if (m_terminalPanelAction && m_terminalPanelAction->isChecked() && Settings::terminalFollowActiveFolder()) {
        m_terminalPanel->follow(pane->url());
    }
    if (m_dualLayoutAction) {
        toggleLayout(m_dualLayoutAction->isChecked());
    }
    m_hiddenFilesAction->setChecked(pane->view()->showHiddenFiles());
    syncFolderTrees();
    updateActions();
    updateStatusBar();
    updateFreeSpace();
    updateWindowTitle();
    updatePreviewPanel();
    updateInformationPanel();
}

void MainWindow::openUrls(const QList<QUrl> &urls)
{
    const bool startup = !m_startupLinkAttempted;
    m_startupLinkAttempted = true;
    if (urls.isEmpty()) {
        if (!m_sessionRestored) {
            restoreSession();
        }
        if (startup && Settings::linkPanesAtStartup()) {
            m_linkedPanesAction->setChecked(true);
        }
        return;
    }
    if (!m_sessionRestored) {
        restoreSession();
    }
    // Like `mc a b`: the first path goes to the active pane, the second to
    // the other pane, anything further becomes extra tabs in the active one.
    Pane *pane = activePane();
    Pane *other = inactivePane();
    for (int i = 0; i < urls.size(); ++i) {
        if (i == 1) {
            other->openInNewTab(urls.at(i), true);
        } else {
            pane->openInNewTab(urls.at(i), true);
        }
    }
    if (startup && Settings::linkPanesAtStartup()) {
        m_linkedPanesAction->setChecked(true);
    }
}

void MainWindow::restoreSession()
{
    m_sessionRestored = true;
    const KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(SessionGroup));
    const WindowSession session = Startup::resolve(Session::load(group), Startup::fromSettings());

    m_left->restoreState(session.left);
    m_right->restoreState(session.right);
    setActivePane(session.activePane == 1 ? m_right : m_left);
    m_dualLayoutAction->setChecked(session.dual);
    setStackedLayout(session.stacked);
}

void MainWindow::saveSession()
{
    WindowSession session;
    session.left = m_left->saveState();
    session.right = m_right->saveState();
    session.activePane = activePane() == m_right ? 1 : 0;
    session.dual = isDualLayout();
    session.stacked = isStackedLayout();

    KSharedConfig::Ptr state = KSharedConfig::openStateConfig();
    KConfigGroup group = state->group(QLatin1String(SessionGroup));
    Session::save(session, group);
    KConfigGroup panels = state->group(QLatin1String(PanelsGroup));
    panels.writeEntry("FoldersExpanded", m_folderTree->expandedUrls());
    panels.writeEntry("CombinedExpanded", m_combinedPanel->folderTree()->expandedUrls());
    panels.writeEntry("CollapsedSections", m_combinedPanel->collapsedSections());
    state->group(QLatin1String(WindowLayoutGroup)).writeEntry("DualPane", m_dualLayoutAction->isChecked());
    state->sync();
}

bool MainWindow::queryClose()
{
    const auto transfers = FileOperations::transferQueue()->entries();
    const bool transfersPending = std::any_of(transfers.cbegin(), transfers.cend(), [](const QueuedTransfer *transfer) {
        return transfer->state() == QueuedTransfer::State::Waiting || transfer->state() == QueuedTransfer::State::Running
            || transfer->state() == QueuedTransfer::State::Paused;
    });
    if (transfersPending
        && KMessageBox::warningContinueCancel(this,
                                              i18nc("@info",
                                                    "Transfers are still queued or running. Closing the application may cancel active transfers "
                                                    "and discards waiting transfers. Close this window anyway?"),
                                              i18nc("@title:window", "Transfers in Progress"),
                                              KGuiItem(i18nc("@action:button", "Close Window"), QStringLiteral("window-close")))
            != KMessageBox::Continue) {
        return false;
    }
    const bool severalTabs = m_left->tabs()->count() > 1 || m_right->tabs()->count() > 1;
    if (Settings::confirmClosingMultipleTabs() && severalTabs
        && KMessageBox::warningContinueCancel(this,
                                              i18nc("@info", "This window has several tabs open. Close them all?"),
                                              i18nc("@title:window", "Close Tabs"),
                                              KGuiItem(i18nc("@action:button", "Close Window"), QStringLiteral("window-close")))
            != KMessageBox::Continue) {
        return false;
    }
    if (m_editor && !m_editor->queryClose()) {
        return false;
    }
    const auto galleries = findChildren<GalleryWindow *>(QString(), Qt::FindDirectChildrenOnly);
    for (GalleryWindow *gallery : galleries) {
        if (!gallery->close()) {
            return false;
        }
    }
    saveSession();
    return KXmlGuiWindow::queryClose();
}

void MainWindow::readProperties(const KConfigGroup &group)
{
    const WindowSession session = Session::load(group).sanitized(QUrl::fromLocalFile(QDir::homePath()));
    m_sessionRestored = true;
    m_left->restoreState(session.left);
    m_right->restoreState(session.right);
    setActivePane(session.activePane == 1 ? m_right : m_left);
    m_dualLayoutAction->setChecked(session.dual);
    setStackedLayout(session.stacked);
}

void MainWindow::saveProperties(KConfigGroup &group)
{
    WindowSession session;
    session.left = m_left->saveState();
    session.right = m_right->saveState();
    session.activePane = activePane() == m_right ? 1 : 0;
    session.dual = isDualLayout();
    session.stacked = isStackedLayout();
    Session::save(session, group);
}

void MainWindow::updateActions()
{
    KActionCollection *actions = actionCollection();
    FolderView *view = activePane()->view();
    const KFileItemList selected = view->selectedItems();
    const bool hasSelection = !selected.isEmpty();
    const KFileItemListProperties props(selected);
    const KFileItem root = view->rootItem();

    const bool bothVisible = isDualLayout() && !m_left->isHidden() && !m_right->isHidden();
    auto *tabPosition = qobject_cast<KSelectAction *>(actions->action(QStringLiteral("tab_bar_position")));
    tabPosition->setCurrentItem(static_cast<int>(activePane()->tabBarPosition()));
    actions->action(QStringLiteral("stacked_tab_bars_together"))->setEnabled(isStackedLayout());
    actions->action(QStringLiteral("swap_current_tabs"))
        ->setEnabled(bothVisible && m_left->tabs()->currentIndex() >= 0 && m_right->tabs()->currentIndex() >= 0);
    actions->action(QStringLiteral("swap_pane_sides"))->setEnabled(bothVisible);
    actions->action(QStringLiteral("copy_to_other_pane"))->setEnabled(hasSelection);
    actions->action(QStringLiteral("move_to_other_pane"))->setEnabled(hasSelection && props.supportsMoving());
    actions->action(QStringLiteral("rename"))->setEnabled(hasSelection && props.supportsMoving());
    actions->action(QStringLiteral("batch_rename"))
        ->setEnabled(hasSelection && props.supportsMoving() && std::all_of(selected.cbegin(), selected.cend(), [](const KFileItem &item) {
                         return item.url().isLocalFile();
                     }));
    actions->action(QStringLiteral("create_symlink"))->setEnabled(selected.size() == 1 && selected.first().url().isLocalFile());
    actions->action(QStringLiteral("move_to_trash"))->setEnabled(hasSelection && props.supportsDeleting() && props.isLocal());
    actions->action(QStringLiteral("delete"))->setEnabled(hasSelection && props.supportsDeleting());
    actions->action(stdName(KStandardAction::Cut))->setEnabled(hasSelection && props.supportsMoving());
    actions->action(stdName(KStandardAction::Copy))->setEnabled(hasSelection);
    actions->action(QStringLiteral("open_terminal"))->setEnabled(activePane()->url().isLocalFile());
    const bool commandEnabled = activePane()->url().isLocalFile() && std::all_of(selected.cbegin(), selected.cend(), [](const KFileItem &item) {
                                    return item.url().isLocalFile();
                                });
    actions->action(QStringLiteral("custom_command"))->setEnabled(commandEnabled);
    for (QAction *action : std::as_const(m_customCommandActions)) {
        action->setEnabled(commandEnabled);
    }
    actions->action(QStringLiteral("content_chart"))->setEnabled(activePane()->url().isValid() && !activePane()->url().isEmpty());
    actions->action(QStringLiteral("find_duplicates"))->setEnabled(activePane()->url().isLocalFile());
    actions->action(QStringLiteral("compare_pane_folders"))
        ->setEnabled(isDualLayout() && !inactivePane()->isHidden() && activePane()->url().isValid() && !activePane()->url().isEmpty()
                     && inactivePane()->url().isValid() && !inactivePane()->url().isEmpty());
    actions->action(QStringLiteral("compare_folders_filtered"))
        ->setEnabled(isDualLayout() && !inactivePane()->isHidden() && activePane()->url().isLocalFile() && inactivePane()->url().isLocalFile());
    const bool localFiles = hasSelection && std::all_of(selected.cbegin(), selected.cend(), [](const KFileItem &item) {
                                return item.url().isLocalFile() && item.isFile() && !item.isLink();
                            });
    actions->action(QStringLiteral("create_checksum"))->setEnabled(localFiles);
    actions->action(QStringLiteral("verify_checksum"))->setEnabled(localFiles && selected.size() == 1);
    actions->action(QStringLiteral("edit_metadata"))->setEnabled(localFiles && selected.size() == 1);
    actions->action(QStringLiteral("split_file"))->setEnabled(localFiles && selected.size() == 1);
    actions->action(QStringLiteral("combine_files"))->setEnabled(localFiles);
    actions->action(QStringLiteral("compare_files"))->setEnabled(!comparisonFiles(activePane(), inactivePane(), isDualLayout()).isEmpty());
    const bool hasMedia = std::any_of(selected.cbegin(), selected.cend(), [](const KFileItem &item) {
        if (item.isDir() || !item.url().isLocalFile()) {
            return false;
        }
        const QString mime = item.mimetype();
        return mime.startsWith(QLatin1String("image/")) || mime.startsWith(QLatin1String("video/"));
    });
    actions->action(QStringLiteral("open_gallery"))->setEnabled(hasMedia);
    actions->action(QStringLiteral("test_archive_integrity"))->setEnabled(std::any_of(selected.cbegin(), selected.cend(), [](const KFileItem &item) {
        return !item.isDir() && Archives::browseUrl(item.mostLocalUrl(), item.mimetype()).isValid();
    }));
    actions->action(QStringLiteral("extract_protected_archive"))->setEnabled(std::any_of(selected.cbegin(), selected.cend(), [](const KFileItem &item) {
        return !item.localPath().isEmpty() && !item.isDir() && Archives::browseUrl(item.mostLocalUrl(), item.mimetype()).isValid();
    }));

    bool pasteEnabled = false;
    const QString pasteText = FileOperations::pasteActionText(root, &pasteEnabled);
    const bool flat = view->isFlat();
    m_pasteAction->setEnabled(pasteEnabled && !root.isNull() && root.isWritable() && !flat);
    m_flatAction->setChecked(flat);
    const QString flatReason = flat ? i18nc("@info:tooltip", "Not available in a flat list, whose items come from many folders") : QString();
    for (const QString &name : {QStringLiteral("new_menu"), QStringLiteral("new_folder")}) {
        QAction *create = actions->action(name);
        create->setEnabled(!flat);
        create->setToolTip(flat ? flatReason : create->text());
    }
    if (!pasteText.isEmpty()) {
        m_pasteAction->setText(pasteText);
    }

    actions->action(stdName(KStandardAction::Back))->setEnabled(activePane()->tabs()->canGoBack());
    actions->action(stdName(KStandardAction::Forward))->setEnabled(activePane()->tabs()->canGoForward());
    const TabModel *tabs = activePane()->tabs();
    actions->action(QStringLiteral("close_tab"))
        ->setEnabled(tabs->count() > 1 && !tabs->isPinned(tabs->currentIndex()) && !tabs->isLocked(tabs->currentIndex()));

    m_selectionModeAction->setChecked(view->selectionMode());
    m_selectionMarkersAction->setChecked(view->selectionMarkersVisible());
    actions->action(QStringLiteral("folder_format_source"))->setText(view->formatRuleDescription());
    actions->action(QStringLiteral("open_folder_format_source"))
        ->setEnabled(!flat && !view->formatLocked() && !view->formatRuleSource().isEmpty()
                     && QUrl::fromLocalFile(view->formatRuleSource()).adjusted(QUrl::StripTrailingSlash) != view->url().adjusted(QUrl::StripTrailingSlash));
    actions->action(QStringLiteral("clear_folder_format"))->setEnabled(!flat && !view->formatLocked() && view->hasLocalFolderFormat());
    actions->action(QStringLiteral("lock_folder_format"))->setChecked(view->formatLocked());
    QAction *inheritFormat = actions->action(QStringLiteral("inherit_folder_format"));
    inheritFormat->setChecked(view->formatIncludesDescendants());
    inheritFormat->setEnabled(view->url().isLocalFile() && !view->formatLocked() && !flat);
    QAction *automaticSizes = actions->action(QStringLiteral("automatic_folder_sizes"));
    automaticSizes->setChecked(view->automaticFolderSizes());
    automaticSizes->setEnabled(view->url().isLocalFile() && !view->formatLocked() && !flat);
    actions->action(QStringLiteral("default_folder_size_policy"))->setEnabled(view->url().isLocalFile() && !view->formatLocked() && !flat);
    actions->action(QStringLiteral("calculate_folder_sizes"))->setEnabled(!flat && !view->rootItem().isNull());
    actions->action(QStringLiteral("automatic_local_folder_sizes"))->setChecked(Settings::automaticLocalFolderSizes());
    actions->action(QStringLiteral("automatic_remote_folder_sizes"))->setChecked(Settings::automaticRemoteFolderSizes());
    const bool details = view->viewMode() == FolderView::ViewMode::Details;
    m_viewModeAction->setText(details ? i18nc("@action", "Icons View") : i18nc("@action", "Details View"));
    m_viewModeAction->setIcon(QIcon::fromTheme(details ? QStringLiteral("view-list-icons") : QStringLiteral("view-list-details")));
    actions->action(QStringLiteral("calculate_occupied_space"))->setEnabled(!view->rootItem().isNull());
    m_jumpBackAction->setEnabled(m_jumpBackPoints.value(activePane()).isValid());
}

void MainWindow::updateStatusBar()
{
    const bool perPane = Settings::statusBarLayout() == Settings::EnumStatusBarLayout::PerPane;
    const auto itemText = [](Pane *pane) {
        const KFileItemList selected = pane->view()->selectedItems();
        const int total = pane->view()->itemCount();
        if (selected.isEmpty()) {
            return i18ncp("@info:status", "%1 item", "%1 items", total);
        }
        KIO::filesize_t size = 0;
        int folders = 0;
        for (const KFileItem &item : selected) {
            if (item.isDir()) {
                ++folders;
            } else {
                size += item.size();
            }
        }
        QString text = i18ncp("@info:status", "%1 of %2 selected", "%1 of %2 selected", selected.size(), total);
        if (selected.size() > folders && Settings::showSelectionSize()) {
            text += QStringLiteral(" (%1)").arg(KIO::convertSize(size));
        }
        return text;
    };
    for (Pane *pane : {m_left, m_right}) {
        const bool showCounts = Settings::showItemCounts();
        const bool showFreeSpace = Settings::showFreeSpace();
        m_paneItemsLabels.value(pane)->setText(itemText(pane));
        m_paneItemsLabels.value(pane)->setVisible(perPane && showCounts);
        m_paneFreeSpaceLabels.value(pane)->setVisible(perPane && showFreeSpace);
        pane->statusBarWidget()->setVisible(perPane && (showCounts || showFreeSpace));
    }
    m_itemsLabel->setText(itemText(activePane()));
    m_itemsLabel->setVisible(!perPane && Settings::showItemCounts());
    m_freeSpaceLabel->setVisible(!perPane && Settings::showFreeSpace());

    const KFileItemList selected = activePane()->view()->selectedItems();
    if (selected.size() == 1) {
        const KFileItem &item = selected.first();
        QStringList parts{item.text(), item.mimeComment()};
        if (!item.isDir()) {
            parts.append(KIO::convertSize(item.size()));
        }
        parts.append(item.timeString(KFileItem::ModificationTime));
        parts.append(item.permissionsString());
        if (item.isLink()) {
            parts.append(i18nc("@info:status symlink target", "→ %1", item.linkDest()));
        }
        m_messageLabel->setText(parts.join(QStringLiteral("  ·  ")));
    } else {
        m_messageLabel->clear();
    }
}

void MainWindow::updateFreeSpace(Pane *pane)
{
    const bool perPane = Settings::statusBarLayout() == Settings::EnumStatusBarLayout::PerPane;
    const QList<Pane *> panes = pane ? QList<Pane *>{pane} : (perPane ? QList<Pane *>{m_left, m_right} : QList<Pane *>{activePane()});
    for (Pane *target : panes) {
        QLabel *label = m_paneFreeSpaceLabels.value(target);
        const QUrl url = target->url();
        label->clear();
        if (target == activePane()) {
            m_freeSpaceLabel->clear();
        }
        if (!url.isValid() || !Settings::showFreeSpace()) {
            continue;
        }
        KIO::FileSystemFreeSpaceJob *job = KIO::fileSystemFreeSpace(url);
        connect(job, &KJob::result, this, [this, job, target, url] {
            if (url != target->url()) {
                return;
            }
            if (job->error()) {
                m_paneFreeSpaceLabels.value(target)->clear();
                if (target == activePane()) {
                    m_freeSpaceLabel->clear();
                }
                return;
            }
            const QString text = i18nc("@info:status free disk space", "%1 free of %2", KIO::convertSize(job->availableSize()), KIO::convertSize(job->size()));
            m_paneFreeSpaceLabels.value(target)->setText(text);
            if (target == activePane()) {
                m_freeSpaceLabel->setText(text);
            }
        });
    }
}

void MainWindow::updateWindowTitle()
{
    setCaption(activePane()->url().toDisplayString(QUrl::PreferLocalFile));
}

void MainWindow::updateTabBars()
{
    const bool force = Settings::alwaysShowTabBar() || m_left->tabs()->count() > 1 || m_right->tabs()->count() > 1;
    m_left->setTabBarForcedVisible(force);
    m_right->setTabBarForcedVisible(force);
}

void MainWindow::showStatusMessage(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::recordFolderVisit(Pane *pane, const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        return;
    }
    const QUrl normalized = url.adjusted(QUrl::StripTrailingSlash);
    const QString key = normalized.toString(QUrl::FullyEncoded);
    const QUrl previous = m_lastFolderByPane.value(pane);
    if (!m_jumpingBack && previous.isValid() && previous != normalized) {
        m_jumpBackPoints.insert(pane, previous);
    }
    m_lastFolderByPane.insert(pane, normalized);
    m_popularFolderVisits.insert(key, m_popularFolderVisits.value(key) + 1);
    savePopularFolderVisits();
    if (m_jumpBackAction) {
        m_jumpBackAction->setEnabled(m_jumpBackPoints.value(pane).isValid());
    }
}

void MainWindow::savePopularFolderVisits()
{
    QList<QPair<QString, int>> entries;
    entries.reserve(m_popularFolderVisits.size());
    for (auto it = m_popularFolderVisits.cbegin(); it != m_popularFolderVisits.cend(); ++it) {
        entries.append({it.key(), it.value()});
    }
    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    if (entries.size() > 100) {
        entries.resize(100);
        m_popularFolderVisits.clear();
        for (const auto &[url, visits] : entries) {
            m_popularFolderVisits.insert(url, visits);
        }
    }
    QStringList urls;
    QList<int> visits;
    for (const auto &[url, count] : entries) {
        urls.append(url);
        visits.append(count);
    }
    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(PopularFoldersGroup));
    group.writeEntry("Urls", urls);
    group.writeEntry("Visits", visits);
    group.sync();
}

void MainWindow::populatePopularFolders()
{
    m_popularFoldersMenu->clear();
    QList<QPair<QString, int>> entries;
    entries.reserve(m_popularFolderVisits.size());
    for (auto it = m_popularFolderVisits.cbegin(); it != m_popularFolderVisits.cend(); ++it) {
        entries.append({it.key(), it.value()});
    }
    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    if (entries.isEmpty()) {
        m_popularFoldersMenu->addAction(i18nc("@info:menu", "No visited folders"))->setEnabled(false);
        return;
    }
    for (qsizetype i = 0; i < qMin<qsizetype>(entries.size(), 12); ++i) {
        const QUrl url(entries.at(i).first, QUrl::StrictMode);
        const QString label = url.fileName().isEmpty() ? url.toDisplayString(QUrl::PreferLocalFile) : url.fileName();
        QAction *action = m_popularFoldersMenu->addAction(label);
        action->setToolTip(url.toDisplayString(QUrl::PreferLocalFile));
        connect(action, &QAction::triggered, this, [this, url] {
            activePane()->setUrl(url);
        });
    }
}

void MainWindow::populateSelectionFilters()
{
    m_selectionFiltersMenu->clear();
    const QStringList patterns = KSharedConfig::openConfig()->group(QLatin1String(SelectionFiltersGroup)).readEntry("Patterns", QStringList());
    if (patterns.isEmpty()) {
        m_selectionFiltersMenu->addAction(i18nc("@info:menu", "No saved filters"))->setEnabled(false);
        return;
    }
    for (const QString &pattern : patterns) {
        QAction *action = m_selectionFiltersMenu->addAction(pattern);
        connect(action, &QAction::triggered, this, [this, pattern] {
            applySelectionFilter(pattern);
        });
    }
}

void MainWindow::applySelectionFilter(const QString &pattern)
{
    const int matched = activePane()->view()->applySelectionPattern(pattern, true);
    showStatusMessage(i18ncp("@info:status selected items by pattern", "Selected %1 item", "Selected %1 items", matched));
}

void MainWindow::selectByPattern(bool select)
{
    bool accepted = false;
    const QString pattern = QInputDialog::getText(this,
                                                  select ? i18nc("@title:window", "Select by Pattern") : i18nc("@title:window", "Deselect by Pattern"),
                                                  i18nc("@label:textbox", "File name pattern:"),
                                                  QLineEdit::Normal,
                                                  activePane()->view()->nameFilter().text(),
                                                  &accepted)
                                .trimmed();
    if (!accepted || pattern.isEmpty()) {
        return;
    }
    const int matched = activePane()->view()->applySelectionPattern(pattern, select);
    showStatusMessage(i18ncp(select ? "@info:status selected items by pattern" : "@info:status deselected items by pattern",
                             select ? "Selected %1 item" : "Deselected %1 item",
                             select ? "Selected %1 items" : "Deselected %1 items",
                             matched));
}

void MainWindow::selectByPathFilter(bool select)
{
    const QPointer<FolderView> view = activePane()->view();
    if (!view->url().isLocalFile()) {
        showStatusMessage(i18nc("@info", "Path selection filters support local folders only."));
        return;
    }
    const QUrl folder = view->url();
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("selectionPathDialog"));
    dialog.setWindowTitle(select ? i18nc("@title:window", "Select by Path Filter") : i18nc("@title:window", "Deselect by Path Filter"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(i18nc("@info", "Applies to the current top-level listing, or all shown items in flat view."), &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *editor = new PathFilterEditor(QStringLiteral("selection"), &dialog);
    layout->addWidget(editor);
    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("selectionPathError"));
    error->setTextFormat(Qt::PlainText);
    error->setWordWrap(true);
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (!view || view->url() != folder || view->isLoading()) {
            error->setText(i18nc("@info", "The folder changed or is still loading. Close this dialog and try again."));
            return;
        }
        QString message;
        const int matched = view->applySelectionPathFilter(editor->filter(), select, &message);
        if (matched < 0) {
            error->setText(message);
            return;
        }
        showStatusMessage(i18ncp(select ? "@info:status selected items by path" : "@info:status deselected items by path",
                                 select ? "Selected %1 item" : "Deselected %1 item",
                                 select ? "Selected %1 items" : "Deselected %1 items",
                                 matched));
        dialog.accept();
    });
    dialog.exec();
}

void MainWindow::saveSelectionFilter()
{
    bool accepted = false;
    const QString pattern = QInputDialog::getText(this,
                                                  i18nc("@title:window", "Save Selection Filter"),
                                                  i18nc("@label:textbox", "File name pattern:"),
                                                  QLineEdit::Normal,
                                                  activePane()->view()->nameFilter().text(),
                                                  &accepted)
                                .trimmed();
    if (!accepted || pattern.isEmpty()) {
        return;
    }
    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(SelectionFiltersGroup));
    QStringList patterns = group.readEntry("Patterns", QStringList());
    if (!patterns.contains(pattern)) {
        patterns.append(pattern);
        group.writeEntry("Patterns", patterns);
        group.sync();
    }
    populateSelectionFilters();
    showStatusMessage(i18nc("@info:status", "Saved selection filter %1", pattern));
}

void MainWindow::calculateOccupiedSpace()
{
    const KFileItemList items = activePane()->view()->selectedItemsOrRoot();
    if (items.isEmpty()) {
        return;
    }
    showStatusMessage(i18nc("@info:status", "Calculating occupied space…"));
    KIO::DirectorySizeJob *job = KIO::directorySize(items);
    KJobWidgets::setWindow(job, this);
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            showStatusMessage(job->errorString());
            return;
        }
        showStatusMessage(i18nc("@info:status occupied size",
                                "%1 occupied in %2 and %3",
                                KIO::convertSize(job->totalSize()),
                                i18ncp("@info:status", "%1 file", "%1 files", job->totalFiles()),
                                i18ncp("@info:status", "%1 folder", "%1 folders", job->totalSubdirs())));
    });
}

void MainWindow::testArchiveIntegrity()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    for (const KFileItem &item : items) {
        if (Archives::browseUrl(item.mostLocalUrl(), item.mimetype()).isEmpty()) {
            continue;
        }
        if (item.localPath().isEmpty()) {
            showStatusMessage(i18nc("@info:status archive integrity", "Integrity testing requires a local archive."));
            return;
        }
        testArchiveFile(item.localPath());
    }
}

void MainWindow::testArchiveFile(const QString &fileName, const QString &password)
{
    using TestReport = QPair<Archives::TestResult, QString>;
    auto *watcher = new QFutureWatcher<TestReport>(this);
    connect(watcher, &QFutureWatcher<TestReport>::finished, this, [this, watcher, fileName, password] {
        const TestReport report = watcher->result();
        watcher->deleteLater();
        if (report.first == Archives::TestResult::PasswordRequired) {
            bool accepted = false;
            const QString entered = QInputDialog::getText(this,
                                                          i18nc("@title:window", "Archive Password"),
                                                          i18nc("@label:textbox", "Password for %1:", QFileInfo(fileName).fileName()),
                                                          QLineEdit::Password,
                                                          QString(),
                                                          &accepted);
            if (!accepted) {
                showStatusMessage(i18nc("@info:status archive integrity", "Archive test canceled."));
                return;
            }
            testArchiveFile(fileName, entered);
            return;
        }
        if (report.first == Archives::TestResult::Passed) {
            showStatusMessage(i18nc("@info:status archive integrity", "Archive is intact: %1", QFileInfo(fileName).fileName()));
        } else {
            showStatusMessage(i18nc("@info:status archive integrity", "Archive test failed for %1: %2", QFileInfo(fileName).fileName(), report.second));
        }
    });
    watcher->setFuture(QtConcurrent::run([fileName, password] {
        QString error;
        const Archives::TestResult result = Archives::testIntegrity(fileName, password, &error);
        return TestReport{result, error};
    }));
}

void MainWindow::jumpBack()
{
    Pane *pane = activePane();
    const QUrl target = m_jumpBackPoints.value(pane);
    if (!target.isValid()) {
        return;
    }
    const QUrl current = pane->url();
    m_jumpBackPoints.insert(pane, current);
    m_jumpingBack = true;
    pane->setUrl(target);
    m_jumpingBack = false;
    m_jumpBackAction->setEnabled(m_jumpBackPoints.value(pane).isValid());
}

void MainWindow::activateItem(const KFileItem &item)
{
    const QUrl archive = item.isDir() ? QUrl() : Archives::browseUrl(item.mostLocalUrl(), item.mimetype());
    if (item.isDir()) {
        activePane()->setUrl(item.targetUrl());
    } else if (archive.isValid()) {
        activePane()->setUrl(archive);
    } else {
        FileOperations::open(item, this);
    }
}

void MainWindow::activatePaneItem(Pane *pane, const KFileItem &item)
{
    setActivePane(pane);
    const QUrl target = item.targetUrl();
    const bool executable = target.isLocalFile() && Executables::isProgram(item.mimetype(), QFileInfo(target.toLocalFile()).isExecutable());
    const bool archive = !item.isDir() && Archives::browseUrl(item.mostLocalUrl(), item.mimetype()).isValid();
    if (item.isDir() || executable || archive) {
        activateItem(item);
        return;
    }
    const auto destination = Settings::fileActivationDestination();
    const QString mime = item.mimetype();
    if (destination == Settings::EnumFileActivationDestination::Gallery && item.url().isLocalFile()
        && (mime.startsWith(QLatin1String("image/")) || mime.startsWith(QLatin1String("video/")))) {
        pane->view()->selectUrls({item.url()});
        openGallery();
    } else if (destination == Settings::EnumFileActivationDestination::Preview) {
        pane->view()->selectUrls({item.url()});
        if (!m_previewPanelAction->isChecked()) {
            m_previewPanelAction->setChecked(true);
        } else {
            updatePreviewPanel();
        }
    } else {
        activateItem(item);
    }
}

void MainWindow::setupEditor()
{
    m_editor = new EditorPanel(m_splitter);
    m_splitter->addWidget(m_editor);
    m_editor->hide();
    m_editorDock = new QDockWidget(i18nc("@title:window", "Editor"), this);
    m_editorDock->setObjectName(QStringLiteral("editorDock"));
    m_editorDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_editorDock);
    m_editorDock->hide();
    m_editorWindow = new QDialog(this, Qt::Window);
    m_editorWindow->setObjectName(QStringLiteral("editorWindow"));
    m_editorWindow->setWindowTitle(i18nc("@title:window", "Editor"));
    m_editorWindow->resize(900, 650);
    new QVBoxLayout(m_editorWindow);
    connect(m_editor, &EditorPanel::placementRequested, this, &MainWindow::setEditorPlacement);
    connect(m_editor, &EditorPanel::hideRequested, this, [this] {
        switch (m_editorPlacement) {
        case EditorPanel::Placement::Panel:
            m_editor->hide();
            break;
        case EditorPanel::Placement::Below:
        case EditorPanel::Placement::Right:
            m_editorDock->hide();
            break;
        case EditorPanel::Placement::Maximized:
        case EditorPanel::Placement::Window:
            m_editorWindow->hide();
            break;
        }
        toggleLayout(m_dualLayoutAction->isChecked());
    });
    connect(m_editorDock, &QDockWidget::visibilityChanged, this, [this] {
        toggleLayout(m_dualLayoutAction->isChecked());
    });
}

void MainWindow::setEditorPlacement(EditorPanel::Placement placement)
{
    m_editorPlacement = placement;
    m_editorWindow->hide();
    m_editorDock->hide();
    m_editor->setParent(nullptr);
    switch (placement) {
    case EditorPanel::Placement::Panel:
        m_splitter->addWidget(m_editor);
        m_editor->show();
        break;
    case EditorPanel::Placement::Below:
    case EditorPanel::Placement::Right:
        addDockWidget(placement == EditorPanel::Placement::Below ? Qt::BottomDockWidgetArea : Qt::RightDockWidgetArea, m_editorDock);
        m_editorDock->setWidget(m_editor);
        m_editorDock->show();
        m_editor->show();
        break;
    case EditorPanel::Placement::Maximized:
    case EditorPanel::Placement::Window:
        m_editorWindow->layout()->addWidget(m_editor);
        m_editor->show();
        if (placement == EditorPanel::Placement::Maximized) {
            m_editorWindow->showMaximized();
        } else {
            m_editorWindow->showNormal();
        }
        break;
    }
    toggleLayout(m_dualLayoutAction->isChecked());
    restoreSplitterSizes(true, false);
    restoreEditorDockSize();
}

void MainWindow::openEditor()
{
    if (!m_editor) {
        setupEditor();
    }
    const bool alreadyOpen = (m_editorPlacement == EditorPanel::Placement::Panel && !m_editor->isHidden())
        || ((m_editorPlacement == EditorPanel::Placement::Below || m_editorPlacement == EditorPanel::Placement::Right) && !m_editorDock->isHidden())
        || ((m_editorPlacement == EditorPanel::Placement::Maximized || m_editorPlacement == EditorPanel::Placement::Window) && !m_editorWindow->isHidden());
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.size() == 1 && !items.first().isDir() && !m_editor->openUrl(items.first().url())) {
        return;
    }
    if (alreadyOpen) {
        if (m_editorPlacement == EditorPanel::Placement::Maximized || m_editorPlacement == EditorPanel::Placement::Window) {
            if (m_editorWindow->isMinimized()) {
                if (m_editorPlacement == EditorPanel::Placement::Maximized) {
                    m_editorWindow->showMaximized();
                } else {
                    m_editorWindow->showNormal();
                }
            }
            m_editorWindow->raise();
            m_editorWindow->activateWindow();
        }
        return;
    }
    switch (Settings::editorOpening()) {
    case Settings::EnumEditorOpening::Embedded:
        setEditorPlacement(Settings::editorEmbeddedPlacement() == Settings::EnumEditorEmbeddedPlacement::Right ? EditorPanel::Placement::Right
                                                                                                               : EditorPanel::Placement::Below);
        break;
    case Settings::EnumEditorOpening::Maximized:
        setEditorPlacement(EditorPanel::Placement::Maximized);
        break;
    case Settings::EnumEditorOpening::Window:
        setEditorPlacement(EditorPanel::Placement::Window);
        break;
    default:
        setEditorPlacement(EditorPanel::Placement::Panel);
        break;
    }
}

void MainWindow::openGallery()
{
    FolderView *view = activePane()->view();
    const KFileItemList selected = view->selectedItems();
    QSet<QUrl> selectedUrls;
    for (const KFileItem &item : selected) {
        selectedUrls.insert(item.url());
    }
    QList<QUrl> urls;
    QSet<QUrl> videos;
    for (const KFileItem &item : view->itemsInViewOrder()) {
        if (item.isDir() || !item.url().isLocalFile() || (selected.size() > 1 && !selectedUrls.contains(item.url()))) {
            continue;
        }
        const QString mime = item.mimetype();
        const bool video = mime.startsWith(QLatin1String("video/"));
        if (video || mime.startsWith(QLatin1String("image/"))) {
            urls.append(item.url());
            if (video) {
                videos.insert(item.url());
            }
        }
    }
    if (urls.isEmpty()) {
        showStatusMessage(i18nc("@info:status", "No local images or videos are available for the gallery."));
        return;
    }
    int index = urls.indexOf(view->currentItem().url());
    if (index < 0 && !selected.isEmpty()) {
        index = urls.indexOf(selected.first().url());
    }
    auto *gallery = new GalleryWindow(urls, videos, std::max(index, 0), this);
    const QPointer<FolderView> source(view);
    const QUrl sourceFolder = view->url();
    connect(gallery, &GalleryWindow::currentUrlChanged, gallery, [source, sourceFolder](const QUrl &url) {
        if (source && source->url() == sourceFolder) {
            source->selectUrls({url});
        }
    });
    connect(gallery, &QDialog::finished, this, [this, source] {
        if (source && activePane()->view() == source) {
            source->currentView()->setFocus();
        }
    });
    gallery->show();
}

void MainWindow::showContextMenu(Pane *pane, const QPoint &globalPos, const KFileItemList &items)
{
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("fileContextMenu"));
    KActionCollection *actions = actionCollection();
    const KConfigGroup context = KSharedConfig::openConfig()->group(QStringLiteral("ContextMenu"));
    const QStringList hidden = Settings::hiddenContextActions();
    const auto visible = [&hidden](const char *id) {
        return !hidden.contains(QLatin1String(id));
    };

    if (items.isEmpty()) {
        if (visible("new_menu")) {
            m_newFileMenu->setWorkingDirectory(pane->url());
            m_newFileMenu->checkUpToDate();
            menu.addAction(m_newFileMenu);
        }
        menu.addSeparator();
        if (visible("paste")) {
            menu.addAction(m_pasteAction);
        }
        if (visible("new_tab") && context.readEntry("ShowOpenInNewTab", true)) {
            menu.addAction(actions->action(QStringLiteral("new_tab")));
        }
        menu.addSeparator();
        if (visible("refresh")) {
            menu.addAction(actions->action(stdName(KStandardAction::Redisplay)));
        }
        if (visible("terminal") && Settings::showOpenTerminal()) {
            menu.addAction(actions->action(QStringLiteral("open_terminal")));
        }
        menu.addAction(actions->action(QStringLiteral("calculate_occupied_space")));
        menu.addAction(actions->action(QStringLiteral("content_chart")));
        QAction *folderLook = nullptr;
        if (pane->url().isLocalFile()) {
            folderLook = menu.addAction(i18nc("@action:inmenu", "Folder Look…"));
        }
        if (visible("copy_path") && Settings::showCopyPath()) {
            menu.addAction(actions->action(QStringLiteral("copy_path")));
        }
        menu.addSeparator();
        if (visible("properties")) {
            menu.addAction(actions->action(QStringLiteral("properties")));
        }
        if (menu.exec(globalPos) == folderLook && folderLook) {
            editFolderLook(pane->url());
        }
        return;
    }

    const bool singleFolder = items.size() == 1 && items.first().isDir();
    const KFileItemListProperties props(items);

    QAction *openAction = nullptr;
    if (visible("open")) {
        openAction = menu.addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:inmenu", "Open"));
    }
    QAction *openInTab = nullptr;
    QAction *openInOtherPane = nullptr;
    if (singleFolder) {
        if (visible("open_in_tab") && Settings::showOpenInNewTab()) {
            openInTab = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"));
        }
        if (m_dualLayoutAction->isChecked() && visible("open_in_other_pane") && Settings::showOpenInOtherPane()) {
            openInOtherPane = menu.addAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Other Pane"));
        }
    }
    m_fileItemActions->setItemListProperties(props);
    if (visible("open_with")) {
        m_fileItemActions->insertOpenWithActionsTo(nullptr, &menu, QStringList());
    }

    menu.addSeparator();
    if (visible("cut")) {
        menu.addAction(actions->action(stdName(KStandardAction::Cut)));
    }
    if (visible("copy")) {
        menu.addAction(actions->action(stdName(KStandardAction::Copy)));
    }
    if (singleFolder && visible("paste")) {
        menu.addAction(m_pasteAction);
    }
    if (visible("copy_to")) {
        if (Settings::copyToSubmenu()) {
            addTransferMenu(&menu, items, false);
        } else if (Settings::showCopyToOtherPane()) {
            menu.addAction(actions->action(QStringLiteral("copy_to_other_pane")));
        }
    }
    if (visible("move_to")) {
        if (Settings::moveToSubmenu()) {
            addTransferMenu(&menu, items, true);
        } else if (Settings::showMoveToOtherPane()) {
            menu.addAction(actions->action(QStringLiteral("move_to_other_pane")));
        }
    }
    if (Settings::showCopyMoveToMenus()) {
        QList<QUrl> urls;
        urls.reserve(items.size());
        for (const KFileItem &item : items) {
            urls.append(item.url());
        }
        m_copyToMenu->setUrls(urls);
        m_copyToMenu->setReadOnly(!props.supportsMoving());
        m_copyToMenu->addActionsTo(&menu);
    }
    menu.addSeparator();
    if (visible("rename")) {
        menu.addAction(actions->action(QStringLiteral("rename")));
    }
    menu.addAction(actions->action(QStringLiteral("batch_rename")));
    menu.addAction(actions->action(QStringLiteral("create_symlink")));
    if (visible("trash")) {
        menu.addAction(actions->action(QStringLiteral("move_to_trash")));
    }
    if (visible("delete") && Settings::showDelete()) {
        menu.addAction(actions->action(QStringLiteral("delete")));
    }
    menu.addSeparator();
    if (singleFolder && items.first().isLocalFile() && visible("terminal") && Settings::showOpenTerminal()) {
        menu.addAction(actions->action(QStringLiteral("open_terminal")));
    }
    if (singleFolder) {
        menu.addAction(actions->action(QStringLiteral("content_chart")));
        menu.addAction(actions->action(QStringLiteral("add_to_favorites")));
        if (items.first().isLocalFile()) {
            QAction *look = menu.addAction(i18nc("@action:inmenu", "Folder Look…"));
            connect(look, &QAction::triggered, this, [this, url = items.first().targetUrl()] {
                editFolderLook(url);
            });
        }
    }

    const bool allLocal = std::all_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
        return !item.localPath().isEmpty();
    });
    if (allLocal) {
        QMenu *metadata = menu.addMenu(i18nc("@menu", "Metadata"));
        QMenu *labels = metadata->addMenu(i18nc("@menu", "Label"));
        QAction *clearLabel = labels->addAction(i18nc("@action:inmenu", "No Label"));
        connect(clearLabel, &QAction::triggered, this, [this] {
            setSelectionLabel(QString());
        });
        for (LabelColor color : LabelColors::all()) {
            const QString key = LabelColors::key(color);
            QAction *label = labels->addAction(labelText(color));
            connect(label, &QAction::triggered, this, [this, key] {
                setSelectionLabel(key);
            });
        }

        QMenu *colors = metadata->addMenu(i18nc("@menu", "File Colour"));
        QAction *clearColor = colors->addAction(i18nc("@action:inmenu", "No Colour"));
        connect(clearColor, &QAction::triggered, this, [this] {
            setSelectionColor(QString());
        });
        for (LabelColor color : LabelColors::all()) {
            const QString value = LabelColors::rgb(color).name();
            QAction *entry = colors->addAction(labelText(color));
            connect(entry, &QAction::triggered, this, [this, value] {
                setSelectionColor(value);
            });
        }

        QMenu *ratings = metadata->addMenu(i18nc("@menu", "Rating"));
        QAction *clearRating = ratings->addAction(i18nc("@action:inmenu", "No Rating"));
        connect(clearRating, &QAction::triggered, this, [this] {
            setSelectionRating(0);
        });
        for (int rating = 1; rating <= 5; ++rating) {
            QAction *entry = ratings->addAction(i18ncp("@action:inmenu", "%1 Star", "%1 Stars", rating));
            connect(entry, &QAction::triggered, this, [this, rating] {
                setSelectionRating(rating);
            });
        }
        QAction *note = metadata->addAction(i18nc("@action:inmenu", "Edit Note…"));
        connect(note, &QAction::triggered, this, &MainWindow::setSelectionNote);
        QAction *pin = metadata->addAction(i18nc("@action:inmenu", "Toggle Pin"));
        connect(pin, &QAction::triggered, this, &MainWindow::toggleSelectionPin);
    }
    const bool anyArchive = std::any_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
        return !item.isDir() && Archives::browseUrl(item.mostLocalUrl(), item.mimetype()).isValid();
    });
    if (anyArchive) {
        menu.addAction(actions->action(QStringLiteral("extract_to_other_pane")));
        menu.addAction(actions->action(QStringLiteral("extract_protected_archive")));
        menu.addAction(actions->action(QStringLiteral("test_archive_integrity")));
    }
    if (allLocal) {
        menu.addAction(actions->action(QStringLiteral("compress")));
    }
    menu.addAction(actions->action(QStringLiteral("calculate_occupied_space")));
    if (visible("copy_path") && Settings::showCopyPath()) {
        menu.addAction(actions->action(QStringLiteral("copy_path")));
    }

    m_fileItemActions->addActionsTo(&menu, KFileItemActions::MenuActionSource::All, {}, Settings::hiddenServiceActions());

    menu.addSeparator();
    if (visible("properties")) {
        menu.addAction(actions->action(QStringLiteral("properties")));
    }

    QAction *chosen = menu.exec(globalPos);
    if (chosen && chosen == openAction) {
        if (singleFolder) {
            activateItem(items.first());
        } else {
            for (const KFileItem &item : items) {
                FileOperations::open(item, this);
            }
        }
    } else if (chosen && chosen == openInTab) {
        pane->openInNewTab(items.first().targetUrl(), true);
    } else if (chosen && chosen == openInOtherPane) {
        inactivePane()->setUrl(items.first().targetUrl());
    }
}

void MainWindow::addTransferMenu(QMenu *menu, const KFileItemList &items, bool move)
{
    auto *submenu = menu->addMenu(move ? i18nc("@title:menu", "Move To") : i18nc("@title:menu", "Copy To"));
    submenu->setObjectName(move ? QStringLiteral("moveToMenu") : QStringLiteral("copyToMenu"));
    submenu->setEnabled(!move || KFileItemListProperties(items).supportsMoving());
    const QList<QUrl> sources = urlsOf(items);
    const auto transfer = [this, sources, move](const QUrl &destination) {
        if (move) {
            FileOperations::move(sources, destination, this);
        } else {
            FileOperations::copy(sources, destination, this);
        }
    };
    if ((!move && Settings::showCopyToOtherPane()) || (move && Settings::showMoveToOtherPane())) {
        connect(submenu->addAction(i18nc("@action:inmenu", "Other Pane")), &QAction::triggered, this, [this, transfer] {
            transfer(inactivePane()->url());
        });
    }
    connect(submenu->addAction(i18nc("@action:inmenu", "Home Folder")), &QAction::triggered, this, [transfer] {
        transfer(Settings::homeUrl());
    });
    connect(submenu->addAction(i18nc("@action:inmenu", "Choose Folder…")), &QAction::triggered, this, [this, transfer] {
        const QUrl destination = QFileDialog::getExistingDirectoryUrl(this, i18nc("@title:window", "Choose Destination Folder"), activePane()->url());
        if (destination.isValid() && !destination.isEmpty()) {
            transfer(destination);
        }
    });
}

void MainWindow::compareFiles()
{
    const KFileItemList items = comparisonFiles(activePane(), inactivePane(), isDualLayout());
    if (items.isEmpty()) {
        KMessageBox::error(this, i18nc("@info", "Select two local files to compare."));
        return;
    }
    auto *dialog = new FileDiffDialog(items.at(0).url().toLocalFile(), items.at(1).url().toLocalFile(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::createSymlink()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.size() != 1 || !items.first().url().isLocalFile()) {
        KMessageBox::error(this, i18nc("@info", "Select one local file or folder."));
        return;
    }
    const KFileItem item = items.first();
    bool accepted = false;
    const QString suffix = Settings::symlinkSuffix();
    const QString name =
        QInputDialog::getText(this,
                              i18nc("@title:window", "Create Symbolic Link"),
                              i18nc("@label:textbox", "Link name:"),
                              QLineEdit::Normal,
                              suffix.isEmpty() ? i18nc("@item:intext default symbolic link name", "%1 - link", item.name()) : item.name() + suffix,
                              &accepted);
    if (!accepted) {
        return;
    }
    QString error;
    if (!FileOperations::createSymlink(item.url(), name, error, this)) {
        KMessageBox::error(this, error);
    }
}

void MainWindow::exportListing(bool pdf)
{
    FolderView *view = activePane()->view();
    if (view->isLoading()) {
        KMessageBox::information(this, i18nc("@info", "Wait for the folder listing to finish before exporting."));
        return;
    }
    QList<int> columns;
    if (Settings::listingExportCustomColumns()) {
        columns.append(KDirModel::Name);
        if (Settings::listingExportType()) {
            columns.append(KDirModel::Type);
        }
        if (Settings::listingExportSize()) {
            columns.append(KDirModel::Size);
        }
        if (Settings::listingExportModified()) {
            columns.append(KDirModel::ModifiedTime);
        }
        if (Settings::listingExportPermissions()) {
            columns.append(KDirModel::Permissions);
        }
        if (Settings::listingExportOwner()) {
            columns.append(KDirModel::Owner);
        }
        if (Settings::listingExportGroup()) {
            columns.append(KDirModel::Group);
        }
    }
    const ListingExport::Table table = view->listingForExport(columns);
    QFileDialog dialog(this, i18nc("@title:window", "Export Listing"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(pdf ? QStringLiteral("pdf") : QStringLiteral("csv"));
    dialog.setNameFilter(pdf ? i18nc("@item:inlistbox", "PDF files (*.pdf)") : i18nc("@item:inlistbox", "CSV files (*.csv)"));
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return;
    }
    const QString destination = dialog.selectedFiles().first();
    const QString error = pdf ? ListingExport::toPdf(table, destination) : ListingExport::toCsv(table, destination);
    if (!error.isEmpty()) {
        KMessageBox::error(this, i18nc("@info", "Could not export the listing: %1", error));
        return;
    }
    showStatusMessage(i18nc("@info:status", "Listing exported to %1", destination));
}

void MainWindow::showContentChart()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    const QUrl folder = items.size() == 1 && items.first().isDir() ? items.first().url() : activePane()->url();
    if (!folder.isValid() || folder.isEmpty()) {
        KMessageBox::error(this, i18nc("@info", "No folder is available to scan."));
        return;
    }
    auto *dialog = new ContentChartDialog(folder, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showSearch()
{
    auto *dialog = new SearchDialog(activePane()->url(), this);
    connect(dialog, &SearchDialog::revealRequested, this, [this](const QUrl &url) {
        activePane()->revealUrl(url);
    });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showDuplicateFinder()
{
    const QUrl folder = activePane()->url();
    if (!folder.isLocalFile()) {
        KMessageBox::error(this, i18nc("@info", "Duplicate finding requires a local folder."));
        return;
    }
    auto *dialog = new DuplicateFinderDialog(folder, this);
    connect(dialog, &DuplicateFinderDialog::trashRequested, this, [this](const QList<QUrl> &urls) {
        FileOperations::trash(urls, this);
    });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::createChecksumFile()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.isEmpty() || !std::all_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
            return item.url().isLocalFile() && item.isFile() && !item.isLink();
        })) {
        KMessageBox::error(this, i18nc("@info", "Select local files that are not symbolic links."));
        return;
    }
    QStringList paths;
    for (const KFileItem &item : items) {
        paths.append(item.url().toLocalFile());
    }
    QFileDialog chooser(this, i18nc("@title:window", "Create Checksum File"), QFileInfo(paths.first()).absolutePath());
    chooser.setAcceptMode(QFileDialog::AcceptSave);
    chooser.setFileMode(QFileDialog::AnyFile);
    chooser.setDefaultSuffix(QStringLiteral("sha256"));
    chooser.setNameFilter(i18nc("@item:inlistbox", "SHA-256 checksum files (*.sha256)"));
    chooser.selectFile(Settings::checksumFileName());
    if (chooser.exec() != QDialog::Accepted || chooser.selectedFiles().isEmpty()) {
        return;
    }
    auto *dialog = new ChecksumDialog(ChecksumDialog::Mode::Create, paths, chooser.selectedFiles().first(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::verifyChecksumFile()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.size() != 1 || !items.first().url().isLocalFile() || !items.first().isFile() || items.first().isLink()) {
        KMessageBox::error(this, i18nc("@info", "Select one local checksum file that is not a symbolic link."));
        return;
    }
    auto *dialog = new ChecksumDialog(ChecksumDialog::Mode::Verify, {}, items.first().url().toLocalFile(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::splitFile()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.size() != 1 || !items.first().url().isLocalFile() || !items.first().isFile() || items.first().isLink()) {
        KMessageBox::error(this, i18nc("@info", "Select one local file that is not a symbolic link."));
        return;
    }
    auto *dialog = new FileSplitDialog(FileSplitDialog::Mode::Split, {items.first().url().toLocalFile()}, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::combineFiles()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.isEmpty() || !std::all_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
            return item.url().isLocalFile() && item.isFile() && !item.isLink();
        })) {
        KMessageBox::error(this, i18nc("@info", "Select local files that are not symbolic links."));
        return;
    }
    QStringList paths;
    for (const KFileItem &item : items) {
        paths.append(item.url().toLocalFile());
    }
    auto *dialog = new FileSplitDialog(FileSplitDialog::Mode::Combine, paths, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::compareFoldersFiltered()
{
    const QUrl first = activePane()->url();
    const QUrl second = inactivePane()->url();
    if (!isDualLayout() || inactivePane()->isHidden() || !first.isLocalFile() || !second.isLocalFile()) {
        showStatusMessage(i18nc("@info", "Choose two visible local folders to compare."));
        return;
    }
    auto *dialog = new FolderSyncDialog(first.toLocalFile(), second.toLocalFile(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::comparePaneFolders()
{
    Pane *first = activePane();
    Pane *second = inactivePane();
    const QUrl firstUrl = first->url();
    const QUrl secondUrl = second->url();
    if (!isDualLayout() || second->isHidden() || firstUrl.isEmpty() || !firstUrl.isValid() || secondUrl.isEmpty() || !secondUrl.isValid()) {
        KMessageBox::error(this, i18nc("@info", "Show two panes with valid folders to compare."));
        return;
    }
    auto *dialog = new FolderComparisonDialog(firstUrl, secondUrl, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const QPointer<Pane> firstPane(first);
    const QPointer<Pane> secondPane(second);
    connect(dialog,
            &FolderComparisonDialog::selectDifferences,
            this,
            [this, firstPane, secondPane, firstUrl, secondUrl](const QList<QUrl> &left, const QList<QUrl> &right) {
                if (!firstPane || !secondPane || !isDualLayout() || firstPane->isHidden() || secondPane->isHidden()
                    || firstPane->url().adjusted(QUrl::StripTrailingSlash) != firstUrl.adjusted(QUrl::StripTrailingSlash)
                    || secondPane->url().adjusted(QUrl::StripTrailingSlash) != secondUrl.adjusted(QUrl::StripTrailingSlash)) {
                    showStatusMessage(i18nc("@info:status", "Pane folders changed or are hidden; compare them again before selecting."));
                    return;
                }
                firstPane->view()->selectUrls(left);
                secondPane->view()->selectUrls(right);
                if (firstPane->view()->selectedItems().size() != left.size() || secondPane->view()->selectedItems().size() != right.size()) {
                    showStatusMessage(i18nc("@info:status", "Some differences could not be selected. Check pane filters and hidden files."));
                } else {
                    showStatusMessage(i18nc("@info:status", "Differences selected in both panes."));
                }
            });
    dialog->show();
}

void MainWindow::swapCurrentTabs()
{
    const QScopedValueRollback guard(m_syncingLinkedPanes, true);
    if (m_left->tabs()->swapCurrent(*m_right->tabs())) {
        std::swap(m_linkedLeftRoot, m_linkedRightRoot);
    }
}

void MainWindow::swapPaneSides()
{
    const QList<int> sizes = m_splitter->sizes();
    m_splitter->insertWidget(0, m_right);
    std::swap(m_left, m_right);
    std::swap(m_linkedLeftRoot, m_linkedRightRoot);
    updateTabBarPlacement();
    m_splitter->setSizes(sizes);
    updateActions();
}

void MainWindow::syncLinkedPane(Pane *pane, const QUrl &url)
{
    if (!m_linkedPanesAction || !m_linkedPanesAction->isChecked() || m_syncingLinkedPanes) {
        return;
    }
    const bool left = pane == m_left;
    const auto result = LinkedNavigation::target(left ? m_linkedLeftRoot : m_linkedRightRoot, left ? m_linkedRightRoot : m_linkedLeftRoot, url);
    if (!result.error.isEmpty()) {
        m_linkedPanesAction->setChecked(false);
        showStatusMessage(i18nc("@info:status", "Pane navigation unlinked: %1", result.error));
        return;
    }
    const QScopedValueRollback guard(m_syncingLinkedPanes, true);
    (left ? m_right : m_left)->setUrl(result.target);
}

void MainWindow::copyToOtherPane()
{
    FileOperations::copy(urlsOf(activePane()->view()->selectedItems()), inactivePane()->url(), this);
}

void MainWindow::moveToOtherPane()
{
    FileOperations::move(urlsOf(activePane()->view()->selectedItems()), inactivePane()->url(), this);
}

void MainWindow::cut()
{
    FileOperations::copyToClipboard(urlsOf(activePane()->view()->selectedItems()), true);
}

void MainWindow::copy()
{
    FileOperations::copyToClipboard(urlsOf(activePane()->view()->selectedItems()), false);
}

void MainWindow::paste()
{
    const KFileItemList selected = activePane()->view()->selectedItems();
    const QUrl destination = selected.size() == 1 && selected.first().isDir() ? selected.first().targetUrl() : activePane()->url();
    FileOperations::paste(destination, this);
}

void MainWindow::rename()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.size() > 1 && std::all_of(items.cbegin(), items.cend(), [](const KFileItem &item) {
            return item.url().isLocalFile();
        })) {
        batchRename();
    } else {
        FileOperations::rename(items, this);
    }
}

void MainWindow::batchRename()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    QStringList paths;
    for (const KFileItem &item : items) {
        if (!item.url().isLocalFile()) {
            showStatusMessage(i18nc("@info", "Batch rename supports local items only."));
            return;
        }
        paths.append(item.url().toLocalFile());
    }
    if (paths.isEmpty()) {
        showStatusMessage(i18nc("@info", "Select at least one item to rename."));
        return;
    }
    auto *dialog = new BatchRenameDialog(paths, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::editMetadata()
{
    const KFileItemList items = activePane()->view()->selectedItems();
    if (items.size() != 1 || !items.first().url().isLocalFile() || !items.first().isFile() || items.first().isLink()) {
        showStatusMessage(i18nc("@info", "Select one local regular file to edit metadata."));
        return;
    }
    auto *dialog = new MetadataEditDialog(items.first().url().toLocalFile(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showTransferQueue()
{
    auto *dialog = new TransferQueueDialog(FileOperations::transferQueue(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showCustomCommand()
{
    showCustomCommandPreset({});
}

void MainWindow::showCustomCommandPreset(const QString &name)
{
    const QUrl folder = activePane()->url();
    if (!folder.isLocalFile()) {
        showStatusMessage(i18nc("@info", "Commands require a local working folder."));
        return;
    }
    QStringList paths;
    for (const KFileItem &item : activePane()->view()->selectedItems()) {
        if (!item.url().isLocalFile()) {
            showStatusMessage(i18nc("@info", "Commands require local selected items."));
            return;
        }
        paths.append(item.url().toLocalFile());
    }
    paths.sort();
    auto *dialog = new CustomCommandDialog(folder.toLocalFile(), paths, this);
    connect(dialog, &CustomCommandDialog::presetsChanged, this, &MainWindow::refreshCustomCommandActions);
    if (!name.isEmpty()) {
        dialog->loadPreset(name);
    }
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::refreshCustomCommandActions()
{
    QString error;
    const auto commands = CustomCommands::load(KSharedConfig::openConfig()->group(QStringLiteral("CustomCommands")), &error);
    if (!error.isEmpty()) {
        showStatusMessage(error);
        return;
    }
    QSet<QString> current;
    for (const auto &command : commands) {
        current.insert(command.name);
        QAction *action = m_customCommandActions.value(command.name);
        if (!action) {
            const QString id = QStringLiteral("custom_command_preset_")
                + QString::fromLatin1(QCryptographicHash::hash(command.name.toUtf8(), QCryptographicHash::Sha256).toHex());
            action = actionCollection()->addAction(id);
            action->setIcon(SymbolicIcon::fromTheme(QStringLiteral("system-run")));
            connect(action, &QAction::triggered, this, [this, name = command.name] {
                showCustomCommandPreset(name);
            });
            m_customCommandActions.insert(command.name, action);
        }
        action->setText(command.name);
        action->setToolTip(i18nc("@info:tooltip", "Preview saved command: %1", command.name));
    }
    for (auto it = m_customCommandActions.begin(); it != m_customCommandActions.end();) {
        if (!current.contains(it.key())) {
            QAction *action = it.value();
            it = m_customCommandActions.erase(it);
            actionCollection()->removeAction(action);
        } else {
            ++it;
        }
    }
    if (m_active) {
        updateActions();
    }
}

void MainWindow::trash()
{
    FileOperations::trash(urlsOf(activePane()->view()->selectedItems()), this);
}

void MainWindow::deletePermanently()
{
    FileOperations::deletePermanently(urlsOf(activePane()->view()->selectedItems()), this);
}

void MainWindow::showProperties()
{
    FileOperations::showProperties(activePane()->view()->selectedItemsOrRoot(), this);
}

void MainWindow::openTerminal()
{
    FileOperations::openTerminal(activePane()->url(), this);
}

void MainWindow::newFolder()
{
    m_newFileMenu->setWorkingDirectory(activePane()->url());
    m_newFileMenu->createDirectory();
}

void MainWindow::showNewMenu()
{
    m_newFileMenu->setWorkingDirectory(activePane()->url());
    m_newFileMenu->checkUpToDate();
    m_newFileMenu->menu()->popup(QCursor::pos());
}

void MainWindow::toggleLayout(bool dual)
{
    const bool previewWasVisible = !m_previewPanel->isHidden();
    const bool leftWasVisible = !m_left->isHidden();
    const bool rightWasVisible = !m_right->isHidden();
    const bool preview = m_previewPanelAction && m_previewPanelAction->isChecked();
    const bool editorEmbedded = m_editor
        && ((m_editorPlacement == EditorPanel::Placement::Panel && !m_editor->isHidden())
            || ((m_editorPlacement == EditorPanel::Placement::Below || m_editorPlacement == EditorPanel::Placement::Right) && !m_editorDock->isHidden()));
    const bool terminalShown = m_terminalPanelAction && m_terminalPanelAction->isChecked();
    m_left->setVisible((dual && (!preview || editorEmbedded || terminalShown)) || activePane() == m_left);
    m_right->setVisible((dual && (!preview || editorEmbedded || terminalShown)) || activePane() == m_right);
    m_previewPanel->setVisible(preview);
    if (previewWasVisible != preview || leftWasVisible != !m_left->isHidden() || rightWasVisible != !m_right->isHidden()) {
        restoreSplitterSizes(true, false);
    }
    if (m_linkedPanesAction && m_linkedPanesAction->isChecked() && (!dual || m_left->isHidden() || m_right->isHidden())) {
        m_linkedPanesAction->setChecked(false);
        showStatusMessage(i18nc("@info:status", "Pane navigation unlinked because both panes are not visible."));
    }
    updateActions();
}

void MainWindow::saveSplitterSizes()
{
    const auto state = KSharedConfig::openStateConfig();
    KConfigGroup group = state->group(QLatin1String(SplitterSizesGroup)).group(QLatin1String(isStackedLayout() ? "Stacked" : "SideBySide"));
    const auto save = [&group](QSplitter *splitter, const QList<QPair<QWidget *, QString>> &widgets) {
        const QList<int> sizes = splitter->sizes();
        int total = 0;
        int previousVisible = 0;
        bool savedHidden = false;
        for (int i = 0; i < splitter->count(); ++i) {
            if (!splitter->widget(i)->isHidden()) {
                total += sizes.at(i);
            }
        }
        for (const auto &[widget, key] : widgets) {
            if (!group.hasKey(key)) {
                continue;
            }
            const int index = splitter->indexOf(widget);
            if (index >= 0 && !widget->isHidden() && sizes.at(index) > 0) {
                previousVisible += qBound(1, group.readEntry(key, 2500), 10000);
            } else {
                savedHidden = true;
            }
        }
        if (total <= 0) {
            return;
        }
        const int budget = savedHidden && previousVisible > 0 ? previousVisible : 10000;
        for (const auto &[widget, key] : widgets) {
            const int index = splitter->indexOf(widget);
            if (index >= 0 && !widget->isHidden() && sizes.at(index) > 0) {
                group.writeEntry(key, qMax(1, int(qint64(sizes.at(index)) * budget / total)));
            }
        }
    };
    save(m_splitter,
         {{m_left, QStringLiteral("FirstPane")},
          {m_right, QStringLiteral("SecondPane")},
          {m_previewPanel, QStringLiteral("Preview")},
          {m_editor, QStringLiteral("Editor")}});
    save(m_workspaceSplitter, {{m_splitter, QStringLiteral("Workspace")}, {m_terminalPanel, QStringLiteral("Terminal")}});
    if (m_editorDock && !m_editorDock->isHidden() && !m_editorDock->isFloating()) {
        const bool right = dockWidgetArea(m_editorDock) == Qt::RightDockWidgetArea;
        const int dimension = right ? width() : height();
        if (dimension > 0) {
            group.writeEntry(right ? "EditorDockRight" : "EditorDockBelow",
                             qBound(1, int(qint64(right ? m_editorDock->width() : m_editorDock->height()) * 10000 / dimension), 6000));
        }
    }
    state->sync();
}

void MainWindow::restoreSplitterSizes(bool panes, bool workspace, bool defaultsIfMissing)
{
    const KConfigGroup group =
        KSharedConfig::openStateConfig()->group(QLatin1String(SplitterSizesGroup)).group(QLatin1String(isStackedLayout() ? "Stacked" : "SideBySide"));
    const auto restore = [this, &group, defaultsIfMissing](QSplitter *splitter, const QList<QPair<QWidget *, QString>> &widgets, const QList<int> &defaults) {
        QList<int> sizes(splitter->count(), 2500);
        bool saved = false;
        for (const auto &[widget, key] : widgets) {
            const int index = splitter->indexOf(widget);
            if (index >= 0 && group.hasKey(key)) {
                sizes[index] = qBound(1, group.readEntry(key, 2500), 10000);
                saved = true;
            }
        }
        if (saved) {
            const auto preservePrimaryArea = [&sizes, splitter](QWidget *primary, QWidget *secondary, int minimumTotalShare) {
                const int primaryIndex = splitter->indexOf(primary);
                const int secondaryIndex = splitter->indexOf(secondary);
                if (primaryIndex < 0 || secondaryIndex < 0 || primary->isHidden() || secondary->isHidden()) {
                    return;
                }
                int total = 0;
                int otherTotal = 0;
                for (int i = 0; i < splitter->count(); ++i) {
                    if (splitter->widget(i)->isHidden()) {
                        continue;
                    }
                    total += sizes.at(i);
                    if (i != primaryIndex && i != secondaryIndex) {
                        otherTotal += sizes.at(i);
                    }
                }
                if (total <= 0) {
                    return;
                }
                const int primaryBudget = qMax(total * minimumTotalShare / 10000, total - otherTotal);
                const int otherBudget = total - primaryBudget;
                const int otherRemaining = otherBudget;
                if (otherTotal > 0) {
                    for (int i = 0; i < splitter->count(); ++i) {
                        if (!splitter->widget(i)->isHidden() && i != primaryIndex && i != secondaryIndex) {
                            sizes[i] = qMax(1, int(qint64(sizes.at(i)) * otherRemaining / otherTotal));
                        }
                    }
                }
                const int primaryTotal = sizes.at(primaryIndex) + sizes.at(secondaryIndex);
                const int firstShare = primaryTotal > 0 ? int(qint64(primaryBudget) * sizes.at(primaryIndex) / primaryTotal) : primaryBudget / 2;
                const int minimumPane = primaryBudget / 4;
                sizes[primaryIndex] = qBound(minimumPane, firstShare, primaryBudget - minimumPane);
                sizes[secondaryIndex] = primaryBudget - sizes.at(primaryIndex);
            };
            if (splitter == m_splitter) {
                preservePrimaryArea(m_left, m_right, 6000);
            } else if (splitter == m_workspaceSplitter && !m_terminalPanel->isHidden()) {
                const int workspaceIndex = splitter->indexOf(m_splitter);
                const int terminalIndex = splitter->indexOf(m_terminalPanel);
                if (workspaceIndex >= 0 && terminalIndex >= 0) {
                    const int total = sizes.at(workspaceIndex) + sizes.at(terminalIndex);
                    const int primary = qMax(total * 6000 / 10000, sizes.at(workspaceIndex));
                    sizes[workspaceIndex] = primary;
                    sizes[terminalIndex] = total - primary;
                }
            }
            splitter->setSizes(sizes);
        } else if (defaultsIfMissing) {
            splitter->setSizes(defaults);
        }
    };
    if (panes) {
        restore(m_splitter,
                {{m_left, QStringLiteral("FirstPane")},
                 {m_right, QStringLiteral("SecondPane")},
                 {m_previewPanel, QStringLiteral("Preview")},
                 {m_editor, QStringLiteral("Editor")}},
                QList<int>(m_splitter->count(), 1));
    }
    if (workspace) {
        restore(m_workspaceSplitter, {{m_splitter, QStringLiteral("Workspace")}, {m_terminalPanel, QStringLiteral("Terminal")}}, {3, 1});
    }
}

void MainWindow::restoreEditorDockSize()
{
    if (!m_editorDock || m_editorDock->isHidden() || m_editorDock->isFloating()) {
        return;
    }
    const KConfigGroup group =
        KSharedConfig::openStateConfig()->group(QLatin1String(SplitterSizesGroup)).group(QLatin1String(isStackedLayout() ? "Stacked" : "SideBySide"));
    const bool right = dockWidgetArea(m_editorDock) == Qt::RightDockWidgetArea;
    const char *key = right ? "EditorDockRight" : "EditorDockBelow";
    if (group.hasKey(key)) {
        const int fraction = qBound(1, group.readEntry(key, 2500), 6000);
        const int dimension = right ? width() : height();
        resizeDocks({m_editorDock}, {qMax(1, int(qint64(dimension) * fraction / 10000))}, right ? Qt::Horizontal : Qt::Vertical);
    }
}

void MainWindow::resetSplitterSizes()
{
    const auto state = KSharedConfig::openStateConfig();
    KConfigGroup group = state->group(QLatin1String(SplitterSizesGroup)).group(QLatin1String(isStackedLayout() ? "Stacked" : "SideBySide"));
    group.deleteGroup();
    group.writeEntry("EditorDockRight", 2500);
    group.writeEntry("EditorDockBelow", 2500);
    state->sync();
    m_splitter->setSizes(QList<int>(m_splitter->count(), 1));
    m_workspaceSplitter->setSizes({3, 1});
    if (m_editorDock && !m_editorDock->isHidden() && !m_editorDock->isFloating()) {
        const bool right = dockWidgetArea(m_editorDock) == Qt::RightDockWidgetArea;
        resizeDocks({m_editorDock}, {qMax(1, (right ? width() : height()) / 4)}, right ? Qt::Horizontal : Qt::Vertical);
    }
}

void MainWindow::setStackedLayout(bool stacked)
{
    const bool changed = isStackedLayout() != stacked;
    m_stackedLayoutAction->setChecked(stacked);
    m_splitter->setOrientation(stacked ? Qt::Vertical : Qt::Horizontal);
    updateTerminalPlacement();
    restoreSplitterSizes(true, true, changed);
    restoreEditorDockSize();
    updateTabBarPlacement();
}

void MainWindow::updateTabBarPlacement()
{
    const bool stacked = isStackedLayout();
    const bool together = stacked && Settings::stackedTabBarsTogether();
    const int first = together ? int(Pane::TabBarPosition::Bottom) : stacked ? Settings::stackedFirstTabPosition() : Settings::horizontalFirstTabPosition();
    const int second = together ? int(Pane::TabBarPosition::Top) : stacked ? Settings::stackedSecondTabPosition() : Settings::horizontalSecondTabPosition();
    m_left->setTabBarPosition(static_cast<Pane::TabBarPosition>(qBound(0, first, 3)));
    m_right->setTabBarPosition(static_cast<Pane::TabBarPosition>(qBound(0, second, 3)));
    auto *position = qobject_cast<KSelectAction *>(actionCollection()->action(QStringLiteral("tab_bar_position")));
    position->setCurrentItem(static_cast<int>(activePane()->tabBarPosition()));
    actionCollection()->action(QStringLiteral("stacked_tab_bars_together"))->setEnabled(stacked);
}

void MainWindow::updateTerminalPlacement()
{
    const auto placement = Settings::terminalPlacement();
    const bool right = placement == Settings::EnumTerminalPlacement::Right || (placement == Settings::EnumTerminalPlacement::Automatic && isStackedLayout());
    m_workspaceSplitter->setOrientation(right ? Qt::Horizontal : Qt::Vertical);
}

void MainWindow::updatePreviewPanel()
{
    if (!m_previewPanelAction || !m_previewPanelAction->isChecked()) {
        return;
    }
    FolderView *view = activePane()->view();
    const KFileItemList selected = view->selectedItems();
    QList<QUrl> images;
    if (selected.size() == 1 && !selected.first().isDir() && previewMimeType(selected.first(), true).startsWith(QLatin1String("image/"))) {
        for (const KFileItem &item : view->itemsInViewOrder()) {
            if (!item.isDir() && (item.url() == selected.first().url() || previewMimeType(item).startsWith(QLatin1String("image/")))) {
                images.append(item.url());
            }
        }
    }
    m_previewPanel->setItems(selected, images);
}

void MainWindow::updateInformationPanel()
{
    if (!m_informationPanel) {
        return;
    }
    if (Settings::informationHover() && !m_hoveredInformationItem.isNull()) {
        m_informationPanel->setItem(m_hoveredInformationItem);
        return;
    }
    const KFileItemList selected = activePane()->view()->selectedItems();
    if (selected.size() == 1) {
        m_informationPanel->setItem(selected.first());
    } else if (selected.isEmpty()) {
        m_informationPanel->setItem(activePane()->view()->currentItem());
    } else {
        m_informationPanel->setItem({});
    }
}

bool MainWindow::isStackedLayout() const
{
    return m_splitter->orientation() == Qt::Vertical;
}

bool MainWindow::isDualLayout() const
{
    return m_dualLayoutAction->isChecked();
}

void MainWindow::applySettings()
{
    const auto terminalOrientation = m_workspaceSplitter->orientation();
    updateTerminalPlacement();
    if (terminalOrientation != m_workspaceSplitter->orientation()) {
        restoreSplitterSizes(false, true);
    }
    actionCollection()->action(QStringLiteral("dim_inactive_pane"))->setChecked(Settings::dimInactivePane());
    actionCollection()->action(QStringLiteral("stacked_tab_bars_together"))->setChecked(Settings::stackedTabBarsTogether());
    actionCollection()->action(QStringLiteral("show_tab_close_buttons"))->setChecked(Settings::showTabCloseButtons());
    updateTabBarPlacement();
    for (GalleryWindow *gallery : findChildren<GalleryWindow *>()) {
        gallery->applyPresentationSettings();
    }
    if (m_terminalPanelAction && m_terminalPanelAction->isChecked()) {
        m_terminalPanel->start(activePane()->url());
    }
    if (m_editor) {
        m_editor->applySettings();
    }
    const bool sizeDefaultsChanged =
        m_automaticLocalSizeDefault != Settings::automaticLocalFolderSizes() || m_automaticRemoteSizeDefault != Settings::automaticRemoteFolderSizes();
    m_automaticLocalSizeDefault = Settings::automaticLocalFolderSizes();
    m_automaticRemoteSizeDefault = Settings::automaticRemoteFolderSizes();
    for (Pane *pane : {m_left, m_right}) {
        pane->setInactiveDimming(Settings::dimInactivePane());
        pane->reloadTabSettings();
        pane->reloadFilterSettings();
        pane->setNavigationBarVisible(Settings::showNavigationBar());
        pane->setNewTabsAfterCurrent(Settings::newTabsAfterCurrent());
        pane->setFollowSymlinks(Settings::followSymlinks());

        FolderView *view = pane->view();
        view->setInformationHoverEnabled(Settings::informationHover());
        view->reloadListingSettings();
        if (sizeDefaultsChanged) {
            view->refreshFolderSizePolicy();
        }
        if (!view->hasFolderFormat()) {
            view->setRatingColumn(Settings::showRatingColumn());
            view->setColumnVisible(KDirModel::Size, Settings::showSizeColumn());
            view->setColumnVisible(KDirModel::ModifiedTime, Settings::showModifiedColumn());
            view->setColumnVisible(KDirModel::Permissions, Settings::showPermissionsColumn());
            view->setColumnVisible(KDirModel::Owner, Settings::showOwnerColumn());
            view->setColumnVisible(KDirModel::Group, Settings::showGroupColumn() || Settings::showRatingColumn());
            view->setColumnVisible(KDirModel::Type, Settings::showTypeColumn());
        }
        view->setSortFoldersFirst(Settings::foldersFirst());
        view->setShowToolTips(Settings::showToolTips());
        view->setDragFromAnywhere(Settings::dragFromAnywhere());
        view->setItemSpacing(Density::itemSpace(itemDensity<Settings::EnumDetailsDensity>(Settings::detailsDensity()), Settings::detailsCustomSpace()),
                             Density::itemSpace(itemDensity<Settings::EnumIconsDensity>(Settings::iconsDensity()), Settings::iconsCustomSpace()));
        view->setPreviewPlugins(Settings::previewPlugins());
        view->setRemotePreviewsEnabled(Settings::showRemotePreviews());
    }
    m_informationPanel->setFields(Settings::informationFields());
    if (!Settings::informationHover()) {
        m_hoveredInformationPane = nullptr;
        m_hoveredInformationItem = {};
    }
    updateTabBars();
    const int barMargin = Density::barMargin(barDensity(Settings::barDensity()), Settings::barCustomSpacing());
    const auto bars = toolBars();
    for (KToolBar *bar : bars) {
        setBarMargin(bar, barMargin);
    }
    setBarMargin(statusBar(), barMargin);
    setBarMargin(m_left->navigationBar(), barMargin);
    setBarMargin(m_right->navigationBar(), barMargin);
    applyPanelSettings();
    updateStatusBar();
    updateFreeSpace();
    updateInformationPanel();

    QAction *switchPane = actionCollection()->action(QStringLiteral("switch_pane"));
    QList<QKeySequence> shortcuts = switchPane->shortcuts();
    shortcuts.removeAll(QKeySequence(Qt::Key_Tab));
    if (Settings::tabSwitchesPanes()) {
        shortcuts.prepend(QKeySequence(Qt::Key_Tab));
    }
    switchPane->setShortcuts(shortcuts);
    m_previewPanel->applyPresentationSettings();
    updatePreviewPanel();
    updateActions();
}

void MainWindow::applyPanelSettings()
{
    const bool combined = m_panelLayout == PanelLayout::Combined;
    const Qt::DockWidgetArea area = Settings::panelPosition() == Settings::EnumPanelPosition::right ? Qt::RightDockWidgetArea : Qt::LeftDockWidgetArea;
    m_autoFocusFolders = Settings::panelAutoFocus();
    addDockWidget(area, m_combinedDock);
    addDockWidget(area, m_placesDock);
    addDockWidget(area, m_foldersDock);
    splitDockWidget(m_placesDock, m_foldersDock, Qt::Vertical);
    m_combinedDock->setVisible(Settings::panelsVisible() && combined);
    m_placesDock->setVisible(Settings::panelsVisible() && !combined && Settings::showPlaces());
    m_foldersDock->setVisible(Settings::panelsVisible() && !combined);
    for (QDockWidget *dock : {m_combinedDock, m_placesDock, m_foldersDock}) {
        QWidget *titleBar = dock->titleBarWidget();
        if (Settings::panelTitlesVisible()) {
            if (titleBar) {
                dock->setTitleBarWidget(nullptr);
                delete titleBar;
            }
        } else if (!titleBar) {
            titleBar = new QWidget(dock);
            titleBar->setFixedHeight(0);
            dock->setTitleBarWidget(titleBar);
        }
    }
    m_combinedPanel->setSectionVisibility(Settings::favoritesVisible(), Settings::placesVisible(), Settings::foldersVisible());
    if (m_active && m_autoFocusFolders) {
        syncFolderTrees();
    }
}

void MainWindow::rememberPlacesShown(bool shown)
{
    Settings::setShowPlaces(shown);
    Settings::self()->save();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_placesDock && event->type() == QEvent::Close) {
        rememberPlacesShown(false);
    }
    return KXmlGuiWindow::eventFilter(watched, event);
}

void MainWindow::applyViewDefaults()
{
    const auto mode = Settings::defaultViewMode() == Settings::EnumDefaultViewMode::Icons ? FolderView::ViewMode::Icons : FolderView::ViewMode::Details;
    const Qt::SortOrder order = Settings::sortDescending() ? Qt::DescendingOrder : Qt::AscendingOrder;
    for (Pane *pane : {m_left, m_right}) {
        pane->view()->setViewMode(mode);
        pane->view()->setSorting(Settings::sortColumn(), order);
        pane->view()->setShowHiddenFiles(Settings::showHiddenFiles());
    }
    m_previewsAction->setChecked(Settings::showPreviews());
    setPreviewsEnabled(Settings::showPreviews());
}

void MainWindow::showSettings()
{
    if (KConfigDialog::showDialog(QLatin1String(SettingsDialog::Name))) {
        return;
    }
    auto *dialog = new SettingsDialog(m_placesModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::applySettings);
    connect(dialog, &SettingsDialog::customCommandsChanged, this, &MainWindow::refreshCustomCommandActions);
    dialog->show();
}

void MainWindow::toggleViewMode()
{
    FolderView *view = activePane()->view();
    view->setViewMode(view->viewMode() == FolderView::ViewMode::Details ? FolderView::ViewMode::Icons : FolderView::ViewMode::Details);
    updateActions();
}

void MainWindow::setFlat(bool flat)
{
    const QString key = flatKey(activePane()->url());
    m_flatLocations.removeAll(key);
    if (flat) {
        m_flatLocations.append(key);
    }
    KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(FlatGroup));
    group.writeEntry("Locations", m_flatLocations);
    group.sync();
    activePane()->view()->setFlat(flat);
    updateActions();
}

void MainWindow::syncFolderTrees()
{
    const QUrl url = activePane()->url();
    const bool hidden = activePane()->view()->showHiddenFiles();
    for (FolderTree *tree : {m_folderTree, m_combinedPanel->folderTree()}) {
        tree->setShowHiddenFolders(hidden);
        if (m_autoFocusFolders) {
            tree->reveal(url);
        }
    }
}

void MainWindow::showFolderTreeMenu(const QUrl &url, const QPoint &globalPos)
{
    QMenu menu(this);
    const KConfigGroup context = KSharedConfig::openConfig()->group(QStringLiteral("ContextMenu"));
    QAction *openInTab = context.readEntry("ShowOpenInNewTab", true)
        ? menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"))
        : nullptr;
    QAction *openInOther = nullptr;
    if (context.readEntry("ShowOpenInOtherPane", true) && m_dualLayoutAction->isChecked()) {
        openInOther = menu.addAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Other Pane"));
    }
    QAction *favorite = menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18nc("@action:inmenu", "Add to Favorites"));
    favorite->setEnabled(!m_favorites->contains(url));
    menu.addSeparator();
    QAction *properties = menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action:inmenu", "Properties"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen && chosen == openInTab) {
        activePane()->openInNewTab(url, true);
    } else if (chosen && chosen == openInOther) {
        inactivePane()->setUrl(url);
    } else if (chosen == favorite) {
        m_favorites->add(url);
    } else if (chosen == properties) {
        FileOperations::showProperties(KFileItemList{KFileItem(url)}, this);
    }
}

void MainWindow::setShowHiddenFiles(bool show)
{
    activePane()->view()->setShowHiddenFiles(show);
    syncFolderTrees();
}

void MainWindow::setPreviewsEnabled(bool enabled)
{
    m_left->view()->setPreviewsEnabled(enabled);
    m_right->view()->setPreviewsEnabled(enabled);
}
