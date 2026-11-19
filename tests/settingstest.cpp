// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"
#include "combinedpanel.h"
#include "compressdialog.h"
#include "fileoperations.h"
#include "foldermetadata.h"
#include "foldertree.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "previewpanel.h"
#include "settingsdialog.h"
#include "settingswidgets.h"
#include "tabmodel.h"

#include <KActionCollection>
#include <KDirModel>
#include <KFileItemDelegate>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KPageWidget>
#include <KSharedConfig>
#include <KToolBar>
#include <KUrlNavigator>
#include <KUrlRequester>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDropEvent>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>

#include <limits>

class SettingsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;

    QUrl url(const QString &relative) const
    {
        return QUrl::fromLocalFile(m_root.filePath(relative));
    }

    static KFilePlacesModel *placesOf(MainWindow &window)
    {
        return qobject_cast<KFilePlacesModel *>(window.findChild<KFilePlacesView *>()->model());
    }

    /// Title of the dialog tab that holds @p widget, or an empty string.
    static QString tabTitleOf(const QWidget *widget)
    {
        for (const QWidget *ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (const auto *tabs = qobject_cast<const QTabWidget *>(ancestor)) {
                for (int i = 0; i < tabs->count(); ++i) {
                    if (tabs->widget(i)->isAncestorOf(widget)) {
                        return tabs->tabText(i);
                    }
                }
            }
        }
        return {};
    }

    /// The sidebar the dialog currently shows.
    static QAbstractItemView *sidebarOf(SettingsDialog &dialog)
    {
        const auto views = dialog.findChildren<QAbstractItemView *>();
        for (QAbstractItemView *view : views) {
            if (view->model() == dialog.findChild<KPageWidget *>()->model() && view->isVisibleTo(&dialog)) {
                return view;
            }
        }
        return nullptr;
    }

    /// Theme name of the first sidebar page's icon.
    static QString firstPageIconName(SettingsDialog &dialog)
    {
        const QAbstractItemModel *pages = dialog.findChild<KPageWidget *>()->model();
        return pages->data(pages->index(0, 0), Qt::DecorationRole).value<QIcon>().name();
    }

    static QAction *action(MainWindow &window, const QString &name)
    {
        return window.actionCollection()->action(name);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString serviceMenus = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kio/servicemenus");
        QVERIFY(QDir().mkpath(serviceMenus));
        QFile serviceMenu(serviceMenus + QStringLiteral("/opusfm-settings-test.desktop"));
        QVERIFY(serviceMenu.open(QIODevice::WriteOnly));
        serviceMenu.write(
            "[Desktop Entry]\nType=Service\nMimeType=all/allfiles;\nActions=opusfmSettingsTest;\n\n"
            "[Desktop Action opusfmSettingsTest]\nName=Settings Test Action\nExec=true %F\n");
        serviceMenu.close();
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("c")));
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        QVERIFY(QFile::link(m_root.filePath(QStringLiteral("a")), m_root.filePath(QStringLiteral("link"))));
        m_a = url(QStringLiteral("a"));
        m_b = url(QStringLiteral("b"));
    }

    void init()
    {
        for (const QString &folder : {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}) {
            QFile::remove(m_root.filePath(folder + QStringLiteral("/.mfm.json")));
        }
        Settings::self()->setDefaults();
        // Closing windows with several tabs would otherwise block on a dialog.
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::self()->save();
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void dialogHasAWidgetForEverySetting()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const auto items = Settings::self()->items();
        for (const KConfigSkeletonItem *item : items) {
            QVERIFY2(dialog.findChild<QWidget *>(QStringLiteral("kcfg_") + item->name()), qPrintable(item->name()));
        }
    }

    void dialogApplyWritesSettings()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StartupMode"))->setCurrentIndex(Settings::EnumStartupMode::FixedFolders);
        dialog.findChild<KUrlRequester *>(QStringLiteral("kcfg_LeftStartUrl"))->setUrl(m_b);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FollowSymlinks"))->setChecked(true);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_FolderSizeRefreshInterval"))->setValue(60);
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(Settings::startupMode(), Settings::EnumStartupMode::FixedFolders);
        QCOMPARE(Settings::leftStartUrl(), m_b);
        QVERIFY(Settings::followSymlinks());
        QCOMPARE(Settings::folderSizeRefreshInterval(), 60);
    }

    void pagesGroupFeaturesInTabs()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const QAbstractItemModel *pages = dialog.findChild<KPageWidget *>()->model();
        QStringList titles;
        for (int row = 0; row < pages->rowCount(); ++row) {
            titles.append(pages->index(row, 0).data().toString());
        }
        QCOMPARE(titles,
                 QStringList({QStringLiteral("General"),
                              QStringLiteral("Interface"),
                              QStringLiteral("View"),
                              QStringLiteral("Context Menu"),
                              QStringLiteral("Archives"),
                              QStringLiteral("Files"),
                              QStringLiteral("Gallery")}));

        const auto tabOf = [&dialog](const QString &entry) {
            return tabTitleOf(dialog.findChild<QWidget *>(QStringLiteral("kcfg_") + entry));
        };
        QCOMPARE(tabOf(QStringLiteral("SidebarTitlePosition")), QStringLiteral("Settings Dialog"));
        QCOMPARE(tabOf(QStringLiteral("StartupMode")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("FollowSymlinks")), QStringLiteral("Folders and Tabs"));
        QCOMPARE(tabOf(QStringLiteral("ShowPreviews")), QStringLiteral("Previews"));
        QCOMPARE(tabOf(QStringLiteral("ConfirmClosingMultipleTabs")), QStringLiteral("Confirmations"));
        QCOMPARE(tabOf(QStringLiteral("ExecutableAction")), QStringLiteral("Confirmations"));
        QCOMPARE(tabOf(QStringLiteral("DropAction")), QStringLiteral("Confirmations"));
        QCOMPARE(tabTitleOf(dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmTrash"))), QStringLiteral("Confirmations"));
        QCOMPARE(tabOf(QStringLiteral("ShowPlaces")), QStringLiteral("Panels"));
        QCOMPARE(tabOf(QStringLiteral("ShowFreeSpace")), QStringLiteral("Status and Location Bars"));
        QCOMPARE(tabOf(QStringLiteral("DefaultViewMode")), QStringLiteral("General"));
        QCOMPARE(tabOf(QStringLiteral("FolderSizeRefreshInterval")), QStringLiteral("General"));
        QCOMPARE(tabOf(QStringLiteral("DetailsIconSize")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("DefaultSelectionMarkers")), QStringLiteral("Selection"));
        QCOMPARE(tabOf(QStringLiteral("DefaultFilterScope")), QStringLiteral("Filters"));
        QCOMPARE(tabOf(QStringLiteral("CompactIconSize")), QStringLiteral("Compact"));
        QCOMPARE(tabOf(QStringLiteral("AutomaticLocalFolderSizes")), QStringLiteral("Folder Formats"));
        QCOMPARE(tabOf(QStringLiteral("ShowFilePins")), QStringLiteral("Metadata"));
        QCOMPARE(tabOf(QStringLiteral("ShowSizeColumn")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("DragFromAnywhere")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("ShowOpenInNewTab")), QStringLiteral("Actions"));
        QCOMPARE(tabOf(QStringLiteral("ShowCopyMoveToMenus")), QStringLiteral("Actions"));
        QCOMPARE(tabOf(QStringLiteral("DetailsDensity")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("IconsDensity")), QStringLiteral("Icons"));
        QCOMPARE(tabOf(QStringLiteral("BarDensity")), QStringLiteral("Status and Location Bars"));
        QCOMPARE(tabOf(QStringLiteral("IconsIconSize")), QStringLiteral("Icons"));
        QCOMPARE(tabOf(QStringLiteral("TextPreviewLimitKiB")), QStringLiteral("Previews"));
        QCOMPARE(tabOf(QStringLiteral("FileActivationDestination")), QStringLiteral("Opening"));
        QCOMPARE(tabOf(QStringLiteral("GalleryShowInformation")), QStringLiteral("Information"));
        QCOMPARE(tabOf(QStringLiteral("GallerySlideshowInterval")), QStringLiteral("Slideshow"));
        QCOMPARE(tabOf(QStringLiteral("GalleryCopyFormat")), QStringLiteral("Image Conversion"));
    }

    void gallerySettingsApplyAndRestoreDefaults()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *information = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GalleryShowInformation"));
        auto *interval = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GallerySlideshowInterval"));
        auto *loop = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GallerySlideshowLoop"));
        auto *textLimit = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_TextPreviewLimitKiB"));
        auto *format = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GalleryCopyFormat"));
        auto *quality = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_GalleryCopyQuality"));
        auto *destination = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_FileActivationDestination"));
        QVERIFY(information && interval && loop && textLimit && format && quality && destination);
        information->setChecked(true);
        interval->setValue(8);
        loop->setChecked(false);
        textLimit->setValue(12);
        format->setCurrentIndex(Settings::EnumGalleryCopyFormat::Webp);
        quality->setValue(72);
        destination->setCurrentIndex(Settings::EnumFileActivationDestination::Preview);
        dialog.button(QDialogButtonBox::Apply)->click();
        Settings::self()->load();
        QVERIFY(Settings::galleryShowInformation());
        QCOMPARE(Settings::gallerySlideshowInterval(), 8);
        QVERIFY(!Settings::gallerySlideshowLoop());
        QCOMPARE(Settings::textPreviewLimitKiB(), 12);
        QCOMPARE(Settings::galleryCopyFormat(), Settings::EnumGalleryCopyFormat::Webp);
        QCOMPARE(Settings::galleryCopyQuality(), 72);
        QCOMPARE(Settings::fileActivationDestination(), Settings::EnumFileActivationDestination::Preview);
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(!information->isChecked());
        QCOMPARE(interval->value(), 3);
        QVERIFY(loop->isChecked());
        QCOMPARE(textLimit->value(), 1024);
        QCOMPARE(format->currentIndex(), Settings::EnumGalleryCopyFormat::Png);
        QCOMPARE(quality->value(), 90);
        QCOMPARE(destination->currentIndex(), Settings::EnumFileActivationDestination::Application);
    }

    void applyingTextLimitRefreshesCurrentPreview()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("text.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray(2500, 'x')), 2500);
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        action(window, QStringLiteral("show_preview_panel"))->setChecked(true);
        auto *text = window.findChild<PreviewPanel *>()->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QVERIFY(text);
        QTRY_COMPARE(text->toPlainText().size(), 2500);
        action(window, QStringLiteral("options_configure"))->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        dialog->findChild<QSpinBox *>(QStringLiteral("kcfg_TextPreviewLimitKiB"))->setValue(1);
        dialog->button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(text->toPlainText().size(), 1024);
        dialog->close();
    }

    void sidebarDefaultsToSmallSymbolicIconsBesideTitles()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        QCOMPARE(dialog.findChild<KPageWidget *>()->faceType(), KPageView::FlatList);
        QVERIFY(sidebarOf(dialog));
        QCOMPARE(sidebarOf(dialog)->iconSize(), QSize(24, 24));
        QCOMPARE(firstPageIconName(dialog), QStringLiteral("configure-symbolic"));
    }

    void sidebarStyleChangesOnApply()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"))->setCurrentIndex(Settings::EnumSidebarTitlePosition::Below);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"))->setValue(32);
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(dialog.findChild<KPageWidget *>()->faceType(), KPageView::List);
        QVERIFY(sidebarOf(dialog));
        QCOMPARE(sidebarOf(dialog)->iconSize(), QSize(32, 32));
        QCOMPARE(firstPageIconName(dialog), QStringLiteral("configure"));
    }

    void m04AppearanceAndArchiveSettingsApply()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FavoritesVisible"))->setChecked(false);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_PanelPosition"))->setCurrentIndex(Settings::EnumPanelPosition::right);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TabIcons"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ExpandTabs"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTabColors"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ArchiveOtherPaneByDefault"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_InlineFolderExpansion"))->setChecked(false);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DetailsColumnWidth"))->setCurrentIndex(Settings::EnumDetailsColumnWidth::Automatic);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DefaultSelectionMarkers"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DefaultSelectionMode"))->setChecked(true);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultArchiveFormat"))->setCurrentIndex(static_cast<int>(ArchiveWriter::Format::TarXz));
        dialog.button(QDialogButtonBox::Apply)->click();

        QVERIFY(!Settings::favoritesVisible());
        QCOMPARE(Settings::panelPosition(), Settings::EnumPanelPosition::right);
        QCOMPARE(Settings::defaultArchiveFormat(), static_cast<int>(ArchiveWriter::Format::TarXz));
        QVERIFY(Settings::archiveOtherPaneByDefault());
        QVERIFY(!Settings::showTabColors());
        auto *tree = qobject_cast<QTreeView *>(window.activePane()->view()->currentView());
        QVERIFY(tree);
        QVERIFY(!tree->itemsExpandable());
        QCOMPARE(tree->header()->sectionResizeMode(KDirModel::Size), QHeaderView::ResizeToContents);
        QVERIFY(window.activePane()->view()->selectionMarkersVisible());
        QVERIFY(window.activePane()->view()->selectionMode());
        QVERIFY(action(window, QStringLiteral("selection_markers"))->isChecked());
        QVERIFY(action(window, QStringLiteral("selection_mode"))->isChecked());
        window.activePane()->view()->setSelectionMode(false);
        window.applySettings();
        QVERIFY(!window.activePane()->view()->selectionMode());
        const auto config = KSharedConfig::openConfig();
        QCOMPARE(config->group(QStringLiteral("PanelSettings")).readEntry("Position", QString()), QStringLiteral("right"));
        QCOMPARE(config->group(QStringLiteral("Archives")).readEntry("DefaultFormat", -1), static_cast<int>(ArchiveWriter::Format::TarXz));
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("combinedDock"));
        QVERIFY(dock);
        QCOMPARE(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        auto *bar = window.activePane()->findChild<QTabBar *>();
        QVERIFY(bar);
        QVERIFY(!bar->expanding());
        QVERIFY(bar->tabIcon(0).isNull());
        CompressDialog archive(QStringLiteral("test"), false, &window, static_cast<ArchiveWriter::Format>(Settings::defaultArchiveFormat()));
        QCOMPARE(archive.format(), ArchiveWriter::Format::TarXz);
        CompressDialog otherArchive(QStringLiteral("test"), true, &window, ArchiveWriter::Format::Zip, Settings::archiveOtherPaneByDefault());
        QVERIFY(otherArchive.toOtherPane());
        CompressDialog unavailable(QStringLiteral("test"), false, &window, ArchiveWriter::Format::Zip, Settings::archiveOtherPaneByDefault());
        QVERIFY(!unavailable.toOtherPane());

        QVERIFY(dialog.button(QDialogButtonBox::RestoreDefaults)->isEnabled());
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FavoritesVisible"))->isChecked());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::favoritesVisible());
        QCOMPARE(Settings::panelPosition(), Settings::EnumPanelPosition::left);
        QCOMPARE(Settings::defaultArchiveFormat(), static_cast<int>(ArchiveWriter::Format::Zip));
        QVERIFY(!Settings::archiveOtherPaneByDefault());
        QVERIFY(Settings::showTabColors());
        QVERIFY(tree->itemsExpandable());
        QVERIFY(!window.activePane()->view()->selectionMarkersVisible());
        QCOMPARE(tree->header()->sectionResizeMode(KDirModel::Name), QHeaderView::Stretch);
        QVERIFY(bar->expanding());
    }

    void compactViewSettingsApply()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CompactIconSize"))->setValue(32);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CompactItemSpacing"))->setValue(5);
        auto *information = dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_CompactInformation"));
        QVERIFY(information);
        information->setInformation({QStringLiteral("size"), QStringLiteral("owner")});
        dialog.button(QDialogButtonBox::Apply)->click();
        FolderView *view = window.activePane()->view();
        view->setViewMode(FolderView::ViewMode::Compact);
        auto *compact = qobject_cast<QListView *>(view->currentView());
        QVERIFY(compact);
        QCOMPARE(compact->iconSize(), QSize(32, 32));
        QCOMPARE(compact->spacing(), 5);
        auto *delegate = qobject_cast<KFileItemDelegate *>(compact->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), (KFileItemDelegate::InformationList{KFileItemDelegate::Size, KFileItemDelegate::Owner}));
    }

    void selectionFiltersCanBeManagedInSettings()
    {
        KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("SelectionFilters"));
        group.writeEntry("Patterns", QStringList{QStringLiteral("*.txt")});
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        auto *editor = dialog.findChild<RuleListEditor *>(QStringLiteral("selectionFilterRules"));
        QVERIFY(editor);
        QCOMPARE(editor->rules(), QStringList{QStringLiteral("*.txt")});
        editor->setRules({QStringLiteral("*.jpg"), QStringLiteral("*.cpp")});
        QCOMPARE(group.readEntry("Patterns", QStringList()), QStringList{QStringLiteral("*.txt")});
        QVERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), (QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.cpp")}));
        QAction *select = action(window, QStringLiteral("select_menu"));
        QMenu *saved = nullptr;
        for (QAction *entry : select->menu()->actions()) {
            if (entry->menu() && entry->text() == QLatin1String("Saved Selection Filters")) {
                saved = entry->menu();
            }
        }
        QVERIFY(saved);
        QMetaObject::invokeMethod(saved, "aboutToShow");
        QCOMPARE(saved->actions().at(0)->text(), QStringLiteral("*.jpg"));
        QCOMPARE(saved->actions().at(1)->text(), QStringLiteral("*.cpp"));
        editor->setRules({QStringLiteral("*.jpg")});
        group.writeEntry("Patterns", QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.cpp"), QStringLiteral("*.png")});
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), (QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.png")}));
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(group.readEntry("Patterns", QStringList()).isEmpty());
        editor->setRules({QStringLiteral("*.jpg"), QStringLiteral("*.png")});
        dialog.button(QDialogButtonBox::Apply)->click();
        group.writeEntry("Patterns", QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.png"), QStringLiteral("*.gif")});
        auto *showLabels = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowFileLabels"));
        QVERIFY(showLabels);
        showLabels->setChecked(!showLabels->isChecked());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), (QStringList{QStringLiteral("*.jpg"), QStringLiteral("*.png"), QStringLiteral("*.gif")}));
        showLabels->setChecked(!showLabels->isChecked());
        dialog.button(QDialogButtonBox::Apply)->click();
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(group.readEntry("Patterns", QStringList()), QStringList{QStringLiteral("*.gif")});
        group.deleteEntry("Patterns");
    }

    void folderSizeDefaultsApplyAndMenuStaysInSync()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_AutomaticLocalFolderSizes"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_AutomaticRemoteFolderSizes"))->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(window.activePane()->view()->automaticFolderSizes());
        QVERIFY(action(window, QStringLiteral("automatic_local_folder_sizes"))->isChecked());
        QVERIFY(!action(window, QStringLiteral("automatic_remote_folder_sizes"))->isChecked());
        action(window, QStringLiteral("automatic_local_folder_sizes"))->trigger();
        QVERIFY(!Settings::automaticLocalFolderSizes());
        QVERIFY(!window.activePane()->view()->automaticFolderSizes());
        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("FolderSizes")).readEntry("AutomaticLocal", false), false);
    }

    void inheritedFormatSourceIsAccessibleFromViewMenu()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("child")));
        FolderMetadata::Format format{QStringLiteral("list"), {}, QStringLiteral("name"), true, QStringLiteral("none"), true};
        QVERIFY(FolderMetadata::setFormat(root.path(), format));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("child")))});
        QAction *openSource = action(window, QStringLiteral("open_folder_format_source"));
        QAction *clear = action(window, QStringLiteral("clear_folder_format"));
        QVERIFY(openSource->isEnabled());
        QVERIFY(!clear->isEnabled());
        openSource->trigger();
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(root.path()));
        QVERIFY(clear->isEnabled());
        action(window, QStringLiteral("lock_folder_format"))->trigger();
        QVERIFY(!clear->isEnabled());
    }

    void hiddenPanelSectionsKeepTheirCollapsedState()
    {
        MainWindow window;
        auto *panel = window.findChild<CombinedPanel *>();
        QVERIFY(panel);
        panel->setCollapsedSections({QStringLiteral("tree")});
        panel->setSectionVisibility(false, false, false);
        QCOMPARE(panel->collapsedSections(), QStringList({QStringLiteral("tree")}));
        panel->setSectionVisibility(true, true, true);
        QCOMPARE(panel->collapsedSections(), QStringList({QStringLiteral("tree")}));
    }

    void listingDefaultsApplyAndPreserveFolderGrouping()
    {
        MainWindow window;
        window.openUrls({m_a});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GroupBy"))->setCurrentIndex(Settings::EnumGroupBy::Type);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SizeFormat"))->setCurrentIndex(Settings::EnumSizeFormat::bytes);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DateFormat"))->setCurrentIndex(Settings::EnumDateFormat::iso);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowSeconds"))->setChecked(true);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_IconNameLines"))->setValue(4);
        auto *information = dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_IconInformation"));
        QVERIFY(information);
        information->setInformation({QStringLiteral("size")});
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("View")).readEntry("GroupBy", QString()), QStringLiteral("type"));
        QCOMPARE(Settings::iconInformation(), QStringList({QStringLiteral("size")}));
        FolderView *view = window.activePane()->view();
        QCOMPARE(view->groupBy(), QStringLiteral("type"));
        QTRY_COMPARE(view->itemCount(), 1);
        const QAbstractItemModel *model = view->currentView()->model();
        QCOMPARE(model->index(0, KDirModel::Size).data().toString(), QStringLiteral("5 B"));
        QVERIFY(model->index(0, KDirModel::ModifiedTime)
                    .data()
                    .toString()
                    .contains(QRegularExpression(QStringLiteral("^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}$"))));
        view->setViewMode(FolderView::ViewMode::Icons);
        auto *icons = qobject_cast<QListView *>(view->currentView());
        QVERIFY(icons);
        auto *delegate = qobject_cast<KFileItemDelegate *>(icons->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), KFileItemDelegate::InformationList({KFileItemDelegate::Size}));
        view->setGroupBy(QStringLiteral("name"));
        window.applySettings();
        QCOMPARE(view->groupBy(), QStringLiteral("name"));
    }

    void fileRulesApplyToExistingListing()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *colors = dialog.findChild<RuleListEditor *>(QStringLiteral("kcfg_ExtensionColors"));
        auto *ignored = dialog.findChild<RuleListEditor *>(QStringLiteral("kcfg_IgnoreList"));
        QVERIFY(colors && ignored);
        colors->setRules({QStringLiteral("txt=#123456")});
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(view->currentView()->model()->index(0, 0).data(Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#123456")));
        ignored->setRules({QStringLiteral("*.txt")});
        dialog.button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(view->itemCount(), 0);
    }

    void kioConfirmationsAreSharedWithDolphin()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *trash = dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmTrash"));
        auto *remove = dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmDelete"));
        QVERIFY(trash && remove);
        trash->setChecked(true);
        remove->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();

        const KConfigGroup kio = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("Confirmations"));
        QVERIFY(kio.readEntry("ConfirmTrash", false));
        QVERIFY(!kio.readEntry("ConfirmDelete", true));
    }

    void permanentDeletionHonorsSharedConfirmation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("delete.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        KConfigGroup confirmations = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("Confirmations"));
        confirmations.writeEntry("ConfirmDelete", true);
        confirmations.sync();
        bool prompted = false;
        QTimer prompt;
        connect(&prompt, &QTimer::timeout, &window, [&prompted] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                prompted = true;
                dialog->reject();
            }
        });
        prompt.start(10);
        FileOperations::deletePermanently({QUrl::fromLocalFile(path)}, &window);
        QTRY_VERIFY(prompted);
        QVERIFY(QFile::exists(path));
        prompted = false;
        confirmations.writeEntry("ConfirmDelete", false);
        confirmations.sync();
        FileOperations::deletePermanently({QUrl::fromLocalFile(path)}, &window);
        QTRY_VERIFY(!QFile::exists(path));
        QVERIFY(!prompted);
        prompt.stop();
    }

    void closingWithSeveralTabsAsks()
    {
        Settings::setConfirmClosingMultipleTabs(true);
        MainWindow window;
        window.openUrls({m_a});
        window.activePane()->openInNewTab(m_b);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        // Accepts or rejects the confirmation once it is shown.
        const auto answer = [&window](bool accept) {
            auto *poll = new QTimer(&window);
            QObject::connect(poll, &QTimer::timeout, poll, [poll, accept] {
                const auto boxes =
                    QApplication::activeModalWidget() ? QApplication::activeModalWidget()->findChildren<QDialogButtonBox *>() : QList<QDialogButtonBox *>();
                for (QDialogButtonBox *box : boxes) {
                    const auto buttons = box->buttons();
                    for (QAbstractButton *button : buttons) {
                        const auto role = box->buttonRole(button);
                        const bool accepting = role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole;
                        const bool rejecting = role == QDialogButtonBox::RejectRole || role == QDialogButtonBox::NoRole;
                        if (accept ? accepting : rejecting) {
                            poll->deleteLater();
                            button->click();
                            return;
                        }
                    }
                }
            });
            poll->start(20);
        };
        answer(false);
        QVERIFY(!window.close());
        answer(true);
        QVERIFY(window.close());
    }

    void dropWithoutModifierCopiesOrMoves()
    {
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("drop")));
        QFile copyMe(m_root.filePath(QStringLiteral("copy-me.txt")));
        QVERIFY(copyMe.open(QIODevice::WriteOnly));
        copyMe.close();
        QFile moveMe(m_root.filePath(QStringLiteral("move-me.txt")));
        QVERIFY(moveMe.open(QIODevice::WriteOnly));
        moveMe.close();

        FolderView view;
        view.setUrl(url(QStringLiteral("drop")));
        view.resize(400, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QWidget *viewport = view.currentView()->viewport();

        const auto drop = [viewport](const QUrl &source) {
            QMimeData mime;
            mime.setUrls({source});
            const QPoint position(5, 200);
            QDragEnterEvent enter(position, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &enter);
            QDragMoveEvent move(position, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &move);
            QDropEvent dropEvent(position, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &dropEvent);
        };

        Settings::setDropAction(Settings::EnumDropAction::Copy);
        drop(url(QStringLiteral("copy-me.txt")));
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("drop/copy-me.txt"))));
        QVERIFY(QFile::exists(m_root.filePath(QStringLiteral("copy-me.txt"))));

        Settings::setDropAction(Settings::EnumDropAction::Move);
        drop(url(QStringLiteral("move-me.txt")));
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("drop/move-me.txt"))));
        QTRY_VERIFY(!QFile::exists(m_root.filePath(QStringLiteral("move-me.txt"))));
    }

    void panelDropsHonorTheDefaultAction()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        const QUrl destination = QUrl::fromLocalFile(root.filePath(QStringLiteral("destination")));
        MainWindow window;
        auto *combined = window.findChild<CombinedPanel *>();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("foldersDock"));
        QVERIFY(combined && dock);
        auto *tree = dock->findChild<FolderTree *>();
        QVERIFY(tree);
        const auto drop = [&root, &destination](auto *panel, const QString &name, Qt::KeyboardModifiers modifiers) {
            QFile file(root.filePath(name));
            if (!file.open(QIODevice::WriteOnly)) {
                return false;
            }
            file.close();
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(root.filePath(name))});
            QDropEvent event(QPointF(0, 0), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, modifiers);
            Q_EMIT panel->urlsDropped(destination, &event);
            return true;
        };
        Settings::setDropAction(Settings::EnumDropAction::Copy);
        QVERIFY(drop(tree, QStringLiteral("tree-copy.txt"), Qt::NoModifier));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/tree-copy.txt"))));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("tree-copy.txt"))));
        Settings::setDropAction(Settings::EnumDropAction::Move);
        QVERIFY(drop(combined, QStringLiteral("combined-move.txt"), Qt::NoModifier));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/combined-move.txt"))));
        QTRY_VERIFY(!QFile::exists(root.filePath(QStringLiteral("combined-move.txt"))));
        QVERIFY(drop(combined, QStringLiteral("modifier-copy.txt"), Qt::ControlModifier));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/modifier-copy.txt"))));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("modifier-copy.txt"))));
    }

    void pressingBesideTheNameDrawsASelectionRectangle()
    {
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("rows")));
        for (const char *name : {"r1.txt", "r2.txt", "r3.txt"}) {
            QFile file(m_root.filePath(QStringLiteral("rows/") + QLatin1String(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FolderView view;
        view.setDragFromAnywhere(false);
        view.setUrl(url(QStringLiteral("rows")));
        view.resize(700, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_COMPARE(view.itemCount(), 3);

        auto *details = view.currentView();
        QWidget *viewport = details->viewport();
        const QAbstractItemModel *model = details->model();
        const QPoint from = details->visualRect(model->index(0, KDirModel::ModifiedTime)).center();
        const QPoint to = details->visualRect(model->index(2, KDirModel::ModifiedTime)).center();
        const auto send = [viewport](QEvent::Type type, const QPoint &pos, Qt::MouseButtons buttons) {
            QMouseEvent event(type, pos, viewport->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
            QApplication::sendEvent(viewport, &event);
        };
        send(QEvent::MouseButtonPress, from, Qt::LeftButton);
        send(QEvent::MouseMove, to, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, to, Qt::NoButton);
        QCOMPARE(view.selectedItems().size(), 3);

        send(QEvent::MouseButtonPress, to, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, to, Qt::NoButton);
        QCOMPARE(view.selectedItems().size(), 1);
    }

    void itemDensityChangesRowHeightAndIconSpacing()
    {
        FolderView view;
        view.setUrl(m_a);
        view.resize(600, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_VERIFY(view.itemCount() > 0);
        auto *details = view.currentView();
        const auto rowHeight = [details] {
            details->doItemsLayout();
            return details->visualRect(details->model()->index(0, 0)).height();
        };

        view.setItemSpacing(2, 2);
        const int normal = rowHeight();
        view.setItemSpacing(0, 0);
        QCOMPARE(rowHeight(), normal - 4);
        view.setItemSpacing(10, 10);
        QCOMPARE(rowHeight(), normal + 16);

        view.setViewMode(FolderView::ViewMode::Icons);
        QCOMPARE(qobject_cast<QListView *>(view.currentView())->spacing(), 20);
    }

    void barDensityTightensBars()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto barsHeight = [&window] {
            return window.toolBar()->sizeHint().height() + window.statusBar()->sizeHint().height();
        };
        const int normal = barsHeight();

        Settings::setBarDensity(Settings::EnumBarDensity::VeryCompact);
        window.applySettings();
        QVERIFY(barsHeight() < normal);

        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        window.applySettings();
        QCOMPARE(barsHeight(), normal);
    }

    void customBarSpacingAppliesAndRestores()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *density = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_BarDensity"));
        auto *spacing = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_BarCustomSpacing"));
        QVERIFY(density && spacing);
        QVERIFY(!spacing->isEnabled());
        density->setCurrentIndex(Settings::EnumBarDensity::Custom);
        QVERIFY(spacing->isEnabled());
        spacing->setValue(11);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::barDensity(), Settings::EnumBarDensity::Custom);
        QCOMPARE(Settings::barCustomSpacing(), 11);
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        window.applySettings();
        QLayout *toolbar = window.toolBar()->layout();
        QLayout *navigation = window.activePane()->navigationBar()->layout();
        const QMargins initialToolbar = toolbar->contentsMargins();
        const QMargins initialNavigation = navigation->contentsMargins();
        Settings::setBarDensity(Settings::EnumBarDensity::Custom);
        Settings::setBarCustomSpacing(11);
        window.applySettings();
        QCOMPARE(toolbar->contentsMargins().top(), 11);
        QCOMPARE(navigation->contentsMargins().top(), 11);
        Settings::setBarDensity(Settings::EnumBarDensity::Normal);
        window.applySettings();
        QCOMPARE(toolbar->contentsMargins(), initialToolbar);
        QCOMPARE(navigation->contentsMargins(), initialNavigation);
    }

    void contextListsSaveUnavailableEntries()
    {
        MainWindow window;
        Settings::setHiddenServiceActions({QStringLiteral("not-installed")});
        SettingsDialog dialog(placesOf(window), &window);
        auto *actions = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_HiddenContextActions"));
        auto *services = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_HiddenServiceActions"));
        auto *copy = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_CopyToSubmenu"));
        auto *move = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_MoveToSubmenu"));
        QVERIFY(actions && services && copy && move);
        QListWidgetItem *rename = nullptr;
        QListWidgetItem *service = nullptr;
        for (int row = 0; row < actions->count(); ++row) {
            if (actions->item(row)->data(Qt::UserRole).toString() == QLatin1String("rename")) {
                rename = actions->item(row);
            }
        }
        for (int row = 0; row < services->count(); ++row) {
            if (services->item(row)->data(Qt::UserRole).toString() == QLatin1String("opusfmSettingsTest")) {
                service = services->item(row);
            }
        }
        QVERIFY(rename && service);
        rename->setCheckState(Qt::Unchecked);
        service->setCheckState(Qt::Unchecked);
        copy->setChecked(true);
        move->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::copyToSubmenu());
        QVERIFY(Settings::moveToSubmenu());
        QVERIFY(Settings::hiddenContextActions().contains(QStringLiteral("rename")));
        QVERIFY(Settings::hiddenServiceActions().contains(QStringLiteral("opusfmSettingsTest")));
        QVERIFY(Settings::hiddenServiceActions().contains(QStringLiteral("not-installed")));
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::hiddenContextActions().isEmpty());
        QVERIFY(Settings::hiddenServiceActions().isEmpty());
        QVERIFY(!Settings::copyToSubmenu());
        QVERIFY(!Settings::moveToSubmenu());
    }

    void panelNavigationCanFocusTheActivePane()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *panel = window.findChild<CombinedPanel *>();
        QVERIFY(panel);
        auto *tree = panel->folderTree()->findChild<QTreeView *>();
        QVERIFY(tree);
        tree->setFocus();
        QTRY_VERIFY(tree->hasFocus());
        Settings::setAutoFocusPaneAfterPanelNavigation(false);
        Q_EMIT panel->urlActivated(m_b);
        QCOMPARE(window.activePane()->url(), m_b);
        QVERIFY(tree->hasFocus());
        Settings::setAutoFocusPaneAfterPanelNavigation(true);
        Q_EMIT panel->urlActivated(m_a);
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());

        window.setPanelLayout(MainWindow::PanelLayout::Separate);
        auto *places = window.findChild<KFilePlacesView *>();
        QVERIFY(places);
        Settings::setAutoFocusPaneAfterPanelNavigation(false);
        places->setFocus();
        QVERIFY(QMetaObject::invokeMethod(places, "urlChanged", Qt::DirectConnection, Q_ARG(QUrl, m_b)));
        QCOMPARE(window.activePane()->url(), m_b);
        QVERIFY(places->hasFocus());
        Settings::setAutoFocusPaneAfterPanelNavigation(true);
        QVERIFY(QMetaObject::invokeMethod(places, "urlChanged", Qt::DirectConnection, Q_ARG(QUrl, m_a)));
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());
        auto *foldersDock = window.findChild<QDockWidget *>(QStringLiteral("foldersDock"));
        QVERIFY(foldersDock);
        auto *folderTree = foldersDock->findChild<FolderTree *>();
        QVERIFY(folderTree);
        Q_EMIT folderTree->urlActivated(m_b);
        QCOMPARE(window.activePane()->url(), m_b);
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());
    }

    void contextMenuFollowsSettings()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();

        // Opens the context menu for folder b and returns its entries, then closes it.
        const auto menuTexts = [view, this] {
            QStringList texts;
            auto *poll = new QTimer;
            QObject::connect(poll, &QTimer::timeout, poll, [poll, &texts] {
                auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                if (!menu) {
                    return;
                }
                const auto actions = menu->actions();
                for (const QAction *action : actions) {
                    texts.append(action->text().remove(u'&'));
                }
                poll->deleteLater();
                menu->close();
            });
            poll->start(20);
            Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), {KFileItem(m_b)});
            return texts;
        };

        const QStringList defaults = menuTexts();
        QVERIFY(defaults.contains(QStringLiteral("Open in New Tab")));
        QVERIFY(defaults.contains(QStringLiteral("Copy to Other Pane")));
        QVERIFY(defaults.contains(QStringLiteral("Delete Permanently")));
        QVERIFY(!defaults.contains(QStringLiteral("Copy To")));

        Settings::setShowOpenInNewTab(false);
        Settings::setShowCopyToOtherPane(false);
        Settings::setShowDelete(false);
        Settings::setShowCopyMoveToMenus(true);
        const QStringList changed = menuTexts();
        QVERIFY(!changed.contains(QStringLiteral("Open in New Tab")));
        QVERIFY(!changed.contains(QStringLiteral("Copy to Other Pane")));
        QVERIFY(!changed.contains(QStringLiteral("Delete Permanently")));
        QVERIFY(changed.contains(QStringLiteral("Copy To")));
        QVERIFY(changed.contains(QStringLiteral("Move To")));
    }

    void contextListsAndTransferMenusApply()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("source")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        QFile file(root.filePath(QStringLiteral("source/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QUrl source = QUrl::fromLocalFile(root.filePath(QStringLiteral("source")));
        const QUrl destination = QUrl::fromLocalFile(root.filePath(QStringLiteral("destination")));
        MainWindow window;
        window.openUrls({source, destination});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectAll();
        const KFileItemList items = view->selectedItems();
        QCOMPARE(items.size(), 1);
        QStringList defaultActions;
        QTimer::singleShot(0, [&defaultActions] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            const auto collect = [&defaultActions](auto &&self, QMenu *current) -> void {
                for (QAction *action : current->actions()) {
                    defaultActions.append(action->text().remove(u'&'));
                    if (action->menu()) {
                        self(self, action->menu());
                    }
                }
            };
            collect(collect, menu);
            menu->close();
        });
        Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), items);
        QVERIFY(defaultActions.contains(QStringLiteral("Settings Test Action")));
        Settings::setCopyToSubmenu(true);
        Settings::setMoveToSubmenu(true);
        Settings::setHiddenContextActions({QStringLiteral("open"), QStringLiteral("rename")});
        Settings::setHiddenServiceActions({QStringLiteral("opusfmSettingsTest")});
        QStringList actions;
        QTimer::singleShot(0, [&actions] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            const auto collect = [&actions](auto &&self, QMenu *current) -> void {
                for (QAction *action : current->actions()) {
                    actions.append(action->text().remove(u'&'));
                    if (action->menu()) {
                        self(self, action->menu());
                    }
                }
            };
            collect(collect, menu);
            auto *copy = menu->findChild<QMenu *>(QStringLiteral("copyToMenu"));
            auto *move = menu->findChild<QMenu *>(QStringLiteral("moveToMenu"));
            if (copy && move) {
                actions.append(copy->objectName());
                actions.append(move->objectName());
                copy->actions().first()->trigger();
            }
            menu->close();
        });
        Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), items);
        QVERIFY(actions.contains(QStringLiteral("copyToMenu")));
        QVERIFY(actions.contains(QStringLiteral("moveToMenu")));
        QVERIFY(!actions.contains(QStringLiteral("Open")));
        QVERIFY(!actions.contains(QStringLiteral("Rename")));
        QVERIFY(!actions.contains(QStringLiteral("Settings Test Action")));
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/file.txt"))));

        QTimer::singleShot(0, [] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (menu) {
                if (auto *move = menu->findChild<QMenu *>(QStringLiteral("moveToMenu"))) {
                    move->actions().first()->trigger();
                }
                menu->close();
            }
        });
        QFile::remove(root.filePath(QStringLiteral("destination/file.txt")));
        Q_EMIT view->contextMenuRequested(view->mapToGlobal(QPoint(10, 10)), items);
        QTRY_VERIFY(QFile::exists(root.filePath(QStringLiteral("destination/file.txt"))));
        QVERIFY(!QFile::exists(root.filePath(QStringLiteral("source/file.txt"))));
    }

    void serviceMenusAreSharedWithDolphin()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        const auto checks = dialog.findChildren<QCheckBox *>();
        QCheckBox *service = nullptr;
        for (QCheckBox *check : checks) {
            if (check->objectName().startsWith(QLatin1String("serviceMenu_"))) {
                service = check;
                break;
            }
        }
        QVERIFY(service);
        QCOMPARE(tabTitleOf(service), QStringLiteral("Service Menus"));
        const QString key = service->objectName().mid(QStringLiteral("serviceMenu_").size());
        service->setChecked(!service->isChecked());
        const bool wanted = service->isChecked();
        dialog.button(QDialogButtonBox::Apply)->click();

        const KConfig config(QStringLiteral("kservicemenurc"), KConfig::NoGlobals);
        QCOMPARE(config.group(QStringLiteral("Show")).readEntry(key, !wanted), wanted);
    }

    void runningAnExecutableRunsIt()
    {
        const QString marker = m_root.filePath(QStringLiteral("ran"));
        const QString script = m_root.filePath(QStringLiteral("touch-marker.sh"));
        QFile file(script);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\ntouch '" + marker.toUtf8() + "'\n");
        file.close();
        QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));

        Settings::setExecutableAction(Settings::EnumExecutableAction::Run);
        MainWindow window;
        FileOperations::open(KFileItem(QUrl::fromLocalFile(script)), &window);
        QTRY_VERIFY(QFile::exists(marker));
    }

    void startupHomeFolderIgnoresSavedTabs()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.close();
        }
        Settings::setStartupMode(Settings::EnumStartupMode::HomeFolder);
        Settings::setHomeUrl(url(QStringLiteral("c")));

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->tabs()->urls(), QList<QUrl>{url(QStringLiteral("c"))});
        QCOMPARE(window.inactivePane()->tabs()->urls(), QList<QUrl>{url(QStringLiteral("c"))});
    }

    void startupFixedFolders()
    {
        Settings::setStartupMode(Settings::EnumStartupMode::FixedFolders);
        Settings::setLeftStartUrl(m_a);
        Settings::setRightStartUrl(m_b);

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
    }

    void singlePaneLayoutIsRememberedAsLastUsed()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            action(window, QStringLiteral("dual_layout"))->trigger();
            QVERIFY(!window.isDualLayout());
            window.close();
        }
        MainWindow restored;
        restored.openUrls({});
        QVERIFY(!restored.isDualLayout());
    }

    void dualPaneAlwaysOverridesLastUsed()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            action(window, QStringLiteral("dual_layout"))->trigger();
            window.close();
        }
        Settings::setDualPaneAtStartup(Settings::EnumDualPaneAtStartup::Always);
        MainWindow restored;
        restored.openUrls({});
        QVERIFY(restored.isDualLayout());
    }

    void alwaysShowTabBar()
    {
        MainWindow window;
        window.openUrls({});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTabBar *tabBar = window.activePane()->findChild<QTabBar *>();
        QVERIFY(!tabBar->isVisible());

        Settings::setAlwaysShowTabBar(true);
        window.applySettings();
        QVERIFY(tabBar->isVisible());
    }

    void hideNavigationBar()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *navigator = window.activePane()->findChild<KUrlNavigator *>();
        QVERIFY(navigator->isVisible());

        Settings::setShowNavigationBar(false);
        window.applySettings();
        QVERIFY(!window.activePane()->isNavigationBarVisible());
        QVERIFY(!navigator->isVisible());
    }

    void newTabsAfterCurrent()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        pane->openInNewTab(m_b, false);
        pane->tabs()->setCurrentIndex(0);

        Settings::setNewTabsAfterCurrent(true);
        window.applySettings();
        pane->openInNewTab(url(QStringLiteral("c")), true);
        QCOMPARE(pane->tabs()->currentIndex(), 1);
        QCOMPARE(pane->tabs()->urls().last(), m_b);
    }

    void followSymlinks()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        const QUrl link = url(QStringLiteral("link"));

        pane->setUrl(link);
        QCOMPARE(pane->url(), link);

        Settings::setFollowSymlinks(true);
        window.applySettings();
        pane->setUrl(m_b);
        pane->setUrl(link);
        QCOMPARE(pane->url(), QUrl::fromLocalFile(QDir(m_a.toLocalFile()).canonicalPath()));
    }

    void tabSwitchesPanes()
    {
        MainWindow window;
        QAction *switchPane = action(window, QStringLiteral("switch_pane"));
        QVERIFY(switchPane->shortcuts().contains(QKeySequence(Qt::Key_Tab)));

        Settings::setTabSwitchesPanes(false);
        window.applySettings();
        QVERIFY(!switchPane->shortcuts().contains(QKeySequence(Qt::Key_Tab)));
    }

    void viewDefaultsApplyToNewWindow()
    {
        Settings::setDefaultViewMode(Settings::EnumDefaultViewMode::Icons);
        Settings::setSortColumn(Settings::EnumSortColumn::Size);
        Settings::setSortDescending(true);
        Settings::setShowHiddenFiles(true);
        Settings::setShowPreviews(false);

        MainWindow window;
        window.openUrls({m_a});
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            FolderView *view = pane->view();
            QCOMPARE(view->viewMode(), FolderView::ViewMode::Icons);
            QCOMPARE(view->sortColumn(), int(KDirModel::Size));
            QCOMPARE(view->sortOrder(), Qt::DescendingOrder);
            QVERIFY(view->showHiddenFiles());
            QVERIFY(!view->previewsEnabled());
        }
        QVERIFY(!action(window, QStringLiteral("show_previews"))->isChecked());
    }

    void liveViewSettings()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(!view->isColumnVisible(KDirModel::Owner));

        Settings::setDetailsIconSize(32);
        Settings::setIconsIconSize(128);
        Settings::setShowOwnerColumn(true);
        Settings::setShowSizeColumn(false);
        Settings::setFoldersFirst(false);
        Settings::setShowToolTips(false);
        window.applySettings();

        QCOMPARE(view->currentView()->iconSize(), QSize(32, 32));
        view->setViewMode(FolderView::ViewMode::Icons);
        QCOMPARE(view->currentView()->iconSize(), QSize(128, 128));
        view->setPreviewsEnabled(false);
        QCOMPARE(view->currentView()->iconSize(), QSize(128, 128));
        view->setPreviewsEnabled(true);
        QVERIFY(view->previewsShown());
        QCOMPARE(view->currentView()->iconSize(), QSize(128, 128));
        QVERIFY(view->isColumnVisible(KDirModel::Owner));
        QVERIFY(!view->isColumnVisible(KDirModel::Size));
        QVERIFY(view->isColumnVisible(KDirModel::Name));
        QVERIFY(!view->sortFoldersFirst());
        QVERIFY(!view->showToolTips());

        window.activePane()->setUrl(m_b);
        QVERIFY(view->isColumnVisible(KDirModel::Owner));
        QVERIFY(!view->isColumnVisible(KDirModel::Size));
    }

    void applyingSettingsPreservesFolderRatingColumn()
    {
        FolderMetadata::Format format;
        format.view = QStringLiteral("list");
        format.columns = {QStringLiteral("rating")};
        format.sortKey = QStringLiteral("name");
        format.group = QStringLiteral("none");
        QVERIFY(FolderMetadata::setFormat(m_a.toLocalFile(), format));

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
        window.applySettings();
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
    }

    void legacyRatingDefaultsSurviveMigrationAndNavigation()
    {
        KConfigGroup legacy = KSharedConfig::openConfig()->group(QStringLiteral("View"));
        legacy.writeEntry("VisibleColumns", QStringList{QStringLiteral("size"), QStringLiteral("rating")});
        legacy.sync();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(Settings::showRatingColumn());
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
        window.activePane()->setUrl(m_b);
        window.applySettings();
        QVERIFY(view->isColumnVisible(KDirModel::Group));
        QCOMPARE(view->currentView()->model()->headerData(KDirModel::Group, Qt::Horizontal).toString(), QStringLiteral("Rating"));
        QVERIFY(!legacy.hasKey("VisibleColumns"));
    }

    void previewSettings()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QVERIFY(view->previewsShown());

        Settings::setPreviewPlugins({QStringLiteral("imagethumbnail")});
        window.applySettings();
        QCOMPARE(view->previewPlugins(), QStringList{QStringLiteral("imagethumbnail")});

        window.activePane()->setUrl(QUrl(QStringLiteral("trash:/")));
        QVERIFY(view->previewsEnabled());
        QVERIFY(!view->previewsShown());
        Settings::setShowRemotePreviews(true);
        window.applySettings();
        QVERIFY(view->previewsShown());
    }

    void previewSizeSpinBoxMapsZeroToNoLimit()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *size = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_MaximumPreviewSize"));
        QVERIFY(size);
        size->setValue(10);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::maximumPreviewSize(), 10ULL * 1024 * 1024);

        size->setValue(0);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::maximumPreviewSize(), std::numeric_limits<qulonglong>::max());
    }

    void previewPluginListWritesCheckedPlugins()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("kcfg_PreviewPlugins"));
        QVERIFY(list);
        if (list->count() < 2) {
            QSKIP("needs at least two KIO thumbnailer plugins installed");
        }
        for (int row = 0; row < list->count(); ++row) {
            list->item(row)->setCheckState(row == 0 ? Qt::Checked : Qt::Unchecked);
        }
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::previewPlugins(), QStringList{list->item(0)->data(Qt::UserRole).toString()});
    }

    void placesPanelVisibility()
    {
        MainWindow window;
        window.setPanelLayout(MainWindow::PanelLayout::Separate);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
        QVERIFY(dock->isVisible());

        Settings::setShowPlaces(false);
        window.applySettings();
        QVERIFY(!dock->isVisible());

        action(window, QStringLiteral("show_places"))->trigger();
        QVERIFY(dock->isVisible());
        QVERIFY(Settings::showPlaces());

        // Closing the window hides the dock without turning the panel off.
        window.close();
        QVERIFY(Settings::showPlaces());
    }

    void closingPlacesPanelIsRemembered()
    {
        MainWindow window;
        window.setPanelLayout(MainWindow::PanelLayout::Separate);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QDockWidget *>(QStringLiteral("placesDock"))->close();
        QVERIFY(!Settings::showPlaces());
    }

    void placesSectionsAreHiddenThroughTheDialog()
    {
        MainWindow window;
        KFilePlacesModel *places = placesOf(window);
        QVERIFY(places);
        places->setGroupHidden(KFilePlacesModel::RemoteType, false);

        SettingsDialog dialog(placesOf(window), &window);
        auto *remote = dialog.findChild<QCheckBox *>(QStringLiteral("placesGroup_RemoteType"));
        QVERIFY(remote);
        QVERIFY(remote->isChecked());
        remote->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(places->isGroupHidden(KFilePlacesModel::RemoteType));

        places->setGroupHidden(KFilePlacesModel::RemoteType, false);
    }

    void statusBarContents()
    {
        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *items = window.findChild<QLabel *>(QStringLiteral("itemsLabel"));
        auto *freeSpace = window.findChild<QLabel *>(QStringLiteral("freeSpaceLabel"));
        view->selectAll();
        QVERIFY(items->text().contains(QLatin1Char('(')));

        Settings::setShowSelectionSize(false);
        Settings::setShowFreeSpace(false);
        window.applySettings();
        QVERIFY(!items->text().contains(QLatin1Char('(')));
        QVERIFY(freeSpace->isHidden());

        Settings::setShowItemCounts(false);
        window.applySettings();
        QVERIFY(items->isHidden());
    }

    void goHomeUsesHomeSetting()
    {
        Settings::setHomeUrl(m_b);
        MainWindow window;
        window.openUrls({m_a});
        action(window, QStringLiteral("go_home"))->trigger();
        QCOMPARE(window.activePane()->url(), m_b);
    }
};

QTEST_MAIN(SettingsTest)

#include "settingstest.moc"
