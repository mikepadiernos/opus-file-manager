// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "batchrenamedialog.h"
#include "customcommanddialog.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "mainwindow.h"
#include "pathfilterpresetsdialog.h"
#include "savedpathfilters.h"
#include "searchstoredialog.h"
#include "settings.h"
#include "settingswidgets.h"
#include "viewsettings.h"
#include "workspacecatalog.h"
#include "workspacerestore.h"

#include <KActionCollection>
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
#include <QDockWidget>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
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
                                     QStringLiteral("Search"),
                                     QStringLiteral("Workspaces"),
                                     QStringLiteral("WorkspaceSettings"),
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
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("exportWorkspaceProfiles")));
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("importWorkspaceProfiles")));
    }

    void workspaceManagementSummarizesCatalogAndOpensManager()
    {
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Research"), QStringLiteral("RS"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Media"), QStringLiteral("MD"), error), qPrintable(error));
        const QString research = catalog.workspaces.first().id;
        QVERIFY2(WorkspaceCatalog::assignLocation(catalog, WorkspaceCatalog::locationId(QUrl::fromLocalFile(QStringLiteral("/srv/a"))), research, error),
                 qPrintable(error));
        QVERIFY2(WorkspaceCatalog::activate(catalog, research, error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(KSharedConfig::openConfig()->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        KSharedConfig::openConfig()->sync();

        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *summary = dialog.findChild<QListWidget *>(QStringLiteral("workspaceSummary"));
        auto *manage = dialog.findChild<QPushButton *>(QStringLiteral("manageWorkspaces"));
        QVERIFY(summary && manage);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("workspaceSecurityNotice")));
        QCOMPARE(summary->count(), 2);
        QVERIFY(summary->item(0)->text().contains(QStringLiteral("Research (RS)")));
        QVERIFY(summary->item(0)->text().contains(QStringLiteral("1 assigned location")));
        QVERIFY(summary->item(0)->text().contains(QStringLiteral("active")));
        QVERIFY(!summary->item(1)->text().contains(QStringLiteral("active")));
        QSignalSpy requested(&dialog, &SettingsDialog::manageWorkspacesRequested);
        manage->click();
        QCOMPARE(requested.count(), 1);

        QVERIFY2(WorkspaceCatalog::remove(catalog, catalog.workspaces.last().id, error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(KSharedConfig::openConfig()->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        KSharedConfig::openConfig()->sync();
        dialog.reloadWorkspaces();
        QCOMPARE(summary->count(), 1);
    }

    void startupWorkspacePersistsAndKeepsUnavailableReference()
    {
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Research"), QStringLiteral("RS"), error), qPrintable(error));
        const QString id = catalog.workspaces.first().id;
        QVERIFY2(WorkspaceCatalog::save(KSharedConfig::openConfig()->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        KSharedConfig::openConfig()->sync();

        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *startup = dialog.findChild<StartupWorkspaceCombo *>(QStringLiteral("kcfg_StartupWorkspace"));
            QVERIFY(startup);
            QCOMPARE(startup->count(), 3);
            QCOMPARE(startup->workspace(), QString());
            startup->setCurrentIndex(startup->findData(id));
            QCOMPARE(startup->workspace(), id);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::startupWorkspace(), id);

        QVERIFY2(WorkspaceCatalog::remove(catalog, id, error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(KSharedConfig::openConfig()->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        KSharedConfig::openConfig()->sync();
        {
            SettingsDialog dialog(&places, nullptr);
            auto *startup = dialog.findChild<StartupWorkspaceCombo *>(QStringLiteral("kcfg_StartupWorkspace"));
            QVERIFY(startup);
            QCOMPARE(startup->workspace(), id);
            QVERIFY(startup->currentText().contains(QStringLiteral("Unavailable")));
            startup->setCurrentIndex(startup->findData(QStringLiteral("none")));
            dialog.reject();
        }
        QCOMPARE(Settings::startupWorkspace(), id);
    }

    void toolbarSettingsOpenExistingEditors()
    {
        MainWindow window;
        auto *places = window.findChild<KFilePlacesModel *>();
        QVERIFY(places);
        SettingsDialog dialog(places, &window);
        auto *mainToolbar = dialog.findChild<QPushButton *>(QStringLiteral("configureMainToolbarEntries"));
        auto *resetToolbars = dialog.findChild<QPushButton *>(QStringLiteral("resetToolbarCustomization"));
        auto *firstPaneToolbar = dialog.findChild<QPushButton *>(QStringLiteral("configureFirstPaneToolbarEntries"));
        auto *secondPaneToolbar = dialog.findChild<QPushButton *>(QStringLiteral("configureSecondPaneToolbarEntries"));
        QVERIFY(mainToolbar && resetToolbars && firstPaneToolbar && secondPaneToolbar);
        QVERIFY(mainToolbar->isEnabled() && resetToolbars->isEnabled() && firstPaneToolbar->isEnabled() && secondPaneToolbar->isEnabled());

        window.show();
        dialog.show();
        QTest::mouseClick(mainToolbar, Qt::LeftButton);
        QTRY_VERIFY(window.actionCollection()->action(QStringLiteral("customize_main_toolbar"))->isChecked());
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("toolbarActionsDock"));
        QVERIFY(dock);
        QTRY_VERIFY(dock->isVisible());
        window.actionCollection()->action(QStringLiteral("customize_main_toolbar"))->trigger();
    }

    void combinedPanelSettingsOpenExistingCustomizer()
    {
        MainWindow window;
        auto *places = window.findChild<KFilePlacesModel *>();
        QVERIFY(places);
        SettingsDialog dialog(places, &window);
        auto *configure = dialog.findChild<QPushButton *>(QStringLiteral("configureCombinedPanelSections"));
        QVERIFY(configure && configure->isEnabled());

        window.show();
        dialog.show();
        bool customizerOpened = false;
        QTimer closeCustomizer(&window);
        closeCustomizer.setInterval(10);
        connect(&closeCustomizer, &QTimer::timeout, &window, [&] {
            if (auto *customizer = window.findChild<QDialog *>(QStringLiteral("combinedPanelSectionsDialog"))) {
                customizerOpened = true;
                closeCustomizer.stop();
                customizer->reject();
            }
        });
        closeCustomizer.start();
        QTest::mouseClick(configure, Qt::LeftButton);
        QTRY_VERIFY(customizerOpened);
        QVERIFY(!dialog.isVisible());
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

    void interfaceLanguagePersistsAndCancels()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *language = dialog.findChild<InterfaceLanguageCombo *>(QStringLiteral("kcfg_InterfaceLanguage"));
            auto *locale = dialog.findChild<RegionalLocaleCombo *>(QStringLiteral("kcfg_RegionalLocale"));
            auto *direction = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TranslationDirection"));
            auto *calendar = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_CalendarOverride"));
            auto *date = dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_CustomDateFormat"));
            auto *time = dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_CustomTimeFormat"));
            QVERIFY(language && locale && direction && calendar && date && time);
            QCOMPARE(language->language(), QString());
            QCOMPARE(locale->localeName(), QString());
            language->setLanguage(QStringLiteral("en_US"));
            locale->setLocaleName(QStringLiteral("fa_IR"));
            direction->setCurrentIndex(Settings::EnumTranslationDirection::LeftToRight);
            calendar->setCurrentIndex(Settings::EnumCalendarOverride::JalaliCalendar);
            date->setText(QStringLiteral("yyyy/MM/dd"));
            time->setText(QStringLiteral("HH:mm"));
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::interfaceLanguage(), QStringLiteral("en_US"));
        QCOMPARE(Settings::regionalLocale(), QStringLiteral("fa_IR"));
        QCOMPARE(Settings::translationDirection(), Settings::EnumTranslationDirection::LeftToRight);
        QCOMPARE(Settings::calendarOverride(), Settings::EnumCalendarOverride::JalaliCalendar);
        QCOMPARE(Settings::customDateFormat(), QStringLiteral("yyyy/MM/dd"));
        QCOMPARE(Settings::customTimeFormat(), QStringLiteral("HH:mm"));
        {
            SettingsDialog dialog(&places, nullptr);
            auto *language = dialog.findChild<InterfaceLanguageCombo *>(QStringLiteral("kcfg_InterfaceLanguage"));
            QVERIFY(language);
            QCOMPARE(language->language(), QStringLiteral("en_US"));
            language->setLanguage(QString());
            dialog.findChild<RegionalLocaleCombo *>(QStringLiteral("kcfg_RegionalLocale"))->setLocaleName(QString());
            dialog.reject();
        }
        QCOMPARE(Settings::interfaceLanguage(), QStringLiteral("en_US"));
        QCOMPARE(Settings::regionalLocale(), QStringLiteral("fa_IR"));
    }

    void customNumeralsRequireTenDistinctSymbols()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *style = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_NumeralStyle"));
        auto *digits = dialog.findChild<QLineEdit *>(QStringLiteral("kcfg_CustomNumerals"));
        QVERIFY(style && digits);
        style->setCurrentIndex(Settings::EnumNumeralStyle::CustomDigits);
        digits->setText(QStringLiteral("123"));
        QVERIFY(!dialog.button(QDialogButtonBox::Ok)->isEnabled());
        QVERIFY(!dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_FollowSymlinks"))->setChecked(true);
        QVERIFY(!dialog.button(QDialogButtonBox::Ok)->isEnabled());
        QVERIFY(!dialog.button(QDialogButtonBox::Apply)->isEnabled());
        digits->setText(QStringLiteral("abcdefghij"));
        QVERIFY(dialog.button(QDialogButtonBox::Ok)->isEnabled());
        QVERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Ok)->click();
        QCOMPARE(Settings::numeralStyle(), Settings::EnumNumeralStyle::CustomDigits);
        QCOMPARE(Settings::customNumerals(), QStringLiteral("abcdefghij"));
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

    void statusBarIndicatorOrderPersistsAndCancels()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *order = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StatusBarIndicatorOrder"));
            QVERIFY(order);
            QCOMPARE(order->count(), 2);
            order->setCurrentIndex(Settings::EnumStatusBarIndicatorOrder::FreeSpaceFirst);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::statusBarIndicatorOrder(), Settings::EnumStatusBarIndicatorOrder::FreeSpaceFirst);
        {
            SettingsDialog dialog(&places, nullptr);
            auto *order = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_StatusBarIndicatorOrder"));
            QVERIFY(order);
            QCOMPARE(order->currentIndex(), Settings::EnumStatusBarIndicatorOrder::FreeSpaceFirst);
            order->setCurrentIndex(Settings::EnumStatusBarIndicatorOrder::ItemsFirst);
            dialog.reject();
        }
        QCOMPARE(Settings::statusBarIndicatorOrder(), Settings::EnumStatusBarIndicatorOrder::FreeSpaceFirst);
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
            QCOMPARE(size->value(), 16);
            QCOMPARE(position->currentIndex(), int(Settings::EnumSidebarTitlePosition::Beside));
            QCOMPARE(pages->faceType(), KPageView::FlatList);
            QTRY_VERIFY(sidebar());
            QCOMPARE(sidebar()->iconSize(), QSize(16, 16));

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
            QVERIFY(follow->isChecked());
            follow->setChecked(false);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumTerminalPlacement::Automatic));
            placement->setCurrentIndex(Settings::EnumTerminalPlacement::Right);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        {
            SettingsDialog dialog(&places, nullptr);
            auto *placement = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_TerminalPlacement"));
            auto *follow = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_TerminalFollowActiveFolder"));
            QVERIFY(placement);
            QVERIFY(follow && !follow->isChecked());
            follow->setChecked(true);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumTerminalPlacement::Right));
            placement->setCurrentIndex(Settings::EnumTerminalPlacement::Below);
            dialog.reject();
        }
        QCOMPARE(Settings::terminalPlacement(), int(Settings::EnumTerminalPlacement::Right));
        QVERIFY(!Settings::terminalFollowActiveFolder());
        Settings::setTerminalFollowActiveFolder(true);
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

    void workspaceSearchDefaultPersists()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *workspace = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceSearchByDefault"));
            QVERIFY(workspace);
            QVERIFY(workspace->isChecked());
            workspace->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QVERIFY(!Settings::workspaceSearchByDefault());
        QVERIFY(!KSharedConfig::openConfig()->group(QStringLiteral("Search")).readEntry("WorkspaceSearchByDefault", true));
    }

    void workspaceLayoutRestoreOptionsPersist()
    {
        KFilePlacesModel places;
        const QStringList entries{QStringLiteral("WorkspaceRestorePaneArrangement"),
                                  QStringLiteral("WorkspaceRestoreViews"),
                                  QStringLiteral("WorkspaceRestoreToolbars"),
                                  QStringLiteral("WorkspaceRestorePanels"),
                                  QStringLiteral("ConfirmWorkspaceLayoutOverwrite")};
        {
            SettingsDialog dialog(&places, nullptr);
            for (const QString &entry : entries) {
                auto *option = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_") + entry);
                QVERIFY2(option, qPrintable(entry));
                QVERIFY(option->isChecked());
                option->setChecked(false);
            }
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QVERIFY(!Settings::workspaceRestorePaneArrangement());
        QVERIFY(!Settings::workspaceRestoreViews());
        QVERIFY(!Settings::workspaceRestoreToolbars());
        QVERIFY(!Settings::workspaceRestorePanels());
        QVERIFY(!Settings::confirmWorkspaceLayoutOverwrite());
        const auto group = KSharedConfig::openConfig()->group(QStringLiteral("WorkspaceSettings"));
        for (const QString &entry : entries) {
            QVERIFY2(!group.readEntry(entry, true), qPrintable(entry));
        }
    }

    void workspaceSnapshotOptionsPersist()
    {
        KFilePlacesModel places;
        const QStringList entries{QStringLiteral("WorkspaceRestoreTabs"),
                                  QStringLiteral("WorkspaceRestoreActiveTabs"),
                                  QStringLiteral("WorkspaceRestoreFilters")};
        {
            SettingsDialog dialog(&places, nullptr);
            for (const QString &entry : entries) {
                auto *option = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_") + entry);
                QVERIFY2(option, qPrintable(entry));
                QVERIFY(option->isChecked());
                option->setChecked(false);
            }
            auto *confirm = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_ConfirmWorkspaceSwitch"));
            QVERIFY(confirm);
            QCOMPARE(confirm->currentIndex(), int(Settings::EnumConfirmWorkspaceSwitch::WhenUnsaved));
            confirm->setCurrentIndex(Settings::EnumConfirmWorkspaceSwitch::Always);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        const auto options = WorkspaceRestore::fromSettings();
        QVERIFY(!options.tabs && !options.activeTabs && !options.filters);
        QVERIFY(options.paneArrangement && options.views && options.toolbars && options.panels);
        QCOMPARE(Settings::confirmWorkspaceSwitch(), Settings::EnumConfirmWorkspaceSwitch::Always);
    }

    void workspaceRecoveryPromptPersists()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *prompt = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceRecoveryPrompt"));
            QVERIFY(prompt);
            QVERIFY(prompt->isChecked());
            prompt->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QVERIFY(!Settings::workspaceRecoveryPrompt());
    }

    void workspaceIndicatorOptionsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *placement = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_WorkspaceSelectorPlacement"));
            auto *unsaved = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowWorkspaceUnsavedIndicator"));
            auto *restore = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ConfirmWorkspaceLayoutRestore"));
            QVERIFY(placement && unsaved && restore);
            QCOMPARE(placement->currentIndex(), int(Settings::EnumWorkspaceSelectorPlacement::ViewMenu));
            QVERIFY(unsaved->isChecked() && restore->isChecked());
            placement->setCurrentIndex(Settings::EnumWorkspaceSelectorPlacement::StatusBar);
            unsaved->setChecked(false);
            restore->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::workspaceSelectorPlacement(), Settings::EnumWorkspaceSelectorPlacement::StatusBar);
        QVERIFY(!Settings::showWorkspaceUnsavedIndicator());
        QVERIFY(!Settings::confirmWorkspaceLayoutRestore());
    }

    void paneLocationBarsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *left = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_LeftLocationPresentation"));
            auto *right = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_RightLocationPresentation"));
            QVERIFY(left && right);
            QCOMPARE(left->currentIndex(), int(Settings::EnumLeftLocationPresentation::FullBreadcrumbs));
            QCOMPARE(right->currentIndex(), int(Settings::EnumRightLocationPresentation::FullBreadcrumbs));
            QCOMPARE(left->count(), 3);
            left->setCurrentIndex(Settings::EnumLeftLocationPresentation::EditablePath);
            right->setCurrentIndex(Settings::EnumRightLocationPresentation::AbbreviatedBreadcrumbs);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::leftLocationPresentation(), Settings::EnumLeftLocationPresentation::EditablePath);
        QCOMPARE(Settings::rightLocationPresentation(), Settings::EnumRightLocationPresentation::AbbreviatedBreadcrumbs);
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
        QVERIFY(link->isChecked());
        link->setChecked(false);
        dialog.button(QDialogButtonBox::Ok)->click();
        QVERIFY(!Settings::linkPanesAtStartup());
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
        dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PanelTitlesVisible"))->setChecked(true);
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
        QVERIFY(panels.readEntry("TitlesVisible", false));
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
        QVERIFY(hover->isChecked());
        hover->setChecked(false);
        dialog.button(QDialogButtonBox::Ok)->click();

        QCOMPARE(Settings::informationFields(), selected);
        QVERIFY(!Settings::informationHover());
        const KConfigGroup config = KSharedConfig::openConfig()->group(QStringLiteral("InformationPanel"));
        QCOMPARE(config.readEntry("InformationFields", QStringList()), selected);
        QVERIFY(!config.readEntry("InformationHover", true));
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
        auto *minimapWidth = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_FileListMinimapWidth"));
        auto *showSelectionSize = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowSelectionSize"));
        auto *copyMoveMenus = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowCopyMoveToMenus"));
        QVERIFY(startupMode && homeUrl && tabsAfterCurrent && followLinks && dropAction && previewLimit);
        QVERIFY(defaultView && showMinimap && minimapWidth && showSelectionSize && copyMoveMenus);
        QVERIFY(!minimapWidth->isEnabled());

        startupMode->setCurrentIndex(Settings::EnumStartupMode::FixedFolders);
        homeUrl->setUrl(QUrl::fromLocalFile(home.path()));
        QVERIFY(tabsAfterCurrent->isChecked());
        tabsAfterCurrent->setChecked(false);
        followLinks->setChecked(true);
        dropAction->setCurrentIndex(Settings::EnumDropAction::Copy);
        previewLimit->setValue(8);
        defaultView->setCurrentIndex(Settings::EnumDefaultViewMode::Icons);
        showMinimap->setChecked(true);
        QVERIFY(minimapWidth->isEnabled());
        minimapWidth->setValue(22);
        showSelectionSize->setChecked(false);
        copyMoveMenus->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("kioConfirmTrash"))->setChecked(true);
        dialog.button(QDialogButtonBox::Ok)->click();

        const auto config = KSharedConfig::openConfig();
        QCOMPARE(config->group(QStringLiteral("Startup")).readEntry("StartupMode", QString()), QStringLiteral("FixedFolders"));
        QCOMPARE(config->group(QStringLiteral("Startup")).readEntry("HomeUrl", QUrl()), QUrl::fromLocalFile(home.path()));
        QVERIFY(!config->group(QStringLiteral("Panes")).readEntry("NewTabsAfterCurrent", true));
        QVERIFY(config->group(QStringLiteral("Navigation")).readEntry("FollowSymlinks", false));
        QCOMPARE(config->group(QStringLiteral("Confirmations")).readEntry("DropAction", QString()), QStringLiteral("Copy"));
        QCOMPARE(config->group(QStringLiteral("Views")).readEntry("DefaultViewMode", QString()), QStringLiteral("Icons"));
        QVERIFY(config->group(QStringLiteral("Views")).readEntry("ShowFileListMinimap", false));
        QCOMPARE(config->group(QStringLiteral("Views")).readEntry("FileListMinimapWidth", 0), 22);
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
