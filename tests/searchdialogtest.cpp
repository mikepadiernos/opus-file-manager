// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchdialog.h"

#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "savedpathfilters.h"
#include "settings.h"

#include <KActionCollection>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class SearchDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void usesSavedSearchDefaults()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        auto defaults = config->group(QStringLiteral("Search"));
        defaults.writeEntry("SearchIndexedByDefault", true);
        defaults.writeEntry("SearchContentsByDefault", true);
        QVERIFY(defaults.sync());
        SearchDialog indexed(QUrl::fromLocalFile(root.path()), nullptr, config);
        auto *scope = indexed.findChild<QComboBox *>(QStringLiteral("searchScope"));
        auto *field = indexed.findChild<QComboBox *>(QStringLiteral("searchField"));
        QVERIFY(scope && field);
        QCOMPARE(scope->currentIndex(), 1);
        QCOMPARE(field->currentIndex(), 1);
        QVERIFY(field->currentText().contains(QStringLiteral("Indexed")));
        QVERIFY(indexed.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("Baloo")));
        defaults.writeEntry("SearchIndexedByDefault", false);
        defaults.writeEntry("SearchContentsByDefault", false);
        QVERIFY(defaults.sync());
        SearchDialog live(QUrl::fromLocalFile(root.path()), nullptr, config);
        QCOMPARE(live.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 0);
        QCOMPARE(live.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 0);
    }

    void usesApplicationSettings()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        Settings::setSearchIndexedByDefault(true);
        Settings::setSearchContentsByDefault(true);
        Settings::self()->save();
        SearchDialog dialog(QUrl::fromLocalFile(root.path()));
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 1);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 1);
        Settings::setSearchIndexedByDefault(false);
        Settings::setSearchContentsByDefault(false);
        Settings::self()->save();
    }

    void loadsSharedPathFilter()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("parent/excluded")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("parent/deep/nested")));
        for (const QString &name : {QStringLiteral("parent/report.txt"),
                                    QStringLiteral("parent/excluded/report.txt"),
                                    QStringLiteral("parent/deep/nested/report.txt"),
                                    QStringLiteral("parent/other.txt")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchrc")), KConfig::SimpleConfig);
        QString error;
        const PathFilter filter{QStringLiteral("report"), QStringLiteral("parent"), {root.filePath(QStringLiteral("parent/excluded"))}, 1};
        QVERIFY(SavedPathFilters::save(config->group(QStringLiteral("SavedPathFilters")), {{QStringLiteral("Work"), filter}}, &error));
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchFilterPresets"))->currentText(), QStringLiteral("Work"));
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadFilterPreset"))->click();
        QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("searchPathFilter"))->text(), filter.pathText);
        QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("searchParentFilter"))->text(), filter.parentText);
        QCOMPARE(dialog.findChild<QPlainTextEdit *>(QStringLiteral("searchExcludedFolders"))->toPlainText(), filter.excludedFolders.first());
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->value(), 1);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral(".txt"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), root.filePath(QStringLiteral("parent/report.txt")));
        QVERIFY(SavedPathFilters::save(config->group(QStringLiteral("SavedPathFilters")), {}, &error));
        dialog.findChild<QPushButton *>(QStringLiteral("searchLoadFilterPreset"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchFilterPresetsStatus"))->text().contains(QStringLiteral("no longer exists")));
        dialog.findChild<QPushButton *>(QStringLiteral("searchRefreshFilterPresets"))->click();
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("searchLoadFilterPreset"))->isEnabled());
    }

    void searchesAndReportsFailures()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("example.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("matching content"), 16);
        file.close();
        SearchDialog dialog(QUrl::fromLocalFile(root.path()));
        auto *query = dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"));
        auto *field = dialog.findChild<QComboBox *>(QStringLiteral("searchField"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("searchStatus"));
        QVERIFY(query && field && start && results && status);
        start->click();
        QVERIFY(status->text().contains(QStringLiteral("Enter text")));
        query->setText(QStringLiteral("example"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QVERIFY(dialog.findChild<QProgressBar *>(QStringLiteral("searchProgress"))->isHidden());
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), file.fileName());
        results->setCurrentItem(results->topLevelItem(0));
        dialog.findChild<QPushButton *>(QStringLiteral("searchCopy"))->click();
        QCOMPARE(QApplication::clipboard()->text(), QUrl::fromLocalFile(file.fileName()).toString(QUrl::FullyEncoded));
        field->setCurrentIndex(1);
        query->setText(QStringLiteral("content"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), file.fileName());
    }

    void refusesRemoteFolders()
    {
        SearchDialog dialog(QUrl(QStringLiteral("sftp://example.invalid/path")));
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("x"));
        dialog.findChild<QPushButton *>(QStringLiteral("searchStart"))->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("local folders only")));
    }

    void indexedScopeReportsInvalidQueries()
    {
        SearchDialog dialog(QUrl(QStringLiteral("sftp://example.invalid/path")));
        auto *scope = dialog.findChild<QComboBox *>(QStringLiteral("searchScope"));
        auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *errors = dialog.findChild<QTreeWidget *>(QStringLiteral("searchErrors"));
        QVERIFY(scope && start && errors);
        scope->setCurrentIndex(1);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("a\"b"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(errors->topLevelItemCount(), 1);
        QVERIFY(errors->topLevelItem(0)->text(0).contains(QStringLiteral("double quotes")));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("incomplete")));
        dialog.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->setValue(0);
        dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("term"));
        start->click();
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("searchStatus"))->text().contains(QStringLiteral("local current folder")));
    }

    void mainWindowOpensSearch()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        QAction *action = window.actionCollection()->action(QStringLiteral("search_folder"));
        QVERIFY(action);
        action->trigger();
        QVERIFY(window.findChild<SearchDialog *>(QStringLiteral("searchDialog")));
    }

    void revealsResultInActivePaneAndReportsMissingItems()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("sub")));
        const QString filePath = root.filePath(QStringLiteral("sub/find-me.txt"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        window.actionCollection()->action(QStringLiteral("search_folder"))->trigger();
        auto *dialog = window.findChild<SearchDialog *>(QStringLiteral("searchDialog"));
        QVERIFY(dialog);
        auto *query = dialog->findChild<QLineEdit *>(QStringLiteral("searchQuery"));
        auto *start = dialog->findChild<QPushButton *>(QStringLiteral("searchStart"));
        auto *reveal = dialog->findChild<QPushButton *>(QStringLiteral("searchReveal"));
        auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QVERIFY(query && start && reveal && results);
        QVERIFY(!reveal->isEnabled());
        query->setText(QStringLiteral("find-me"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 1);
        results->setCurrentItem(results->topLevelItem(0));
        QVERIFY(reveal->isEnabled());
        Pane *pane = window.activePane();
        reveal->click();
        QTRY_COMPARE(pane->url(), QUrl::fromLocalFile(root.filePath(QStringLiteral("sub"))));
        QTRY_COMPARE(pane->view()->selectedItems().size(), 1);
        QCOMPARE(pane->view()->selectedItems().first().url(), QUrl::fromLocalFile(filePath));
        QSignalSpy status(pane, &Pane::statusMessage);
        pane->view()->setNameFilter(NameFilter(QStringLiteral("not-present"), NameFilter::Scope::FilesAndFolders));
        reveal->click();
        QCOMPARE(status.size(), 1);
        QVERIFY(status.takeFirst().first().toString().contains(QStringLiteral("filtered")));
        pane->view()->setNameFilter(NameFilter());
        pane->view()->setFlat(true);
        reveal->click();
        QCOMPARE(status.size(), 1);
        QVERIFY(status.takeFirst().first().toString().contains(QStringLiteral("flat view")));
        pane->view()->setFlat(false);
        QVERIFY(QFile::remove(filePath));
        reveal->click();
        QCOMPARE(status.size(), 1);
        QVERIFY(status.first().first().toString().contains(QStringLiteral("no longer available")));
    }

    void savesAndRestoresSearchCriteria()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QTemporaryDir configDir;
        QVERIFY(configDir.isValid());
        QFile file(root.filePath(QStringLiteral("result.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("needle"), 6);
        file.close();
        const auto config = KSharedConfig::openConfig(configDir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        {
            SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
            dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("needle"));
            dialog.findChild<QComboBox *>(QStringLiteral("searchField"))->setCurrentIndex(1);
            dialog.findChild<QLineEdit *>(QStringLiteral("searchPathFilter"))->setText(QStringLiteral("result"));
            dialog.findChild<QLineEdit *>(QStringLiteral("searchParentFilter"))->setText(root.path());
            dialog.findChild<QPlainTextEdit *>(QStringLiteral("searchExcludedFolders"))->setPlainText(root.filePath(QStringLiteral("excluded")));
            dialog.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->setValue(0);
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Documents"));
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("searchSave"))->click();
            QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 1);
        }
        auto defaults = config->group(QStringLiteral("Search"));
        defaults.writeEntry("SearchIndexedByDefault", true);
        defaults.writeEntry("SearchContentsByDefault", false);
        QVERIFY(defaults.sync());
        config->reparseConfiguration();
        SearchDialog restored(QUrl::fromLocalFile(QStringLiteral("/")), nullptr, config);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 1);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 1);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 0);
        restored.findChild<QPushButton *>(QStringLiteral("searchLoad"))->click();
        QCOMPARE(restored.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->text(), QStringLiteral("needle"));
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchField"))->currentIndex(), 1);
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchScope"))->currentIndex(), 0);
        QCOMPARE(restored.findChild<QLineEdit *>(QStringLiteral("searchPathFilter"))->text(), QStringLiteral("result"));
        QCOMPARE(restored.findChild<QLineEdit *>(QStringLiteral("searchParentFilter"))->text(), root.path());
        QCOMPARE(restored.findChild<QPlainTextEdit *>(QStringLiteral("searchExcludedFolders"))->toPlainText(), root.filePath(QStringLiteral("excluded")));
        QCOMPARE(restored.findChild<QSpinBox *>(QStringLiteral("searchMaxDepth"))->value(), 0);
        auto *start = restored.findChild<QPushButton *>(QStringLiteral("searchStart"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        auto *results = restored.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), file.fileName());
        auto *deleteSaved = restored.findChild<QPushButton *>(QStringLiteral("searchDelete"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        deleteSaved->click();
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 1);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        deleteSaved->click();
        QCOMPARE(restored.findChild<QComboBox *>(QStringLiteral("searchSaved"))->count(), 0);
        QVERIFY(QFile::exists(file.fileName()));
    }

    void savingFromTwoDialogsPreservesBothSearches()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        SearchDialog first(QUrl::fromLocalFile(root.path()), nullptr, config);
        SearchDialog second(QUrl::fromLocalFile(root.path()), nullptr, config);
        const auto save = [](SearchDialog &dialog, const QString &name) {
            dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("term"));
            QTimer::singleShot(0, [name] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(name);
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("searchSave"))->click();
        };
        save(first, QStringLiteral("One"));
        save(second, QStringLiteral("Two"));
        SearchDialog restored(QUrl::fromLocalFile(root.path()), nullptr, config);
        auto *saved = restored.findChild<QComboBox *>(QStringLiteral("searchSaved"));
        QCOMPARE(saved->count(), 2);
        QCOMPARE(saved->itemText(0), QStringLiteral("One"));
        QCOMPARE(saved->itemText(1), QStringLiteral("Two"));
    }

    void collectsReferencesAcrossSearchesWithoutMovingFiles()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("first")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("second")));
        const QString first = root.filePath(QStringLiteral("first/match-a"));
        const QString second = root.filePath(QStringLiteral("second/match-b"));
        QFile a(first);
        QFile b(second);
        QVERIFY(a.open(QIODevice::WriteOnly));
        a.close();
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.close();
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        for (const QString &folder : {root.filePath(QStringLiteral("first")), root.filePath(QStringLiteral("second"))}) {
            SearchDialog dialog(QUrl::fromLocalFile(folder), nullptr, config);
            dialog.findChild<QLineEdit *>(QStringLiteral("searchQuery"))->setText(QStringLiteral("match"));
            auto *start = dialog.findChild<QPushButton *>(QStringLiteral("searchStart"));
            start->click();
            QTRY_VERIFY(start->isEnabled());
            auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("searchResults"));
            QCOMPARE(results->topLevelItemCount(), 1);
            results->setCurrentItem(results->topLevelItem(0));
            dialog.findChild<QPushButton *>(QStringLiteral("searchCollect"))->click();
        }
        config->reparseConfiguration();
        SearchDialog dialog(QUrl::fromLocalFile(root.path()), nullptr, config);
        dialog.findChild<QTabWidget *>(QStringLiteral("searchResultTabs"))->setCurrentIndex(1);
        auto *collection = dialog.findChild<QTreeWidget *>(QStringLiteral("searchCollection"));
        QCOMPARE(collection->topLevelItemCount(), 2);
        QCOMPARE(collection->topLevelItem(0)->text(0), first);
        QCOMPARE(collection->topLevelItem(1)->text(0), second);
        collection->setCurrentItem(collection->topLevelItem(0));
        QSignalSpy reveal(&dialog, &SearchDialog::revealRequested);
        dialog.findChild<QPushButton *>(QStringLiteral("searchReveal"))->click();
        QCOMPARE(reveal.size(), 1);
        QCOMPARE(reveal.first().first().toUrl(), QUrl::fromLocalFile(first));
        dialog.findChild<QPushButton *>(QStringLiteral("searchUncollect"))->click();
        QCOMPARE(collection->topLevelItemCount(), 1);
        QVERIFY(QFile::exists(first) && QFile::exists(second));
    }
};

QTEST_MAIN(SearchDialogTest)
#include "searchdialogtest.moc"
