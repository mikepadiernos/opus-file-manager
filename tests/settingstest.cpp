// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "settingsdialog.h"
#include "tabmodel.h"

#include <KActionCollection>
#include <KDirModel>
#include <KSharedConfig>
#include <KUrlNavigator>
#include <KUrlRequester>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>

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

    static QAction *action(MainWindow &window, const QString &name)
    {
        return window.actionCollection()->action(name);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("c")));
        QVERIFY(QFile::link(m_root.filePath(QStringLiteral("a")), m_root.filePath(QStringLiteral("link"))));
        m_a = url(QStringLiteral("a"));
        m_b = url(QStringLiteral("b"));
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::self()->save();
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void dialogHasAWidgetForEverySetting()
    {
        MainWindow window;
        SettingsDialog dialog(&window);
        const auto items = Settings::self()->items();
        for (const KConfigSkeletonItem *item : items) {
            QVERIFY2(dialog.findChild<QWidget *>(QStringLiteral("kcfg_") + item->name()), qPrintable(item->name()));
        }
    }

    void dialogApplyWritesSettings()
    {
        MainWindow window;
        SettingsDialog dialog(&window);
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StartupMode"))->setCurrentIndex(Settings::EnumStartupMode::FixedFolders);
        dialog.findChild<KUrlRequester *>(QStringLiteral("kcfg_LeftStartUrl"))->setUrl(m_b);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FollowSymlinks"))->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();

        QCOMPARE(Settings::startupMode(), Settings::EnumStartupMode::FixedFolders);
        QCOMPARE(Settings::leftStartUrl(), m_b);
        QVERIFY(Settings::followSymlinks());
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
        SettingsDialog dialog(&window);
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
        SettingsDialog dialog(&window);
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
