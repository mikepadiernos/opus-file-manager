// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "viewsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
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
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("View"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PanelSettings"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Archives"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Tabs"));
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.deleteEntry("IgnoreList");
        files.deleteEntry("ExtensionColors");
        KSharedConfig::openConfig()->sync();
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
};

QTEST_MAIN(SettingsDialogTest)

#include "settingsdialogtest.moc"
