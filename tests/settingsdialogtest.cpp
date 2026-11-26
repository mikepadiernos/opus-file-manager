// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "archivewriter.h"
#include "batchrenamedialog.h"
#include "branding.h"
#include "customcommanddialog.h"
#include "embeddedmetadata.h"
#include "embeddedmetadatasettings.h"
#include "filecolorrules.h"
#include "ignorelist.h"
#include "libraryarchive.h"
#include "libraryconfirmations.h"
#include "libraryregistry.h"
#include "libraryscan.h"
#include "mainwindow.h"
#include "pathfilterpresetsdialog.h"
#include "savedpathfilters.h"
#include "searchhistory.h"
#include "searchstoredialog.h"
#include "settings.h"
#include "settingsbackup.h"
#include "settingswidgets.h"
#include "tagfieldmanagerwidget.h"
#include "taggingsettings.h"
#include "taglibrary.h"
#include "viewsettings.h"
#include "workspacecatalog.h"
#include "workspacerestore.h"
#include "yazimode.h"

#include <KActionCollection>
#include <KColorButton>
#include <KConfig>
#include <KConfigGroup>
#include <KEditListWidget>
#include <KFilePlacesModel>
#include <KPageWidget>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <KShortcutsDialog>
#include <KToolBar>
#include <KUrlRequester>
#include <KXMLGUIFactory>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
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
#include <QTreeWidget>

