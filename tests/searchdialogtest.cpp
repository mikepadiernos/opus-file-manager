// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchdialog.h"

#include "mainwindow.h"

#include <KActionCollection>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

class SearchDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
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
};

QTEST_MAIN(SearchDialogTest)
#include "searchdialogtest.moc"
