// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

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
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.deleteEntry("IgnoreList");
        files.deleteEntry("ExtensionColors");
        KSharedConfig::openConfig()->sync();
    }

    void savesViewAndFileRules()
    {
        SettingsDialog dialog;
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
    }
};

QTEST_MAIN(SettingsDialogTest)

#include "settingsdialogtest.moc"