#include <array>

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
                                     QStringLiteral("ContextMenu"),
                                     QStringLiteral("Tagging"),
                                     QStringLiteral("Branding"),
                                     QStringLiteral("YaziMode")}) {
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
        auto *portability = dialog.findChild<QWidget *>(QStringLiteral("workspacePortabilityPage"));
        QVERIFY(portability);
        QVERIFY(portability->findChild<QPushButton *>(QStringLiteral("exportWorkspaceProfiles")));
        QVERIFY(portability->findChild<QPushButton *>(QStringLiteral("importWorkspaceProfiles")));
    }

    void taggingPrivacySettings()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);

        // The Tagging page follows the Files page.
        auto *pages = dialog.findChild<KPageWidget *>();
        QVERIFY(pages);
        QStringList titles;
        for (int row = 0; row < pages->model()->rowCount(); ++row) {
            titles.append(pages->model()->index(row, 0).data(Qt::DisplayRole).toString());
        }
        QVERIFY(titles.contains(QStringLiteral("Tagging")));
        QCOMPARE(titles.indexOf(QStringLiteral("Tagging")), titles.indexOf(QStringLiteral("Files")) + 1);

        auto *page = dialog.findChild<QWidget *>(QStringLiteral("taggingPrivacyPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(page)), QStringLiteral("Privacy"));

        auto *explanation = page->findChild<QLabel *>(QStringLiteral("taggingPrivacyExplanation"));
        auto *storage = page->findChild<QComboBox *>(QStringLiteral("kcfg_NewLibraryStorage"));
        auto *folder = page->findChild<KUrlRequester *>(QStringLiteral("kcfg_LibraryBackupFolder"));
        auto *limit = page->findChild<QSpinBox *>(QStringLiteral("kcfg_LibraryBackupLimit"));
        auto *writes = page->findChild<QCheckBox *>(QStringLiteral("kcfg_AllowEmbeddedMetadataWrites"));
        QVERIFY(explanation && storage && folder && limit && writes);
        QVERIFY(explanation->wordWrap());
        QVERIFY(explanation->text().contains(QStringLiteral("network")));

        // Defaults: in the library folder, the default backup folder, 20 backups and no writes to files.
        QCOMPARE(storage->count(), 2);
        QCOMPARE(storage->currentIndex(), int(Settings::EnumNewLibraryStorage::LibraryFolder));
        QVERIFY(folder->url().isEmpty());
        QCOMPARE(folder->placeholderText(), LibraryArchive::backupDirectory());
        QCOMPARE(limit->value(), 20);
        QCOMPARE(limit->minimum(), 0);
        QCOMPARE(limit->maximum(), 1000);
        limit->setValue(0);
        QCOMPARE(limit->text(), QStringLiteral("All"));
        QVERIFY(!writes->isChecked());
        QCOMPARE(Settings::newLibraryStorage(), Settings::EnumNewLibraryStorage::LibraryFolder);
        QCOMPARE(Settings::libraryBackupLimit(), 20);
        QVERIFY(Settings::libraryBackupFolder().isEmpty());
        QVERIFY(!Settings::allowEmbeddedMetadataWrites());
    }

    void taggingLibrariesSettings()
    {
        for (const QString &root : LibraryRegistry::roots()) {
            LibraryRegistry::remove(root);
        }
        const QString privateFolder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/libraries");
        const auto cleanup = qScopeGuard([privateFolder] {
            for (const QString &root : LibraryRegistry::roots()) {
                LibraryRegistry::remove(root);
            }
            QDir(privateFolder).removeRecursively();
        });
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("Photos"));
        QVERIFY(QDir().mkpath(root));
        QVERIFY(LibraryRegistry::create(root).isEmpty());

        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        QList<LibraryConfirmations::Confirmation> asked;
        dialog.setLibraryConfirmation([&asked](const LibraryConfirmations::Confirmation &confirmation) {
            asked.append(confirmation);
            return true;
        });
        QSignalSpy changed(&dialog, &SettingsDialog::libraryChanged);
        QSignalSpy added(&dialog, &SettingsDialog::libraryAdded);

        // The Libraries tab follows Privacy.
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("taggingLibrariesPage"));
        auto *scroll = dialog.findChild<QWidget *>(QStringLiteral("taggingLibrariesScroll"));
        QVERIFY(page && scroll);
        auto *tabs = qobject_cast<QTabWidget *>(scroll->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(scroll)), QStringLiteral("Libraries"));
        QCOMPARE(tabs->indexOf(scroll), tabs->indexOf(dialog.findChild<QWidget *>(QStringLiteral("taggingPrivacyPage"))) + 1);

        auto *libraries = page->findChild<QTreeWidget *>(QStringLiteral("taggingLibraries"));
        auto *add = page->findChild<QPushButton *>(QStringLiteral("taggingAddLibrary"));
        auto *remove = page->findChild<QPushButton *>(QStringLiteral("taggingRemoveLibrary"));
        auto *move = page->findChild<QPushButton *>(QStringLiteral("taggingMoveLibraryMetadata"));
        auto *note = page->findChild<QLabel *>(QStringLiteral("taggingLibrariesNote"));
        auto *exclusions = page->findChild<KEditListWidget *>(QStringLiteral("kcfg_LibraryExclusions"));
        auto *hidden = page->findChild<QCheckBox *>(QStringLiteral("kcfg_IndexHiddenItems"));
        auto *limit = page->findChild<QSpinBox *>(QStringLiteral("kcfg_LibraryScanLimit"));
        auto *depth = page->findChild<QSpinBox *>(QStringLiteral("kcfg_LibraryScanDepth"));
        auto *automatic = page->findChild<QComboBox *>(QStringLiteral("kcfg_AutomaticLibraryRefresh"));
        auto *interval = page->findChild<QSpinBox *>(QStringLiteral("kcfg_LibraryRefreshInterval"));
        QVERIFY(libraries && add && remove && move && note && exclusions && hidden && limit && depth && automatic && interval);
        QVERIFY(note->text().contains(QStringLiteral("immediately")));

        // Defaults: no exclusions, hidden items indexed, no limits and no automatic refresh.
        QVERIFY(exclusions->items().isEmpty());
        QVERIFY(hidden->isChecked());
        QCOMPARE(limit->value(), 0);
        QCOMPARE(limit->maximum(), 10000000);
        QCOMPARE(limit->text(), QStringLiteral("No limit"));
        QCOMPARE(depth->value(), 0);
        QCOMPARE(depth->maximum(), 1000);
        QCOMPARE(depth->text(), QStringLiteral("No limit"));
        QCOMPARE(automatic->count(), 3);
        QCOMPARE(automatic->currentIndex(), int(Settings::EnumAutomaticLibraryRefresh::Never));
        QCOMPARE(interval->value(), 60);
        QCOMPARE(interval->minimum(), 1);
        QCOMPARE(interval->maximum(), 10080);
        QVERIFY(!interval->isEnabled());
        automatic->setCurrentIndex(int(Settings::EnumAutomaticLibraryRefresh::WhenOpened));
        QVERIFY(interval->isEnabled());

        // Exclusions are saved as a string list.
        exclusions->setItems({QStringLiteral("node_modules"), QStringLiteral("Exports/*")});
        hidden->setChecked(false);
        limit->setValue(500);
        depth->setValue(3);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::libraryExclusions(), (QStringList{QStringLiteral("node_modules"), QStringLiteral("Exports/*")}));
        QVERIFY(!Settings::indexHiddenItems());
        QCOMPARE(Settings::libraryScanLimit(), 500);
        QCOMPARE(Settings::libraryScanDepth(), 3);
        QCOMPARE(Settings::automaticLibraryRefresh(), Settings::EnumAutomaticLibraryRefresh::WhenOpened);
        const LibraryScan::Options options = TaggingSettings::scanOptions();
        QCOMPARE(options.exclusions, Settings::libraryExclusions());
        QVERIFY(!options.hidden);
        QCOMPARE(options.limit, 500);
        QCOMPARE(options.depth, 3);

        // The registered library is listed, ready, with its metadata in the folder; nothing is selected yet.
        QCOMPARE(libraries->topLevelItemCount(), 1);
        QTreeWidgetItem *item = libraries->topLevelItem(0);
        QCOMPARE(item->text(0), root);
        QCOMPARE(item->text(1), QStringLiteral("Library folder"));
        QCOMPARE(item->text(2), QStringLiteral("Ready"));
        QVERIFY(add->isEnabled());
        QVERIFY(!remove->isEnabled());
        QVERIFY(!move->isEnabled());

        // Moving the metadata keeps it privately, and the button then offers to move it back.
        libraries->setCurrentItem(item);
        QVERIFY(remove->isEnabled());
        QVERIFY(move->isEnabled());
        QCOMPARE(move->text(), QStringLiteral("Keep Metadata Privately"));
        move->click();
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.constLast().title, QStringLiteral("Move Library Metadata"));
        QVERIFY(TagLibrary::isPrivate(root));
        QVERIFY(!QFile::exists(TagLibrary::folderDocumentPath(root)));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.constLast().constFirst().toString(), root);
        QCOMPARE(libraries->topLevelItem(0)->text(1), QStringLiteral("Private"));
        QCOMPARE(move->text(), QStringLiteral("Keep Metadata in Library Folder"));

        // Removing unregisters the library and keeps its document.
        remove->click();
        QCOMPARE(asked.size(), 2);
        QVERIFY(asked.constLast().warning);
        QVERIFY(asked.constLast().question.contains(QStringLiteral("No files are deleted")));
        QVERIFY(!LibraryRegistry::contains(root));
        QVERIFY(QFile::exists(TagLibrary::privateDocumentPath(root)));
        QCOMPARE(libraries->topLevelItemCount(), 0);
        QCOMPARE(changed.size(), 2);
        QVERIFY(!remove->isEnabled());
        QVERIFY(!move->isEnabled());

        // Adding a folder registers it, adopting the kept private document, and asks for its first refresh.
        dialog.setLibraryFolderChooser([] {
            return QString();
        });
        add->click();
        QCOMPARE(asked.size(), 2);
        dialog.setLibraryFolderChooser([root] {
            return root;
        });
        add->click();
        QCOMPARE(asked.size(), 3);
        QCOMPARE(asked.constLast().title, QStringLiteral("Create Library"));
        QCOMPARE(asked.constLast().dontAskAgainName, QStringLiteral("ConfirmCreateLibrary"));
        QVERIFY(LibraryRegistry::contains(root));
        QVERIFY(TagLibrary::isPrivate(root));
        QCOMPARE(added.size(), 1);
        QCOMPARE(added.constLast().constFirst().toString(), root);
        QCOMPARE(changed.size(), 3);
        QCOMPARE(libraries->topLevelItemCount(), 1);
        QCOMPARE(libraries->currentItem()->text(1), QStringLiteral("Private"));

        // A new folder follows the storage shown in the Privacy tab.
        const QString other = dir.filePath(QStringLiteral("Music"));
        QVERIFY(QDir().mkpath(other));
        dialog.findChild<QComboBox *>(QStringLiteral("kcfg_NewLibraryStorage"))->setCurrentIndex(int(Settings::EnumNewLibraryStorage::PrivateFolder));
        dialog.setLibraryFolderChooser([other] {
            return other;
        });
        add->click();
        QVERIFY(LibraryRegistry::contains(other));
        QVERIFY(TagLibrary::isPrivate(other));
        QVERIFY(!QFile::exists(TagLibrary::folderDocumentPath(other)));
        QCOMPARE(libraries->topLevelItemCount(), 2);

        // A declined question changes nothing.
        dialog.setLibraryConfirmation([](const LibraryConfirmations::Confirmation &) {
            return false;
        });
        remove->click();
        QVERIFY(LibraryRegistry::contains(other));
        QCOMPARE(libraries->topLevelItemCount(), 2);
    }

    void taggingTagsSettings()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);

        // The Tags tab follows Libraries.
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("taggingTagsPage"));
        auto *scroll = dialog.findChild<QWidget *>(QStringLiteral("taggingTagsScroll"));
        QVERIFY(page && scroll);
        auto *tabs = qobject_cast<QTabWidget *>(scroll->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(scroll)), QStringLiteral("Tags"));
        QCOMPARE(tabs->indexOf(scroll), tabs->indexOf(dialog.findChild<QWidget *>(QStringLiteral("taggingLibrariesScroll"))) + 1);

        auto *placement = page->findChild<QComboBox *>(QStringLiteral("kcfg_TagPanelPlacement"));
        auto *showColors = page->findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTagColors"));
        auto *defaultColor = page->findChild<TagColorChooser *>(QStringLiteral("kcfg_DefaultTagColor"));
        auto *showTags = page->findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTagsInInformationPanel"));
        auto *showInherited = page->findChild<QCheckBox *>(QStringLiteral("kcfg_ShowInheritedTags"));
        auto *childTags = page->findChild<QCheckBox *>(QStringLiteral("kcfg_TagMatchesChildTags"));
        QVERIFY(placement && showColors && defaultColor && showTags && showInherited && childTags);
        auto *colorEnabled = defaultColor->findChild<QCheckBox *>(QStringLiteral("defaultTagColorEnabled"));
        auto *colorButton = defaultColor->findChild<KColorButton *>(QStringLiteral("defaultTagColorButton"));
        QVERIFY(colorEnabled && colorButton);
        QVERIFY(page->findChild<QLabel *>(QStringLiteral("taggingChildTagsHelp"))->text().contains(QStringLiteral("tag=")));

        // Defaults: the panel on the right, colours shown, no colour for new tags, direct tags only and child tags matched.
        QCOMPARE(placement->count(), 2);
        QCOMPARE(placement->currentIndex(), int(Settings::EnumTagPanelPlacement::Right));
        QVERIFY(showColors->isChecked());
        QVERIFY(defaultColor->color().isEmpty());
        QVERIFY(!colorEnabled->isChecked());
        QVERIFY(!colorButton->isEnabled());
        QVERIFY(showTags->isChecked());
        QVERIFY(!showInherited->isChecked());
        QVERIFY(showInherited->isEnabled());
        QVERIFY(childTags->isChecked());
        QVERIFY(TaggingSettings::tagSearchOptions().childTags);
        QVERIFY(TaggingSettings::defaultTagColor().isEmpty());

        // Inherited tags are only listed where tags are shown.
        showTags->setChecked(false);
        QVERIFY(!showInherited->isEnabled());
        showTags->setChecked(true);
        QVERIFY(showInherited->isEnabled());

        // Checking the colour enables the button and reports its colour.
        QSignalSpy colorChanged(defaultColor, &TagColorChooser::colorChanged);
        colorEnabled->setChecked(true);
        QVERIFY(colorButton->isEnabled());
        QCOMPARE(colorChanged.size(), 1);
        colorButton->setColor(QColor(QStringLiteral("#AA3355")));
        QCOMPARE(defaultColor->color(), QStringLiteral("#aa3355"));

        placement->setCurrentIndex(int(Settings::EnumTagPanelPlacement::Below));
        showColors->setChecked(false);
        showInherited->setChecked(true);
        childTags->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::tagPanelPlacement(), Settings::EnumTagPanelPlacement::Below);
        QVERIFY(!Settings::showTagColors());
        QCOMPARE(Settings::defaultTagColor(), QStringLiteral("#aa3355"));
        QVERIFY(Settings::showInheritedTags());
        QVERIFY(!Settings::tagMatchesChildTags());
        QVERIFY(!TaggingSettings::tagSearchOptions().childTags);
        QCOMPARE(TaggingSettings::defaultTagColor(), QStringLiteral("#aa3355"));

        // No default colour saves an empty value.
        colorEnabled->setChecked(false);
        QVERIFY(!colorButton->isEnabled());
        QVERIFY(defaultColor->color().isEmpty());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::defaultTagColor().isEmpty());

        // A value that isn't #rrggbb reads as no colour.
        defaultColor->setColor(QStringLiteral("#123456"));
        QVERIFY(colorEnabled->isChecked());
        QCOMPARE(defaultColor->color(), QStringLiteral("#123456"));
        defaultColor->setColor(QStringLiteral("red"));
        QVERIFY(!colorEnabled->isChecked());
        QVERIFY(defaultColor->color().isEmpty());
        Settings::setDefaultTagColor(QStringLiteral("#12345"));
        QVERIFY(TaggingSettings::defaultTagColor().isEmpty());
        Settings::setDefaultTagColor(QStringLiteral(" #ABCDEF "));
        QCOMPARE(TaggingSettings::defaultTagColor(), QStringLiteral("#abcdef"));
        Settings::self()->setDefaults();
        Settings::self()->save();
    }

    void taggingSearchSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);

        // The Search tab follows Tags.
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("taggingSearchPage"));
        auto *scroll = dialog.findChild<QWidget *>(QStringLiteral("taggingSearchScroll"));
        QVERIFY(page && scroll);
        auto *tabs = qobject_cast<QTabWidget *>(scroll->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(scroll)), QStringLiteral("Search"));
        QCOMPARE(tabs->indexOf(scroll), tabs->indexOf(dialog.findChild<QWidget *>(QStringLiteral("taggingTagsScroll"))) + 1);

        auto *scope = page->findChild<QComboBox *>(QStringLiteral("kcfg_TagSearchScope"));
        auto *aliases = page->findChild<QCheckBox *>(QStringLiteral("kcfg_TagMatchAliases"));
        auto *outside = page->findChild<QCheckBox *>(QStringLiteral("kcfg_UntaggedIncludesOutsideLibraries"));
        QVERIFY(scope && aliases && outside);
        QVERIFY(page->findChild<QLabel *>(QStringLiteral("taggingSearchScopeHelp"))->text().contains(QStringLiteral("tagged:")));
        QVERIFY(page->findChild<QLabel *>(QStringLiteral("taggingSearchMatchingHelp")));

        // Defaults: the current folder, aliases matched and tagged:no limited to libraries.
        QCOMPARE(scope->count(), 2);
        QCOMPARE(scope->currentIndex(), int(Settings::EnumTagSearchScope::CurrentFolder));
        QVERIFY(aliases->isChecked());
        QVERIFY(!outside->isChecked());
        TagSearch::Options options = TaggingSettings::tagSearchOptions();
        QCOMPARE(options.scope, TagSearch::Scope::CurrentFolder);
        QVERIFY(options.aliases);
        QVERIFY(!options.untaggedOutsideLibraries);

        scope->setCurrentIndex(int(Settings::EnumTagSearchScope::LibraryRoot));
        aliases->setChecked(false);
        outside->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::tagSearchScope(), Settings::EnumTagSearchScope::LibraryRoot);
        QVERIFY(!Settings::tagMatchAliases());
        QVERIFY(Settings::untaggedIncludesOutsideLibraries());
        options = TaggingSettings::tagSearchOptions();
        QCOMPARE(options.scope, TagSearch::Scope::LibraryRoot);
        QVERIFY(!options.aliases);
        QVERIFY(options.untaggedOutsideLibraries);
        // Child-tag matching stays with the Tags tab.
        QVERIFY(options.childTags);

        // Defaults restore every entry.
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(scope->currentIndex(), int(Settings::EnumTagSearchScope::CurrentFolder));
        QVERIFY(aliases->isChecked());
        QVERIFY(!outside->isChecked());
    }

    void taggingFieldsSettings()
    {
        for (const QString &root : LibraryRegistry::roots()) {
            LibraryRegistry::remove(root);
        }
        const auto cleanup = qScopeGuard([] {
            for (const QString &root : LibraryRegistry::roots()) {
                LibraryRegistry::remove(root);
            }
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        const auto fieldNames = [](const QString &root) {
            QStringList names;
            const TagLibrary::Library library = TagLibrary::Library::load(root);
            for (const TagLibrary::Field &field : library.fields()) {
                names.append(field.name);
            }
            return names;
        };
        const auto ok = [](TagFieldDialog *editor) {
            return editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        };
        const auto nameOf = [](TagFieldDialog *editor) {
            return editor->findChild<QLineEdit *>(QStringLiteral("tagFieldName"));
        };
        const auto typeOf = [](TagFieldDialog *editor) {
            return editor->findChild<QComboBox *>(QStringLiteral("tagFieldType"));
        };

        {
            // Without libraries, the tab says how to add one instead of showing an empty editor.
            KFilePlacesModel places;
            SettingsDialog dialog(&places, nullptr);
            auto *picker = dialog.findChild<QComboBox *>(QStringLiteral("taggingFieldLibrary"));
            auto *placeholder = dialog.findChild<QLabel *>(QStringLiteral("taggingNoFieldLibraries"));
            auto *manager = dialog.findChild<TagFieldManagerWidget *>(QStringLiteral("taggingFieldManager"));
            QVERIFY(picker && placeholder && manager);
            QCOMPARE(picker->count(), 0);
            QVERIFY(!picker->isEnabled());
            QVERIFY(!placeholder->isHidden());
            QVERIFY(manager->isHidden());
            QVERIFY(manager->library().isEmpty());
        }

        QTemporaryDir dir;
        const QString music = dir.filePath(QStringLiteral("Music"));
        const QString photos = dir.filePath(QStringLiteral("Photos"));
        const QString broken = dir.filePath(QStringLiteral("Broken"));
        for (const QString &root : {music, photos, broken}) {
            QVERIFY(QDir().mkpath(root));
        }
        QVERIFY(
            LibraryRegistry::create(photos, TagLibrary::Storage::LibraryFolder, TagLibrary::parseFieldTemplates({QStringLiteral("date:Shot on")})).isEmpty());
        QVERIFY(LibraryRegistry::create(music).isEmpty());
        QVERIFY(LibraryRegistry::create(broken).isEmpty());
        {
            QFile document(TagLibrary::documentPath(broken));
            QVERIFY(document.open(QIODevice::WriteOnly | QIODevice::Truncate));
            document.write("not json");
        }

        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        dialog.setLibraryConfirmation([](const LibraryConfirmations::Confirmation &) {
            return true;
        });
        QSignalSpy changed(&dialog, &SettingsDialog::libraryChanged);

        // The Fields tab follows Search.
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("taggingFieldsPage"));
        auto *scroll = dialog.findChild<QWidget *>(QStringLiteral("taggingFieldsScroll"));
        QVERIFY(page && scroll);
        auto *tabs = qobject_cast<QTabWidget *>(scroll->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(scroll)), QStringLiteral("Fields"));
        QCOMPARE(tabs->indexOf(scroll), tabs->indexOf(dialog.findChild<QWidget *>(QStringLiteral("taggingSearchScroll"))) + 1);

        auto *picker = page->findChild<QComboBox *>(QStringLiteral("taggingFieldLibrary"));
        auto *placeholder = page->findChild<QLabel *>(QStringLiteral("taggingNoFieldLibraries"));
        auto *manager = page->findChild<TagFieldManagerWidget *>(QStringLiteral("taggingFieldManager"));
        auto *note = page->findChild<QLabel *>(QStringLiteral("taggingFieldsNote"));
        auto *templates = page->findChild<FieldTemplateEditor *>(QStringLiteral("kcfg_NewLibraryFields"));
        QVERIFY(picker && placeholder && manager && note && templates);
        QVERIFY(note->text().contains(QStringLiteral("immediately")));
        auto *fieldList = manager->findChild<QTreeWidget *>(QStringLiteral("tagFieldList"));
        auto *newField = manager->findChild<QPushButton *>(QStringLiteral("tagFieldNew"));
        QVERIFY(fieldList && newField);

        // The picker lists the registered libraries, sorted, and the editor shows the first one's fields.
        QCOMPARE(picker->count(), 3);
        QCOMPARE(picker->itemData(0).toString(), broken);
        QCOMPARE(picker->itemData(1).toString(), music);
        QCOMPARE(picker->itemData(2).toString(), photos);
        QVERIFY(picker->isEnabled());
        QVERIFY(placeholder->isHidden());
        QVERIFY(!manager->isHidden());

        // An unreadable library is reported and can't be edited.
        QCOMPARE(manager->library(), broken);
        QCOMPARE(fieldList->topLevelItemCount(), 0);
        QVERIFY(!manager->message().isEmpty());
        QVERIFY(!newField->isEnabled());

        // Switching libraries loads the chosen library's fields.
        picker->setCurrentIndex(2);
        QCOMPARE(manager->library(), photos);
        QCOMPARE(fieldList->topLevelItemCount(), 1);
        QCOMPARE(fieldList->topLevelItem(0)->text(0), QStringLiteral("Shot on"));
        QCOMPARE(fieldList->topLevelItem(0)->text(1), QStringLiteral("Date"));
        picker->setCurrentIndex(1);
        QCOMPARE(manager->library(), music);
        QCOMPARE(fieldList->topLevelItemCount(), 0);
        QVERIFY(newField->isEnabled());

        // A new field is saved to the library at once, without applying Preferences, and open windows are told.
        TagFieldDialog *editor = manager->createField();
        QVERIFY(editor);
        nameOf(editor)->setText(QStringLiteral("Artist"));
        ok(editor)->click();
        QCOMPARE(fieldNames(music), QStringList{QStringLiteral("Artist")});
        QCOMPARE(fieldList->topLevelItemCount(), 1);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.constLast().constFirst().toString(), music);

        // The picker follows libraries added and removed in the Libraries tab, keeping the chosen library.
        const QString books = dir.filePath(QStringLiteral("Books"));
        QVERIFY(QDir().mkpath(books));
        dialog.setLibraryFolderChooser([books] {
            return books;
        });
        dialog.findChild<QPushButton *>(QStringLiteral("taggingAddLibrary"))->click();
        QVERIFY(LibraryRegistry::contains(books));
        QCOMPARE(picker->count(), 4);
        QCOMPARE(picker->itemData(0).toString(), books);
        QCOMPARE(picker->currentData().toString(), music);
        QCOMPARE(manager->library(), music);
        QVERIFY(fieldNames(books).isEmpty());

        auto *libraries = dialog.findChild<QTreeWidget *>(QStringLiteral("taggingLibraries"));
        for (int row = 0; row < libraries->topLevelItemCount(); ++row) {
            if (libraries->topLevelItem(row)->data(0, Qt::UserRole).toString() == music) {
                libraries->setCurrentItem(libraries->topLevelItem(row));
            }
        }
        dialog.findChild<QPushButton *>(QStringLiteral("taggingRemoveLibrary"))->click();
        QVERIFY(!LibraryRegistry::contains(music));
        QCOMPARE(picker->count(), 3);
        QCOMPARE(picker->findData(music), -1);
        QCOMPARE(manager->library(), picker->currentData().toString());

        // Fields for new libraries: empty by default, edited in a list and saved when applied.
        QVERIFY(templates->templates().isEmpty());
        auto *templateList = templates->findChild<QTreeWidget *>(QStringLiteral("fieldTemplateList"));
        auto *removeTemplate = templates->findChild<QPushButton *>(QStringLiteral("fieldTemplateRemove"));
        auto *upTemplate = templates->findChild<QPushButton *>(QStringLiteral("fieldTemplateUp"));
        QVERIFY(templateList && removeTemplate && upTemplate);
        QVERIFY(!removeTemplate->isEnabled());

        editor = templates->addField();
        QVERIFY(!ok(editor)->isEnabled());
        nameOf(editor)->setText(QStringLiteral("Title"));
        ok(editor)->click();
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        editor = templates->addField();
        nameOf(editor)->setText(QStringLiteral("title"));
        QVERIFY(!ok(editor)->isEnabled());
        nameOf(editor)->setText(QStringLiteral("Shot on"));
        typeOf(editor)->setCurrentIndex(typeOf(editor)->findData(int(TagLibrary::FieldType::Date)));
        ok(editor)->click();
        editor = templates->addField();
        nameOf(editor)->setText(QStringLiteral("Draft"));
        ok(editor)->click();
        QCOMPARE(templates->templates(), (QStringList{QStringLiteral("text:Title"), QStringLiteral("date:Shot on"), QStringLiteral("text:Draft")}));
        QCOMPARE(templateList->topLevelItem(1)->text(1), QStringLiteral("Date"));
        QCOMPARE(templates->currentRow(), 2);

        // Editing keeps the row, removing drops it and the order can change.
        editor = templates->editField(0);
        QVERIFY(editor);
        nameOf(editor)->setText(QStringLiteral("Caption"));
        ok(editor)->click();
        QVERIFY(!templates->editField(5));
        QCOMPARE(templates->currentRow(), 0);
        templates->setCurrentRow(2);
        removeTemplate->click();
        QCOMPARE(templates->templates(), (QStringList{QStringLiteral("text:Caption"), QStringLiteral("date:Shot on")}));
        templates->setCurrentRow(1);
        upTemplate->click();
        QCOMPARE(templates->templates(), (QStringList{QStringLiteral("date:Shot on"), QStringLiteral("text:Caption")}));
        QCOMPARE(templates->currentRow(), 0);
        QVERIFY(Settings::newLibraryFields().isEmpty());

        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::newLibraryFields(), (QStringList{QStringLiteral("date:Shot on"), QStringLiteral("text:Caption")}));
        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("Tagging")).readEntry("NewLibraryFields", QStringList()), Settings::newLibraryFields());
        QVERIFY(!dialog.button(QDialogButtonBox::Apply)->isEnabled());

        // Libraries added in the Libraries tab start with the fields as shown, even before they are applied.
        editor = templates->addField();
        nameOf(editor)->setText(QStringLiteral("Place"));
        ok(editor)->click();
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        const QString films = dir.filePath(QStringLiteral("Films"));
        QVERIFY(QDir().mkpath(films));
        dialog.setLibraryFolderChooser([films] {
            return films;
        });
        dialog.findChild<QPushButton *>(QStringLiteral("taggingAddLibrary"))->click();
        QCOMPARE(fieldNames(films), (QStringList{QStringLiteral("Shot on"), QStringLiteral("Caption"), QStringLiteral("Place")}));
        QCOMPARE(TagLibrary::Library::load(films).fields().constFirst().type, TagLibrary::FieldType::Date);

        // Invalid and repeated stored entries are left out, and Defaults clear the list.
        templates->setTemplates({QStringLiteral("text:A"), QStringLiteral("bogus"), QStringLiteral("date:a")});
        QCOMPARE(templates->templates(), QStringList{QStringLiteral("text:A")});
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(templates->templates().isEmpty());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::newLibraryFields().isEmpty());
    }

    void taggingEmbeddedMetadataSettings()
    {
        for (const QString &root : LibraryRegistry::roots()) {
            LibraryRegistry::remove(root);
        }
        const auto cleanup = qScopeGuard([] {
            for (const QString &root : LibraryRegistry::roots()) {
                LibraryRegistry::remove(root);
            }
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QTemporaryDir dir;
        const QString photos = dir.filePath(QStringLiteral("Photos"));
        QVERIFY(QDir().mkpath(photos));
        QVERIFY(LibraryRegistry::create(photos,
                                        TagLibrary::Storage::LibraryFolder,
                                        TagLibrary::parseFieldTemplates({QStringLiteral("text:Camera"), QStringLiteral("date:Shot on")}))
                    .isEmpty());
        Settings::setNewLibraryFields({QStringLiteral("text:Caption"), QStringLiteral("text:camera")});
        Settings::self()->save();

        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        const auto group = [] {
            return KSharedConfig::openConfig()->group(QStringLiteral("Tagging"));
        };

        // The Embedded Metadata tab is the last Tagging tab, after Fields.
        auto *scroll = dialog.findChild<QWidget *>(QStringLiteral("taggingEmbeddedMetadataScroll"));
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("taggingEmbeddedMetadataPage"));
        QVERIFY(scroll && page);
        auto *tabs = qobject_cast<QTabWidget *>(scroll->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(scroll)), QStringLiteral("Embedded Metadata"));
        QCOMPARE(tabs->indexOf(scroll), tabs->indexOf(dialog.findChild<QWidget *>(QStringLiteral("taggingFieldsScroll"))) + 1);
        QCOMPARE(tabs->indexOf(scroll), tabs->count() - 1);

        auto *keys = page->findChild<EmbeddedMetadataKeyList *>(QStringLiteral("kcfg_DisabledEmbeddedMetadataKeys"));
        auto *table = page->findChild<EmbeddedMetadataMappingTable *>(QStringLiteral("kcfg_EmbeddedMetadataMappings"));
        auto *conflicts = page->findChild<QComboBox *>(QStringLiteral("kcfg_EmbeddedMetadataConflicts"));
        auto *onRefresh = page->findChild<QCheckBox *>(QStringLiteral("kcfg_ImportEmbeddedMetadataOnRefresh"));
        auto *hint = page->findChild<QLabel *>(QStringLiteral("taggingEmbeddedMetadataRefreshHint"));
        QVERIFY(keys && table && conflicts && onRefresh && hint);
        auto *tree = keys->findChild<QTreeWidget *>(QStringLiteral("embeddedMetadataKeyTree"));
        auto *mappingTree = table->findChild<QTreeWidget *>(QStringLiteral("embeddedMetadataMappingTable"));
        QVERIFY(tree && mappingTree);
        const auto rowHidden = [mappingTree, table](const QString &key) {
            for (int row = 0; row < mappingTree->topLevelItemCount(); ++row) {
                if (mappingTree->itemWidget(mappingTree->topLevelItem(row), 1) == table->target(key)) {
                    return mappingTree->topLevelItem(row)->isHidden();
                }
            }
            return false;
        };

        // Every property is read by default, grouped by the files it's read from.
        QCOMPARE(keys->enabledKeys(), EmbeddedMetadata::keys());
        QVERIFY(keys->disabledKeys().isEmpty());
        QCOMPARE(tree->topLevelItemCount(), 3);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Images"));
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("Audio"));
        QCOMPARE(tree->topLevelItem(2)->text(0), QStringLiteral("Images and Audio"));
        QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), EmbeddedMetadata::displayName(EmbeddedMetadata::imageKeys().constFirst()));
        int listed = 0;
        for (int group = 0; group < tree->topLevelItemCount(); ++group) {
            QCOMPARE(tree->topLevelItem(group)->checkState(0), Qt::Checked);
            listed += tree->topLevelItem(group)->childCount();
        }
        QCOMPARE(listed, EmbeddedMetadata::keys().size());
        QCOMPARE(mappingTree->topLevelItemCount(), EmbeddedMetadata::keys().size());

        // Nothing is imported by default, so importing on refresh is unavailable.
        QVERIFY(table->mappings().isEmpty());
        QCOMPARE(table->target(QStringLiteral("genre"))->currentIndex(), 0);
        QCOMPARE(conflicts->currentIndex(), 0);
        QVERIFY(!onRefresh->isChecked());
        QVERIFY(!onRefresh->isEnabled());
        QVERIFY(!hint->isHidden());

        // Fields of libraries and of new libraries are offered once each, for the properties they can hold.
        QComboBox *model = table->target(QStringLiteral("model"));
        QComboBox *taken = table->target(QStringLiteral("photoDateTimeOriginal"));
        QVERIFY(model && taken);
        QCOMPARE(model->count(), 4);
        QCOMPARE(model->itemData(1).toString(), QStringLiteral("tags"));
        QVERIFY(model->findData(QStringLiteral("field:Camera")) >= 0);
        QVERIFY(model->findData(QStringLiteral("field:Caption")) >= 0);
        QCOMPARE(model->findData(QStringLiteral("field:Shot on")), -1);
        QVERIFY(taken->findData(QStringLiteral("field:Shot on")) >= 0);

        // Unchecking a property hides its mapping, and Apply saves it.
        keys->setKeyEnabled(QStringLiteral("genre"), false);
        QCOMPARE(tree->topLevelItem(1)->checkState(0), Qt::PartiallyChecked);
        QVERIFY(rowHidden(QStringLiteral("genre")));
        QVERIFY(!rowHidden(QStringLiteral("model")));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::disabledEmbeddedMetadataKeys(), QStringList{QStringLiteral("genre")});
        QCOMPARE(group().readEntry("DisabledEmbeddedMetadataKeys", QStringList()), QStringList{QStringLiteral("genre")});
        QVERIFY(!TaggingSettings::embeddedMetadataKeys().contains(QStringLiteral("genre")));

        // Choosing targets allows importing on refresh, and Apply saves the choices.
        model->setCurrentIndex(model->findData(QStringLiteral("field:Camera")));
        QComboBox *manufacturer = table->target(QStringLiteral("manufacturer"));
        manufacturer->setCurrentIndex(manufacturer->findData(QStringLiteral("tags")));
        QVERIFY(onRefresh->isEnabled());
        QVERIFY(hint->isHidden());
        onRefresh->setChecked(true);
        conflicts->setCurrentIndex(1);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        const QStringList saved{QStringLiteral("manufacturer=tags"), QStringLiteral("model=field:Camera")};
        QCOMPARE(Settings::embeddedMetadataMappings(), saved);
        QCOMPARE(group().readEntry("EmbeddedMetadataMappings", QStringList()), saved);
        QCOMPARE(Settings::embeddedMetadataConflicts(), Settings::EnumEmbeddedMetadataConflicts::Replace);
        QVERIFY(Settings::importEmbeddedMetadataOnRefresh());
        QVERIFY(TaggingSettings::importsEmbeddedMetadataOnRefresh());
        QCOMPARE(TaggingSettings::embeddedMetadataPolicy(), MetadataImport::Policy::Replace);

        // Mappings of unchecked properties are kept but don't allow importing on refresh.
        keys->setKeyEnabled(QStringLiteral("manufacturer"), false);
        keys->setKeyEnabled(QStringLiteral("model"), false);
        QVERIFY(!onRefresh->isEnabled());
        QVERIFY(!hint->isHidden());
        QCOMPARE(table->mappings(), saved);
        keys->setKeyEnabled(QStringLiteral("manufacturer"), true);
        keys->setKeyEnabled(QStringLiteral("model"), true);
        QVERIFY(onRefresh->isEnabled());

        // Fields added on the Fields tab are offered at once, keeping the choice.
        auto *templates = dialog.findChild<FieldTemplateEditor *>(QStringLiteral("kcfg_NewLibraryFields"));
        QVERIFY(templates);
        templates->setTemplates({QStringLiteral("text:Caption"), QStringLiteral("text:Lens")});
        QVERIFY(model->findData(QStringLiteral("field:Lens")) >= 0);
        QCOMPARE(model->currentData().toString(), QStringLiteral("field:Camera"));

        // A saved field that no library has yet is still shown.
        table->setMappings({QStringLiteral("album=field:Album Name")});
        QCOMPARE(table->target(QStringLiteral("album"))->currentData().toString(), QStringLiteral("field:Album Name"));
        QCOMPARE(table->target(QStringLiteral("model"))->currentIndex(), 0);

        // Defaults read every property and import none.
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(keys->disabledKeys().isEmpty());
        QVERIFY(!rowHidden(QStringLiteral("genre")));
        QVERIFY(table->mappings().isEmpty());
        QCOMPARE(conflicts->currentIndex(), 0);
        QVERIFY(!onRefresh->isChecked());
        QVERIFY(!onRefresh->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::disabledEmbeddedMetadataKeys().isEmpty());
        QVERIFY(Settings::embeddedMetadataMappings().isEmpty());
        QCOMPARE(Settings::embeddedMetadataConflicts(), Settings::EnumEmbeddedMetadataConflicts::Keep);
        QVERIFY(!Settings::importEmbeddedMetadataOnRefresh());
    }

    void brandingPresentationSettings()
    {
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_StartupPresentation"));
        auto *variant = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_LogoVariant"));
        auto *preview = dialog.findChild<QLabel *>(QStringLiteral("brandingPreview"));
        QVERIFY(enabled && variant && preview);
        auto *tabs = qobject_cast<QTabWidget *>(preview->parentWidget()->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(preview->parentWidget())), QStringLiteral("Presentation"));

        // Off and following the colour scheme by default, with the logo choice disabled.
        QVERIFY(!enabled->isChecked());
        QCOMPARE(variant->currentIndex(), int(Branding::LogoVariant::Automatic));
        QVERIFY(!variant->isEnabled());
        QVERIFY(!preview->pixmap().isNull());

        enabled->setChecked(true);
        QVERIFY(variant->isEnabled());
        QVERIFY(preview->isEnabled());

        // The preview follows the selected variant.
        variant->setCurrentIndex(int(Branding::LogoVariant::DarkText));
        const QImage dark = preview->pixmap().toImage();
        variant->setCurrentIndex(int(Branding::LogoVariant::LightText));
        // The dialog tracks user selection in plain combo boxes.
        Q_EMIT variant->activated(variant->currentIndex());
        const QImage light = preview->pixmap().toImage();
        QVERIFY(!dark.isNull() && !light.isNull());
        QVERIFY(dark != light);
        const QPoint centre(dark.width() / 2, 4 * dark.devicePixelRatio());
        QVERIFY(dark.pixelColor(centre).lightnessF() > 0.5);
        QVERIFY(light.pixelColor(centre).lightnessF() < 0.5);

        // Following the colour scheme, the preview redraws when the palette changes.
        variant->setCurrentIndex(int(Branding::LogoVariant::Automatic));
        QPalette palette = preview->palette();
        palette.setColor(QPalette::Window, QColor(20, 20, 20));
        palette.setColor(QPalette::WindowText, Qt::white);
        preview->setPalette(palette);
        QVERIFY(preview->pixmap().toImage().pixelColor(centre).lightnessF() < 0.5);
        palette.setColor(QPalette::Window, QColor(240, 240, 240));
        palette.setColor(QPalette::WindowText, Qt::black);
        preview->setPalette(palette);
        QVERIFY(preview->pixmap().toImage().pixelColor(centre).lightnessF() > 0.5);
        variant->setCurrentIndex(int(Branding::LogoVariant::LightText));
        Q_EMIT variant->activated(variant->currentIndex());

        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::startupPresentation());
        QCOMPARE(int(Settings::logoVariant()), int(Branding::LogoVariant::LightText));

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(!enabled->isChecked());
        QCOMPARE(variant->currentIndex(), int(Branding::LogoVariant::Automatic));
        QVERIFY(!variant->isEnabled());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::startupPresentation());
        QCOMPARE(int(Settings::logoVariant()), int(Branding::LogoVariant::Automatic));
    }

    void importsSelectedWorkspaceProfiles()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto sourceConfig = KSharedConfig::openConfig(root.filePath(QStringLiteral("source")));
        WorkspaceCatalog::Catalog source;
        QString error;
        QVERIFY(WorkspaceCatalog::create(source, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(source, QStringLiteral("Other"), QStringLiteral("O"), error));
        QVERIFY(WorkspaceCatalog::create(source, QStringLiteral("Home"), QStringLiteral("H"), error));
        QVERIFY(WorkspaceCatalog::save(sourceConfig->group(QStringLiteral("Workspaces")), source, error));
        QVERIFY(sourceConfig->sync());
        const QString archive = root.filePath(QStringLiteral("profiles.tar.gz"));
        const QStringList ids{source.workspaces.at(0).id, source.workspaces.at(1).id, source.workspaces.at(2).id};
        const auto state = KSharedConfig::openConfig(root.filePath(QStringLiteral("state")));
        state->group(QStringLiteral("WindowLayout")).writeEntry("Stacked", true);
        QVERIFY(state->sync());
        const QString exportError = SettingsBackup::createWorkspaceProfiles(archive, sourceConfig, state, ids);
        QVERIFY2(exportError.isEmpty(), qPrintable(exportError));

        const auto config = KSharedConfig::openConfig();
        WorkspaceCatalog::Catalog current;
        QVERIFY(WorkspaceCatalog::create(current, QStringLiteral("Work"), QStringLiteral("Existing"), error));
        QVERIFY(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), current, error));
        QVERIFY(config->sync());

        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *import = dialog.findChild<QPushButton *>(QStringLiteral("importWorkspaceProfiles"));
        QVERIFY(import);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        const auto nativeDialogs = qScopeGuard([] {
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
        });
        QStringList listed;
        bool keepEnabledWithoutSelection = true;
        QString result;
        bool fileChosen = false;
        bool profilesChosen = false;
        QTimer dialogs;
        dialogs.setInterval(20);
        connect(&dialogs, &QTimer::timeout, &dialogs, [&] {
            QWidget *modal = QApplication::activeModalWidget();
            if (auto *files = qobject_cast<QFileDialog *>(modal); files && !fileChosen) {
                fileChosen = true;
                // The dialog opens in a missing Documents folder, so type the full path.
                files->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))->setText(archive);
                QMetaObject::invokeMethod(files, "accept", Qt::QueuedConnection);
            } else if (modal && modal->objectName() == QLatin1String("workspaceImportSelection") && !profilesChosen) {
                profilesChosen = true;
                auto *profiles = modal->findChild<QListWidget *>(QStringLiteral("workspaceImportProfiles"));
                auto *keep = modal->findChild<QPushButton *>(QStringLiteral("workspaceImportKeep"));
                for (int i = 0; i < profiles->count(); ++i) {
                    listed.append(profiles->item(i)->text());
                    profiles->item(i)->setCheckState(Qt::Unchecked);
                }
                keepEnabledWithoutSelection = keep->isEnabled();
                // Import Work, which conflicts, and Home, but not Other.
                profiles->item(0)->setCheckState(Qt::Checked);
                profiles->item(2)->setCheckState(Qt::Checked);
                QMetaObject::invokeMethod(keep, &QPushButton::click, Qt::QueuedConnection);
            } else if (auto *box = qobject_cast<QMessageBox *>(modal)) {
                result = box->text();
                dialogs.stop();
                QMetaObject::invokeMethod(box, "accept", Qt::QueuedConnection);
            }
        });
        dialogs.start();
        import->click();
        QTRY_VERIFY(!dialogs.isActive());

        QCOMPARE(listed.size(), 3);
        QVERIFY(listed.at(0).startsWith(QStringLiteral("Work")) && listed.at(0).contains(QStringLiteral("conflict")));
        QVERIFY(!listed.at(2).contains(QStringLiteral("conflict")));
        QVERIFY(!keepEnabledWithoutSelection);
        QVERIFY2(result.contains(QStringLiteral("Home")), qPrintable(result));
        WorkspaceCatalog::Catalog imported;
        QVERIFY(WorkspaceCatalog::load(config->group(QStringLiteral("Workspaces")), imported, error));
        QStringList names;
        for (const auto &workspace : imported.workspaces) {
            names.append(workspace.name + QLatin1Char('/') + workspace.shortLabel);
        }
        QCOMPARE(names, (QStringList{QStringLiteral("Work/Existing"), QStringLiteral("Home/H")}));
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
        QString error;
        QVERIFY(SavedPathFilters::save(KSharedConfig::openConfig()->group(QStringLiteral("SavedPathFilters")),
                                       {{QStringLiteral("Reports"), PathFilter{QStringLiteral("reports"), {}, {}, -1}}},
                                       &error));
        const auto removeFilters = qScopeGuard([] {
            KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SavedPathFilters"));
            Settings::setDefaultSearchPathFilter(QString());
            Settings::self()->save();
        });
        {
            SettingsDialog dialog(&places, nullptr);
            auto *scope = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultSearchScope"));
            auto *field = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultSearchField"));
            auto *filter = dialog.findChild<SavedPathFilterCombo *>(QStringLiteral("kcfg_DefaultSearchPathFilter"));
            QVERIFY(scope && field && filter);
            QCOMPARE(scope->currentIndex(), int(Settings::EnumDefaultSearchScope::CurrentFolder));
            QCOMPARE(field->currentIndex(), int(Settings::EnumDefaultSearchField::FileName));
            QCOMPARE(filter->filterName(), QString());
            QCOMPARE(filter->count(), 2);
            scope->setCurrentIndex(Settings::EnumDefaultSearchScope::SearchFolders);
            field->setCurrentIndex(Settings::EnumDefaultSearchField::Query);
            filter->setCurrentIndex(filter->findData(QStringLiteral("Reports")));
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("Search")).readEntry("DefaultSearchScope", QString()), QStringLiteral("SearchFolders"));
        {
            SettingsDialog dialog(&places, nullptr);
            auto *scope = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultSearchScope"));
            auto *field = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultSearchField"));
            auto *filter = dialog.findChild<SavedPathFilterCombo *>(QStringLiteral("kcfg_DefaultSearchPathFilter"));
            QVERIFY(scope && field && filter);
            QCOMPARE(scope->currentIndex(), int(Settings::EnumDefaultSearchScope::SearchFolders));
            QCOMPARE(field->currentIndex(), int(Settings::EnumDefaultSearchField::Query));
            QCOMPARE(filter->filterName(), QStringLiteral("Reports"));
            scope->setCurrentIndex(Settings::EnumDefaultSearchScope::Indexed);
            filter->setCurrentIndex(0);
            dialog.reject();
        }
        QCOMPARE(Settings::defaultSearchScope(), int(Settings::EnumDefaultSearchScope::SearchFolders));
        QCOMPARE(Settings::defaultSearchField(), int(Settings::EnumDefaultSearchField::Query));
        QCOMPARE(Settings::defaultSearchPathFilter(), QStringLiteral("Reports"));

        // A removed filter stays chosen as unavailable until the user changes it.
        QVERIFY(SavedPathFilters::save(KSharedConfig::openConfig()->group(QStringLiteral("SavedPathFilters")), {}, &error));
        {
            SettingsDialog dialog(&places, nullptr);
            auto *filter = dialog.findChild<SavedPathFilterCombo *>(QStringLiteral("kcfg_DefaultSearchPathFilter"));
            QVERIFY(filter);
            QCOMPARE(filter->filterName(), QStringLiteral("Reports"));
            QVERIFY(filter->currentText().contains(QStringLiteral("Unavailable")));
            dialog.reject();
        }
        Settings::setDefaultSearchScope(Settings::EnumDefaultSearchScope::CurrentFolder);
        Settings::setDefaultSearchField(Settings::EnumDefaultSearchField::FileName);
        Settings::self()->save();
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

    void searchResultAndHistorySettingsPersist()
    {
        KFilePlacesModel places;
        auto history = KSharedConfig::openStateConfig()->group(QStringLiteral("SearchHistory"));
        SearchHistory::save(history, {QStringLiteral("alpha"), QStringLiteral("beta")});
        QVERIFY(history.sync());
        const auto restore = qScopeGuard([] {
            Settings::setDefaultSearchResultPresentation(Settings::EnumDefaultSearchResultPresentation::FullPaths);
            Settings::setRetainSearchHistory(true);
            Settings::self()->save();
        });
        {
            SettingsDialog dialog(&places, nullptr);
            auto *presentation = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultSearchResultPresentation"));
            auto *retain = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RetainSearchHistory"));
            auto *clear = dialog.findChild<QPushButton *>(QStringLiteral("clearSearchHistory"));
            QVERIFY(presentation && retain && clear);
            QCOMPARE(presentation->currentIndex(), int(Settings::EnumDefaultSearchResultPresentation::FullPaths));
            QVERIFY(retain->isChecked());
            QVERIFY(clear->isEnabled());
            // Clearing takes effect at once, even when the dialog is cancelled.
            clear->click();
            QVERIFY(!clear->isEnabled());
            presentation->setCurrentIndex(Settings::EnumDefaultSearchResultPresentation::NamesAndFolders);
            retain->setChecked(false);
            dialog.reject();
        }
        QVERIFY(SearchHistory::load(KSharedConfig::openStateConfig()->group(QStringLiteral("SearchHistory"))).isEmpty());
        QCOMPARE(Settings::defaultSearchResultPresentation(), int(Settings::EnumDefaultSearchResultPresentation::FullPaths));
        QVERIFY(Settings::retainSearchHistory());
        {
            SettingsDialog dialog(&places, nullptr);
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("clearSearchHistory"))->isEnabled());
            dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DefaultSearchResultPresentation"))
                ->setCurrentIndex(Settings::EnumDefaultSearchResultPresentation::NamesAndFolders);
            dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RetainSearchHistory"))->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        const auto search = KSharedConfig::openConfig()->group(QStringLiteral("Search"));
        QCOMPARE(search.readEntry("DefaultSearchResultPresentation", QString()), QStringLiteral("NamesAndFolders"));
        QVERIFY(!search.readEntry("RetainSearchHistory", true));
    }

    void toolbarSearchFieldSettingsUseTheMainToolbar()
    {
        const auto config = KSharedConfig::openConfig();
        KConfigGroup customization = config->group(QStringLiteral("ToolbarCustomization"));
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto backup = KSharedConfig::openConfig(root.filePath(QStringLiteral("toolbar-backup")));
        KConfigGroup savedCustomization = backup->group(QStringLiteral("ToolbarCustomization"));
        customization.copyTo(&savedCustomization);
        customization.deleteGroup();
        const auto restore = qScopeGuard([&] {
            customization.deleteGroup();
            savedCustomization.copyTo(&customization);
            config->sync();
            Settings::setToolbarSearchOpensPanel(true);
            Settings::self()->save();
        });
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *places = window.findChild<KFilePlacesModel *>();
        QAction *field = window.actionCollection()->action(QStringLiteral("search_toolbar"));
        QAction *searchButton = window.actionCollection()->action(QStringLiteral("search_folder"));
        QVERIFY(places && field && searchButton);
        QVERIFY(!window.toolBar()->actions().contains(field));
        {
            SettingsDialog dialog(places, &window);
            auto *show = dialog.findChild<QCheckBox *>(QStringLiteral("searchToolbarField"));
            auto *opens = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ToolbarSearchOpensPanel"));
            QVERIFY(show && opens);
            QVERIFY(show->isEnabled());
            QVERIFY(!show->isChecked());
            QVERIFY(opens->isChecked());
            show->setChecked(true);
            opens->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QList<QAction *> entries = window.toolBar()->actions();
        QVERIFY(entries.contains(field));
        QCOMPARE(entries.indexOf(field) + 1, entries.indexOf(searchButton));
        QVERIFY(!Settings::toolbarSearchOpensPanel());
        QTRY_VERIFY(customization.readEntry("Actions", QStringList()).contains(QStringLiteral("action:search_toolbar")));
        {
            SettingsDialog dialog(places, &window);
            auto *show = dialog.findChild<QCheckBox *>(QStringLiteral("searchToolbarField"));
            QVERIFY(show->isChecked());
            show->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QVERIFY(!window.toolBar()->actions().contains(field));
        QTRY_VERIFY(!customization.readEntry("Actions", QStringList()).contains(QStringLiteral("action:search_toolbar")));

        // Without a main window the option can't apply.
        KFilePlacesModel model;
        SettingsDialog detached(&model, nullptr);
        QVERIFY(!detached.findChild<QCheckBox *>(QStringLiteral("searchToolbarField"))->isEnabled());
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

    void workspaceDefaultBehaviorsPersist()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);
            auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_WorkspaceDefaultBehaviorMode"));
            auto *location = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceDefaultLocationPresentation"));
            auto *search = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceDefaultSearchScope"));
            auto *layout = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceDefaultLayout"));
            auto *panes = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceDefaultPaneTabRestoration"));
            auto *appearance = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceDefaultAppearance"));
            auto *yazi = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_WorkspaceDefaultYaziMode"));
            QVERIFY(mode && location && search && layout && panes && appearance && yazi);
            QCOMPARE(mode->currentIndex(), int(Settings::EnumWorkspaceDefaultBehaviorMode::All));
            QVERIFY(location->isChecked() && search->isChecked() && layout->isChecked() && panes->isChecked() && appearance->isChecked() && yazi->isChecked());
            QVERIFY(!search->isEnabledTo(&dialog));
            QVERIFY(!appearance->isEnabledTo(&dialog));
            QVERIFY(!yazi->isEnabledTo(&dialog));
            mode->setCurrentIndex(Settings::EnumWorkspaceDefaultBehaviorMode::Selected);
            QVERIFY(search->isEnabledTo(&dialog));
            QVERIFY(appearance->isEnabledTo(&dialog));
            QVERIFY(yazi->isEnabledTo(&dialog));
            search->setChecked(false);
            layout->setChecked(false);
            appearance->setChecked(false);
            yazi->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QCOMPARE(Settings::workspaceDefaultBehaviorMode(), Settings::EnumWorkspaceDefaultBehaviorMode::Selected);
        QVERIFY(Settings::workspaceDefaultLocationPresentation());
        QVERIFY(!Settings::workspaceDefaultSearchScope());
        QVERIFY(!Settings::workspaceDefaultLayout());
        QVERIFY(Settings::workspaceDefaultPaneTabRestoration());
        QVERIFY(!Settings::workspaceDefaultAppearance());
        QVERIFY(!Settings::workspaceDefaultYaziMode());
        // New workspaces take the default.
        QVERIFY(!WorkspaceRestore::defaultBehaviorSelection().yaziMode.value_or(true));
    }

    void yaziActivationSettings()
    {
        KFilePlacesModel places;
        {
            SettingsDialog dialog(&places, nullptr);

            // The Yazi Mode page follows the View page, with Activation as its first tab.
            auto *pages = dialog.findChild<KPageWidget *>();
            QVERIFY(pages);
            QStringList titles;
            for (int row = 0; row < pages->model()->rowCount(); ++row) {
                titles.append(pages->model()->index(row, 0).data(Qt::DisplayRole).toString());
            }
            QVERIFY(titles.contains(QStringLiteral("Yazi Mode")));
            QCOMPARE(titles.indexOf(QStringLiteral("Yazi Mode")), titles.indexOf(QStringLiteral("View")) + 1);
            auto *page = dialog.findChild<QWidget *>(QStringLiteral("yaziActivationPage"));
            QVERIFY(page);
            auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
            QVERIFY(tabs);
            QCOMPARE(tabs->indexOf(page), 0);
            QCOMPARE(tabs->tabText(0), QStringLiteral("Activation"));

            auto *leftDefault = page->findChild<QCheckBox *>(QStringLiteral("kcfg_YaziLeftPaneDefault"));
            auto *rightDefault = page->findChild<QCheckBox *>(QStringLiteral("kcfg_YaziRightPaneDefault"));
            auto *leftRestore = page->findChild<QCheckBox *>(QStringLiteral("kcfg_YaziLeftPaneRestore"));
            auto *rightRestore = page->findChild<QCheckBox *>(QStringLiteral("kcfg_YaziRightPaneRestore"));
            auto *notice = page->findChild<QLabel *>(QStringLiteral("yaziActivationNotice"));
            QVERIFY(leftDefault && rightDefault && leftRestore && rightRestore && notice);
            QVERIFY(notice->wordWrap());
            QVERIFY(notice->text().contains(QStringLiteral("workspace")));
            for (QCheckBox *check : {leftDefault, leftRestore}) {
                auto *group = qobject_cast<QGroupBox *>(check->parentWidget());
                QVERIFY(group);
                QCOMPARE(group->title(), QStringLiteral("First Pane"));
            }
            for (QCheckBox *check : {rightDefault, rightRestore}) {
                auto *group = qobject_cast<QGroupBox *>(check->parentWidget());
                QVERIFY(group);
                QCOMPARE(group->title(), QStringLiteral("Second Pane"));
            }

            // The mode is off by default, and the last session's mode is restored.
            QVERIFY(!leftDefault->isChecked());
            QVERIFY(!rightDefault->isChecked());
            QVERIFY(leftRestore->isChecked());
            QVERIFY(rightRestore->isChecked());

            leftDefault->setChecked(true);
            rightRestore->setChecked(false);
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QVERIFY(Settings::yaziLeftPaneDefault());
        QVERIFY(!Settings::yaziRightPaneDefault());
        QVERIFY(Settings::yaziLeftPaneRestore());
        QVERIFY(!Settings::yaziRightPaneRestore());

        SettingsDialog reopened(&places, nullptr);
        QVERIFY(reopened.findChild<QCheckBox *>(QStringLiteral("kcfg_YaziLeftPaneDefault"))->isChecked());
        QVERIFY(!reopened.findChild<QCheckBox *>(QStringLiteral("kcfg_YaziRightPaneRestore"))->isChecked());
    }

    void yaziColumnsSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        Settings::self()->setDefaults();
        Settings::self()->save();
        KFilePlacesModel places;
        const auto spinsOf = [](QWidget *editor) {
            return std::array<QSpinBox *, 3>{editor->findChild<QSpinBox *>(QStringLiteral("yaziParentRatio")),
                                             editor->findChild<QSpinBox *>(QStringLiteral("yaziCurrentRatio")),
                                             editor->findChild<QSpinBox *>(QStringLiteral("yaziPreviewRatio"))};
        };
        const auto valuesOf = [&spinsOf](QWidget *editor) {
            const auto spins = spinsOf(editor);
            return QList<int>{spins[0]->value(), spins[1]->value(), spins[2]->value()};
        };
        // Whether the dialog would leave YaziColumnRatios as it is, compared as KConfigDialogManager does.
        const auto unchanged = [](YaziColumnRatiosEditor *editor) {
            return Settings::self()->findItem(QStringLiteral("YaziColumnRatios"))->isEqual(editor->property("ratios"));
        };
        {
            SettingsDialog dialog(&places, nullptr);

            // Columns is the Yazi Mode page's second tab.
            auto *page = dialog.findChild<QWidget *>(QStringLiteral("yaziColumnsPage"));
            QVERIFY(page);
            auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
            QVERIFY(tabs);
            QCOMPARE(tabs->indexOf(page), 1);
            QCOMPARE(tabs->tabText(1), QStringLiteral("Columns"));
            QCOMPARE(tabs->tabText(0), QStringLiteral("Activation"));

            // The preview checkbox moved here from Interface › Previews; each setting has one widget.
            for (const QString &entry : {QStringLiteral("YaziParentColumnVisible"), QStringLiteral("YaziPreviewVisible"), QStringLiteral("YaziColumnRatios")}) {
                QCOMPARE(dialog.findChildren<QWidget *>(QStringLiteral("kcfg_") + entry).size(), 1);
                QVERIFY2(page->findChild<QWidget *>(QStringLiteral("kcfg_") + entry), qPrintable(entry));
            }
            auto *parent = page->findChild<QCheckBox *>(QStringLiteral("kcfg_YaziParentColumnVisible"));
            auto *preview = page->findChild<QCheckBox *>(QStringLiteral("kcfg_YaziPreviewVisible"));
            auto *editor = page->findChild<YaziColumnRatiosEditor *>(QStringLiteral("kcfg_YaziColumnRatios"));
            QVERIFY(parent && preview && editor);
            QVERIFY(parent->isChecked());
            QVERIFY(preview->isChecked());
            for (QSpinBox *spin : spinsOf(editor)) {
                QVERIFY(spin);
                QCOMPARE(spin->minimum(), 1);
                QCOMPARE(spin->maximum(), YaziMode::MaximumRatio);
            }
            QCOMPARE(valuesOf(editor), QList<int>({1, 3, 2}));
            auto *shares = editor->findChild<QLabel *>(QStringLiteral("yaziColumnShares"));
            QVERIFY(shares);
            for (const char *share : {"17%", "50%", "33%", "60%", "40%"}) {
                QVERIFY2(shares->text().contains(QLatin1String(share)), qPrintable(shares->text()));
            }
            for (const char *name : {"yaziShownColumnsNotice", "yaziColumnWidthsNotice", "yaziNarrowPanesNotice"}) {
                auto *notice = page->findChild<QLabel *>(QLatin1String(name));
                QVERIFY2(notice && notice->wordWrap(), name);
            }
            const QString narrow = page->findChild<QLabel *>(QStringLiteral("yaziNarrowPanesNotice"))->text();
            QVERIFY(narrow.contains(QString::number(YaziMode::ThreeColumnWidth)));
            QVERIFY(narrow.contains(QString::number(YaziMode::TwoColumnWidth)));
            QVERIFY(narrow.contains(QStringLiteral("other file pane")));

            // Editing the widths changes the setting when applied.
            QVERIFY(unchanged(editor));
            const auto spins = spinsOf(editor);
            spins[0]->setValue(2);
            spins[1]->setValue(5);
            spins[2]->setValue(3);
            QCOMPARE(editor->ratios(), QList<int>({2, 5, 3}));
            QVERIFY(shares->text().contains(QStringLiteral("20%")));
            QVERIFY(!unchanged(editor));
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            parent->setChecked(false);
            preview->setChecked(false);
            dialog.button(QDialogButtonBox::Apply)->click();
            QCOMPARE(Settings::yaziColumnRatios(), QList<int>({2, 5, 3}));
            QVERIFY(!Settings::yaziParentColumnVisible());
            QVERIFY(!Settings::yaziPreviewVisible());

            // Restore Default Widths shows 1 : 3 : 2 without saving it.
            editor->findChild<QPushButton *>(QStringLiteral("yaziRestoreColumnWidths"))->click();
            QCOMPARE(valuesOf(editor), QList<int>({1, 3, 2}));
            QCOMPARE(Settings::yaziColumnRatios(), QList<int>({2, 5, 3}));
            QVERIFY(!unchanged(editor));
            spins[0]->setValue(2);
            spins[1]->setValue(5);
            spins[2]->setValue(3);
            QVERIFY(unchanged(editor));

            // Widths saved by dragging a column's edge while the dialog is open are shown, unless edited here.
            Settings::setYaziColumnRatios({240, 700, 460});
            Settings::self()->save();
            QCOMPARE(valuesOf(editor), QList<int>({240, 700, 460}));
            QVERIFY(unchanged(editor));
            spins[0]->setValue(300);
            Settings::setYaziColumnRatios({250, 700, 450});
            Settings::self()->save();
            QCOMPARE(valuesOf(editor), QList<int>({300, 700, 460}));

            // Defaults restores every column setting.
            QTRY_VERIFY(dialog.button(QDialogButtonBox::RestoreDefaults)->isEnabled());
            dialog.button(QDialogButtonBox::RestoreDefaults)->click();
            QVERIFY(parent->isChecked());
            QVERIFY(preview->isChecked());
            QCOMPARE(valuesOf(editor), QList<int>({1, 3, 2}));
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::yaziColumnRatios(), QList<int>({1, 3, 2}));
        QVERIFY(Settings::yaziParentColumnVisible());
        QVERIFY(Settings::yaziPreviewVisible());

        // The dialog shows the saved widths, such as dragged pixel widths, and an invalid value as the defaults without changing it.
        Settings::setYaziColumnRatios({240, 700, 460});
        Settings::self()->save();
        {
            SettingsDialog dialog(&places, nullptr);
            auto *editor = dialog.findChild<YaziColumnRatiosEditor *>(QStringLiteral("kcfg_YaziColumnRatios"));
            QCOMPARE(valuesOf(editor), QList<int>({240, 700, 460}));
            QVERIFY(unchanged(editor));
        }
        Settings::setYaziColumnRatios({1, 0, 2});
        Settings::self()->save();
        {
            SettingsDialog dialog(&places, nullptr);
            auto *editor = dialog.findChild<YaziColumnRatiosEditor *>(QStringLiteral("kcfg_YaziColumnRatios"));
            QCOMPARE(valuesOf(editor), QList<int>({1, 3, 2}));
            QCOMPARE(editor->ratios(), QList<int>({1, 0, 2}));
            QVERIFY(unchanged(editor));
            spinsOf(editor)[2]->setValue(4);
            QCOMPARE(editor->ratios(), QList<int>({1, 3, 4}));
            dialog.button(QDialogButtonBox::Ok)->click();
        }
        QCOMPARE(Settings::yaziColumnRatios(), QList<int>({1, 3, 4}));
    }

    void yaziKeyboardSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        Settings::self()->setDefaults();
        Settings::self()->save();
        KFilePlacesModel places;
        // The commands that the tree lists, by identifier, with their keys.
        const auto rowsOf = [](QTreeWidget *tree) {
            QMap<QString, QString> rows;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                rows.insert(tree->topLevelItem(i)->data(0, Qt::UserRole).toString(), tree->topLevelItem(i)->text(0));
            }
            return rows;
        };
        {
            SettingsDialog dialog(&places, nullptr);
            int asked = 0;
            dialog.setYaziKeyConfirmation([&asked](const QString &, const QStringList &) {
                ++asked;
                return false;
            });

            // Keyboard is the Yazi Mode page's third tab.
            auto *page = dialog.findChild<QWidget *>(QStringLiteral("yaziKeyboardPage"));
            QVERIFY(page);
            auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
            QVERIFY(tabs);
            QCOMPARE(tabs->indexOf(page), 2);
            QCOMPARE(tabs->tabText(2), QStringLiteral("Keyboard"));
            QCOMPARE(tabs->tabText(1), QStringLiteral("Columns"));

            QCOMPARE(dialog.findChildren<QWidget *>(QStringLiteral("kcfg_YaziKeyPreset")).size(), 1);
            auto *preset = page->findChild<QComboBox *>(QStringLiteral("kcfg_YaziKeyPreset"));
            auto *bindings = page->findChild<QTreeWidget *>(QStringLiteral("yaziKeyBindings"));
            auto *counts = page->findChild<QLabel *>(QStringLiteral("yaziKeyCountsNotice"));
            auto *summary = page->findChild<QLabel *>(QStringLiteral("yaziKeyConflictSummary"));
            auto *conflicts = page->findChild<QListWidget *>(QStringLiteral("yaziKeyConflicts"));
            auto *configure = page->findChild<QPushButton *>(QStringLiteral("yaziConfigureShortcuts"));
            QVERIFY(preset && bindings && counts && summary && conflicts && configure);
            QCOMPARE(preset->count(), 2);
            QCOMPARE(preset->itemText(Settings::EnumYaziKeyPreset::Standard), QStringLiteral("Standard keys"));
            QCOMPARE(preset->itemText(Settings::EnumYaziKeyPreset::Vim), QStringLiteral("Yazi-style keys (Vim-style movement and multi-key actions)"));
            QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Standard));
            for (const char *name : {"yaziKeyPresetNotice", "yaziKeyConflictsNotice", "yaziKeyboardAccessibilityNotice"}) {
                auto *notice = page->findChild<QLabel *>(QLatin1String(name));
                QVERIFY2(notice && notice->wordWrap(), name);
            }
            const QString accessibility = page->findChild<QLabel *>(QStringLiteral("yaziKeyboardAccessibilityNotice"))->text();
            for (const char *part : {"terminal", "Tab", "Escape", "search and filter fields"}) {
                QVERIFY2(accessibility.contains(QLatin1String(part)), part);
            }
            // Without a window there are no window shortcuts to configure or shadow.
            QVERIFY(!configure->isEnabled());

            // The standard keys, one row per command.
            QMap<QString, QString> rows = rowsOf(bindings);
            QCOMPARE(bindings->columnCount(), 2);
            QCOMPARE(rows.size(), bindings->topLevelItemCount());
            QCOMPARE(rows.value(QStringLiteral("move-down")), QKeySequence(Qt::Key_Down).toString(QKeySequence::NativeText));
            QVERIFY(!rows.contains(QStringLiteral("trash")));
            QCOMPARE(bindings->findItems(QStringLiteral("Move down"), Qt::MatchExactly, 1).size(), 1);
            QVERIFY(counts->isHidden());
            QVERIFY(summary->text().startsWith(QStringLiteral("No conflicts")));
            QCOMPARE(conflicts->count(), 0);
            QVERIFY(conflicts->isHidden());

            // The Vim-style keys add letters, sequences and counts.
            choosePreset(preset, Settings::EnumYaziKeyPreset::Vim);
            rows = rowsOf(bindings);
            QCOMPARE(rows.value(QStringLiteral("move-down")), QKeySequence(Qt::Key_Down).toString(QKeySequence::NativeText) + QStringLiteral(", j"));
            QCOMPARE(rows.value(QStringLiteral("trash")), QStringLiteral("dd"));
            QCOMPARE(rows.value(QStringLiteral("move-first")), QKeySequence(Qt::Key_Home).toString(QKeySequence::NativeText) + QStringLiteral(", gg"));
            QVERIFY(!counts->isHidden());
            QVERIFY(summary->text().startsWith(QStringLiteral("No conflicts")));

            // Without conflicts the preset applies without asking.
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
            QCOMPARE(asked, 0);
            QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Vim));

            // The View menu's preset, saved while the dialog is open, is shown unless changed here.
            Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Standard);
            Settings::self()->save();
            QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Standard));
            QVERIFY(counts->isHidden());
            choosePreset(preset, Settings::EnumYaziKeyPreset::Vim);
            Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Standard);
            Settings::self()->save();
            QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Vim));

            // Defaults chooses the standard keys.
            QTRY_VERIFY(dialog.button(QDialogButtonBox::RestoreDefaults)->isEnabled());
            dialog.button(QDialogButtonBox::RestoreDefaults)->click();
            QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Standard));
        }
    }

    void yaziKeyPresetConflictsAskBeforeApplying()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        Settings::self()->setDefaults();
        Settings::self()->save();
        MainWindow window;
        window.show();
        QAction *vimKeys = window.actionCollection()->action(QStringLiteral("toggle_yazi_vim_keys"));
        QAction *gallery = window.actionCollection()->action(QStringLiteral("open_gallery"));
        QVERIFY(vimKeys && gallery);
        QVERIFY(!vimKeys->isChecked());
        QCOMPARE(gallery->shortcut(), QKeySequence(Qt::Key_Space));
        window.actionCollection()->action(QStringLiteral("options_configure"))->trigger();
        QPointer<SettingsDialog> dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        int asked = 0;
        bool answer = false;
        QString question;
        QStringList listed;
        dialog->setYaziKeyConfirmation([&](const QString &text, const QStringList &conflicts) {
            ++asked;
            question = text;
            listed = conflicts;
            return answer;
        });
        auto *preset = dialog->findChild<QComboBox *>(QStringLiteral("kcfg_YaziKeyPreset"));
        auto *summary = dialog->findChild<QLabel *>(QStringLiteral("yaziKeyConflictSummary"));
        auto *conflicts = dialog->findChild<QListWidget *>(QStringLiteral("yaziKeyConflicts"));
        auto *configure = dialog->findChild<QPushButton *>(QStringLiteral("yaziConfigureShortcuts"));
        QVERIFY(preset && summary && conflicts && configure);
        QVERIFY(configure->isEnabled());
        const auto shown = [conflicts] {
            QStringList texts;
            for (int i = 0; i < conflicts->count(); ++i) {
                texts.append(conflicts->item(i)->text());
            }
            return texts;
        };
        const auto mentions = [](const QStringList &texts, const QString &part) {
            return std::any_of(texts.cbegin(), texts.cend(), [&part](const QString &text) {
                return text.contains(part);
            });
        };

        // The standard keys shadow none of the window's default shortcuts.
        QCOMPARE(conflicts->count(), 0);
        QVERIFY(summary->text().startsWith(QStringLiteral("No conflicts")));

        // The Vim-style keys take Space from Open Gallery and Ctrl+D from Add to Favorites.
        choosePreset(preset, Settings::EnumYaziKeyPreset::Vim);
        QStringList vimConflicts = shown();
        QVERIFY(!vimConflicts.isEmpty());
        QVERIFY2(mentions(vimConflicts, QStringLiteral("“Open Gallery”")), qPrintable(vimConflicts.join(QLatin1Char('\n'))));
        QVERIFY2(mentions(vimConflicts, QStringLiteral("“Add to Favorites”")), qPrintable(vimConflicts.join(QLatin1Char('\n'))));
        QVERIFY(mentions(vimConflicts, QKeySequence(Qt::Key_Space).toString(QKeySequence::NativeText)));
        QVERIFY(!conflicts->isHidden());
        QVERIFY(summary->text().contains(QString::number(vimConflicts.size())));

        // A shortcut changed in Configure Keyboard Shortcuts no longer conflicts once saved.
        gallery->setShortcut(QKeySequence(Qt::Key_F12));
        Q_EMIT window.guiFactory()->shortcutsSaved();
        QVERIFY(!mentions(shown(), QStringLiteral("“Open Gallery”")));
        QCOMPARE(shown().size(), vimConflicts.size() - 1);
        gallery->setShortcut(QKeySequence(Qt::Key_Space));
        Q_EMIT window.guiFactory()->shortcutsSaved();
        QCOMPARE(shown(), vimConflicts);

        // Declining keeps the standard keys and Preferences open.
        QTRY_VERIFY(dialog->button(QDialogButtonBox::Apply)->isEnabled());
        dialog->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(asked, 1);
        QCOMPARE(listed, vimConflicts);
        QVERIFY(question.contains(QStringLiteral("Yazi-style keys")));
        QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Standard));
        QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Standard));
        QCOMPARE(conflicts->count(), 0);
        QVERIFY(!vimKeys->isChecked());
        QVERIFY(!dialog->button(QDialogButtonBox::Apply)->isEnabled());

        choosePreset(preset, Settings::EnumYaziKeyPreset::Vim);
        QVERIFY(dialog->button(QDialogButtonBox::Apply)->isEnabled());
        dialog->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(asked, 2);
        QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Standard));
        QVERIFY(dialog && dialog->isVisible());

        // Accepting on OK asks once, applies the preset and updates the View menu.
        answer = true;
        choosePreset(preset, Settings::EnumYaziKeyPreset::Vim);
        dialog->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(asked, 3);
        QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Vim));
        QVERIFY(vimKeys->isChecked());
        QTRY_VERIFY(!dialog);

        // The View menu switches the preset shown in a reopened dialog, which doesn't ask again.
        window.actionCollection()->action(QStringLiteral("options_configure"))->trigger();
        dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        dialog->setYaziKeyConfirmation([&asked](const QString &, const QStringList &) {
            ++asked;
            return true;
        });
        preset = dialog->findChild<QComboBox *>(QStringLiteral("kcfg_YaziKeyPreset"));
        QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Vim));
        QVERIFY(!dialog->button(QDialogButtonBox::Apply)->isEnabled());
        vimKeys->trigger();
        QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Standard));
        QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Standard));
        QVERIFY(!dialog->button(QDialogButtonBox::Apply)->isEnabled());
        vimKeys->trigger();
        QCOMPARE(preset->currentIndex(), int(Settings::EnumYaziKeyPreset::Vim));
        QVERIFY(!dialog->button(QDialogButtonBox::Apply)->isEnabled());
        dialog->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(asked, 3);
        QTRY_VERIFY(!dialog);
    }

    void yaziKeyboardOpensTheShortcutsEditor()
    {
        MainWindow window;
        SettingsDialog dialog(window.findChild<KFilePlacesModel *>(), &window);
        auto *configure = dialog.findChild<QPushButton *>(QStringLiteral("yaziConfigureShortcuts"));
        QVERIFY(configure && configure->isEnabled());

        window.show();
        dialog.show();
        bool editorOpened = false;
        QTimer closeEditor(&window);
        closeEditor.setInterval(10);
        connect(&closeEditor, &QTimer::timeout, &window, [&] {
            if (auto *editor = window.findChild<KShortcutsDialog *>()) {
                editorOpened = true;
                closeEditor.stop();
                editor->reject();
            }
        });
        closeEditor.start();
        QTest::mouseClick(configure, Qt::LeftButton);
        QTRY_VERIFY(editorOpened);
        // Preferences stays open to show the updated conflicts.
        QVERIFY(dialog.isVisible());
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

    void searchFoldersAndExclusionsAreEditable()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *folders = dialog.findChild<FolderListEditor *>(QStringLiteral("kcfg_SearchFolders"));
        auto *exclusions = dialog.findChild<FolderListEditor *>(QStringLiteral("kcfg_SearchExclusions"));
        auto *doubleShift = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_DoubleShiftSearch"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("doubleShiftStatus"));
        QVERIFY(folders && exclusions && doubleShift && status);
        QVERIFY(!status->text().isEmpty());
        QVERIFY(doubleShift->isChecked());
        auto *remove = folders->findChild<QPushButton *>(QStringLiteral("removeFolder"));
        QVERIFY(remove && !remove->isEnabled());

        QVERIFY(!folders->addFolder(QStringLiteral("relative")));
        QVERIFY(folders->addFolder(root.path() + QStringLiteral("/")));
        QVERIFY(!folders->addFolder(root.path()));
        QVERIFY(folders->addFolder(root.filePath(QStringLiteral("other"))));
        QVERIFY(exclusions->addFolder(root.filePath(QStringLiteral("cache"))));
        QCOMPARE(folders->folders(), (QStringList{root.path(), root.filePath(QStringLiteral("other"))}));
        QVERIFY(remove->isEnabled());
        remove->click();
        QCOMPARE(folders->folders(), QStringList{root.path()});
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::searchFolders(), QStringList{root.path()});
        QCOMPARE(Settings::searchExclusions(), QStringList{root.filePath(QStringLiteral("cache"))});

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(folders->folders().isEmpty());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::searchFolders().isEmpty());
        QVERIFY(Settings::searchExclusions().isEmpty());
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

    void editsFolderColourRules()
    {
        FolderColorRuleDialog ruleDialog;
        auto *ok = ruleDialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        auto *match = ruleDialog.findChild<QComboBox *>();
        auto *pattern = ruleDialog.findChild<QLineEdit *>();
        QVERIFY(ok && match && pattern);
        QVERIFY(!ok->isEnabled());
        pattern->setText(QStringLiteral("*-archive"));
        QVERIFY(ok->isEnabled());
        match->setCurrentIndex(match->findData(int(FolderColorRules::Match::Path)));
        QVERIFY2(!ok->isEnabled(), "path rules need an absolute or home-relative path");
        pattern->setText(QStringLiteral("~/Work"));
        QVERIFY(ok->isEnabled());
        ruleDialog.findChild<QCheckBox *>()->setChecked(true);
        const std::optional<FolderColorRules::Rule> rule = ruleDialog.rule();
        QVERIFY(rule);
        QCOMPARE(rule->match, FolderColorRules::Match::Path);
        QVERIFY(rule->subfolders);

        const QString first = FolderColorRules::entry({FolderColorRules::Match::Name, QStringLiteral("docs"), Qt::red, false});
        const QString second = FolderColorRules::entry(*rule);
        const auto restore = qScopeGuard([] {
            Settings::setFolderColorRules({});
            Settings::self()->save();
        });
        KFilePlacesModel places;
        SettingsDialog dialog(&places, nullptr);
        auto *editor = dialog.findChild<FolderColorRuleEditor *>(QStringLiteral("kcfg_FolderColorRules"));
        QVERIFY(editor);
        QSignalSpy changed(editor, &FolderColorRuleEditor::rulesChanged);
        editor->setRules({first, QStringLiteral("invalid"), second, first});
        QCOMPARE(editor->rules(), QStringList({first, second}));
        editor->moveRule(1, -1);
        QCOMPARE(editor->rules(), QStringList({second, first}));
        editor->moveRule(0, -1);
        QCOMPARE(editor->rules(), QStringList({second, first}));
        QVERIFY(changed.count() >= 2);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Ok)->click();
        QCOMPARE(Settings::folderColorRules(), QStringList({second, first}));
    }

    void cleanupTestCase()
    {
        const auto config = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
        config->deleteGroup(QStringLiteral("Confirmations"));
        config->deleteGroup(QStringLiteral("PreviewSettings"));
        config->sync();
    }

private:
    /// Chooses @p index in @p preset as the user does, which KConfigDialogManager notices.
    static void choosePreset(QComboBox *preset, int index)
    {
        preset->setCurrentIndex(index);
        Q_EMIT preset->activated(index);
    }
};

QTEST_MAIN(SettingsDialogTest)

#include "settingsdialogtest.moc"
