// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "batchrenamedialog.h"
#include "customcommanddialog.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "pathfilterpresetsdialog.h"
#include "savedpathfilters.h"
#include "searchstoredialog.h"
#include "settings.h"
#include "settingswidgets.h"
#include "viewsettings.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KPageWidget>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <KUrlRequester>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
                                     QStringLiteral("InformationPanel"),
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

    void settingsBackupActionsAreAvailable()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("createSettingsBackup")));
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("restoreSettingsBackup")));
    }

    void fileToolDefaultsPersistAndCancel()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *suffix = dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_SymlinkSuffix"));
            auto *manifest = dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_ChecksumFileName"));
            auto *size = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_SplitPartSizeMiB"));
            QVERIFY(suffix && manifest && size);
            QVERIFY(suffix->text().isEmpty());
            QCOMPARE(manifest->text(), QStringLiteral("checksums.sha256"));
            QCOMPARE(size->value(), 100);
            suffix->setText(QStringLiteral(".link"));
            manifest->setText(QStringLiteral("manifest.sha256"));
            size->setValue(12);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_SymlinkSuffix"))->text(), QStringLiteral(".link"));
            QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_ChecksumFileName"))->text(), QStringLiteral("manifest.sha256"));
            auto *size = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_SplitPartSizeMiB"));
            QCOMPARE(size->value(), 12);
            size->setValue(2);
            dialog.reject();
        }
        QCOMPARE(Settings::splitPartSizeMiB(), 12);
    }

    void tabBarPreferencesPersistAndCancel()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *first = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_HorizontalFirstTabPosition"));
            auto *second = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StackedSecondTabPosition"));
            auto *together = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_StackedTabBarsTogether"));
            auto *close = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTabCloseButtons"));
            QVERIFY(first && second && together && close);
            first->setCurrentIndex(2);
            second->setCurrentIndex(3);
            together->setChecked(false);
            close->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::horizontalFirstTabPosition(), 2);
        QCOMPARE(Settings::stackedSecondTabPosition(), 3);
        QVERIFY(!Settings::stackedTabBarsTogether());
        QVERIFY(!Settings::showTabCloseButtons());
        {
            SettingsDialog dialog(&places, nullptr);
            auto *first = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_HorizontalFirstTabPosition"));
            QCOMPARE(first->currentIndex(), 2);
            first->setCurrentIndex(0);
            dialog.reject();
        }
        QCOMPARE(Settings::horizontalFirstTabPosition(), 2);
    }

    void metadataFieldDefaultsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *audio = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_MetadataAudioArtist"));
            auto *image = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_MetadataImageArtist"));
            QVERIFY(audio && image);
            QVERIFY(audio->isChecked() && image->isChecked());
            image->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        SettingsDialog dialog(&places, nullptr);
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_MetadataAudioArtist"))->isChecked());
        QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_MetadataImageArtist"))->isChecked());
        dialog.reject();
    }

    void transferQueueDefaultsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TransferQueueEnabled"));
            auto *limit = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_TransferQueueConcurrency"));
            auto *conflict = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TransferConflictPolicy"));
            QVERIFY(enabled && limit && conflict);
            QVERIFY(enabled->isChecked());
            QCOMPARE(limit->value(), 1);
            QCOMPARE(conflict->currentIndex(), int(Settings::EnumTransferConflictPolicy::Ask));
            limit->setValue(3);
            conflict->setCurrentIndex(Settings::EnumTransferConflictPolicy::Skip);
            enabled->setChecked(false);
            QVERIFY(!limit->isEnabled());
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        SettingsDialog dialog(&places, nullptr);
        QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TransferQueueEnabled"))->isChecked());
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_TransferQueueConcurrency"))->value(), 3);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TransferConflictPolicy"))->currentIndex(), int(Settings::EnumTransferConflictPolicy::Skip));
        dialog.reject();
    }

    void listingExportDefaultsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *format = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_ListingExportFormat"));
            auto *custom = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ListingExportCustomColumns"));
            auto *owner = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ListingExportOwner"));
            QVERIFY(format && custom && owner);
            QCOMPARE(format->currentIndex(), int(Settings::EnumListingExportFormat::Csv));
            QVERIFY(!custom->isChecked());
            QVERIFY(!owner->isEnabled());
            custom->setChecked(true);
            QVERIFY(owner->isEnabled());
            owner->setChecked(true);
            format->setCurrentIndex(Settings::EnumListingExportFormat::Pdf);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        SettingsDialog dialog(&places, nullptr);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("kcfg_ListingExportFormat"))->currentIndex(), int(Settings::EnumListingExportFormat::Pdf));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ListingExportOwner"))->isChecked());
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ListingExportOwner"))->isEnabled());
        dialog.reject();
    }

    void opensCommandManager()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *manage = dialog.findChild<QPushButton *>(QStringLiteral("manageCommands"));
        QVERIFY(manage);
        QTimer::singleShot(0, [&dialog] {
            auto *manager = dialog.findChild<CustomCommandDialog *>(QStringLiteral("customCommandDialog"));
            QVERIFY(manager);
            QVERIFY(manager->findChild<QPushButton *>(QStringLiteral("commandRun"))->isHidden());
            manager->reject();
        });
        manage->click();
    }

    void opensBatchRenamePresetManager()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *manage = dialog.findChild<QPushButton *>(QStringLiteral("manageBatchRenamePresets"));
        QVERIFY(manage);
        QTimer::singleShot(0, [&dialog] {
            auto *manager = dialog.findChild<BatchRenameDialog *>(QStringLiteral("batchRenameDialog"));
            QVERIFY(manager);
            QVERIFY(manager->findChild<QPushButton *>(QStringLiteral("batchRenameApply"))->isHidden());
            manager->reject();
        });
        manage->click();
    }

    void folderSyncDefaultsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *recursive = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FolderSyncRecursive"));
            auto *contents = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FolderSyncCompareContents"));
            auto *links = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FolderSyncCompareLinks"));
            auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_FolderSyncCopyPolicy"));
            auto *speed = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_FolderSyncSpeedLimitKiB"));
            QVERIFY(recursive && contents && links && policy && speed);
            QVERIFY(recursive->isChecked() && !contents->isChecked() && !links->isChecked());
            QCOMPARE(policy->currentIndex(), int(Settings::EnumFolderSyncCopyPolicy::All));
            QCOMPARE(speed->value(), 0);
            recursive->setChecked(false);
            contents->setChecked(true);
            links->setChecked(true);
            policy->setCurrentIndex(Settings::EnumFolderSyncCopyPolicy::NewerOnly);
            speed->setValue(128);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        SettingsDialog dialog(&places, nullptr);
        QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FolderSyncRecursive"))->isChecked());
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FolderSyncCompareContents"))->isChecked());
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FolderSyncCompareLinks"))->isChecked());
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("kcfg_FolderSyncCopyPolicy"))->currentIndex(),
                 int(Settings::EnumFolderSyncCopyPolicy::NewerOnly));
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_FolderSyncSpeedLimitKiB"))->value(), 128);
        dialog.reject();
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
            auto *follow = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TerminalFollowActiveFolder"));
            QVERIFY(placement);
            QVERIFY(follow);
            QVERIFY(!follow->isChecked());
            follow->setChecked(true);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumTerminalPlacement::Automatic));
            placement->setCurrentIndex(Settings::EnumTerminalPlacement::Right);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            auto *placement = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TerminalPlacement"));
            auto *follow = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TerminalFollowActiveFolder"));
            QVERIFY(placement);
            QVERIFY(follow && follow->isChecked());
            follow->setChecked(false);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumTerminalPlacement::Right));
            placement->setCurrentIndex(Settings::EnumTerminalPlacement::Below);
            dialog.reject();
        }
        QCOMPARE(Settings::terminalPlacement(), int(Settings::EnumTerminalPlacement::Right));
        QVERIFY(Settings::terminalFollowActiveFolder());
        Settings::setTerminalFollowActiveFolder(false);
    }

    void searchDefaultsPersistAndCancelLeavesThemUnchanged()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *indexed = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_SearchIndexedByDefault"));
            auto *contents = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_SearchContentsByDefault"));
            QVERIFY(indexed && contents);
            QVERIFY(!indexed->isChecked());
            QVERIFY(!contents->isChecked());
            indexed->setChecked(true);
            contents->setChecked(true);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            auto *indexed = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_SearchIndexedByDefault"));
            auto *contents = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_SearchContentsByDefault"));
            QVERIFY(indexed && contents);
            QVERIFY(indexed->isChecked());
            QVERIFY(contents->isChecked());
            indexed->setChecked(false);
            contents->setChecked(false);
            dialog.reject();
        }
        QVERIFY(Settings::searchIndexedByDefault());
        QVERIFY(Settings::searchContentsByDefault());
    }

    void duplicateFinderDefaultsPersistAndCancelLeavesThemUnchanged()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *name = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DuplicateRequireSameFileName"));
            auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DuplicateSelectionPolicy"));
            QVERIFY(name && policy);
            QVERIFY(!name->isChecked());
            QCOMPARE(policy->currentIndex(), int(Settings::EnumDuplicateSelectionPolicy::KeepNewest));
            name->setChecked(true);
            policy->setCurrentIndex(Settings::EnumDuplicateSelectionPolicy::InFolder);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            auto *name = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DuplicateRequireSameFileName"));
            auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DuplicateSelectionPolicy"));
            QVERIFY(name && policy);
            QVERIFY(name->isChecked());
            QCOMPARE(policy->currentIndex(), int(Settings::EnumDuplicateSelectionPolicy::InFolder));
            name->setChecked(false);
            policy->setCurrentIndex(Settings::EnumDuplicateSelectionPolicy::KeepOldest);
            dialog.reject();
        }
        QVERIFY(Settings::duplicateRequireSameFileName());
        QCOMPARE(Settings::duplicateSelectionPolicy(), int(Settings::EnumDuplicateSelectionPolicy::InFolder));
    }

    void contentChartDefaultsPersistAndCancelLeavesThemUnchanged()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *show = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ContentChartShowTreemap"));
            auto *sort = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_ContentChartSort"));
            auto *descending = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ContentChartSortDescending"));
            QVERIFY(show && sort && descending);
            QVERIFY(show->isChecked());
            QCOMPARE(sort->currentIndex(), int(Settings::EnumContentChartSort::AsListed));
            show->setChecked(false);
            sort->setCurrentIndex(Settings::EnumContentChartSort::Size);
            descending->setChecked(true);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            auto *show = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ContentChartShowTreemap"));
            auto *sort = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_ContentChartSort"));
            auto *descending = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ContentChartSortDescending"));
            QVERIFY(show && sort && descending);
            QVERIFY(!show->isChecked());
            QCOMPARE(sort->currentIndex(), int(Settings::EnumContentChartSort::Size));
            QVERIFY(descending->isChecked());
            show->setChecked(true);
            dialog.reject();
        }
        QVERIFY(!Settings::contentChartShowTreemap());
    }

    void linkedPanesStartupPreferencePersists()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *link = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_LinkPanesAtStartup"));
        QVERIFY(link);
        QVERIFY(!link->isChecked());
        link->setChecked(true);
        dialog.button(QDialogButtonBox::Ok)->click();
        QVERIFY(Settings::linkPanesAtStartup());
    }

    void fileDiffWrapPreferencePersists()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *wrap = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FileDiffWrapLines"));
        QVERIFY(wrap);
        QVERIFY(!wrap->isChecked());
        wrap->setChecked(true);
        dialog.button(QDialogButtonBox::Ok)->click();
        QVERIFY(Settings::fileDiffWrapLines());
    }

    void paneComparisonCategoriesPersist()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *left = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PaneComparisonShowLeftOnly"));
        auto *matching = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PaneComparisonShowMatching"));
        QVERIFY(left && matching);
        QVERIFY(left->isChecked() && matching->isChecked());
        left->setChecked(false);
        matching->setChecked(false);
        dialog.button(QDialogButtonBox::Ok)->click();
        QVERIFY(!Settings::paneComparisonShowLeftOnly());
        QVERIFY(!Settings::paneComparisonShowMatching());
        QVERIFY(Settings::paneComparisonShowRightOnly());
        QVERIFY(Settings::paneComparisonShowDifferent());
    }

    void managesSharedPathFilters()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("filtersrc")), KConfig::SimpleConfig);
        PathFilterPresetsDialog dialog(nullptr, config);
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("pathFilterPresetName"));
        auto *path = dialog.findChild<QLineEdit *>(QStringLiteral("presetPathFilter"));
        auto *excluded = dialog.findChild<QPlainTextEdit *>(QStringLiteral("presetExcludedFolders"));
        auto *depth = dialog.findChild<QSpinBox *>(QStringLiteral("presetMaxDepth"));
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("pathFilterPresetSave"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("pathFilterPresetStatus"));
        QVERIFY(name && path && excluded && depth && save && status);
        name->setText(QStringLiteral("Work"));
        path->setText(QStringLiteral("report"));
        excluded->setPlainText(QStringLiteral("relative"));
        save->click();
        QVERIFY(status->text().contains(QStringLiteral("absolute")));
        excluded->setPlainText(root.filePath(QStringLiteral("private")));
        depth->setValue(2);
        save->click();
        QString error;
        auto presets = SavedPathFilters::load(config->group(QStringLiteral("SavedPathFilters")), &error);
        QCOMPARE(presets.size(), 1);
        QCOMPARE(presets.first().filter.maxDepth, 2);
        QCOMPARE(presets.first().filter.excludedFolders.first(), root.filePath(QStringLiteral("private")));
        path->setText(QStringLiteral("unsaved"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("pathFilterPresetNew"))->click();
        QCOMPARE(path->text(), QStringLiteral("unsaved"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("pathFilterPresetNew"))->click();
        name->setText(QStringLiteral(" work "));
        save->click();
        QVERIFY(status->text().contains(QStringLiteral("unique")));
        QCOMPARE(SavedPathFilters::load(config->group(QStringLiteral("SavedPathFilters")), &error).size(), 1);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QComboBox *>(QStringLiteral("pathFilterPresetList"))->setCurrentIndex(0);
        name->setText(QStringLiteral("Renamed"));
        save->click();
        presets = SavedPathFilters::load(config->group(QStringLiteral("SavedPathFilters")), &error);
        QCOMPARE(presets.size(), 1);
        QCOMPARE(presets.first().name, QStringLiteral("Renamed"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("pathFilterPresetDelete"))->click();
        QVERIFY(SavedPathFilters::load(config->group(QStringLiteral("SavedPathFilters")), &error).isEmpty());
    }

    void pathFilterManagerRejectsStaleEdits()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("filtersrc")), KConfig::SimpleConfig);
        QString error;
        QVERIFY(SavedPathFilters::save(config->group(QStringLiteral("SavedPathFilters")), {{QStringLiteral("Work"), {}}}, &error));
        PathFilterPresetsDialog first(nullptr, config);
        PathFilterPresetsDialog second(nullptr, config);
        first.findChild<QLineEdit *>(QStringLiteral("presetPathFilter"))->setText(QStringLiteral("first"));
        first.findChild<QPushButton *>(QStringLiteral("pathFilterPresetSave"))->click();
        second.findChild<QLineEdit *>(QStringLiteral("presetPathFilter"))->setText(QStringLiteral("second"));
        second.findChild<QPushButton *>(QStringLiteral("pathFilterPresetSave"))->click();
        QVERIFY(second.findChild<QLabel *>(QStringLiteral("pathFilterPresetStatus"))->text().contains(QStringLiteral("changed elsewhere")));
        QCOMPARE(SavedPathFilters::load(config->group(QStringLiteral("SavedPathFilters")), &error).first().filter.pathText, QStringLiteral("first"));
    }

    void opensPathFilterManagerFromPreferences()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        QTimer::singleShot(0, [&dialog] {
            auto *manager = dialog.findChild<QDialog *>(QStringLiteral("pathFilterPresetsDialog"));
            QVERIFY(manager);
            manager->reject();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("managePathFilters"))->click();
    }

    void managesSavedSearchesAndCollections()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        const auto searchGroup = config->group(QStringLiteral("SavedSearches"));
        const auto collectionGroup = config->group(QStringLiteral("SearchCollection"));
        const QUrl url = QUrl::fromLocalFile(file.fileName());
        QString error;
        QVERIFY(SavedSearches::save(searchGroup,
                                    {{QStringLiteral("One"), QStringLiteral("term"), QUrl::fromLocalFile(root.path())},
                                     {QStringLiteral("Two"), QStringLiteral("other"), QUrl::fromLocalFile(root.path())}},
                                    &error));
        QVERIFY(SearchCollection::save(collectionGroup, {url}, &error));
        QVERIFY(SearchCollection::saveSaved(collectionGroup, {{QStringLiteral("Collection"), {url}}}, &error));
        SearchStoreDialog dialog(nullptr, config);
        auto *searches = dialog.findChild<QListWidget *>(QStringLiteral("managedSearches"));
        auto *collections = dialog.findChild<QListWidget *>(QStringLiteral("managedCollections"));
        auto *references = dialog.findChild<QListWidget *>(QStringLiteral("managedCollectionReferences"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("searchStoreStatus"));
        QVERIFY(searches && collections && references && status);
        QCOMPARE(searches->count(), 2);
        QCOMPARE(references->count(), 1);
        const auto rename = [&dialog](const QString &button, const QString &name) {
            QTimer::singleShot(0, [name] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(name);
                input->accept();
            });
            dialog.findChild<QPushButton *>(button)->click();
        };
        rename(QStringLiteral("managedSearchRename"), QStringLiteral("Two"));
        QVERIFY(status->text().contains(QStringLiteral("unique")));
        rename(QStringLiteral("managedSearchRename"), QStringLiteral("Renamed"));
        QCOMPARE(SavedSearches::load(searchGroup, &error).first().name, QStringLiteral("Renamed"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("managedSearchDelete"))->click();
        QCOMPARE(SavedSearches::load(searchGroup, &error).size(), 2);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("managedSearchDelete"))->click();
        QCOMPARE(SavedSearches::load(searchGroup, &error).size(), 1);
        rename(QStringLiteral("managedCollectionRename"), QStringLiteral("Archive"));
        QCOMPARE(SearchCollection::loadSaved(collectionGroup, &error).first().name, QStringLiteral("Archive"));
        references->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("managedCollectionRemove"))->click();
        QVERIFY(SearchCollection::loadSaved(collectionGroup, &error).first().urls.isEmpty());
        QCOMPARE(SearchCollection::load(collectionGroup, &error), (QList<QUrl>{url}));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("managedCollectionDelete"))->click();
        QVERIFY(SearchCollection::loadSaved(collectionGroup, &error).isEmpty());
        QVERIFY(QFile::exists(file.fileName()));
    }

    void opensSavedSearchManagerFromPreferences()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        QTimer::singleShot(0, [&dialog] {
            auto *manager = dialog.findChild<QDialog *>(QStringLiteral("searchStoreDialog"));
            QVERIFY(manager);
            manager->reject();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("manageSavedSearches"))->click();
    }

    void savedSearchManagerRejectsStaleDeletion()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        const auto group = config->group(QStringLiteral("SavedSearches"));
        QString error;
        QVERIFY(SavedSearches::save(group, {{QStringLiteral("Work"), QStringLiteral("old"), QUrl::fromLocalFile(root.path())}}, &error));
        SearchStoreDialog dialog(nullptr, config);
        QVERIFY(SavedSearches::save(group, {{QStringLiteral("Work"), QStringLiteral("new"), QUrl::fromLocalFile(root.path())}}, &error));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("managedSearchDelete"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStoreStatus"))->text().contains(QStringLiteral("changed elsewhere")));
        QCOMPARE(SavedSearches::load(group, &error).first().text, QStringLiteral("new"));
    }

    void fileAssociationsUseKdeAndReportFailures()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *open = dialog.findChild<QPushButton *>(QStringLiteral("openFileAssociations"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileAssociationsStatus"));
        QVERIFY(open && status);
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto paths = QCoreApplication::libraryPaths();
        QCoreApplication::setLibraryPaths({root.path()});
        open->click();
        const QString missing = status->text();
        QCoreApplication::setLibraryPaths(paths);
        QVERIFY(missing.contains(QStringLiteral("not installed")));
        const auto plugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_filetypes"));
        const auto widgetPlugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings_qwidgets"), QStringLiteral("kcm_filetypes"));
        if (!plugin.isValid() && !widgetPlugin.isValid()) {
            QSKIP("KDE File Associations is not installed.");
        }
        QFile executable(root.filePath(QStringLiteral("kcmshell6")));
        QVERIFY(executable.open(QIODevice::WriteOnly));
        QVERIFY(executable.write("#!/bin/sh\nprintf '%s' \"$1\" > \"$OPUSFM_KCM_TEST_OUTPUT\"\n") > 0);
        executable.close();
        QVERIFY(executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray oldPath = qgetenv("PATH");
        const QByteArray oldOutput = qgetenv("OPUSFM_KCM_TEST_OUTPUT");
        const bool pathSet = qEnvironmentVariableIsSet("PATH");
        const bool outputSet = qEnvironmentVariableIsSet("OPUSFM_KCM_TEST_OUTPUT");
        const auto restore = qScopeGuard([=] {
            if (pathSet) {
                qputenv("PATH", oldPath);
            } else {
                qunsetenv("PATH");
            }
            if (outputSet) {
                qputenv("OPUSFM_KCM_TEST_OUTPUT", oldOutput);
            } else {
                qunsetenv("OPUSFM_KCM_TEST_OUTPUT");
            }
        });
        const QString emptyPath = root.filePath(QStringLiteral("empty"));
        QVERIFY(QDir().mkdir(emptyPath));
        qputenv("PATH", QFile::encodeName(emptyPath));
        open->click();
        QVERIFY(status->text().contains(QStringLiteral("Could not launch")));
        qputenv("PATH", QFile::encodeName(root.path()));
        qputenv("OPUSFM_KCM_TEST_OUTPUT", QFile::encodeName(root.filePath(QStringLiteral("argument"))));
        open->click();
        QVERIFY(status->text().contains(QStringLiteral("shared desktop defaults")));
        const auto readArgument = [&root] {
            QFile argument(root.filePath(QStringLiteral("argument")));
            return argument.open(QIODevice::ReadOnly) ? argument.readAll() : QByteArray();
        };
        QTRY_COMPARE(readArgument(), QByteArray("kcm_filetypes"));
    }

    void fileSearchSettingsReportsMissingModule()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *open = dialog.findChild<QPushButton *>(QStringLiteral("openFileSearchSettings"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileSearchSettingsStatus"));
        QVERIFY(open && status);
        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        const auto paths = QCoreApplication::libraryPaths();
        QCoreApplication::setLibraryPaths({empty.path()});
        open->click();
        const QString message = status->text();
        QCoreApplication::setLibraryPaths(paths);
        QVERIFY(message.contains(QStringLiteral("not installed")));
    }

    void fileSearchSettingsLaunchesTheKdeModule()
    {
        const auto plugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_baloofile"));
        const auto widgetPlugin = KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings_qwidgets"), QStringLiteral("kcm_baloofile"));
        if (!plugin.isValid() && !widgetPlugin.isValid()) {
            QSKIP("KDE File Search settings are not installed.");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString launcher = root.filePath(QStringLiteral("kcmshell6"));
        QFile executable(launcher);
        QVERIFY(executable.open(QIODevice::WriteOnly));
        QVERIFY(executable.write("#!/bin/sh\nprintf '%s' \"$1\" > \"$OPUSFM_KCM_TEST_OUTPUT\"\n") > 0);
        executable.close();
        QVERIFY(executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *open = dialog.findChild<QPushButton *>(QStringLiteral("openFileSearchSettings"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileSearchSettingsStatus"));
        QVERIFY(open && status);
        const QByteArray oldPath = qgetenv("PATH");
        const QByteArray oldOutput = qgetenv("OPUSFM_KCM_TEST_OUTPUT");
        const bool outputSet = qEnvironmentVariableIsSet("OPUSFM_KCM_TEST_OUTPUT");
        const bool pathSet = qEnvironmentVariableIsSet("PATH");
        const QString missingPath = root.filePath(QStringLiteral("empty"));
        QVERIFY(QDir().mkdir(missingPath));
        qputenv("PATH", QFile::encodeName(missingPath));
        open->click();
        const QString missingMessage = status->text();
        qputenv("PATH", QFile::encodeName(root.path()));
        qputenv("OPUSFM_KCM_TEST_OUTPUT", QFile::encodeName(root.filePath(QStringLiteral("argument"))));
        open->click();
        const QString message = status->text();
        if (pathSet) {
            qputenv("PATH", oldPath);
        } else {
            qunsetenv("PATH");
        }
        if (outputSet) {
            qputenv("OPUSFM_KCM_TEST_OUTPUT", oldOutput);
        } else {
            qunsetenv("OPUSFM_KCM_TEST_OUTPUT");
        }
        QVERIFY(missingMessage.contains(QStringLiteral("Could not launch")));
        QVERIFY(message.contains(QStringLiteral("Requested")));
        const auto readArgument = [&root] {
            QFile argument(root.filePath(QStringLiteral("argument")));
            return argument.open(QIODevice::ReadOnly) ? argument.readAll() : QByteArray();
        };
        QTRY_COMPARE(readArgument(), QByteArray("kcm_baloofile"));
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

    void savesInformationPanelFieldOrderAndHoverPreference()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *fields = dialog.findChild<IconInformationList *>(QStringLiteral("kcfg_InformationFields"));
        auto *hover = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_InformationHover"));
        QVERIFY(fields && hover);
        const QStringList selected{QStringLiteral("localPathOrUrl"), QStringLiteral("comment"), QStringLiteral("size")};
        fields->setInformation(selected);
        hover->setChecked(true);
        dialog.button(QDialogButtonBox::Ok)->click();

        QCOMPARE(Settings::informationFields(), selected);
        QVERIFY(Settings::informationHover());
        const KConfigGroup config = KSharedConfig::openConfig()->group(QStringLiteral("InformationPanel"));
        QCOMPARE(config.readEntry("InformationFields", QStringList()), selected);
        QVERIFY(config.readEntry("InformationHover", false));
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
        auto *showMinimap = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowFileListMinimap"));
        auto *showSelectionSize = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowSelectionSize"));
        auto *copyMoveMenus = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowCopyMoveToMenus"));
        QVERIFY(startupMode && homeUrl && tabsAfterCurrent && followLinks && dropAction && previewLimit);
        QVERIFY(defaultView && showMinimap && showSelectionSize && copyMoveMenus);

        startupMode->setCurrentIndex(Settings::EnumStartupMode::FixedFolders);
        homeUrl->setUrl(QUrl::fromLocalFile(home.path()));
        tabsAfterCurrent->setChecked(true);
        followLinks->setChecked(true);
        dropAction->setCurrentIndex(Settings::EnumDropAction::Copy);
        previewLimit->setValue(8);
        defaultView->setCurrentIndex(Settings::EnumDefaultViewMode::Icons);
        showMinimap->setChecked(true);
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
        QVERIFY(config->group(QStringLiteral("Views")).readEntry("ShowFileListMinimap", false));
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
