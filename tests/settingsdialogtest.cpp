// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "viewsettings.h"

#include <KConfig>
#include <KConfigGroup>
#include <KPageWidget>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
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
    }

    void sidebarOptionsPersistAndPreview()
    {
        SettingsDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *pages = dialog.findChild<KPageWidget *>();
        auto *size = dialog.findChild<QSpinBox *>(QStringLiteral("sidebar_icon_size"));
        auto *position = dialog.findChild<QComboBox *>(QStringLiteral("sidebar_title_position"));
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
        QCOMPARE(position->currentData().toString(), QStringLiteral("right"));
        QCOMPARE(pages->faceType(), KPageView::FlatList);
        QTRY_VERIFY(sidebar());
        QCOMPARE(sidebar()->iconSize(), QSize(24, 24));

        auto *current = dialog.currentPage();
        size->setValue(40);
        position->setCurrentIndex(position->findData(QStringLiteral("below")));
        QCOMPARE(pages->faceType(), KPageView::List);
        QTRY_VERIFY(sidebar());
        QTRY_COMPARE(sidebar()->iconSize(), QSize(40, 40));
        QCOMPARE(dialog.currentPage(), current);
        position->setCurrentIndex(position->findData(QStringLiteral("right")));
        QTRY_COMPARE(pages->faceType(), KPageView::FlatList);
        QTRY_VERIFY(sidebar());
        QTRY_COMPARE(sidebar()->iconSize(), QSize(40, 40));
        position->setCurrentIndex(position->findData(QStringLiteral("below")));
        QTRY_COMPARE(pages->faceType(), KPageView::List);
        QTRY_VERIFY(sidebar());
        QTRY_COMPARE(sidebar()->iconSize(), QSize(40, 40));
        dialog.button(QDialogButtonBox::Ok)->click();

        SettingsDialog reopened;
        reopened.show();
        QVERIFY(QTest::qWaitForWindowExposed(&reopened));
        QCOMPARE(reopened.findChild<QSpinBox *>(QStringLiteral("sidebar_icon_size"))->value(), 40);
        QCOMPARE(reopened.findChild<QComboBox *>(QStringLiteral("sidebar_title_position"))->currentData().toString(), QStringLiteral("below"));
        QCOMPARE(reopened.findChild<KPageWidget *>()->faceType(), KPageView::List);
        reopened.findChild<QSpinBox *>(QStringLiteral("sidebar_icon_size"))->setValue(16);
        reopened.findChild<QComboBox *>(QStringLiteral("sidebar_title_position"))->setCurrentIndex(0);
        reopened.reject();

        SettingsDialog afterCancel;
        QCOMPARE(afterCancel.findChild<QSpinBox *>(QStringLiteral("sidebar_icon_size"))->value(), 40);
        QCOMPARE(afterCancel.findChild<QComboBox *>(QStringLiteral("sidebar_title_position"))->currentData().toString(), QStringLiteral("below"));
    }

    void savesViewAndFileRules()
    {
        SettingsDialog dialog;
        QVERIFY(dialog.findChild<QTabWidget *>());
        dialog.findChild<QCheckBox *>(QStringLiteral("column_type"))->setChecked(false);
        auto *groupBy = dialog.findChild<QComboBox *>(QStringLiteral("default_group_by"));
        QVERIFY(groupBy);
        groupBy->setCurrentIndex(groupBy->findData(QStringLiteral("ext")));
        auto *sizeFormat = dialog.findChild<QComboBox *>(QStringLiteral("size_format"));
        QVERIFY(sizeFormat);
        sizeFormat->setCurrentIndex(sizeFormat->findData(QStringLiteral("bytes")));
        auto *dateFormat = dialog.findChild<QComboBox *>(QStringLiteral("date_format"));
        QVERIFY(dateFormat);
        dateFormat->setCurrentIndex(dateFormat->findData(QStringLiteral("iso")));
        dialog.findChild<QCheckBox *>(QStringLiteral("show_seconds"))->setChecked(true);
        dialog.findChild<QSpinBox *>(QStringLiteral("icon_name_lines"))->setValue(4);
        dialog.findChild<QCheckBox *>(QStringLiteral("icon_info_size"))->setChecked(true);
        dialog.findChild<QPlainTextEdit *>(QStringLiteral("ignore_entries"))->setPlainText(QStringLiteral("*.bak\nThumbs.db"));
        dialog.findChild<QCheckBox *>(QStringLiteral("show_panels"))->setChecked(false);
        auto *panelPosition = dialog.findChild<QComboBox *>(QStringLiteral("panel_position"));
        QVERIFY(panelPosition);
        panelPosition->setCurrentIndex(panelPosition->findData(QStringLiteral("right")));
        dialog.findChild<QCheckBox *>(QStringLiteral("follow_active_folder"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("show_panel_titles"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("show_favorites_section"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("show_places_section"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("show_folder_tree_section"))->setChecked(false);
        auto *archiveFormat = dialog.findChild<QComboBox *>(QStringLiteral("default_archive_format"));
        QVERIFY(archiveFormat);
        archiveFormat->setCurrentIndex(archiveFormat->findData(static_cast<int>(ArchiveWriter::Format::TarXz)));
        dialog.findChild<QCheckBox *>(QStringLiteral("show_tab_icons"))->setChecked(false);
        dialog.findChild<QCheckBox *>(QStringLiteral("expand_tabs"))->setChecked(false);

        auto *colors = dialog.findChild<QTableWidget *>(QStringLiteral("extension_color_rules"));
        QVERIFY(colors);
        colors->insertRow(0);
        colors->setItem(0, 0, new QTableWidgetItem(QStringLiteral("cpp")));
        auto *color = new QTableWidgetItem(QStringLiteral("#123456"));
        color->setData(Qt::UserRole, QColor(QStringLiteral("#123456")));
        colors->setItem(0, 1, color);

        QPushButton *ok = dialog.button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        ok->click();

        const ViewSettings settings = ViewSettings::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("View")));
        QVERIFY(!settings.visibleColumns.contains(QStringLiteral("type")));
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
        SettingsDialog dialog;
        auto *startupMode = dialog.findChild<QComboBox *>(QStringLiteral("Startup_StartupMode"));
        auto *homeUrl = dialog.findChild<QLineEdit *>(QStringLiteral("Startup_HomeUrl"));
        auto *tabsAfterCurrent = dialog.findChild<QCheckBox *>(QStringLiteral("Panes_NewTabsAfterCurrent"));
        auto *followLinks = dialog.findChild<QCheckBox *>(QStringLiteral("Navigation_FollowSymlinks"));
        auto *dropAction = dialog.findChild<QComboBox *>(QStringLiteral("Confirmations_DropAction"));
        auto *previewLimit = dialog.findChild<QSpinBox *>(QStringLiteral("PreviewSettings_MaximumSizeMiB"));
        auto *defaultView = dialog.findChild<QComboBox *>(QStringLiteral("Views_DefaultViewMode"));
        auto *showSelectionSize = dialog.findChild<QCheckBox *>(QStringLiteral("StatusBar_ShowSelectionSize"));
        auto *copyMoveMenus = dialog.findChild<QCheckBox *>(QStringLiteral("ContextMenu_ShowCopyMoveToMenus"));
        QVERIFY(startupMode && homeUrl && tabsAfterCurrent && followLinks && dropAction && previewLimit);
        QVERIFY(defaultView && showSelectionSize && copyMoveMenus);

        startupMode->setCurrentIndex(startupMode->findData(QStringLiteral("FixedFolders")));
        homeUrl->setText(QStringLiteral("/tmp/home"));
        tabsAfterCurrent->setChecked(true);
        followLinks->setChecked(true);
        dropAction->setCurrentIndex(dropAction->findData(QStringLiteral("Copy")));
        previewLimit->setValue(8);
        defaultView->setCurrentIndex(defaultView->findData(QStringLiteral("Icons")));
        showSelectionSize->setChecked(false);
        copyMoveMenus->setChecked(true);
        dialog.button(QDialogButtonBox::Ok)->click();

        const auto config = KSharedConfig::openConfig();
        QCOMPARE(config->group(QStringLiteral("Startup")).readEntry("StartupMode", QString()), QStringLiteral("FixedFolders"));
        QCOMPARE(config->group(QStringLiteral("Startup")).readEntry("HomeUrl", QString()), QStringLiteral("/tmp/home"));
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
};

QTEST_MAIN(SettingsDialogTest)

#include "settingsdialogtest.moc"
