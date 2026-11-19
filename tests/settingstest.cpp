// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"
#include "combinedpanel.h"
#include "compressdialog.h"
#include "fileoperations.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "settingsdialog.h"
#include "tabmodel.h"

#include <KActionCollection>
#include <KDirModel>
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
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(Settings::startupMode(), Settings::EnumStartupMode::FixedFolders);
        QCOMPARE(Settings::leftStartUrl(), m_b);
        QVERIFY(Settings::followSymlinks());
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
        QCOMPARE(
            titles,
            QStringList(
                {QStringLiteral("General"), QStringLiteral("Interface"), QStringLiteral("View"), QStringLiteral("Context Menu"), QStringLiteral("Archives")}));

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
        QCOMPARE(tabOf(QStringLiteral("DetailsIconSize")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("ShowSizeColumn")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("DragFromAnywhere")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("ShowOpenInNewTab")), QStringLiteral("Actions"));
        QCOMPARE(tabOf(QStringLiteral("ShowCopyMoveToMenus")), QStringLiteral("Actions"));
        QCOMPARE(tabOf(QStringLiteral("DetailsDensity")), QStringLiteral("Details"));
        QCOMPARE(tabOf(QStringLiteral("IconsDensity")), QStringLiteral("Icons"));
        QCOMPARE(tabOf(QStringLiteral("BarDensity")), QStringLiteral("Status and Location Bars"));
        QCOMPARE(tabOf(QStringLiteral("IconsIconSize")), QStringLiteral("Icons"));
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
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultArchiveFormat"))->setCurrentIndex(static_cast<int>(ArchiveWriter::Format::TarXz));
        dialog.button(QDialogButtonBox::Apply)->click();

        QVERIFY(!Settings::favoritesVisible());
        QCOMPARE(Settings::panelPosition(), Settings::EnumPanelPosition::right);
        QCOMPARE(Settings::defaultArchiveFormat(), static_cast<int>(ArchiveWriter::Format::TarXz));
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

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::favoritesVisible());
        QCOMPARE(Settings::panelPosition(), Settings::EnumPanelPosition::left);
        QCOMPARE(Settings::defaultArchiveFormat(), static_cast<int>(ArchiveWriter::Format::Zip));
        QVERIFY(bar->expanding());
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
        QVERIFY(view->isColumnVisible(KDirModel::Owner));
        QVERIFY(!view->isColumnVisible(KDirModel::Size));
        QVERIFY(view->isColumnVisible(KDirModel::Name));
        QVERIFY(!view->sortFoldersFirst());
        QVERIFY(!view->showToolTips());
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
