// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "batchrenamedialog.h"
#include "checksumdialog.h"
#include "combinedpanel.h"
#include "contentchartdialog.h"
#include "customcommanddialog.h"
#include "customcommands.h"
#include "duplicatefinderdialog.h"
#include "filediffdialog.h"
#include "fileoperations.h"
#include "filesplitdialog.h"
#include "filterbar.h"
#include "foldercomparisondialog.h"
#include "foldersyncdialog.h"
#include "foldertree.h"
#include "folderview.h"
#include "gallerywindow.h"
#include "informationpanel.h"
#include "metadataeditdialog.h"
#include "pane.h"
#include "previewpanel.h"
#include "savedpathfilters.h"
#include "searchdialog.h"
#include "settings.h"
#include "settingsdialog.h"
#include "tabmodel.h"
#include "transferqueuedialog.h"
#include "viewsettings.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KDirModel>
#include <KEditToolBar>
#include <KFileItemDelegate>
#include <KFilePlacesView>
#include <KSelectAction>
#include <KSharedConfig>
#include <KTextEditor/Document>
#include <KToolBar>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QGraphicsView>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>

class MainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a/sub")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("c")));
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_c = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("c")));
    }

    void opensFilteredFolderComparison()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("right")));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(root.filePath(QStringLiteral("right")))});
        auto *action = window.actionCollection()->action(QStringLiteral("compare_folders_filtered"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<FolderSyncDialog *>(QStringLiteral("folderSyncDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void opensTransferQueue()
    {
        MainWindow window;
        auto *action = window.actionCollection()->action(QStringLiteral("transfer_queue"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<TransferQueueDialog *>(QStringLiteral("transferQueueDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void inactivePaneDimmingFollowsFocusAndPersists()
    {
        const bool previous = Settings::dimInactivePane();
        const auto restore = qScopeGuard([previous] {
            Settings::setDimInactivePane(previous);
            Settings::self()->save();
        });
        Settings::setDimInactivePane(false);
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *action = window.actionCollection()->action(QStringLiteral("dim_inactive_pane"));
        QVERIFY(action && action->isCheckable());
        auto *active = window.activePane();
        auto *inactive = window.inactivePane();
        auto *activeEffect = qobject_cast<QGraphicsOpacityEffect *>(active->graphicsEffect());
        auto *inactiveEffect = qobject_cast<QGraphicsOpacityEffect *>(inactive->graphicsEffect());
        QVERIFY(activeEffect && inactiveEffect);
        QVERIFY(!activeEffect->isEnabled());
        QVERIFY(!inactiveEffect->isEnabled());
        QTRY_COMPARE(active->view()->itemCount(), 2);
        QTRY_COMPARE(inactive->view()->itemCount(), 0);
        const QRect inactiveRect(inactive->mapTo(&window, QPoint()), inactive->size());
        const QImage before = window.grab(inactiveRect).toImage();
        action->trigger();
        QVERIFY(Settings::dimInactivePane());
        QVERIFY(!activeEffect->isEnabled());
        QVERIFY(inactiveEffect->isEnabled());
        const QImage after = window.grab(inactiveRect).toImage();
        QVERIFY(before != after);
        QTest::mouseClick(inactive->view()->currentView()->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QCOMPARE(window.activePane(), inactive);
        QVERIFY(activeEffect->isEnabled());
        QVERIFY(!inactiveEffect->isEnabled());
        window.actionCollection()->action(QStringLiteral("switch_pane"))->trigger();
        QCOMPARE(window.activePane(), active);
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!window.activePane()->graphicsEffect()->isEnabled());
        Settings::self()->load();
        QVERIFY(Settings::dimInactivePane());
        MainWindow another;
        auto *otherAction = another.actionCollection()->action(QStringLiteral("dim_inactive_pane"));
        QVERIFY(otherAction->isChecked());
        QVERIFY(another.inactivePane()->graphicsEffect()->isEnabled());
        action->trigger();
        QVERIFY(!activeEffect->isEnabled());
        QVERIFY(!inactiveEffect->isEnabled());
        Settings::setDimInactivePane(true);
        window.applySettings();
        QVERIFY(action->isChecked());
        QVERIFY(inactiveEffect->isEnabled());
    }

    void swappingCurrentTabsPreservesTabsAndLinkedNavigation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("a/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("b/sub")));
        const QUrl a = QUrl::fromLocalFile(root.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(root.filePath(QStringLiteral("b")));
        MainWindow window;
        window.openUrls({a, b});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *left = window.activePane();
        auto *right = window.inactivePane();
        auto *link = window.actionCollection()->action(QStringLiteral("link_panes"));
        link->setChecked(true);
        QVERIFY(link->isChecked());
        const QUrl leftSub = QUrl::fromLocalFile(root.filePath(QStringLiteral("a/sub")));
        const QUrl rightSub = QUrl::fromLocalFile(root.filePath(QStringLiteral("b/sub")));
        left->setUrl(leftSub);
        QCOMPARE(left->url(), leftSub);
        QCOMPARE(right->url(), rightSub);
        left->tabs()->setName(left->tabs()->currentIndex(), QStringLiteral("Left"));
        left->tabs()->setPinned(left->tabs()->currentIndex(), true);
        right->tabs()->setName(right->tabs()->currentIndex(), QStringLiteral("Right"));
        right->tabs()->setLocked(right->tabs()->currentIndex(), true);

        auto *swap = window.actionCollection()->action(QStringLiteral("swap_current_tabs"));
        QVERIFY(swap && swap->isEnabled());
        swap->trigger();
        QCOMPARE(left->url(), rightSub);
        QCOMPARE(right->url(), leftSub);
        QCOMPARE(left->tabs()->nameAt(left->tabs()->currentIndex()), QStringLiteral("Right"));
        QVERIFY(left->tabs()->isLocked(left->tabs()->currentIndex()));
        QCOMPARE(right->tabs()->nameAt(right->tabs()->currentIndex()), QStringLiteral("Left"));
        QVERIFY(right->tabs()->isPinned(right->tabs()->currentIndex()));
        QVERIFY(link->isChecked());
        left->tabs()->setLocked(left->tabs()->currentIndex(), false);
        left->setUrl(b);
        QCOMPARE(right->url(), a);
        QVERIFY(link->isChecked());
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!swap->isEnabled());
    }

    void swappingPaneSidesPreservesPaneWidgetsAndLayout()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("a/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("b/sub")));
        const QUrl a = QUrl::fromLocalFile(root.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(root.filePath(QStringLiteral("b")));
        MainWindow window;
        window.openUrls({a, b});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *first = window.activePane();
        auto *second = window.inactivePane();
        auto *splitter = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(splitter);
        auto *link = window.actionCollection()->action(QStringLiteral("link_panes"));
        link->setChecked(true);
        QVERIFY(link->isChecked());
        window.setStackedLayout(true);
        QCOMPARE(splitter->widget(0), first);
        QCOMPARE(splitter->widget(1), second);
        auto *swap = window.actionCollection()->action(QStringLiteral("swap_pane_sides"));
        QVERIFY(swap && swap->isEnabled());
        swap->trigger();
        QCOMPARE(splitter->widget(0), second);
        QCOMPARE(splitter->widget(1), first);
        QCOMPARE(window.activePane(), first);
        QVERIFY(second->isTabBarAtBottom());
        QVERIFY(!first->isTabBarAtBottom());
        QVERIFY(link->isChecked());
        first->setUrl(QUrl::fromLocalFile(root.filePath(QStringLiteral("a/sub"))));
        QCOMPARE(second->url(), QUrl::fromLocalFile(root.filePath(QStringLiteral("b/sub"))));
        QVERIFY(link->isChecked());
    }

    void tabBarPlacementAndCloseButtonsFollowPaneAndLayout()
    {
        const int horizontalFirst = Settings::horizontalFirstTabPosition();
        const int horizontalSecond = Settings::horizontalSecondTabPosition();
        const int stackedFirst = Settings::stackedFirstTabPosition();
        const int stackedSecond = Settings::stackedSecondTabPosition();
        const bool together = Settings::stackedTabBarsTogether();
        const bool closeButtons = Settings::showTabCloseButtons();
        const auto restore = qScopeGuard([=] {
            Settings::setHorizontalFirstTabPosition(horizontalFirst);
            Settings::setHorizontalSecondTabPosition(horizontalSecond);
            Settings::setStackedFirstTabPosition(stackedFirst);
            Settings::setStackedSecondTabPosition(stackedSecond);
            Settings::setStackedTabBarsTogether(together);
            Settings::setShowTabCloseButtons(closeButtons);
            Settings::self()->save();
        });
        Settings::setHorizontalFirstTabPosition(0);
        Settings::setHorizontalSecondTabPosition(0);
        Settings::setStackedFirstTabPosition(0);
        Settings::setStackedSecondTabPosition(0);
        Settings::setStackedTabBarsTogether(true);
        Settings::setShowTabCloseButtons(true);

        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        window.setStackedLayout(false);
        auto *splitter = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(splitter);
        auto *first = qobject_cast<Pane *>(splitter->widget(0));
        auto *second = qobject_cast<Pane *>(splitter->widget(1));
        QVERIFY(first && second);
        first->setTabBarForcedVisible(true);
        second->setTabBarForcedVisible(true);
        if (window.activePane() != first) {
            window.actionCollection()->action(QStringLiteral("switch_pane"))->trigger();
        }
        auto *position = qobject_cast<KSelectAction *>(window.actionCollection()->action(QStringLiteral("tab_bar_position")));
        auto *stackedTogether = window.actionCollection()->action(QStringLiteral("stacked_tab_bars_together"));
        auto *showClose = window.actionCollection()->action(QStringLiteral("show_tab_close_buttons"));
        QVERIFY(position && stackedTogether && showClose);
        QCOMPARE(position->actions().size(), 4);
        position->actions().at(2)->trigger();
        QCOMPARE(first->tabBarPosition(), Pane::TabBarPosition::Left);
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Top);
        auto *firstBar = first->findChild<QTabBar *>();
        QVERIFY(firstBar);
        QTRY_VERIFY(firstBar->parentWidget()->geometry().right() < first->view()->geometry().left());
        window.actionCollection()->action(QStringLiteral("switch_pane"))->trigger();
        position->actions().at(3)->trigger();
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Right);
        auto *secondBar = second->findChild<QTabBar *>();
        QVERIFY(secondBar);
        QTRY_VERIFY(secondBar->parentWidget()->geometry().left() > second->view()->geometry().right());
        window.setStackedLayout(true);
        QCOMPARE(first->tabBarPosition(), Pane::TabBarPosition::Bottom);
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Top);
        position->actions().at(1)->trigger();
        QVERIFY(!stackedTogether->isChecked());
        QCOMPARE(first->tabBarPosition(), Pane::TabBarPosition::Top);
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Bottom);
        window.setStackedLayout(false);
        QCOMPARE(first->tabBarPosition(), Pane::TabBarPosition::Left);
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Right);
        window.setStackedLayout(true);
        QCOMPARE(first->tabBarPosition(), Pane::TabBarPosition::Top);
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Bottom);
        stackedTogether->setChecked(true);
        QCOMPARE(first->tabBarPosition(), Pane::TabBarPosition::Bottom);
        QCOMPARE(second->tabBarPosition(), Pane::TabBarPosition::Top);

        const int initialTabs = second->tabs()->count();
        second->openInNewTab(m_c, true);
        auto *tabBar = second->findChild<QTabBar *>();
        QVERIFY(tabBar);
        QTRY_COMPARE(tabBar->count(), initialTabs + 1);
        const int lockedIndex = second->tabs()->currentIndex();
        second->tabs()->setPinned(0, true);
        second->tabs()->setLocked(lockedIndex, true);
        showClose->setChecked(false);
        QVERIFY(!tabBar->tabsClosable());
        QVERIFY(tabBar->isVisible());
        Q_EMIT tabBar->tabCloseRequested(0);
        Q_EMIT tabBar->tabCloseRequested(lockedIndex);
        QCOMPARE(second->tabs()->count(), initialTabs + 1);
        showClose->setChecked(true);
        const auto closeSide = static_cast<QTabBar::ButtonPosition>(tabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, tabBar));
        QVERIFY(tabBar->tabButton(0, closeSide) && !tabBar->tabButton(0, closeSide)->isVisible());
        QVERIFY(tabBar->tabButton(lockedIndex, closeSide) && !tabBar->tabButton(lockedIndex, closeSide)->isVisible());
        showClose->setChecked(false);
        second->tabs()->setLocked(lockedIndex, false);
        auto *closeTab = window.actionCollection()->action(QStringLiteral("close_tab"));
        QTRY_VERIFY(closeTab->isEnabled());
        closeTab->trigger();
        QCOMPARE(second->tabs()->count(), initialTabs);

        MainWindow restored;
        restored.openUrls({m_a, m_b});
        restored.show();
        restored.setStackedLayout(false);
        auto *restoredSplitter = restored.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(restoredSplitter);
        QCOMPARE(qobject_cast<Pane *>(restoredSplitter->widget(0))->tabBarPosition(), Pane::TabBarPosition::Left);
        QCOMPARE(qobject_cast<Pane *>(restoredSplitter->widget(1))->tabBarPosition(), Pane::TabBarPosition::Right);
    }

    void savedCommandsCanBeAddedToToolbarAndRequirePreview()
    {
        QTemporaryDir backupDir;
        QVERIFY(backupDir.isValid());
        const auto config = KSharedConfig::openConfig();
        KConfigGroup group = config->group(QStringLiteral("CustomCommands"));
        const auto backup = KSharedConfig::openConfig(backupDir.filePath(QStringLiteral("settings")));
        KConfigGroup saved = backup->group(QStringLiteral("CustomCommands"));
        group.copyTo(&saved);
        const auto cleanup = qScopeGuard([&] {
            group.deleteGroup();
            saved.copyTo(&group);
            config->sync();
        });
        QString error;
        const CustomCommands::Command command{QStringLiteral("Toolbar command"), QStringLiteral("/bin/echo"), {QStringLiteral("{dir}")}, {}};
        QVERIFY2(CustomCommands::save(group, {command}, &error), qPrintable(error));
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        const auto actions = window.actionCollection()->actions();
        const auto it = std::find_if(actions.cbegin(), actions.cend(), [&command](const QAction *action) {
            return action->text() == command.name && action->objectName().startsWith(QLatin1String("custom_command_preset_"));
        });
        QVERIFY(it != actions.cend());
        QPointer<QAction> action = *it;
        QVERIFY(action->isEnabled());
        KEditToolBar editor(window.guiFactory(), &window);
        editor.show();
        const auto listed = [&editor, &command] {
            const auto views = editor.findChildren<QAbstractItemView *>();
            return std::any_of(views.cbegin(), views.cend(), [&command](QAbstractItemView *view) {
                return view->model()
                    && !view->model()->match(view->model()->index(0, 0), Qt::DisplayRole, command.name, 1, Qt::MatchExactly | Qt::MatchRecursive).isEmpty();
            });
        };
        QTRY_VERIFY(listed());
        editor.close();
        window.toolBar()->addAction(action);
        auto *toolbarButton = qobject_cast<QToolButton *>(window.toolBar()->widgetForAction(action));
        QVERIFY(toolbarButton);
        QTest::mouseClick(toolbarButton, Qt::LeftButton);
        auto *dialog = window.findChild<CustomCommandDialog *>(QStringLiteral("customCommandDialog"));
        QVERIFY(dialog);
        QCOMPARE(dialog->findChild<QLineEdit *>(QStringLiteral("commandProgram"))->text(), command.program);
        QCOMPARE(dialog->findChild<QPlainTextEdit *>(QStringLiteral("commandArguments"))->toPlainText(), QStringLiteral("{dir}"));
        QVERIFY(!dialog->findChild<QPushButton *>(QStringLiteral("commandRun"))->isEnabled());
        dialog->findChild<QPushButton *>(QStringLiteral("commandPreview"))->click();
        QVERIFY(dialog->findChild<QPushButton *>(QStringLiteral("commandRun"))->isEnabled());
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog->findChild<QPushButton *>(QStringLiteral("commandDeletePreset"))->click();
        QVERIFY(action.isNull());
        QTimer::singleShot(0, [] {
            auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(input);
            input->setTextValue(QStringLiteral("New toolbar command"));
            input->accept();
        });
        dialog->findChild<QPushButton *>(QStringLiteral("commandSavePreset"))->click();
        const auto refreshed = window.actionCollection()->actions();
        QVERIFY(std::any_of(refreshed.cbegin(), refreshed.cend(), [](const QAction *candidate) {
            return candidate->text() == QLatin1String("New toolbar command") && candidate->objectName().startsWith(QLatin1String("custom_command_preset_"));
        }));
        dialog->close();
    }

    void mainToolbarCanBeEditedByDragAndDrop()
    {
        QTemporaryDir backupDir;
        QVERIFY(backupDir.isValid());
        const auto config = KSharedConfig::openConfig();
        KConfigGroup customization = config->group(QStringLiteral("ToolbarCustomization"));
        const auto backup = KSharedConfig::openConfig(backupDir.filePath(QStringLiteral("settings")));
        KConfigGroup savedGroup = backup->group(QStringLiteral("ToolbarCustomization"));
        customization.copyTo(&savedGroup);
        const auto cleanup = qScopeGuard([&] {
            customization.deleteGroup();
            savedGroup.copyTo(&customization);
            config->sync();
        });
        customization.deleteGroup();

        MainWindow window;
        window.show();
        auto *toolbar = window.toolBar();
        auto *customize = window.actionCollection()->action(QStringLiteral("customize_main_toolbar"));
        auto *home = window.actionCollection()->action(QStringLiteral("go_home"));
        QVERIFY(toolbar && customize && home);
        QVERIFY(!toolbar->actions().contains(home));
        customize->trigger();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("toolbarActionsDock"));
        auto *actionList = window.findChild<QListWidget *>(QStringLiteral("toolbarActionList"));
        QVERIFY(dock && actionList);
        QTRY_VERIFY(dock->isVisible());

        const auto drop = [](QWidget *target, const QString &source, const QString &kind, const QString &name, const QPointF &position) {
            QMimeData mimeData;
            mimeData.setData(QStringLiteral("application/x-opus-toolbar-item"), (source + QLatin1Char('\n') + kind + QLatin1Char('\n') + name).toUtf8());
            QDragEnterEvent enter(position, Qt::MoveAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &enter);
            if (!enter.isAccepted()) {
                return false;
            }
            QDragMoveEvent move(position, Qt::MoveAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &move);
            if (!move.isAccepted()) {
                return false;
            }
            QDropEvent event(position, Qt::MoveAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
            return event.isAccepted();
        };
        QVERIFY(drop(toolbar, QStringLiteral("palette"), QStringLiteral("action"), QStringLiteral("go_home"), QPointF(0, 0)));
        QCOMPARE(toolbar->actions().first(), home);
        QVERIFY(drop(toolbar, QStringLiteral("palette"), QStringLiteral("separator"), {}, QPointF(toolbar->width(), toolbar->height() / 2.0)));
        QVERIFY(toolbar->actions().last()->isSeparator());
        QVERIFY(drop(toolbar, QStringLiteral("palette"), QStringLiteral("spacer"), {}, QPointF(toolbar->width(), toolbar->height() / 2.0)));
        QVERIFY(toolbar->actions().last()->property("toolbarSpacer").toBool());
        QVERIFY(
            drop(toolbar, QStringLiteral("toolbar"), QStringLiteral("action"), QStringLiteral("go_home"), QPointF(toolbar->width(), toolbar->height() / 2.0)));
        QCOMPARE(toolbar->actions().last(), home);
        QVERIFY(drop(actionList->viewport(), QStringLiteral("toolbar"), QStringLiteral("action"), QStringLiteral("go_home"), QPointF(0, 0)));
        QVERIFY(!toolbar->actions().contains(home));

        const QStringList savedItems = customization.readEntry("Actions", QStringList());
        QVERIFY(savedItems.contains(QStringLiteral("separator")));
        QVERIFY(savedItems.contains(QStringLiteral("spacer")));
        QVERIFY(!savedItems.contains(QStringLiteral("action:go_home")));

        const QString customIconName = home->icon().name();
        const QString terminalIconName = window.actionCollection()->action(QStringLiteral("open_terminal"))->icon().name();
        QVERIFY(!customIconName.isEmpty() && QIcon::hasThemeIcon(customIconName));
        KConfigGroup terminalAppearance = customization.group(QStringLiteral("Entries")).group(QStringLiteral("open_terminal"));
        terminalAppearance.writeEntry("ButtonStyle", static_cast<int>(Qt::ToolButtonTextOnly));
        terminalAppearance.writeEntry("IconName", customIconName);
        customization.sync();
        MainWindow restored;
        auto *restoredToolbar = restored.toolBar();
        auto *terminal = restored.actionCollection()->action(QStringLiteral("open_terminal"));
        QVERIFY(restoredToolbar && terminal && restoredToolbar->actions().contains(terminal));
        auto *terminalButton = qobject_cast<QToolButton *>(restoredToolbar->widgetForAction(terminal));
        QVERIFY(terminalButton);
        QCOMPARE(terminalButton->toolButtonStyle(), Qt::ToolButtonTextOnly);
        QCOMPARE(terminalButton->icon().name(), customIconName);
        QCOMPARE(terminal->icon().name(), terminalIconName);
    }

    void paneToolbarsControlTheirOwnPane()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/sub")));
        QFile result(root.filePath(QStringLiteral("right/sub/result.txt")));
        QVERIFY(result.open(QIODevice::WriteOnly));
        result.close();

        MainWindow window;
        const QUrl leftUrl = QUrl::fromLocalFile(root.filePath(QStringLiteral("left")));
        const QUrl rightUrl = QUrl::fromLocalFile(root.filePath(QStringLiteral("right")));
        window.openUrls({leftUrl, rightUrl});
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        QVERIFY(left && right);
        QCOMPARE(left->url(), leftUrl);
        QCOMPARE(right->url(), rightUrl);
        auto *leftToolbar = left->findChild<QToolBar *>(QStringLiteral("paneToolbar"));
        auto *rightToolbar = right->findChild<QToolBar *>(QStringLiteral("paneToolbar"));
        QVERIFY(leftToolbar && rightToolbar && leftToolbar != rightToolbar);

        QAction *hiddenFiles = left->findChild<QAction *>(QStringLiteral("paneHiddenFiles"));
        QAction *previews = left->findChild<QAction *>(QStringLiteral("panePreviews"));
        QAction *zoomIn = left->findChild<QAction *>(QStringLiteral("paneZoomIn"));
        QVERIFY(hiddenFiles && previews && zoomIn);
        const bool rightHidden = right->view()->showHiddenFiles();
        const bool rightPreviews = right->view()->previewsEnabled();
        hiddenFiles->trigger();
        QCOMPARE(left->view()->showHiddenFiles(), hiddenFiles->isChecked());
        QCOMPARE(right->view()->showHiddenFiles(), rightHidden);
        previews->trigger();
        QCOMPARE(left->view()->previewsEnabled(), previews->isChecked());
        QCOMPARE(right->view()->previewsEnabled(), rightPreviews);

        const QSize initialIconSize = left->view()->currentView()->iconSize();
        zoomIn->trigger();
        QVERIFY(left->view()->currentView()->iconSize().width() > initialIconSize.width());
        const int rightSortColumn = right->view()->sortColumn();
        auto *sortButton = left->findChild<QToolButton *>(QStringLiteral("paneSortingButton"));
        QVERIFY(sortButton && sortButton->menu());
        QAction *sortBySize = nullptr;
        for (QAction *action : sortButton->menu()->actions()) {
            if (action->text() == QStringLiteral("Size")) {
                sortBySize = action;
                break;
            }
        }
        QVERIFY(sortBySize);
        sortBySize->trigger();
        QCOMPARE(left->view()->sortColumn(), KDirModel::Size);
        QCOMPARE(right->view()->sortColumn(), rightSortColumn);

        auto *viewButton = left->findChild<QToolButton *>(QStringLiteral("paneViewModeButton"));
        QVERIFY(viewButton && viewButton->menu());
        QAction *compactView = nullptr;
        for (QAction *action : viewButton->menu()->actions()) {
            if (action->text() == QStringLiteral("Compact")) {
                compactView = action;
                break;
            }
        }
        QVERIFY(compactView);
        compactView->trigger();
        QCOMPARE(left->view()->viewMode(), FolderView::ViewMode::Compact);
        QVERIFY(right->view()->viewMode() != FolderView::ViewMode::Compact);

        auto *navigationLayout = qobject_cast<QBoxLayout *>(left->navigationBar()->layout());
        QVERIFY(navigationLayout);
        window.show();
        window.resize(500, 400);
        QCoreApplication::processEvents();
        QTRY_COMPARE(navigationLayout->direction(), QBoxLayout::TopToBottom);

        QAction *search = right->findChild<QAction *>(QStringLiteral("paneSearch"));
        QVERIFY(search);
        search->trigger();
        auto *dialog = window.findChild<SearchDialog *>();
        QVERIFY(dialog);
        const QUrl resultUrl = QUrl::fromLocalFile(root.filePath(QStringLiteral("right/sub/result.txt")));
        dialog->revealRequested(resultUrl);
        QCOMPARE(right->url(), QUrl::fromLocalFile(root.filePath(QStringLiteral("right/sub"))));
        QCOMPARE(left->url(), leftUrl);
    }

    void paneToolbarConfigurationRestoresVisibilityAndOrder()
    {
        QTemporaryDir backupDir;
        QVERIFY(backupDir.isValid());
        const auto config = KSharedConfig::openConfig();
        KConfigGroup toolbars = config->group(QStringLiteral("PaneToolbars"));
        const auto backup = KSharedConfig::openConfig(backupDir.filePath(QStringLiteral("settings")));
        KConfigGroup saved = backup->group(QStringLiteral("PaneToolbars"));
        toolbars.copyTo(&saved);
        const auto cleanup = qScopeGuard([&] {
            toolbars.deleteGroup();
            saved.copyTo(&toolbars);
            config->sync();
        });
        toolbars.deleteGroup();

        MainWindow window;
        Pane *pane = window.activePane();
        auto *toolbar = pane->findChild<QToolBar *>(QStringLiteral("paneToolbar"));
        auto *customize = pane->findChild<QAction *>(QStringLiteral("paneCustomizeToolbar"));
        QVERIFY(toolbar && customize);
        QListWidget *entries = nullptr;
        bool dialogOpened = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<QDialog *>(QStringLiteral("paneToolbarConfigurationDialog"));
            if (!dialog) {
                return;
            }
            dialogOpened = true;
            entries = dialog->findChild<QListWidget *>(QStringLiteral("paneToolbarEntries"));
            if (!entries) {
                dialog->reject();
                return;
            }
            int backRow = -1;
            int hiddenRow = -1;
            for (int row = 0; row < entries->count(); ++row) {
                const QString name = entries->item(row)->data(Qt::UserRole).toString();
                backRow = name == QLatin1String("paneBack") ? row : backRow;
                hiddenRow = name == QLatin1String("paneHiddenFiles") ? row : hiddenRow;
            }
            if (backRow < 0 || hiddenRow < 0) {
                dialog->reject();
                return;
            }
            entries->item(hiddenRow)->setCheckState(Qt::Unchecked);
            entries->addItem(entries->takeItem(backRow));
            dialog->accept();
        });
        customize->trigger();
        QVERIFY(dialogOpened);
        QVERIFY(entries);
        QVERIFY(!pane->findChild<QAction *>(QStringLiteral("paneHiddenFiles"))->isVisible());
        QCOMPARE(toolbar->actions().first()->objectName(), QStringLiteral("paneForward"));

        MainWindow restored;
        Pane *restoredPane = restored.activePane();
        auto *restoredToolbar = restoredPane->findChild<QToolBar *>(QStringLiteral("paneToolbar"));
        QVERIFY(restoredToolbar);
        QVERIFY(!restoredPane->findChild<QAction *>(QStringLiteral("paneHiddenFiles"))->isVisible());
        QCOMPARE(restoredToolbar->actions().first()->objectName(), QStringLiteral("paneForward"));
    }

    void splitterSizesAreSavedPerLayout()
    {
        QTemporaryDir backupDir;
        QVERIFY(backupDir.isValid());
        const auto state = KSharedConfig::openStateConfig();
        KConfigGroup sizes = state->group(QStringLiteral("SplitterSizes"));
        const auto backup = KSharedConfig::openConfig(backupDir.filePath(QStringLiteral("state")));
        KConfigGroup savedSizes = backup->group(QStringLiteral("SplitterSizes"));
        sizes.copyTo(&savedSizes);
        const auto cleanup = qScopeGuard([&] {
            sizes.deleteGroup();
            savedSizes.copyTo(&sizes);
            state->sync();
        });
        sizes.deleteGroup();

        MainWindow window;
        window.resize(1600, 1000);
        window.openUrls({m_a, m_b});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        window.setStackedLayout(false);
        auto *panes = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        auto *workspace = window.findChild<QSplitter *>(QStringLiteral("workspaceSplitter"));
        QVERIFY(panes && workspace);
        workspace->widget(1)->show();
        QCoreApplication::processEvents();
        panes->setSizes({1000, 500, 0});
        workspace->setSizes({700, 300});
        const QList<int> sidePanes = panes->sizes();
        const QList<int> sideWorkspace = workspace->sizes();
        auto *save = window.actionCollection()->action(QStringLiteral("save_splitter_sizes"));
        auto *reset = window.actionCollection()->action(QStringLiteral("reset_splitter_sizes"));
        QVERIFY(save && reset);
        save->trigger();

        window.setStackedLayout(true);
        panes->setSizes({300, 700, 0});
        workspace->setSizes({1000, 500});
        const QList<int> stackedPanes = panes->sizes();
        save->trigger();
        window.setStackedLayout(false);
        QTRY_VERIFY(qAbs(panes->sizes().at(0) - sidePanes.at(0)) < 5);
        QTRY_VERIFY(qAbs(workspace->sizes().at(1) - sideWorkspace.at(1)) < 5);
        window.setStackedLayout(true);
        QTRY_VERIFY(qAbs(panes->sizes().at(0) - stackedPanes.at(0)) < 5);
        reset->trigger();
        QVERIFY(!sizes.group(QStringLiteral("Stacked")).hasKey("FirstPane"));
        QVERIFY(sizes.group(QStringLiteral("SideBySide")).exists());
        window.setStackedLayout(false);
        QTRY_VERIFY(qAbs(panes->sizes().at(0) - sidePanes.at(0)) < 5);
        QVERIFY(panes->sizes().at(0) > 0 && panes->sizes().at(1) > 0);

        MainWindow restored;
        restored.resize(1600, 1000);
        restored.openUrls({m_a, m_b});
        restored.show();
        restored.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        restored.setStackedLayout(false);
        auto *restoredPanes = restored.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(restoredPanes);
        QTRY_VERIFY(restoredPanes->sizes().at(0) > restoredPanes->sizes().at(1));

        const auto editorOpening = Settings::editorOpening();
        const auto restoreEditorSetting = qScopeGuard([editorOpening] {
            Settings::setEditorOpening(editorOpening);
        });
        Settings::setEditorOpening(Settings::EnumEditorOpening::Panel);
        window.activePane()->view()->clearSelection();
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        auto *editor = window.findChild<EditorPanel *>(QStringLiteral("editorPanel"));
        QVERIFY(editor && editor->isVisible());
        auto *previewAction = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        previewAction->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(panes->count(), 4);
        panes->setSizes({600, 450, 300, 250});
        save->trigger();
        KConfigGroup side = sizes.group(QStringLiteral("SideBySide"));
        const int previewSize = side.readEntry("Preview", 0);
        const int editorSize = side.readEntry("Editor", 0);
        QVERIFY(previewSize > 0 && editorSize > 0);
        previewAction->setChecked(false);
        save->trigger();
        QCOMPARE(side.readEntry("Preview", 0), previewSize);
        previewAction->setChecked(true);
        QVERIFY(panes->sizes().at(2) > 0 && panes->sizes().at(3) > 0);

        auto *placeRight = editor->findChild<QAction *>(QStringLiteral("editorPlaceRight"));
        auto *placePanel = editor->findChild<QAction *>(QStringLiteral("editorPlacePanel"));
        QVERIFY(placeRight && placePanel);
        placeRight->trigger();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("editorDock"));
        QVERIFY(dock && dock->isVisible());
        window.resizeDocks({dock}, {400}, Qt::Horizontal);
        QCoreApplication::processEvents();
        save->trigger();
        const int dockSize = side.readEntry("EditorDockRight", 0);
        QVERIFY(dockSize > 0);
        placePanel->trigger();
        placeRight->trigger();
        QCoreApplication::processEvents();
        QVERIFY(qAbs(dock->width() * 10000 / window.width() - dockSize) < 400);
        previewAction->setChecked(false);
        dock->hide();
        placePanel->trigger();
        side.writeEntry("EditorDockRight", 10000);
        placeRight->trigger();
        QCoreApplication::processEvents();
        QVERIFY(dock->width() * 10000 / window.width() <= 6500);

        side.writeEntry("FirstPane", 1);
        side.writeEntry("SecondPane", 9999);
        restored.setStackedLayout(true);
        restored.setStackedLayout(false);
        QTRY_VERIFY(restoredPanes->sizes().at(0) > 0);
        const QList<int> safeSizes = restoredPanes->sizes();
        const int paneTotal = safeSizes.at(0) + safeSizes.at(1);
        QVERIFY(safeSizes.at(0) * 100 / paneTotal >= 15);
        QVERIFY(safeSizes.at(1) * 100 / paneTotal >= 15);

        KConfigGroup sideConfig = sizes.group(QStringLiteral("SideBySide"));
        sideConfig.writeEntry("FirstPane", -100);
        sideConfig.writeEntry("SecondPane", 10000000);
        sideConfig.writeEntry("Preview", 10000000);
        window.resize(600, 400);
        window.setStackedLayout(true);
        window.setStackedLayout(false);
        QCoreApplication::processEvents();
        QVERIFY(panes->sizes().at(0) > 0 && panes->sizes().at(1) > 0);
    }

    void informationPanelFollowsSelectionAndOptionalHover()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QUrl firstUrl = QUrl::fromLocalFile(folder.filePath(QStringLiteral("first.txt")));
        const QUrl secondUrl = QUrl::fromLocalFile(folder.filePath(QStringLiteral("second.txt")));
        for (const QUrl &url : {firstUrl, secondUrl}) {
            QFile file(url.toLocalFile());
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("content");
        }

        const QStringList previousFields = Settings::informationFields();
        const bool previousHover = Settings::informationHover();
        const auto restore = qScopeGuard([previousFields, previousHover] {
            Settings::setInformationFields(previousFields);
            Settings::setInformationHover(previousHover);
            Settings::self()->save();
        });
        Settings::setInformationFields({QStringLiteral("localPathOrUrl"), QStringLiteral("comment"), QStringLiteral("size")});
        Settings::setInformationHover(false);

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        window.show();
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *action = window.actionCollection()->action(QStringLiteral("show_information_panel"));
        QVERIFY(action);
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("informationDock"));
        QVERIFY(dock);
        dock->hide();
        action->setChecked(false);
        action->trigger();
        QVERIFY(dock && dock->isVisible());
        view->selectUrls({firstUrl});
        auto *path = dock->findChild<QLabel *>(QStringLiteral("informationValue_localPathOrUrl"));
        auto *comment = dock->findChild<QLabel *>(QStringLiteral("informationValue_comment"));
        auto *panel = dock->findChild<InformationPanel *>(QStringLiteral("informationPanelContents"));
        QVERIFY(path && comment && panel);
        QTRY_COMPARE(path->text(), firstUrl.toDisplayString(QUrl::PreferLocalFile));
        QCOMPARE(comment->text(), QStringLiteral("Unavailable"));
        auto *form = qobject_cast<QFormLayout *>(panel->layout());
        QVERIFY(form);
        QCOMPARE(form->itemAt(0, QFormLayout::FieldRole)->widget()->objectName(), QStringLiteral("informationValue_localPathOrUrl"));
        QCOMPARE(form->itemAt(1, QFormLayout::FieldRole)->widget()->objectName(), QStringLiteral("informationValue_comment"));

        Settings::setInformationHover(true);
        window.applySettings();
        QModelIndex secondIndex;
        for (int row = 0; row < view->currentView()->model()->rowCount(); ++row) {
            const QModelIndex candidate = view->currentView()->model()->index(row, 0);
            if (candidate.data(Qt::DisplayRole).toString() == QLatin1String("second.txt")) {
                secondIndex = candidate;
                break;
            }
        }
        QVERIFY(secondIndex.isValid());
        QVERIFY(view->currentView()->hasMouseTracking());
        QVERIFY(view->currentView()->viewport()->hasMouseTracking());
        Q_EMIT view->currentView()->entered(secondIndex);
        QTRY_COMPARE(path->text(), secondUrl.toDisplayString(QUrl::PreferLocalFile));
        view->itemHoverCleared();
        QTRY_COMPARE(path->text(), firstUrl.toDisplayString(QUrl::PreferLocalFile));

        Settings::setInformationHover(true);
        window.applySettings();
        Q_EMIT view->currentView()->entered(secondIndex);
        QTRY_COMPARE(path->text(), secondUrl.toDisplayString(QUrl::PreferLocalFile));
        Settings::setInformationHover(false);
        window.applySettings();
        QCOMPARE(path->text(), firstUrl.toDisplayString(QUrl::PreferLocalFile));
    }

    void imageBackgroundSettingsApplyToOpenViews()
    {
        const auto previous = Settings::imageBackground();
        const auto restore = qScopeGuard([previous] {
            Settings::setImageBackground(previous);
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("transparent.png"));
        QImage image(20, 20, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QVERIFY(image.save(path));
        MainWindow window;
        window.show();
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(true);
        auto *preview = window.findChild<PreviewPanel *>(QStringLiteral("previewPanel"));
        QVERIFY(preview);
        preview->setItems({KFileItem(QUrl::fromLocalFile(path))});
        auto *label = preview->findChild<QLabel *>(QStringLiteral("previewImage"));
        QTRY_VERIFY(label && label->isVisible() && !label->pixmap().isNull());
        QPointer<GalleryWindow> gallery = new GalleryWindow({QUrl::fromLocalFile(path)}, {}, 0, &window);
        gallery->show();
        auto *view = gallery->findChild<QGraphicsView *>(QStringLiteral("galleryView"));
        QVERIFY(view);
        Settings::setImageBackground(Settings::EnumImageBackground::Dark);
        window.applySettings();
        QCOMPARE(view->backgroundBrush().color(), QColor(35, 35, 35));
        QCOMPARE(label->grab().toImage().pixelColor(label->width() / 2, label->height() / 2), QColor(35, 35, 35));
        gallery->close();
        QTRY_VERIFY(gallery.isNull());
    }

    void opensCustomCommandForLocalFolder()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        QAction *action = window.actionCollection()->action(QStringLiteral("custom_command"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<CustomCommandDialog *>(QStringLiteral("customCommandDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void opensMetadataEditorForOneLocalFile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *action = window.actionCollection()->action(QStringLiteral("edit_metadata"));
        QVERIFY(action && !action->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        QTRY_VERIFY(action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<MetadataEditDialog *>(QStringLiteral("metadataEditDialog"));
        QVERIFY(dialog);
    }

    void opensBatchRenameForMultipleLocalItems()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString firstPath = root.filePath(QStringLiteral("a.txt"));
        const QString secondPath = root.filePath(QStringLiteral("b.txt"));
        for (const QString &path : {firstPath, secondPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectAll();
        QTRY_COMPARE(view->selectedItems().size(), 2);
        QAction *rename = window.actionCollection()->action(QStringLiteral("rename"));
        QVERIFY(rename && rename->isEnabled());
        rename->trigger();
        auto *dialog = window.findChild<BatchRenameDialog *>(QStringLiteral("batchRenameDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void selectsVisibleItemsByPathFilter()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &name : {QStringLiteral("keep.txt"), QStringLiteral("other.txt")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        QTRY_VERIFY(!view->isLoading());
        auto *action = window.actionCollection()->action(QStringLiteral("select_by_path_filter"));
        QVERIFY(action);
        QTimer::singleShot(0, [&] {
            auto *dialog = window.findChild<QDialog *>(QStringLiteral("selectionPathDialog"));
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>(QStringLiteral("selectionPathFilter"))->setText(QStringLiteral("keep.txt"));
            auto *excluded = dialog->findChild<QPlainTextEdit *>(QStringLiteral("selectionExcludedFolders"));
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            excluded->setPlainText(QStringLiteral("relative"));
            buttons->button(QDialogButtonBox::Ok)->click();
            QVERIFY(!dialog->findChild<QLabel *>(QStringLiteral("selectionPathError"))->text().isEmpty());
            QVERIFY(view->selectedItems().isEmpty());
            excluded->clear();
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        action->trigger();
        QCOMPARE(view->selectedItems().size(), 1);
        QCOMPARE(view->selectedItems().first().name(), QStringLiteral("keep.txt"));
    }

    void selectsUsingSharedPathFilter()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &name : {QStringLiteral("keep.txt"), QStringLiteral("other.txt")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        auto config = KSharedConfig::openConfig();
        const auto group = config->group(QStringLiteral("SavedPathFilters"));
        QString error;
        QVERIFY(SavedPathFilters::save(group, {{QStringLiteral("Keep"), {QStringLiteral("keep.txt"), {}, {}, 0}}}, &error));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        QTRY_VERIFY(!view->isLoading());
        QTimer::singleShot(0, [&] {
            auto *dialog = window.findChild<QDialog *>(QStringLiteral("selectionPathDialog"));
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QComboBox *>(QStringLiteral("selectionFilterPresets"))->currentText(), QStringLiteral("Keep"));
            dialog->findChild<QPushButton *>(QStringLiteral("selectionLoadFilterPreset"))->click();
            QCOMPARE(dialog->findChild<QLineEdit *>(QStringLiteral("selectionPathFilter"))->text(), QStringLiteral("keep.txt"));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        window.actionCollection()->action(QStringLiteral("select_by_path_filter"))->trigger();
        QCOMPARE(view->selectedItems().size(), 1);
        QCOMPARE(view->selectedItems().first().name(), QStringLiteral("keep.txt"));
        config->deleteGroup(QStringLiteral("SavedPathFilters"));
        QVERIFY(config->sync());
    }

    void startupFoldersAndPaneDefaultsApply()
    {
        KConfigGroup startup = KSharedConfig::openConfig()->group(QStringLiteral("Startup"));
        startup.writeEntry("StartupMode", QStringLiteral("FixedFolders"));
        startup.writeEntry("LeftStartUrl", m_a.toString());
        startup.writeEntry("RightStartUrl", m_b.toString());
        KSharedConfig::openConfig()->group(QStringLiteral("Panes")).writeEntry("DualPaneAtStartup", QStringLiteral("Never"));
        KSharedConfig::openConfig()->group(QStringLiteral("Panes")).writeEntry("NewTabsAfterCurrent", true);
        KSharedConfig::openConfig()->group(QStringLiteral("Navigation")).writeEntry("FollowSymlinks", true);
        KSharedConfig::openConfig()->group(QStringLiteral("Views")).writeEntry("DefaultViewMode", QStringLiteral("Icons"));
        KSharedConfig::openConfig()->group(QStringLiteral("Views")).writeEntry("ShowHiddenFiles", true);
        KSharedConfig::openConfig()->sync();
        Settings::self()->load();

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
        QVERIFY(!window.actionCollection()->action(QStringLiteral("dual_layout"))->isChecked());
        QCOMPARE(window.activePane()->view()->viewMode(), FolderView::ViewMode::Icons);
        QVERIFY(window.activePane()->view()->showHiddenFiles());

        Pane *pane = window.activePane();
        pane->openInNewTab(m_b, true);
        pane->tabs()->setCurrentIndex(0);
        pane->openInNewTab(m_c, true);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_c, m_b}));

        const QString link = m_root.filePath(QStringLiteral("link-to-b"));
        QVERIFY(QFile::link(m_b.toLocalFile(), link));
        pane->setUrl(QUrl::fromLocalFile(link));
        QCOMPARE(pane->url(), m_b);
    }

    void init()
    {
        // Start from a clean session so the first window does not inherit
        // tabs or layout from an earlier test.
        QFile::remove(m_root.filePath(QStringLiteral("a/.mfm.json")));
        QFile::remove(m_root.filePath(QStringLiteral("b/.mfm.json")));
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PopularFolders"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SelectionFilters"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("View"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PanelSettings"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Archives"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Tabs"));
        for (const QString &group : {QStringLiteral("Startup"),
                                     QStringLiteral("Panes"),
                                     QStringLiteral("Navigation"),
                                     QStringLiteral("Previews"),
                                     QStringLiteral("PreviewSettings"),
                                     QStringLiteral("Confirmations"),
                                     QStringLiteral("StatusBar"),
                                     QStringLiteral("Views"),
                                     QStringLiteral("ContextMenu")}) {
            KSharedConfig::openConfig()->deleteGroup(group);
        }
        KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).writeEntry("ConfirmClosingMultipleTabs", false);
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("WindowLayout"));
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.deleteEntry("IgnoreList");
        files.deleteEntry("ExtensionColors");
        KSharedConfig::openConfig()->sync();
        Settings::self()->setDefaults();
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::self()->save();
    }

    void opensCommandLinePathsInBothPanes()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 2);
        QTRY_COMPARE(window.inactivePane()->view()->itemCount(), 0);
    }

    void previewFollowsSelectionAndRestoresLayout()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        QTRY_COMPARE(left->view()->itemCount(), 2);
        auto *toggle = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        auto *panel = window.findChild<PreviewPanel *>(QStringLiteral("previewPanel"));
        QVERIFY(toggle && panel);
        QVERIFY(toggle->shortcuts().contains(QKeySequence(Qt::Key_F3)));
        toggle->trigger();
        QVERIFY(panel->isVisible());
        QVERIFY(left->isVisible());
        QVERIFY(!right->isVisible());
        left->view()->selectUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")))});
        auto *text = panel->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QVERIFY(text);
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("hello"));
        left->view()->selectAll();
        QTRY_VERIFY(panel->findChild<QLabel *>(QStringLiteral("previewMessage"))->isVisible());
        window.actionCollection()->action(QStringLiteral("switch_pane"))->trigger();
        QCOMPARE(window.activePane(), right);
        QVERIFY(right->isVisible());
        QVERIFY(!left->isVisible());
        toggle->trigger();
        QVERIFY(!panel->isVisible());
        QVERIFY(left->isVisible());
        QVERIFY(right->isVisible());
    }

    void galleryUsesSortedItemsAndActivationSource()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage image(20, 10, QImage::Format_RGB32);
        image.fill(Qt::red);
        const QUrl alpha = QUrl::fromLocalFile(dir.filePath(QStringLiteral("alpha.png")));
        const QUrl beta = QUrl::fromLocalFile(dir.filePath(QStringLiteral("beta.png")));
        const QUrl gamma = QUrl::fromLocalFile(dir.filePath(QStringLiteral("gamma.png")));
        QVERIFY(image.save(alpha.toLocalFile()));
        QVERIFY(image.save(beta.toLocalFile()));
        QVERIFY(image.save(gamma.toLocalFile()));
        MainWindow window;
        window.openUrls({m_a, QUrl::fromLocalFile(dir.path())});
        window.show();
        Pane *other = window.inactivePane();
        FolderView *view = other->view();
        QTRY_COMPARE(view->itemCount(), 3);
        view->setSorting(KDirModel::Name, Qt::DescendingOrder);
        view->selectUrls({beta});
        Settings::setFileActivationDestination(Settings::EnumFileActivationDestination::Gallery);
        Q_EMIT view->itemActivated(view->selectedItems().first());
        QCOMPARE(window.activePane(), other);
        auto *gallery = window.findChild<GalleryWindow *>(QStringLiteral("galleryWindow"));
        QVERIFY(gallery);
        auto *filmstrip = gallery->findChild<QListWidget *>(QStringLiteral("galleryFilmstrip"));
        QVERIFY(filmstrip);
        QCOMPARE(filmstrip->count(), 3);
        QCOMPARE(filmstrip->item(0)->text(), QStringLiteral("gamma.png"));
        QCOMPARE(filmstrip->item(2)->text(), QStringLiteral("alpha.png"));
        QCOMPARE(filmstrip->currentRow(), 1);
        filmstrip->setCurrentRow(2);
        QCOMPARE(view->selectedItems().first().url(), alpha);
        gallery->close();
        QTRY_VERIFY(!window.findChild<GalleryWindow *>());
        view->selectUrls({alpha, gamma});
        window.actionCollection()->action(QStringLiteral("open_gallery"))->trigger();
        gallery = window.findChild<GalleryWindow *>(QStringLiteral("galleryWindow"));
        QVERIFY(gallery);
        filmstrip = gallery->findChild<QListWidget *>(QStringLiteral("galleryFilmstrip"));
        QCOMPARE(filmstrip->count(), 2);
        QCOMPARE(filmstrip->item(0)->text(), QStringLiteral("gamma.png"));
        QCOMPARE(filmstrip->item(1)->text(), QStringLiteral("alpha.png"));
        gallery->close();
    }

    void previewActivationPreservesFolderNavigation()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        const QUrl text = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        const QUrl folder = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")));
        Settings::setFileActivationDestination(Settings::EnumFileActivationDestination::Preview);
        view->selectUrls({text});
        Q_EMIT view->itemActivated(view->selectedItems().first());
        auto *panel = window.findChild<PreviewPanel *>(QStringLiteral("previewPanel"));
        QVERIFY(panel->isVisible());
        auto *content = panel->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(content->toPlainText(), QStringLiteral("hello"));
        view->selectUrls({folder});
        Q_EMIT view->itemActivated(view->selectedItems().first());
        QTRY_COMPARE(window.activePane()->url(), folder);
    }

    void previewStopsPlaybackOnSelectionChange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile audio(dir.filePath(QStringLiteral("audio.wav")));
        QVERIFY(audio.open(QIODevice::WriteOnly));
        const QByteArray header = QByteArray::fromHex("52494646641f000057415645666d74201000000001000100401f0000401f00000100080064617461401f0000");
        QCOMPARE(audio.write(header), header.size());
        QCOMPARE(audio.write(QByteArray(8000, char(0x80))), 8000);
        audio.close();
        QFile text(dir.filePath(QStringLiteral("next.txt")));
        QVERIFY(text.open(QIODevice::WriteOnly));
        QCOMPARE(text.write("next"), 4);
        text.close();
        PreviewPanel panel;
        panel.show();
        panel.setItems({KFileItem(QUrl::fromLocalFile(audio.fileName()))});
        QPointer<QMediaPlayer> player = panel.findChild<QMediaPlayer *>(QStringLiteral("mediaPlayer"));
        QVERIFY(player);
        QCOMPARE(player->playbackState(), QMediaPlayer::StoppedState);
        panel.setItems({KFileItem(QUrl::fromLocalFile(text.fileName()))});
        QVERIFY(player.isNull());
        auto *preview = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(preview->toPlainText(), QStringLiteral("next"));
    }

    void previewMarkupDoesNotLoadExternalResources()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString imagePath = dir.filePath(QStringLiteral("private.png"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));
        QFile html(dir.filePath(QStringLiteral("page.html")));
        QVERIFY(html.open(QIODevice::WriteOnly));
        const QByteArray markup = QByteArray("<h1>Hello</h1><img src=\"") + QUrl::fromLocalFile(imagePath).toEncoded() + QByteArray("\">");
        QCOMPARE(html.write(markup), markup.size());
        html.close();
        PreviewPanel panel;
        panel.show();
        panel.setItems({KFileItem(QUrl::fromLocalFile(html.fileName()))});
        auto *rendered = panel.findChild<QTextBrowser *>(QStringLiteral("previewRenderedText"));
        auto *source = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *mode = panel.findChild<QToolButton *>(QStringLiteral("previewRenderMode"));
        QVERIFY(rendered && source && mode);
        QTRY_VERIFY(rendered->isVisible());
        QVERIFY(rendered->toPlainText().contains(QStringLiteral("Hello")));
        const QVariant resource = rendered->document()->resource(QTextDocument::ImageResource, QUrl::fromLocalFile(imagePath));
        QCOMPARE(resource.metaType(), QMetaType::fromType<QImage>());
        QVERIFY(resource.value<QImage>().isNull());
        mode->click();
        QCOMPARE(source->toPlainText().toUtf8(), markup);
    }

    void previewPresentationSettings()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile html(dir.filePath(QStringLiteral("page.html")));
        QVERIFY(html.open(QIODevice::WriteOnly));
        QCOMPARE(html.write("<h1>Hello</h1>"), 14);
        html.close();
        QFile markdown(dir.filePath(QStringLiteral("page.md")));
        QVERIFY(markdown.open(QIODevice::WriteOnly));
        QCOMPARE(markdown.write("# Hello"), 7);
        markdown.close();
        QFile code(dir.filePath(QStringLiteral("code.cpp")));
        QVERIFY(code.open(QIODevice::WriteOnly));
        QCOMPARE(code.write("int main() { return 42; }\n"), 26);
        code.close();
        Settings::setPreviewFormatted(false);
        Settings::setPreviewLineNumbers(false);
        Settings::setPreviewSyntaxHighlighting(false);
        PreviewPanel panel;
        panel.show();
        auto *source = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *rendered = panel.findChild<QTextBrowser *>(QStringLiteral("previewRenderedText"));
        auto *mode = panel.findChild<QToolButton *>(QStringLiteral("previewRenderMode"));
        auto *lineNumbers = panel.findChild<QWidget *>(QStringLiteral("previewLineNumbers"));
        QVERIFY(source && rendered && mode && lineNumbers);
        panel.setItems({KFileItem(QUrl::fromLocalFile(html.fileName()))});
        QTRY_VERIFY(source->isVisible());
        QVERIFY(!mode->isChecked());
        QVERIFY(lineNumbers->isHidden());
        mode->click();
        QVERIFY(rendered->isVisible());
        panel.setItems({KFileItem(QUrl::fromLocalFile(markdown.fileName()))});
        QTRY_VERIFY(source->isVisible());
        QVERIFY(!mode->isChecked());
        Settings::setPreviewFormatted(true);
        panel.applyPresentationSettings();
        QVERIFY(rendered->isVisible());
        QVERIFY(mode->isChecked());

        panel.setItems({KFileItem(QUrl::fromLocalFile(code.fileName()))});
        QTRY_VERIFY(source->isVisible());
        QTRY_COMPARE(source->toPlainText(), QStringLiteral("int main() { return 42; }\n"));
        QVERIFY(source->document()->firstBlock().layout()->formats().isEmpty());
        Settings::setPreviewLineNumbers(true);
        Settings::setPreviewSyntaxHighlighting(true);
        panel.applyPresentationSettings();
        QVERIFY(lineNumbers->isVisible());
        QTRY_VERIFY(!source->document()->firstBlock().layout()->formats().isEmpty());
        Settings::setPreviewSyntaxHighlighting(false);
        panel.applyPresentationSettings();
        QTRY_VERIFY(source->document()->firstBlock().layout()->formats().isEmpty());
    }

    void editorPlacementsKeepBothPanesAndDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("sample.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("before"), 6);
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(true);
        auto *open = window.actionCollection()->action(QStringLiteral("open_editor"));
        QVERIFY(open);
        open->trigger();
        auto *editor = window.findChild<EditorPanel *>(QStringLiteral("editorPanel"));
        QVERIFY(editor);
        QTRY_COMPARE(editor->document()->text(), QStringLiteral("before"));
        const auto panes = window.findChildren<Pane *>();
        QCOMPARE(panes.size(), 2);
        QVERIFY(panes.at(0)->isVisible() && panes.at(1)->isVisible());
        QVERIFY(editor->document()->setText(QStringLiteral("after")));
        auto *markdownButton = editor->findChild<QPushButton *>(QStringLiteral("editorMarkdownPreview"));
        auto *markdownPreview = editor->findChild<QTextBrowser *>(QStringLiteral("editorRenderedMarkdown"));
        QVERIFY(markdownButton && markdownPreview);
        markdownButton->setChecked(true);
        QVERIFY(editor->document()->setText(QStringLiteral("# Live heading")));
        QCOMPARE(markdownPreview->toPlainText().trimmed(), QStringLiteral("Live heading"));
        QVERIFY(editor->document()->setText(QStringLiteral("after")));
        const QStringList placements{QStringLiteral("editorPlaceBelow"),
                                     QStringLiteral("editorPlaceRight"),
                                     QStringLiteral("editorPlaceWindow"),
                                     QStringLiteral("editorPlaceMaximized"),
                                     QStringLiteral("editorPlacePanel")};
        for (const QString &name : placements) {
            auto *action = editor->findChild<QAction *>(name);
            QVERIFY(action);
            action->trigger();
            QCOMPARE(editor->document()->text(), QStringLiteral("after"));
            if (name != QLatin1String("editorPlaceWindow") && name != QLatin1String("editorPlaceMaximized")) {
                QVERIFY(panes.at(0)->isVisible() && panes.at(1)->isVisible());
            }
        }
        QVERIFY(editor->document()->documentSave());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray saved = file.readAll();
        QVERIFY(saved == QByteArray("after") || saved == QByteArray("after\n"));
    }

    void editorUsesConfiguredOpeningPlacement()
    {
        Settings::setEditorOpening(Settings::EnumEditorOpening::Embedded);
        Settings::setEditorEmbeddedPlacement(Settings::EnumEditorEmbeddedPlacement::Right);
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("editorDock"));
        QVERIFY(dock);
        QVERIFY(dock->isVisible());
        QCOMPARE(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        Settings::setEditorOpening(Settings::EnumEditorOpening::Window);
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        QVERIFY(dock->isVisible());
        dock->hide();
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        auto *editorWindow = window.findChild<QDialog *>(QStringLiteral("editorWindow"));
        QVERIFY(editorWindow && editorWindow->isVisible());
        QVERIFY(!dock->isVisible());
    }

    void fileComparison()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("-first.txt"));
        const QString second = dir.filePath(QStringLiteral("second.txt"));
        QFile a(first);
        QFile b(second);
        QVERIFY(a.open(QIODevice::WriteOnly));
        QCOMPARE(a.write("same\nold\n"), 9);
        a.close();
        QVERIFY(b.open(QIODevice::WriteOnly));
        QCOMPARE(b.write("same\nnew\n"), 9);
        b.close();
        QVERIFY(!QStandardPaths::findExecutable(QStringLiteral("diff")).isEmpty());

        FileDiffDialog different(first, second);
        auto *status = different.findChild<QLabel *>(QStringLiteral("fileDiffStatus"));
        auto *output = different.findChild<QPlainTextEdit *>(QStringLiteral("fileDiffOutput"));
        QVERIFY(status && output);
        QTRY_COMPARE(status->text(), QStringLiteral("The files differ."));
        QVERIFY(output->toPlainText().contains(QStringLiteral("-old")));
        QVERIFY(output->toPlainText().contains(QStringLiteral("+new")));

        FileDiffDialog identical(first, first);
        QTRY_COMPARE(identical.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(), QStringLiteral("The files are identical."));
        FileDiffDialog missing(first, dir.filePath(QStringLiteral("missing")));
        QTRY_VERIFY(missing.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text().startsWith(QStringLiteral("The comparison failed:")));

        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toLocal8Bit());
        FileDiffDialog missingTool(first, second);
        qputenv("PATH", path);
        QTRY_VERIFY(missingTool.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text().startsWith(QStringLiteral("Could not start diff:")));

        QVERIFY(b.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(b.write(QByteArray(5 * 1024 * 1024, 'x')), qint64(5 * 1024 * 1024));
        b.close();
        FileDiffDialog large(first, second);
        QTRY_COMPARE(large.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(),
                     QStringLiteral("The comparison output exceeded 4 MiB and was stopped."));
        QVERIFY(large.findChild<QPlainTextEdit *>(QStringLiteral("fileDiffOutput"))->toPlainText().isEmpty());
    }

    void fileDiffWrapDoesNotChangeComparison()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile first(dir.filePath(QStringLiteral("a")));
        QFile second(dir.filePath(QStringLiteral("b")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        QVERIFY(first.write(QByteArray(500, 'a')) == 500);
        first.close();
        QVERIFY(second.open(QIODevice::WriteOnly));
        QVERIFY(second.write(QByteArray(500, 'b')) == 500);
        second.close();
        const bool previous = Settings::fileDiffWrapLines();
        const auto restore = qScopeGuard([previous] {
            Settings::setFileDiffWrapLines(previous);
        });
        Settings::setFileDiffWrapLines(true);
        FileDiffDialog dialog(first.fileName(), second.fileName());
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileDiffStatus"));
        auto *output = dialog.findChild<QPlainTextEdit *>(QStringLiteral("fileDiffOutput"));
        auto *wrap = dialog.findChild<QCheckBox *>(QStringLiteral("fileDiffWrapLines"));
        QVERIFY(status && output && wrap);
        QTRY_COMPARE(status->text(), QStringLiteral("The files differ."));
        QVERIFY(wrap->isChecked());
        QCOMPARE(output->lineWrapMode(), QPlainTextEdit::WidgetWidth);
        const QString comparison = output->toPlainText();
        wrap->setChecked(false);
        QCOMPARE(output->lineWrapMode(), QPlainTextEdit::NoWrap);
        QCOMPARE(output->toPlainText(), comparison);
        QCOMPARE(status->text(), QStringLiteral("The files differ."));
    }

    void compareFilesActionUsesBothPanes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("right")));
        const QString first = dir.filePath(QStringLiteral("left/first.txt"));
        const QString second = dir.filePath(QStringLiteral("right/second.txt"));
        QFile a(first);
        QFile b(second);
        QVERIFY(a.open(QIODevice::WriteOnly));
        a.write("first\n");
        a.close();
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.write("second\n");
        b.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")))});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *left = window.activePane()->view();
        auto *right = window.inactivePane()->view();
        QTRY_COMPARE(left->itemCount(), 1);
        QTRY_COMPARE(right->itemCount(), 1);
        auto *action = window.actionCollection()->action(QStringLiteral("compare_files"));
        QVERIFY(action && !action->isEnabled());
        left->selectUrls({QUrl::fromLocalFile(first)});
        QVERIFY(!action->isEnabled());
        right->selectUrls({QUrl::fromLocalFile(second)});
        QTRY_VERIFY(action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<FileDiffDialog *>(QStringLiteral("fileDiffDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(), QStringLiteral("The files differ."));
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(true);
        QVERIFY(!action->isEnabled());
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(false);
        QVERIFY(action->isEnabled());
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!action->isEnabled());
        QPointer<FileDiffDialog> closedDialog = dialog;
        dialog->close();
        QTRY_VERIFY(closedDialog.isNull());
        QFile localSecond(dir.filePath(QStringLiteral("left/third.txt")));
        QVERIFY(localSecond.open(QIODevice::WriteOnly));
        localSecond.write("third\n");
        localSecond.close();
        QTRY_COMPARE(left->itemCount(), 2);
        left->selectUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(localSecond.fileName())});
        QTRY_VERIFY(action->isEnabled());
        action->trigger();
        auto *samePaneDialog = window.findChild<FileDiffDialog *>(QStringLiteral("fileDiffDialog"));
        QVERIFY(samePaneDialog);
        QTRY_COMPARE(samePaneDialog->findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(), QStringLiteral("The files differ."));
    }

    void createsSymbolicLinkFromSelection()
    {
        const QString previousSuffix = Settings::symlinkSuffix();
        const auto restore = qScopeGuard([previousSuffix] {
            Settings::setSymlinkSuffix(previousSuffix);
        });
        Settings::setSymlinkSuffix(QString());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("source.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("source");
        source.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        QAction *action = window.actionCollection()->action(QStringLiteral("create_symlink"));
        QVERIFY(action && !action->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(source.fileName())});
        QTRY_VERIFY(action->isEnabled());
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->textValue(), QStringLiteral("source.txt - link"));
            dialog->reject();
        });
        action->trigger();
        Settings::setSymlinkSuffix(QStringLiteral(".shortcut"));
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->textValue(), QStringLiteral("source.txt.shortcut"));
            dialog->setTextValue(QStringLiteral("alias"));
            dialog->accept();
        });
        action->trigger();
        const QString link = dir.filePath(QStringLiteral("alias"));
        QTRY_VERIFY(QFileInfo(link).isSymLink());
        QCOMPARE(QFileInfo(link).symLinkTarget(), source.fileName());
    }

    void exportsFilteredListingInColumnOrder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &name : {QStringLiteral("z.txt"), QStringLiteral("a.txt"), QStringLiteral("hidden.bin")}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("data");
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        QTRY_VERIFY(!view->isLoading());
        view->setViewMode(FolderView::ViewMode::Details);
        view->setNameFilter(NameFilter(QStringLiteral("*.txt")));
        view->setSorting(KDirModel::Name, Qt::AscendingOrder);
        auto *details = qobject_cast<QTreeView *>(view->currentView());
        QVERIFY(details);
        view->setColumnVisible(KDirModel::Size, true);
        details->header()->moveSection(details->header()->visualIndex(KDirModel::Size), 0);
        const ListingExport::Table table = view->listingForExport();
        QCOMPARE(table.rows.size(), 2);
        QCOMPARE(table.headers.first(), details->model()->headerData(KDirModel::Size, Qt::Horizontal).toString());
        const int nameColumn = table.headers.indexOf(details->model()->headerData(KDirModel::Name, Qt::Horizontal).toString());
        QVERIFY(nameColumn >= 0);
        QCOMPARE(table.rows.at(0).at(nameColumn), QStringLiteral("a.txt"));
        QCOMPARE(table.rows.at(1).at(nameColumn), QStringLiteral("z.txt"));
        const QString output = dir.filePath(QStringLiteral("listing.csv"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(dir.filePath(QStringLiteral("listing")));
            QMetaObject::invokeMethod(dialog, "accept");
        });
        window.actionCollection()->action(QStringLiteral("export_listing_csv"))->trigger();
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray csv = file.readAll();
        QVERIFY(csv.contains("a.txt"));
        QVERIFY(csv.contains("z.txt"));
        QVERIFY(!csv.contains("hidden.bin"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(dir.filePath(QStringLiteral("listing")));
            QMetaObject::invokeMethod(dialog, "accept");
        });
        window.actionCollection()->action(QStringLiteral("export_listing_pdf"))->trigger();
        QFile pdf(dir.filePath(QStringLiteral("listing.pdf")));
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(5), QByteArray("%PDF-"));
    }

    void exportUsesCustomColumnsAndDefaultFormat()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("item.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("data");
        source.close();
        const auto format = Settings::listingExportFormat();
        const bool custom = Settings::listingExportCustomColumns();
        const bool type = Settings::listingExportType();
        const bool size = Settings::listingExportSize();
        const bool modified = Settings::listingExportModified();
        const bool permissions = Settings::listingExportPermissions();
        const bool owner = Settings::listingExportOwner();
        const bool group = Settings::listingExportGroup();
        const auto restore = qScopeGuard([=] {
            Settings::setListingExportFormat(format);
            Settings::setListingExportCustomColumns(custom);
            Settings::setListingExportType(type);
            Settings::setListingExportSize(size);
            Settings::setListingExportModified(modified);
            Settings::setListingExportPermissions(permissions);
            Settings::setListingExportOwner(owner);
            Settings::setListingExportGroup(group);
        });
        Settings::setListingExportFormat(Settings::EnumListingExportFormat::Pdf);
        Settings::setListingExportCustomColumns(true);
        Settings::setListingExportType(false);
        Settings::setListingExportSize(false);
        Settings::setListingExportModified(false);
        Settings::setListingExportPermissions(false);
        Settings::setListingExportOwner(true);
        Settings::setListingExportGroup(false);
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        QTRY_VERIFY(!view->isLoading());
        view->setViewMode(FolderView::ViewMode::Details);
        view->setColumnVisible(KDirModel::Owner, false);
        const ListingExport::Table table = view->listingForExport({KDirModel::Name, KDirModel::Owner});
        QCOMPARE(table.headers.size(), 2);
        QCOMPARE(table.rows.size(), 1);
        QVERIFY(!table.rows.first().at(1).isEmpty());
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(dir.filePath(QStringLiteral("custom")));
            QMetaObject::invokeMethod(dialog, "accept");
        });
        window.actionCollection()->action(QStringLiteral("export_listing_csv"))->trigger();
        QFile csv(dir.filePath(QStringLiteral("custom.csv")));
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const QList<QByteArray> lines = csv.readAll().split('\n');
        QCOMPARE(lines.at(0).count(','), 1);
        QVERIFY(lines.at(1).contains("item.txt"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(dir.filePath(QStringLiteral("default")));
            QMetaObject::invokeMethod(dialog, "accept");
        });
        window.actionCollection()->action(QStringLiteral("export_listing"))->trigger();
        QFile pdf(dir.filePath(QStringLiteral("default.pdf")));
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(5), QByteArray("%PDF-"));
    }

    void contentChartUsesSelectedFolderOrCurrentFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
        QFile file(dir.filePath(QStringLiteral("file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *action = window.actionCollection()->action(QStringLiteral("content_chart"));
        QVERIFY(action && action->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("sub")))});
        action->trigger();
        QPointer<ContentChartDialog> chart = window.findChild<ContentChartDialog *>(QStringLiteral("contentChartDialog"));
        QVERIFY(chart);
        QCOMPARE(chart->findChild<QLabel *>(QStringLiteral("contentChartFolder"))->text(),
                 QUrl::fromLocalFile(dir.filePath(QStringLiteral("sub"))).toDisplayString());
        chart->close();
        QTRY_VERIFY(chart.isNull());
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        action->trigger();
        chart = window.findChild<ContentChartDialog *>(QStringLiteral("contentChartDialog"));
        QVERIFY(chart);
        QCOMPARE(chart->findChild<QLabel *>(QStringLiteral("contentChartFolder"))->text(), QUrl::fromLocalFile(dir.path()).toDisplayString());
    }

    void createsAndVerifiesChecksumFiles()
    {
        const QString previousName = Settings::checksumFileName();
        const auto restore = qScopeGuard([previousName] {
            Settings::setChecksumFileName(previousName);
        });
        Settings::setChecksumFileName(QStringLiteral("custom.sha256"));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("source.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("source");
        source.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *create = window.actionCollection()->action(QStringLiteral("create_checksum"));
        auto *verify = window.actionCollection()->action(QStringLiteral("verify_checksum"));
        QVERIFY(create && verify && !create->isEnabled() && !verify->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(source.fileName())});
        QTRY_VERIFY(create->isEnabled());
        const QString manifest = dir.filePath(QStringLiteral("checksums.sha256"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->selectedFiles(), QStringList{dir.filePath(QStringLiteral("custom.sha256"))});
            dialog->selectFile(manifest);
            QMetaObject::invokeMethod(dialog, "accept");
        });
        create->trigger();
        QPointer<ChecksumDialog> dialog = window.findChild<ChecksumDialog *>(QStringLiteral("checksumDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("checksumStatus"))->text(), QStringLiteral("Checksum file created."));
        QVERIFY(QFileInfo::exists(manifest));
        dialog->close();
        QTRY_VERIFY(dialog.isNull());
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({QUrl::fromLocalFile(manifest)});
        QTRY_VERIFY(verify->isEnabled());
        verify->trigger();
        dialog = window.findChild<ChecksumDialog *>(QStringLiteral("checksumDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("checksumStatus"))->text(), QStringLiteral("All files verified."));
        auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("checksumResults"));
        QVERIFY(results);
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), QStringLiteral("source.txt"));
        dialog->close();
        QTRY_VERIFY(dialog.isNull());
        QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Truncate));
        source.write("changed");
        source.close();
        verify->trigger();
        dialog = window.findChild<ChecksumDialog *>(QStringLiteral("checksumDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("checksumStatus"))->text(), QStringLiteral("Verification finished: 1 file failed."));
    }

    void opensDuplicateFinderForLocalFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *action = window.actionCollection()->action(QStringLiteral("find_duplicates"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<DuplicateFinderDialog *>(QStringLiteral("duplicateFinderDialog"));
        QVERIFY(dialog);
        QTRY_VERIFY(!dialog->findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->isEnabled());
        QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text().startsWith(QStringLiteral("Found 0 groups")));
    }

    void linksPaneNavigationWithinStartingFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/sub/child")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("right/sub/child")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/only-left")));
        const QUrl leftRoot = QUrl::fromLocalFile(dir.filePath(QStringLiteral("left")));
        const QUrl rightRoot = QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")));
        MainWindow window;
        window.openUrls({leftRoot, rightRoot});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *link = window.actionCollection()->action(QStringLiteral("link_panes"));
        QVERIFY(link);
        link->setChecked(true);
        QVERIFY(link->isChecked());
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        left->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub/child"))));
        QCOMPARE(right->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub/child"))));
        right->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub"))));
        QCOMPARE(left->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub"))));
        left->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/only-left"))));
        QVERIFY(!link->isChecked());
        QCOMPARE(right->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub"))));
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("unlinked")));
        left->setUrl(leftRoot);
        right->setUrl(rightRoot);
        link->setChecked(true);
        QVERIFY(link->isChecked());
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!link->isChecked());
        link->setChecked(true);
        QVERIFY(!link->isChecked());
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Show both panes")));
    }

    void linksInitialPaneFoldersOnlyWhenConfigured()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("right/sub")));
        const auto oldLink = Settings::linkPanesAtStartup();
        const auto oldDual = Settings::dualPaneAtStartup();
        const auto oldMode = Settings::startupMode();
        const auto oldHome = Settings::homeUrl();
        const auto restore = qScopeGuard([oldLink, oldDual, oldMode, oldHome] {
            Settings::setLinkPanesAtStartup(oldLink);
            Settings::setDualPaneAtStartup(oldDual);
            Settings::setStartupMode(oldMode);
            Settings::setHomeUrl(oldHome);
        });
        Settings::setStartupMode(Settings::EnumStartupMode::HomeFolder);
        Settings::setHomeUrl(QUrl::fromLocalFile(dir.path()));
        Settings::setDualPaneAtStartup(Settings::EnumDualPaneAtStartup::Always);
        Settings::setLinkPanesAtStartup(false);
        const QUrl left = QUrl::fromLocalFile(dir.filePath(QStringLiteral("left")));
        const QUrl right = QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")));
        {
            MainWindow window;
            window.openUrls({left, right});
            QVERIFY(!window.actionCollection()->action(QStringLiteral("link_panes"))->isChecked());
        }
        Settings::setLinkPanesAtStartup(true);
        {
            MainWindow window;
            window.openUrls({left, right});
            auto *link = window.actionCollection()->action(QStringLiteral("link_panes"));
            QVERIFY(link->isChecked());
            window.activePane()->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub"))));
            QCOMPARE(window.inactivePane()->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub"))));
            link->setChecked(false);
            window.openUrls({});
            QVERIFY(!link->isChecked());
        }
        {
            MainWindow window;
            window.openUrls({left, QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing")))});
            QVERIFY(!window.actionCollection()->action(QStringLiteral("link_panes"))->isChecked());
            QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("existing local folders")));
        }
        Settings::setDualPaneAtStartup(Settings::EnumDualPaneAtStartup::Never);
        {
            MainWindow window;
            window.openUrls({left, right});
            QVERIFY(!window.actionCollection()->action(QStringLiteral("link_panes"))->isChecked());
            QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Show both panes")));
        }
    }

    void opensSplitAndCombineForLocalFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("source")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("data"), qint64(4));
        source.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *split = window.actionCollection()->action(QStringLiteral("split_file"));
        auto *combine = window.actionCollection()->action(QStringLiteral("combine_files"));
        QVERIFY(split && combine);
        QVERIFY(!split->isEnabled());
        QVERIFY(!combine->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(source.fileName())});
        QTRY_VERIFY(split->isEnabled());
        QVERIFY(combine->isEnabled());
        split->trigger();
        auto *dialog = window.findChild<FileSplitDialog *>(QStringLiteral("fileSplitDialog"));
        QVERIFY(dialog);
        dialog->close();
        QTRY_VERIFY(!window.findChild<FileSplitDialog *>(QStringLiteral("fileSplitDialog")));
        combine->trigger();
        QVERIFY(window.findChild<FileSplitDialog *>(QStringLiteral("fileSplitDialog")));
    }

    void paneComparisonFiltersShownResultsWithoutSelectingAutomatically()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("right")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("left/sub")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("right/sub")));
        for (const auto &[name, data] : QList<QPair<QString, QByteArray>>{{QStringLiteral("left/left-only"), "l"},
                                                                          {QStringLiteral("right/right-only"), "r"},
                                                                          {QStringLiteral("left/shared"), "s"},
                                                                          {QStringLiteral("right/shared"), "larger"}}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(data), qint64(data.size()));
        }
        const auto oldLeft = Settings::paneComparisonShowLeftOnly();
        const auto oldRight = Settings::paneComparisonShowRightOnly();
        const auto oldDifferent = Settings::paneComparisonShowDifferent();
        const auto oldMatching = Settings::paneComparisonShowMatching();
        const auto restore = qScopeGuard([oldLeft, oldRight, oldDifferent, oldMatching] {
            Settings::setPaneComparisonShowLeftOnly(oldLeft);
            Settings::setPaneComparisonShowRightOnly(oldRight);
            Settings::setPaneComparisonShowDifferent(oldDifferent);
            Settings::setPaneComparisonShowMatching(oldMatching);
        });
        Settings::setPaneComparisonShowLeftOnly(true);
        Settings::setPaneComparisonShowRightOnly(false);
        Settings::setPaneComparisonShowDifferent(false);
        Settings::setPaneComparisonShowMatching(false);
        FolderComparisonDialog dialog(QUrl::fromLocalFile(root.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(root.filePath(QStringLiteral("right"))));
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("folderComparisonCancel"));
        QTRY_VERIFY(!cancel->isEnabled());
        auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("folderComparisonResults"));
        auto row = [tree](const QString &name) {
            return tree->findItems(name, Qt::MatchExactly).value(0);
        };
        QVERIFY(row(QStringLiteral("left-only")) && !row(QStringLiteral("left-only"))->isHidden());
        QVERIFY(row(QStringLiteral("right-only")) && row(QStringLiteral("right-only"))->isHidden());
        QVERIFY(row(QStringLiteral("shared")) && row(QStringLiteral("shared"))->isHidden());
        QVERIFY(row(QStringLiteral("sub")) && !row(QStringLiteral("sub"))->isHidden());
        QSignalSpy selected(&dialog, &FolderComparisonDialog::selectDifferences);
        QCOMPARE(selected.size(), 0);
        dialog.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"))->click();
        QCOMPARE(selected.size(), 1);
        QCOMPARE(selected.first().at(0).value<QList<QUrl>>(), (QList<QUrl>{QUrl::fromLocalFile(root.filePath(QStringLiteral("left/left-only")))}));
        QVERIFY(selected.first().at(1).value<QList<QUrl>>().isEmpty());
        dialog.findChild<QCheckBox *>(QStringLiteral("folderComparisonShowLeftOnly"))->setChecked(false);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"))->isEnabled());
        dialog.findChild<QCheckBox *>(QStringLiteral("folderComparisonShowRightOnly"))->setChecked(true);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"))->isEnabled());
    }

    void comparesPaneFoldersAndSelectsDifferences()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("right")));
        for (const auto &[name, data] : QList<QPair<QString, QByteArray>>{{QStringLiteral("left/left-only"), "l"},
                                                                          {QStringLiteral("left/.hidden"), "h"},
                                                                          {QStringLiteral("right/right-only"), "r"},
                                                                          {QStringLiteral("left/shared"), "small"},
                                                                          {QStringLiteral("right/shared"), "larger!"}}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(data), qint64(data.size()));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")))});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        QTRY_COMPARE(left->view()->itemCount(), 2);
        QTRY_COMPARE(right->view()->itemCount(), 2);
        QAction *action = window.actionCollection()->action(QStringLiteral("compare_pane_folders"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<FolderComparisonDialog *>(QStringLiteral("folderComparisonDialog"));
        QVERIFY(dialog);
        auto *select = dialog->findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"));
        auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("folderComparisonResults"));
        QVERIFY(select && results);
        QTRY_VERIFY(select->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 4);
        select->click();
        QTRY_COMPARE(left->view()->selectedItems().size(), 2);
        QTRY_COMPARE(right->view()->selectedItems().size(), 2);
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Some differences could not be selected")));
        left->view()->setShowHiddenFiles(true);
        QTRY_COMPARE(left->view()->itemCount(), 3);
        select->click();
        QTRY_COMPARE(left->view()->selectedItems().size(), 3);
        right->setUrl(QUrl::fromLocalFile(dir.path()));
        select->click();
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("changed or are hidden")));
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!action->isEnabled());
    }

    void folderComparisonHandlesErrorsAndCancellation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FolderComparisonDialog failed(QUrl::fromLocalFile(dir.path()), QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing"))));
        auto *status = failed.findChild<QLabel *>(QStringLiteral("folderComparisonStatus"));
        auto *cancel = failed.findChild<QPushButton *>(QStringLiteral("folderComparisonCancel"));
        auto *select = failed.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"));
        QVERIFY(status && cancel && select);
        QTRY_VERIFY(!cancel->isEnabled());
        QVERIFY(status->text().startsWith(QStringLiteral("Could not list")));
        QVERIFY(!select->isEnabled());
        FolderComparisonDialog canceled(QUrl::fromLocalFile(dir.path()), QUrl::fromLocalFile(dir.path()));
        canceled.findChild<QPushButton *>(QStringLiteral("folderComparisonCancel"))->click();
        QCOMPARE(canceled.findChild<QLabel *>(QStringLiteral("folderComparisonStatus"))->text(), QStringLiteral("Comparison canceled."));
        QVERIFY(!canceled.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"))->isEnabled());
    }

    void navigationUpdatesHistory()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *pane = window.activePane();
        QTRY_COMPARE(pane->view()->itemCount(), 2);

        pane->setUrl(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub"))));
        QVERIFY(pane->tabs()->canGoBack());
        pane->goUp();
        QCOMPARE(pane->url(), m_a);
        pane->goBack();
        QCOMPARE(pane->url().toLocalFile(), m_root.filePath(QStringLiteral("a/sub")));
    }

    void displayStyleIsIndependentPerPane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *active = window.activePane();
        Pane *other = window.inactivePane();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Details);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);

        window.actionCollection()->action(QStringLiteral("toggle_view_mode"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Icons);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);
        window.actionCollection()->action(QStringLiteral("compact_view"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Compact);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);
        window.actionCollection()->action(QStringLiteral("toggle_view_mode"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Details);
    }

    void selectionModeActionBarCopiesFromItsPane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *action = window.actionCollection()->action(QStringLiteral("selection_mode"));
        QVERIFY(action);
        action->trigger();
        QVERIFY(view->selectionMode());
        auto *copy = view->findChild<QToolButton *>(QStringLiteral("selectionCopy"));
        QVERIFY(copy);
        QVERIFY(!copy->isEnabled());
        const QUrl file = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        view->selectUrls({file});
        QVERIFY(copy->isEnabled());
        copy->click();
        QCOMPARE(QApplication::clipboard()->mimeData()->urls(), QList<QUrl>{file});
        view->findChild<QToolButton *>(QStringLiteral("selectionDone"))->click();
        QVERIFY(!action->isChecked());
        QVERIFY(!view->selectionMode());
    }

    void tabAppearanceSettingsApplyToBothPanes()
    {
        KConfigGroup tabs = KSharedConfig::openConfig()->group(QStringLiteral("Tabs"));
        tabs.writeEntry("ShowIcons", false);
        tabs.writeEntry("Expand", true);
        tabs.sync();
        Settings::self()->load();

        MainWindow window;
        window.openUrls({m_a});
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            QTabBar *tabBar = pane->findChild<QTabBar *>();
            QVERIFY(tabBar);
            QVERIFY(tabBar->expanding());
            QVERIFY(tabBar->tabIcon(0).isNull());
        }
    }

    void panelSettingsApplyOnStartup()
    {
        KConfigGroup panels = KSharedConfig::openConfig()->group(QStringLiteral("PanelSettings"));
        panels.writeEntry("Visible", false);
        panels.writeEntry("Position", QStringLiteral("right"));
        panels.writeEntry("TitlesVisible", false);
        panels.writeEntry("FavoritesVisible", false);
        panels.writeEntry("PlacesVisible", false);
        panels.writeEntry("FoldersVisible", false);
        panels.sync();
        Settings::self()->load();

        MainWindow window;
        auto *combined = window.findChild<QDockWidget *>(QStringLiteral("combinedDock"));
        auto *places = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
        QVERIFY(combined);
        QVERIFY(places);
        QVERIFY(combined->isHidden());
        QCOMPARE(window.dockWidgetArea(places), Qt::RightDockWidgetArea);
        QVERIFY(places->titleBarWidget());

        auto *panel = combined->findChild<CombinedPanel *>();
        QVERIFY(panel);
        auto *favorites = panel->findChild<QListView *>();
        auto *panelPlaces = panel->findChild<KFilePlacesView *>();
        auto *tree = panel->findChild<FolderTree *>();
        QVERIFY(favorites);
        QVERIFY(panelPlaces);
        QVERIFY(tree);
        QVERIFY(favorites->isHidden());
        QVERIFY(panelPlaces->isHidden());
        QVERIFY(tree->isHidden());
    }

    void jumpBackTogglesBetweenFolders()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        pane->setUrl(m_b);
        QAction *jumpBack = window.actionCollection()->action(QStringLiteral("jump_back"));
        QVERIFY(jumpBack && jumpBack->isEnabled());
        jumpBack->trigger();
        QCOMPARE(pane->url(), m_a);
        jumpBack->trigger();
        QCOMPARE(pane->url(), m_b);
    }

    void popularFoldersMenuListsVisitedLocations()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.activePane()->setUrl(m_b);

        QAction *popularAction = window.actionCollection()->action(QStringLiteral("popular_folders"));
        QVERIFY(popularAction && popularAction->menu());
        QMenu *menu = popularAction->menu();
        QMetaObject::invokeMethod(menu, "aboutToShow");
        QStringList names;
        for (QAction *action : menu->actions()) {
            names.append(action->text());
        }
        QVERIFY(names.contains(QStringLiteral("a")));
        QVERIFY(names.contains(QStringLiteral("b")));
    }

    void savedSelectionFilterSelectsMatchingFiles()
    {
        KSharedConfig::openConfig()->group(QStringLiteral("SelectionFilters")).writeEntry("Patterns", QStringList{QStringLiteral("*.txt")});
        MainWindow window;
        window.openUrls({m_a});
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 2);

        QAction *selectAction = window.actionCollection()->action(QStringLiteral("select_menu"));
        QVERIFY(selectAction && selectAction->menu());
        QMenu *saved = nullptr;
        for (QAction *action : selectAction->menu()->actions()) {
            if (action->menu() && action->text() == QLatin1String("Saved Selection Filters")) {
                saved = action->menu();
                break;
            }
        }
        QVERIFY(saved);
        QMetaObject::invokeMethod(saved, "aboutToShow");
        QCOMPARE(saved->actions().size(), 1);
        saved->actions().first()->trigger();
        QTRY_COMPARE(window.activePane()->view()->selectedItems().size(), 1);
        QCOMPARE(window.activePane()->view()->selectedItems().first().name(), QStringLiteral("file.txt"));
    }

    void occupiedSpaceActionUsesKioDirectorySize()
    {
        QFile nested(m_root.filePath(QStringLiteral("a/sub/nested.txt")));
        QVERIFY(nested.open(QIODevice::WriteOnly));
        QCOMPARE(nested.write("hi"), 2);
        nested.close();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")))});

        window.actionCollection()->action(QStringLiteral("calculate_occupied_space"))->trigger();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("occupied in")));
        QVERIFY2(window.statusBar()->currentMessage().contains(QStringLiteral("5 B")), qPrintable(window.statusBar()->currentMessage()));

        view->clearSelection();
        window.actionCollection()->action(QStringLiteral("calculate_occupied_space"))->trigger();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("folder")));
    }

    void filterPersistsWithThePaneSession()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            auto *filter = window.activePane()->findChild<FilterBar *>();
            QVERIFY(filter);
            filter->setFilter(NameFilter(QStringLiteral("*.txt"), NameFilter::Scope::Files));
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        QCOMPARE(restored.activePane()->view()->nameFilter(), NameFilter(QStringLiteral("*.txt"), NameFilter::Scope::Files));
        QTRY_COMPARE(restored.activePane()->view()->itemCount(), 2);
    }

    void viewSettingsApplyToCurrentListing()
    {
        ViewSettings settings;
        settings.visibleColumns = {QStringLiteral("size")};
        settings.groupBy = QStringLiteral("ext");
        settings.sizeFormat = QStringLiteral("bytes");
        settings.dateFormat = QStringLiteral("iso");
        settings.iconNameLines = 4;
        settings.iconInformation = {QStringLiteral("size")};
        KConfigGroup viewConfig = KSharedConfig::openConfig()->group(QStringLiteral("View"));
        settings.writeConfig(viewConfig);
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.writeEntry("IgnoreList", QStringList{QStringLiteral("sub")});
        files.writeEntry("ExtensionColors", QStringList{QStringLiteral("txt=#123456")});
        KSharedConfig::openConfig()->sync();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        QCOMPARE(view->groupBy(), QStringLiteral("ext"));

        auto *details = view->findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QVERIFY(!details->isColumnHidden(KDirModel::Size));
        QVERIFY(details->isColumnHidden(KDirModel::ModifiedTime));
        const QModelIndex nameIndex = details->model()->index(0, KDirModel::Name);
        QCOMPARE(details->model()->data(nameIndex, Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#123456")));
        QCOMPARE(details->model()->data(details->model()->index(0, KDirModel::Size)).toString(), QStringLiteral("5 B"));
        QVERIFY(QRegularExpression(QStringLiteral(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}$)"))
                    .match(details->model()->data(details->model()->index(0, KDirModel::ModifiedTime)).toString())
                    .hasMatch());

        view->setViewMode(FolderView::ViewMode::Icons);
        auto *delegate = qobject_cast<KFileItemDelegate *>(view->currentView()->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), KFileItemDelegate::InformationList{KFileItemDelegate::Size});
        auto *icons = qobject_cast<QListView *>(view->currentView());
        QVERIFY(icons);
        QVERIFY(icons->gridSize().height() > 112);
    }

    void preferencesActionAppliesSettingsToOpenPanes()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);

        QAction *preferences = window.actionCollection()->action(QStringLiteral("options_configure"));
        QVERIFY(preferences);
        preferences->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        auto *groupBy = dialog->findChild<QComboBox *>(QStringLiteral("kcfg_GroupBy"));
        QVERIFY(groupBy);
        groupBy->setCurrentIndex(Settings::EnumGroupBy::ext);
        auto *typeColumn = dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTypeColumn"));
        QVERIFY(typeColumn);
        typeColumn->setChecked(false);
        QPushButton *ok = dialog->button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        ok->click();

        QCOMPARE(view->groupBy(), QStringLiteral("ext"));
        auto *details = view->findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QCOMPARE(window.inactivePane()->view()->groupBy(), QStringLiteral("ext"));
    }

    void copyToOtherPaneUsesKio()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);

        const QUrl source = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        FileOperations::copy({source}, window.inactivePane()->url(), &window);
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("b/file.txt"))));
        QTRY_COMPARE(window.inactivePane()->view()->itemCount(), 1);
    }

    void stackedLayoutMeetsTabBarsInTheMiddle()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.inactivePane()->openInNewTab(m_a);
        window.resize(800, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        window.setStackedLayout(true);
        QVERIFY(window.isStackedLayout());
        Pane *top = window.activePane();
        Pane *bottom = window.inactivePane();
        auto *splitter = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(splitter);
        QCOMPARE(splitter->orientation(), Qt::Vertical);
        QCOMPARE(splitter->widget(0), top);
        QCOMPARE(splitter->widget(1), bottom);

        QTRY_VERIFY(tabBarOf(top)->isVisible() && tabBarOf(bottom)->isVisible());
        QCOMPARE(tabBarOf(top)->shape(), QTabBar::RoundedSouth);
        QCOMPARE(tabBarOf(bottom)->shape(), QTabBar::RoundedNorth);
        // Top pane: view above tabs. Bottom pane: tabs above view.
        QTRY_VERIFY(globalTop(tabBarOf(top)) > globalTop(top->view()));
        QTRY_VERIFY(globalTop(tabBarOf(bottom)) < globalTop(bottom->view()));
        // Nothing but the splitter handle separates the two tab bars.
        const int gap = globalTop(tabBarOf(bottom)) - (globalTop(tabBarOf(top)) + tabBarOf(top)->height());
        QVERIFY2(gap >= 0 && gap <= splitter->handleWidth(), qPrintable(QString::number(gap)));

        window.setStackedLayout(false);
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        QCOMPARE(tabBarOf(top)->shape(), QTabBar::RoundedNorth);
        QTRY_VERIFY(globalTop(tabBarOf(top)) < globalTop(top->view()));
    }

    void stackedLayoutSurvivesRestart()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.setStackedLayout(true);
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        QVERIFY(restored.isStackedLayout());
    }

    void sessionSurvivesRestart()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.activePane()->openInNewTab(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub"))), true);
            QCOMPARE(window.activePane()->tabs()->count(), 3);
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        // The first window restored an empty session (home), then added a and a/sub.
        const QList<QUrl> left = restored.activePane()->tabs()->urls();
        QVERIFY(left.contains(m_a));
        QVERIFY(left.contains(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")))));
        QCOMPARE(restored.activePane()->url().toLocalFile(), m_root.filePath(QStringLiteral("a/sub")));
        QVERIFY(restored.inactivePane()->tabs()->urls().contains(m_b));
    }

private:
    static QTabBar *tabBarOf(Pane *pane)
    {
        return pane->findChild<QTabBar *>();
    }

    static int globalTop(QWidget *widget)
    {
        return widget->mapToGlobal(QPoint(0, 0)).y();
    }

    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_c;
};

QTEST_MAIN(MainWindowTest)

#include "mainwindowtest.moc"
