// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"

#include "archives.h"
#include "archivewriter.h"
#include "combinedpanel.h"
#include "compressdialog.h"
#include "favoritesmodel.h"
#include "fileoperations.h"
#include "foldermetadata.h"
#include "foldertree.h"
#include "folderview.h"
#include "labelcolor.h"
#include "pane.h"
#include "session.h"
#include "symbolicicon.h"
#include "tabcolors.h"
#include "tabmodel.h"

#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KFileUtils>
#include <KHamburgerMenu>
#include <KIO/CopyJob>
#include <KIO/DropJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/FileUndoManager>
#include <KIO/Global>
#include <KIO/ListJob>
#include <KIO/MkdirJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KNewFileMenu>
#include <KSelectAction>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToggleAction>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QtConcurrentRun>

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

QString flatKey(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash).toString();
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
        return SymbolicIcon::wrap(value.value<QIcon>());
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
}

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_placesModel(new SymbolicPlacesModel(this))
{
    initResources();
    setComponentName(QStringLiteral("opus-file-manager"), i18n("Opus File Manager"));
    m_flatLocations = KSharedConfig::openStateConfig()->group(QLatin1String(FlatGroup)).readEntry("Locations", QStringList());
    m_favorites = new FavoritesModel(KSharedConfig::openConfig()->group(QStringLiteral("Favorites")), this);
    m_tabColors = new TabColors(KSharedConfig::openConfig()->group(QStringLiteral("TabColors")), this);
    setupPanes();
    setupPlaces();
    setupPanels();
    setupStatusBar();
    setupActions();

    setStandardToolBarMenuEnabled(true);
    setupGUI(QSize(1100, 700), Default, QStringLiteral("opus-file-managerui.rc"));
    const KConfigGroup panels = KSharedConfig::openStateConfig()->group(QLatin1String(PanelsGroup));
    setPanelLayout(panels.readEntry("Layout", QString()) == QLatin1String("Separate") ? PanelLayout::Separate : PanelLayout::Combined);

    connect(KIO::FileUndoManager::self(), &KIO::FileUndoManager::undoAvailable, this, [this](bool available) {
        m_undoAction->setEnabled(available);
    });
    connect(KIO::FileUndoManager::self(), &KIO::FileUndoManager::undoTextChanged, this, [this](const QString &text) {
        m_undoAction->setText(text);
    });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateActions);

    setActivePane(m_left);
    updateActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupPanes()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_left = new Pane(m_placesModel, m_tabColors, m_splitter);
    m_right = new Pane(m_placesModel, m_tabColors, m_splitter);
    m_splitter->addWidget(m_left);
    m_splitter->addWidget(m_right);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    setCentralWidget(m_splitter);

    connectPane(m_left);
    connectPane(m_right);
}

