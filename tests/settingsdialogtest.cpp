// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "settings.h"
#include "settingswidgets.h"
#include "viewsettings.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KPageWidget>
#include <KSharedConfig>
#include <KUrlRequester>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

class SettingsDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SettingsDialog"));
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
        KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->deleteGroup(QStringLiteral("Confirmations"));
        KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->deleteGroup(QStringLiteral("PreviewSettings"));
        KSharedConfig::openConfig(QStringLiteral("kservicemenurc"), KConfig::NoGlobals)->deleteGroup(QStringLiteral("Show"));
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.deleteEntry("IgnoreList");
        files.deleteEntry("ExtensionColors");
        KSharedConfig::openConfig()->sync();
        Settings::self()->setDefaults();
        Settings::self()->save();
    }

    void sidebarOptionsPersistAndPreview()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            auto *pages = dialog.findChild<KPageWidget *>();
            auto *size = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"));
            auto *position = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"));
            QVERIFY(pages && size && position);
            const auto sidebar = [pages] {
                for (QAbstractItemView *view : pages->findChildren<QAbstractItemView *>()) {
                    if (view->model() == pages->model() && view->isVisible()) {
                        return view;
                    }
                }
                return static_cast<QAbstractItemView *>(nullptr);
            };
            QCOMPARE(size->value(), 24);
            QCOMPARE(position->currentIndex(), int(Settings::EnumSidebarTitlePosition::Beside));
            QCOMPARE(pages->faceType(), KPageView::FlatList);
            QTRY_VERIFY(sidebar());
            QCOMPARE(sidebar()->iconSize(), QSize(24, 24));

            auto *current = dialog.currentPage();
            size->setValue(40);
            position->setCurrentIndex(Settings::EnumSidebarTitlePosition::Below);
            QCOMPARE(pages->faceType(), KPageView::List);
            QTRY_VERIFY(sidebar());
            QTRY_COMPARE(sidebar()->iconSize(), QSize(40, 40));
            QCOMPARE(dialog.currentPage(), current);
            position->setCurrentIndex(Settings::EnumSidebarTitlePosition::Beside);
            QTRY_COMPARE(pages->faceType(), KPageView::FlatList);
            QTRY_VERIFY(sidebar());
            QTRY_COMPARE(sidebar()->iconSize(), QSize(40, 40));
            position->setCurrentIndex(Settings::EnumSidebarTitlePosition::Below);
            QTRY_COMPARE(pages->faceType(), KPageView::List);
            QTRY_VERIFY(sidebar());
            QTRY_COMPARE(sidebar()->iconSize(), QSize(40, 40));
            dialog.button(QDialogButtonBox::Ok)->click();
        }

        {
            SettingsDialog reopened(&places, nullptr);
            reopened.show();
            QVERIFY(QTest::qWaitForWindowExposed(&reopened));
            QCOMPARE(reopened.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"))->value(), 40);
            QCOMPARE(reopened.findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"))->currentIndex(),
                     int(Settings::EnumSidebarTitlePosition::Below));
            QCOMPARE(reopened.findChild<KPageWidget *>()->faceType(), KPageView::List);
            reopened.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"))->setValue(16);
            reopened.findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"))->setCurrentIndex(0);
            reopened.reject();
        }

        SettingsDialog afterCancel(&places, nullptr);
        QCOMPARE(afterCancel.findChild<QSpinBox *>(QStringLiteral("kcfg_SidebarIconSize"))->value(), 40);
        QCOMPARE(afterCancel.findChild<QComboBox *>(QStringLiteral("kcfg_SidebarTitlePosition"))->currentIndex(),
                 int(Settings::EnumSidebarTitlePosition::Below));
    }

    void terminalPlacementPersistsAndCancelLeavesItUnchanged()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *placement = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TerminalPlacement"));
            QVERIFY(placement);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumTerminalPlacement::Automatic));
            placement->setCurrentIndex(Settings::EnumTerminalPlacement::Right);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            auto *placement = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TerminalPlacement"));
            QVERIFY(placement);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumTerminalPlacement::Right));
            placement->setCurrentIndex(Settings::EnumTerminalPlacement::Below);
            dialog.reject();
        }
        QCOMPARE(Settings::terminalPlacement(), int(Settings::EnumTerminalPlacement::Right));
    }

    void savesViewAndFileRules()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        QVERIFY(dialog.findChild<QTabWidget *>());
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTypeColumn"))->setChecked(false);
        auto *groupBy = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_GroupBy"));
        QVERIFY(groupBy);
        groupBy->setCurrentIndex(Settings::EnumGroupBy::ext);
        auto *sizeFormat = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_SizeFormat"));
        QVERIFY(sizeFormat);
        sizeFormat->setCurrentIndex(Settings::EnumSizeFormat::bytes);
        auto *dateFormat = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DateFormat"));
        QVERIFY(dateFormat);
        dateFormat->setCurrentIndex(Settings::EnumDateFormat::iso);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowSeconds"))->setChecked(true);
        dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_IconNameLines"))->setValue(4);
        dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_IconInformation"))->setInformation({QStringLiteral("size")});
        dialog.findChild<RuleListEditor *>(QStringLiteral("kcfg_IgnoreList"))->setRules({QStringLiteral("*.bak"), QStringLiteral("Thumbs.db")});
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PanelsVisible"))->setChecked(false);
        auto *panelPosition = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_PanelPosition"));
        QVERIFY(panelPosition);
        panelPosition->setCurrentIndex(Settings::EnumPanelPosition::right);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PanelAutoFocus"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PanelTitlesVisible"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FavoritesVisible"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PlacesVisible"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FoldersVisible"))->setChecked(false);
        auto *archiveFormat = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultArchiveFormat"));
        QVERIFY(archiveFormat);
        archiveFormat->setCurrentIndex(static_cast<int>(ArchiveWriter::Format::TarXz));
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TabIcons"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ExpandTabs"))->setChecked(false);

        auto *colors = dialog.findChild<RuleListEditor *>(QStringLiteral("kcfg_ExtensionColors"));
        QVERIFY(colors);
        colors->setRules({QStringLiteral("cpp=#123456")});

        QPushButton *ok = dialog.button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        ok->click();

        const ViewSettings settings = ViewSettings::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("View")));
        QVERIFY(!Settings::showTypeColumn());
        QCOMPARE(settings.groupBy, QStringLiteral("ext"));
        QCOMPARE(settings.sizeFormat, QStringLiteral("bytes"));
        QCOMPARE(settings.dateFormat, QStringLiteral("iso"));
        QVERIFY(settings.showSeconds);
        QCOMPARE(settings.iconNameLines, 4);
        QCOMPARE(settings.iconInformation, QStringList{QStringLiteral("size")});

        const KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        QCOMPARE(IgnoreList::fromConfig(files).entries(), QStringList({QStringLiteral("*.bak"), QStringLiteral("Thumbs.db")}));
        QCOMPARE(FileColorRules::fromConfig(files).colorForFileName(QStringLiteral("main.CPP")), QColor(QStringLiteral("#123456")));

        const KConfigGroup panels = KSharedConfig::openConfig()->group(QStringLiteral("PanelSettings"));
        QVERIFY(!panels.readEntry("Visible", true));
        QCOMPARE(panels.readEntry("Position", QString()), QStringLiteral("right"));
        QVERIFY(!panels.readEntry("AutoFocus", true));
        QVERIFY(!panels.readEntry("TitlesVisible", true));
        QVERIFY(!panels.readEntry("FavoritesVisible", true));
        QVERIFY(!panels.readEntry("PlacesVisible", true));
        QVERIFY(!panels.readEntry("FoldersVisible", true));
        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("Archives")).readEntry("DefaultFormat", -1), static_cast<int>(ArchiveWriter::Format::TarXz));
        const KConfigGroup tabs = KSharedConfig::openConfig()->group(QStringLiteral("Tabs"));
        QVERIFY(!tabs.readEntry("ShowIcons", true));
        QVERIFY(!tabs.readEntry("Expand", true));
    }

    void savesM02InterfaceAndViewSettings()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *startupMode = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StartupMode"));
        auto *homeUrl = dialog.findChild<KUrlRequester *>(QStringLiteral("kcfg_HomeUrl"));
        auto *tabsAfterCurrent = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NewTabsAfterCurrent"));
        auto *followLinks = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FollowSymlinks"));
        auto *dropAction = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DropAction"));
        auto *previewLimit = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_MaximumPreviewSize"));
        auto *defaultView = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultViewMode"));
        auto *showSelectionSize = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowSelectionSize"));
        auto *copyMoveMenus = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowCopyMoveToMenus"));
        QVERIFY(startupMode && homeUrl && tabsAfterCurrent && followLinks && dropAction && previewLimit);
        QVERIFY(defaultView && showSelectionSize && copyMoveMenus);

        startupMode->setCurrentIndex(Settings::EnumStartupMode::FixedFolders);
        homeUrl->setUrl(QUrl::fromLocalFile(home.path()));
        tabsAfterCurrent->setChecked(true);
        followLinks->setChecked(true);
        dropAction->setCurrentIndex(Settings::EnumDropAction::Copy);
        previewLimit->setValue(8);
        defaultView->setCurrentIndex(Settings::EnumDefaultViewMode::Icons);
        showSelectionSize->setChecked(false);
        copyMoveMenus->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmTrash"))->setChecked(true);
        dialog.button(QDialogButtonBox::Ok)->click();

        const auto config = KSharedConfig::openConfig();
        QCOMPARE(config->group(QStringLiteral("Startup")).readEntry("StartupMode", QString()), QStringLiteral("FixedFolders"));
        QCOMPARE(config->group(QStringLiteral("Startup")).readEntry("HomeUrl", QUrl()), QUrl::fromLocalFile(home.path()));
        QVERIFY(config->group(QStringLiteral("Panes")).readEntry("NewTabsAfterCurrent", false));
        QVERIFY(config->group(QStringLiteral("Navigation")).readEntry("FollowSymlinks", false));
        QCOMPARE(config->group(QStringLiteral("Confirmations")).readEntry("DropAction", QString()), QStringLiteral("Copy"));
        QCOMPARE(config->group(QStringLiteral("Views")).readEntry("DefaultViewMode", QString()), QStringLiteral("Icons"));
        QVERIFY(!config->group(QStringLiteral("StatusBar")).readEntry("ShowSelectionSize", true));
        QVERIFY(config->group(QStringLiteral("ContextMenu")).readEntry("ShowCopyMoveToMenus", false));

        const auto kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
        QCOMPARE(kioConfig->group(QStringLiteral("PreviewSettings")).readEntry("MaximumSize", qulonglong(0)), qulonglong(8 * 1024 * 1024));
        QVERIFY(kioConfig->group(QStringLiteral("Confirmations")).readEntry("ConfirmTrash", false));
    }

    void unrelatedChangesAndCancelPreserveKioPreviewLimit()
    {
        KConfigGroup previews = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals)->group(QStringLiteral("PreviewSettings"));
        previews.writeEntry("MaximumSize", qulonglong(5 * 1024 * 1024));
        previews.sync();

        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *limit = dialog.findChild<PreviewSizeSpinBox *>(QStringLiteral("kcfg_MaximumPreviewSize"));
        auto *remote = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowRemotePreviews"));
        QVERIFY(limit && remote);
        remote->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(previews.readEntry("MaximumSize", qulonglong(0)), qulonglong(5 * 1024 * 1024));

        limit->setValue(8);
        dialog.reject();
        QCOMPARE(previews.readEntry("MaximumSize", qulonglong(0)), qulonglong(5 * 1024 * 1024));
    }

    void cleanupTestCase()
    {
        const auto config = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
        config->deleteGroup(QStringLiteral("Confirmations"));
        config->deleteGroup(QStringLiteral("PreviewSettings"));
        config->sync();
    }
};

QTEST_MAIN(SettingsDialogTest)

#include "settingsdialogtest.moc"