void MainWindow::connectPane(Pane *pane)
{
    connect(pane, &Pane::activated, this, [this, pane] {
        setActivePane(pane);
    });
    connect(pane, &Pane::urlChanged, this, [this, pane](const QUrl &url) {
        pane->view()->setFlat(m_flatLocations.contains(flatKey(url)));
        if (pane == m_active) {
            updateActions();
            updateStatusBar();
            updateFreeSpace();
            updateWindowTitle();
            syncFolderTrees();
        }
    });
    connect(pane, &Pane::tabCountChanged, this, &MainWindow::updateTabBars);
    connect(pane, &Pane::statusMessage, this, &MainWindow::showStatusMessage);
    connect(pane->view(), &FolderView::itemActivated, this, &MainWindow::activateItem);
    connect(pane->view(), &FolderView::contextMenuRequested, this, [this, pane](const QPoint &pos, const KFileItemList &items) {
        setActivePane(pane);
        showContextMenu(pane, pos, items);
    });
    connect(pane->view(), &FolderView::selectionChanged, this, [this, pane] {
        if (pane == m_active) {
            updateActions();
            updateStatusBar();
        }
    });
    connect(pane->view(), &FolderView::itemCountChanged, this, [this, pane] {
        if (pane == m_active) {
            updateStatusBar();
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
    });
    connect(m_placesView, &KFilePlacesView::tabRequested, this, [this](const QUrl &url) {
        activePane()->openInNewTab(url);
    });
    connect(m_placesView, &KFilePlacesView::urlsDropped, this, [this](const QUrl &dest, QDropEvent *event, QWidget *) {
        KIO::DropJob *job = KIO::drop(event, dest);
        KJobWidgets::setWindow(job, this);
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
        KIO::DropJob *job = KIO::drop(event, destination);
        KJobWidgets::setWindow(job, this);
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
        KIO::DropJob *job = KIO::drop(event, destination);
        KJobWidgets::setWindow(job, this);
    });
    m_combinedDock = addPanelDock(QStringLiteral("combinedDock"), i18nc("@title:window", "Places"), m_combinedPanel);
}

void MainWindow::setPanelLayout(PanelLayout layout)
{
    m_panelLayout = layout;
    const bool combined = layout == PanelLayout::Combined;
    m_combinedDock->setVisible(combined);
    m_placesDock->setVisible(!combined);
    m_foldersDock->setVisible(!combined);
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
    CompressDialog dialog(defaultName, inactivePane()->url().isLocalFile(), this);
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
    watcher->setFuture(QtConcurrent::run(&ArchiveWriter::write, archivePath, dialog.format(), sources));
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
    m_messageLabel = new QLabel(this);
    m_freeSpaceLabel = new QLabel(this);
    statusBar()->addWidget(m_itemsLabel);
    statusBar()->addWidget(m_messageLabel, 1);
    statusBar()->addPermanentWidget(m_freeSpaceLabel);
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

    QAction *terminal = actions->addAction(QStringLiteral("open_terminal"), this, &MainWindow::openTerminal);
    terminal->setText(i18nc("@action", "Open Terminal Here"));
    terminal->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    actions->setDefaultShortcut(terminal, Qt::SHIFT | Qt::Key_F4);

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

    m_viewModeAction = actions->addAction(QStringLiteral("toggle_view_mode"), this, &MainWindow::toggleViewMode);
    m_viewModeAction->setText(i18nc("@action", "Icons View"));
    m_viewModeAction->setIcon(QIcon::fromTheme(QStringLiteral("view-list-icons")));
    actions->setDefaultShortcut(m_viewModeAction, Qt::CTRL | Qt::SHIFT | Qt::Key_V);

    m_hiddenFilesAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-hidden")), i18nc("@action", "Show Hidden Files"), this);
    actions->addAction(QStringLiteral("show_hidden_files"), m_hiddenFilesAction);
    actions->setDefaultShortcut(m_hiddenFilesAction, Qt::CTRL | Qt::Key_H);
    connect(m_hiddenFilesAction, &KToggleAction::toggled, this, &MainWindow::setShowHiddenFiles);

    m_previewsAction = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-preview")), i18nc("@action", "Previews"), this);
    m_previewsAction->setChecked(true);
    actions->addAction(QStringLiteral("show_previews"), m_previewsAction);
    connect(m_previewsAction, &KToggleAction::toggled, this, &MainWindow::setPreviewsEnabled);

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
            activePane()->setUrl(QUrl::fromLocalFile(QDir::homePath()));
        },
        actions);

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
    QAction *showMenuBar = KStandardAction::showMenubar(menuBar(), &QMenuBar::setVisible, actions);
    KHamburgerMenu *hamburger = KStandardAction::hamburgerMenu(nullptr, nullptr, actions);
    hamburger->setMenuBar(menuBar());
    hamburger->setShowMenuBarAction(showMenuBar);

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
    if (!m_dualLayoutAction || !m_dualLayoutAction->isChecked()) {
        m_left->setVisible(m_active == m_left);
        m_right->setVisible(m_active == m_right);
    }
    m_hiddenFilesAction->setChecked(pane->view()->showHiddenFiles());
    syncFolderTrees();
    updateActions();
    updateStatusBar();
    updateFreeSpace();
    updateWindowTitle();
}

void MainWindow::openUrls(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        if (!m_sessionRestored) {
            restoreSession();
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
}

void MainWindow::restoreSession()
{
    m_sessionRestored = true;
    const QUrl home = QUrl::fromLocalFile(QDir::homePath());
    const KConfigGroup group = KSharedConfig::openStateConfig()->group(QLatin1String(SessionGroup));
    const WindowSession session = Session::load(group).sanitized(home);

    m_left->restoreState(session.left);
    m_right->restoreState(session.right);
    setActivePane(session.activePane == 1 ? m_right : m_left);
    setStackedLayout(session.stacked);
}

void MainWindow::saveSession()
{
    WindowSession session;
    session.left = m_left->saveState();
    session.right = m_right->saveState();
    session.activePane = activePane() == m_right ? 1 : 0;
    session.stacked = isStackedLayout();

    KSharedConfig::Ptr state = KSharedConfig::openStateConfig();
    KConfigGroup group = state->group(QLatin1String(SessionGroup));
    Session::save(session, group);
    KConfigGroup panels = state->group(QLatin1String(PanelsGroup));
    panels.writeEntry("FoldersExpanded", m_folderTree->expandedUrls());
    panels.writeEntry("CombinedExpanded", m_combinedPanel->folderTree()->expandedUrls());
    panels.writeEntry("CollapsedSections", m_combinedPanel->collapsedSections());
    state->sync();
}

bool MainWindow::queryClose()
{
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
    setStackedLayout(session.stacked);
}

void MainWindow::saveProperties(KConfigGroup &group)
{
    WindowSession session;
    session.left = m_left->saveState();
    session.right = m_right->saveState();
    session.activePane = activePane() == m_right ? 1 : 0;
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

    actions->action(QStringLiteral("copy_to_other_pane"))->setEnabled(hasSelection);
    actions->action(QStringLiteral("move_to_other_pane"))->setEnabled(hasSelection && props.supportsMoving());
    actions->action(QStringLiteral("rename"))->setEnabled(hasSelection && props.supportsMoving());
    actions->action(QStringLiteral("move_to_trash"))->setEnabled(hasSelection && props.supportsDeleting() && props.isLocal());
    actions->action(QStringLiteral("delete"))->setEnabled(hasSelection && props.supportsDeleting());
    actions->action(stdName(KStandardAction::Cut))->setEnabled(hasSelection && props.supportsMoving());
    actions->action(stdName(KStandardAction::Copy))->setEnabled(hasSelection);
    actions->action(QStringLiteral("open_terminal"))->setEnabled(activePane()->url().isLocalFile());

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

    const bool icons = view->viewMode() == FolderView::ViewMode::Icons;
    m_viewModeAction->setText(icons ? i18nc("@action", "Details View") : i18nc("@action", "Icons View"));
    m_viewModeAction->setIcon(QIcon::fromTheme(icons ? QStringLiteral("view-list-details") : QStringLiteral("view-list-icons")));
}

void MainWindow::updateStatusBar()
{
    FolderView *view = activePane()->view();
    const KFileItemList selected = view->selectedItems();
    const int total = view->itemCount();

    if (selected.isEmpty()) {
        m_itemsLabel->setText(i18ncp("@info:status", "%1 item", "%1 items", total));
    } else {
        KIO::filesize_t size = 0;
        int folders = 0;
        for (const KFileItem &item : selected) {
            if (item.isDir()) {
                ++folders;
            } else {
                size += item.size();
            }
        }
        const int files = selected.size() - folders;
        QString text = i18ncp("@info:status", "%1 of %2 selected", "%1 of %2 selected", selected.size(), total);
        if (files > 0) {
            text += QStringLiteral(" (%1)").arg(KIO::convertSize(size));
        }
        m_itemsLabel->setText(text);
    }

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

void MainWindow::updateFreeSpace()
{
    const QUrl url = activePane()->url();
    if (!url.isValid()) {
        m_freeSpaceLabel->clear();
        return;
    }
    KIO::FileSystemFreeSpaceJob *job = KIO::fileSystemFreeSpace(url);
    connect(job, &KJob::result, this, [this, job, url] {
        if (url != activePane()->url()) {
            return;
        }
        if (job->error()) {
            m_freeSpaceLabel->clear();
            return;
        }
        m_freeSpaceLabel->setText(
            i18nc("@info:status free disk space", "%1 free of %2", KIO::convertSize(job->availableSize()), KIO::convertSize(job->size())));
    });
}

void MainWindow::updateWindowTitle()
{
    setCaption(activePane()->url().toDisplayString(QUrl::PreferLocalFile));
}

void MainWindow::updateTabBars()
{
    const bool force = m_left->tabs()->count() > 1 || m_right->tabs()->count() > 1;
    m_left->setTabBarForcedVisible(force);
    m_right->setTabBarForcedVisible(force);
}

void MainWindow::showStatusMessage(const QString &message)
{
    statusBar()->showMessage(message, 5000);
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

void MainWindow::showContextMenu(Pane *pane, const QPoint &globalPos, const KFileItemList &items)
{
    QMenu menu(this);
    KActionCollection *actions = actionCollection();

    if (items.isEmpty()) {
        m_newFileMenu->setWorkingDirectory(pane->url());
        m_newFileMenu->checkUpToDate();
        menu.addAction(m_newFileMenu);
        menu.addSeparator();
        menu.addAction(m_pasteAction);
        menu.addAction(actions->action(QStringLiteral("new_tab")));
        menu.addSeparator();
        menu.addAction(actions->action(stdName(KStandardAction::Redisplay)));
        menu.addAction(actions->action(QStringLiteral("open_terminal")));
        QAction *folderLook = nullptr;
        if (pane->url().isLocalFile()) {
            folderLook = menu.addAction(i18nc("@action:inmenu", "Folder Look…"));
        }
        menu.addAction(actions->action(QStringLiteral("copy_path")));
        menu.addSeparator();
        menu.addAction(actions->action(QStringLiteral("properties")));
        if (menu.exec(globalPos) == folderLook && folderLook) {
            editFolderLook(pane->url());
        }
        return;
    }

    const bool singleFolder = items.size() == 1 && items.first().isDir();
    const KFileItemListProperties props(items);

    QAction *openAction = menu.addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:inmenu", "Open"));
    QAction *openInTab = nullptr;
    QAction *openInOtherPane = nullptr;
    if (singleFolder) {
        openInTab = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"));
        if (m_dualLayoutAction->isChecked()) {
            openInOtherPane = menu.addAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Other Pane"));
        }
    }
    m_fileItemActions->setItemListProperties(props);
    m_fileItemActions->insertOpenWithActionsTo(nullptr, &menu, QStringList());

    menu.addSeparator();
    menu.addAction(actions->action(stdName(KStandardAction::Cut)));
    menu.addAction(actions->action(stdName(KStandardAction::Copy)));
    if (singleFolder) {
        menu.addAction(m_pasteAction);
    }
    menu.addAction(actions->action(QStringLiteral("copy_to_other_pane")));
    menu.addAction(actions->action(QStringLiteral("move_to_other_pane")));
    menu.addSeparator();
    menu.addAction(actions->action(QStringLiteral("rename")));
    menu.addAction(actions->action(QStringLiteral("move_to_trash")));
    menu.addAction(actions->action(QStringLiteral("delete")));
    menu.addSeparator();
    if (singleFolder && items.first().isLocalFile()) {
        menu.addAction(actions->action(QStringLiteral("open_terminal")));
    }
    if (singleFolder) {
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
    }
    if (allLocal) {
        menu.addAction(actions->action(QStringLiteral("compress")));
    }
    menu.addAction(actions->action(QStringLiteral("copy_path")));

    m_fileItemActions->addActionsTo(&menu);

    menu.addSeparator();
    menu.addAction(actions->action(QStringLiteral("properties")));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == openAction) {
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
    FileOperations::rename(activePane()->view()->selectedItems(), this);
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
    m_left->setVisible(dual || activePane() == m_left);
    m_right->setVisible(dual || activePane() == m_right);
    updateActions();
}

void MainWindow::setStackedLayout(bool stacked)
{
    m_stackedLayoutAction->setChecked(stacked);
    m_splitter->setOrientation(stacked ? Qt::Vertical : Qt::Horizontal);
    m_splitter->setSizes({1, 1});
    m_left->setTabBarAtBottom(stacked);
}

bool MainWindow::isStackedLayout() const
{
    return m_splitter->orientation() == Qt::Vertical;
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
        tree->reveal(url);
    }
}

void MainWindow::showFolderTreeMenu(const QUrl &url, const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *openInTab = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"));
    QAction *openInOther = nullptr;
    if (m_dualLayoutAction->isChecked()) {
        openInOther = menu.addAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Other Pane"));
    }
    QAction *favorite = menu.addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18nc("@action:inmenu", "Add to Favorites"));
    favorite->setEnabled(!m_favorites->contains(url));
    menu.addSeparator();
    QAction *properties = menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action:inmenu", "Properties"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == openInTab) {
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
