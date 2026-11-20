// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicatefinderdialog.h"

#include "settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

namespace
{
bool write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

class DuplicateFinderDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void scansNestedFilesAndSelectsWithoutDeleting()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
        const QString first = dir.filePath(QStringLiteral("a"));
        const QString second = dir.filePath(QStringLiteral("sub/b"));
        QVERIFY(write(first, "data"));
        QVERIFY(write(second, "data"));
        QVERIFY(write(dir.filePath(QStringLiteral("other")), "else"));
        QVERIFY(QFile::link(first, dir.filePath(QStringLiteral("link"))));
        DuplicateFinderDialog dialog(QUrl::fromLocalFile(dir.path()));
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"));
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"));
        auto *copy = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCopy"));
        QVERIFY(cancel && results && copy);
        QTRY_VERIFY(!cancel->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 1);
        auto *group = results->topLevelItem(0);
        QCOMPARE(group->childCount(), 2);
        QStringList paths{group->child(0)->text(0), group->child(1)->text(0)};
        paths.sort();
        QCOMPARE(paths, (QStringList{first, second}));
        auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("duplicateFinderPolicy"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderApplyPolicy"));
        QVERIFY(policy && apply);
        policy->setCurrentIndex(2);
        dialog.findChild<QLineEdit *>(QStringLiteral("duplicateFinderPolicyFolder"))->setText(dir.filePath(QStringLiteral("sub")));
        apply->click();
        int checked = 0;
        for (int i = 0; i < group->childCount(); ++i) {
            if (group->child(i)->checkState(0) == Qt::Checked) {
                QCOMPARE(group->child(i)->text(0), second);
                ++checked;
            }
        }
        QCOMPARE(checked, 1);
        QVERIFY(QFile::remove(first));
        policy->setCurrentIndex(0);
        apply->click();
        QCOMPARE(group->child(0)->checkState(0), Qt::Unchecked);
        QCOMPARE(group->child(1)->checkState(0), Qt::Unchecked);
        QVERIFY(dialog.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderErrors"))->topLevelItemCount() > 0);
        QVERIFY(write(first, "data"));
        group->child(0)->setCheckState(0, Qt::Checked);
        copy->click();
        QCOMPARE(QApplication::clipboard()->text(), QUrl::fromLocalFile(group->child(0)->text(0)).toString(QUrl::FullyEncoded));
        QVERIFY(QFile::exists(first) && QFile::exists(second));
    }

    void appliesConfiguredCriteriaAndSafeSelectionDefaults()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("one")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("two")));
        for (const QString &file : {QStringLiteral("one/a"), QStringLiteral("two/a"), QStringLiteral("one/b"), QStringLiteral("two/b"), QStringLiteral("c")}) {
            QVERIFY(write(dir.filePath(file), "data"));
        }
        const bool previousNames = Settings::duplicateRequireSameFileName();
        const auto previousPolicy = Settings::duplicateSelectionPolicy();
        const auto restore = qScopeGuard([previousNames, previousPolicy] {
            Settings::setDuplicateRequireSameFileName(previousNames);
            Settings::setDuplicateSelectionPolicy(previousPolicy);
        });
        Settings::setDuplicateRequireSameFileName(true);
        for (const auto policy : {Settings::EnumDuplicateSelectionPolicy::KeepNewest,
                                  Settings::EnumDuplicateSelectionPolicy::KeepOldest,
                                  Settings::EnumDuplicateSelectionPolicy::InFolder}) {
            Settings::setDuplicateSelectionPolicy(policy);
            DuplicateFinderDialog dialog(QUrl::fromLocalFile(dir.path()));
            auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"));
            QTRY_VERIFY(!cancel->isEnabled());
            auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"));
            QCOMPARE(results->topLevelItemCount(), 2);
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderCriteria"))->text().contains(QStringLiteral("exact file name")));
            QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("duplicateFinderPolicy"))->currentIndex(), int(policy));
            QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("duplicateFinderPolicyFolder"))->isEnabled(),
                     policy == Settings::EnumDuplicateSelectionPolicy::InFolder);
            dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderApplyPolicy"))->click();
            for (int i = 0; i < results->topLevelItemCount(); ++i) {
                auto *group = results->topLevelItem(i);
                QCOMPARE(group->childCount(), 2);
                const int checked = int(group->child(0)->checkState(0) == Qt::Checked) + int(group->child(1)->checkState(0) == Qt::Checked);
                QCOMPARE(checked, 1);
            }
            if (policy == Settings::EnumDuplicateSelectionPolicy::KeepNewest) {
                QTimer responder;
                responder.setInterval(10);
                connect(&responder, &QTimer::timeout, &dialog, [&dialog] {
                    if (auto *box = dialog.findChild<QMessageBox *>(QStringLiteral("duplicateFinderTrashConfirmation"))) {
                        box->done(QMessageBox::No);
                    }
                });
                responder.start();
                dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderTrash"))->click();
                QTRY_COMPARE(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text(), QStringLiteral("No files were removed."));
                auto *name = dialog.findChild<QCheckBox *>(QStringLiteral("duplicateFinderSameFileName"));
                QVERIFY(name->isChecked());
                name->setChecked(false);
                QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderTrash"))->isEnabled());
                QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderApplyPolicy"))->isEnabled());
                QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text().contains(QStringLiteral("Rescan")));
                dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderRescan"))->click();
                QTRY_VERIFY(!cancel->isEnabled());
                QCOMPARE(results->topLevelItemCount(), 1);
                QCOMPARE(results->topLevelItem(0)->childCount(), 5);
                QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderApplyPolicy"))->isEnabled());
            }
        }
    }

    void copiesUnambiguousUrlsForUnusualNames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("line\nbreak"));
        const QString second = dir.filePath(QStringLiteral("other"));
        QVERIFY(write(first, "same"));
        QVERIFY(write(second, "same"));
        DuplicateFinderDialog dialog(QUrl::fromLocalFile(dir.path()));
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"));
        QTRY_VERIFY(!cancel->isEnabled());
        auto *group = dialog.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"))->topLevelItem(0);
        QVERIFY(group);
        group->child(0)->setCheckState(0, Qt::Checked);
        group->child(1)->setCheckState(0, Qt::Checked);
        dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCopy"))->click();
        const QStringList urls = QApplication::clipboard()->text().split(QLatin1Char('\n'));
        QCOMPARE(urls.size(), 2);
        QVERIFY(urls.contains(QUrl::fromLocalFile(first).toString(QUrl::FullyEncoded)));
        QVERIFY(urls.contains(QUrl::fromLocalFile(second).toString(QUrl::FullyEncoded)));
    }

    void reportsMissingFolderAndCancellation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DuplicateFinderDialog failed(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing"))));
        QTRY_VERIFY(!failed.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->isEnabled());
        QVERIFY(failed.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text().startsWith(QStringLiteral("Could not list")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("missing")));
        QVERIFY(write(dir.filePath(QStringLiteral("missing/a")), "same"));
        QVERIFY(write(dir.filePath(QStringLiteral("missing/b")), "same"));
        failed.findChild<QPushButton *>(QStringLiteral("duplicateFinderRescan"))->click();
        QTRY_VERIFY(!failed.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->isEnabled());
        QCOMPARE(failed.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"))->topLevelItemCount(), 1);
        DuplicateFinderDialog canceled(QUrl::fromLocalFile(dir.path()));
        canceled.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->click();
        QCOMPARE(canceled.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text(), QStringLiteral("Scan canceled; no results are shown."));
        QVERIFY(!canceled.findChild<QPushButton *>(QStringLiteral("duplicateFinderCopy"))->isEnabled());
        canceled.findChild<QPushButton *>(QStringLiteral("duplicateFinderRescan"))->click();
        QTRY_VERIFY(!canceled.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->isEnabled());
        QCOMPARE(canceled.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"))->topLevelItemCount(), 1);
    }

    void confirmsCompleteGroupBeforeRequestingTrash()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("a"));
        const QString second = dir.filePath(QStringLiteral("b"));
        QVERIFY(write(first, "data"));
        QVERIFY(write(second, "data"));
        DuplicateFinderDialog dialog(QUrl::fromLocalFile(dir.path()));
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"));
        QTRY_VERIFY(!cancel->isEnabled());
        auto *group = dialog.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"))->topLevelItem(0);
        QVERIFY(group);
        auto *trash = dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderTrash"));
        QSignalSpy requested(&dialog, &DuplicateFinderDialog::trashRequested);
        trash->click();
        QCOMPARE(requested.size(), 0);
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text(), QStringLiteral("Check at least one file to move to Trash."));
        QTimer responder;
        responder.setInterval(10);
        bool confirm = false;
        bool all = false;
        connect(&responder, &QTimer::timeout, &dialog, [&] {
            auto *box = dialog.findChild<QMessageBox *>(QStringLiteral("duplicateFinderTrashConfirmation"));
            if (!box) {
                return;
            }
            QCOMPARE(box->text().contains(QStringLiteral("All files")), all);
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
            box->done(confirm ? QMessageBox::Yes : QMessageBox::No);
        });
        responder.start();
        group->child(0)->setCheckState(0, Qt::Checked);
        trash->click();
        QTRY_COMPARE(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text(), QStringLiteral("No files were removed."));
        QCOMPARE(requested.size(), 0);
        group->child(1)->setCheckState(0, Qt::Checked);
        all = true;
        trash->click();
        QTRY_COMPARE(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text(), QStringLiteral("No files were removed."));
        QCOMPARE(requested.size(), 0);
        confirm = true;
        trash->click();
        QTRY_COMPARE(requested.size(), 1);
        responder.stop();
        const auto urls = requested.first().first().value<QList<QUrl>>();
        QCOMPARE(urls.size(), 2);
        QVERIFY(urls.contains(QUrl::fromLocalFile(first)));
        QVERIFY(urls.contains(QUrl::fromLocalFile(second)));
        QVERIFY(QFile::exists(first) && QFile::exists(second));
    }

    void rejectsStaleGroupsBeforeRequestingTrash()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("a"));
        const QString second = dir.filePath(QStringLiteral("b"));
        QVERIFY(write(first, "data"));
        QVERIFY(write(second, "data"));
        DuplicateFinderDialog dialog(QUrl::fromLocalFile(dir.path()));
        QTRY_VERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->isEnabled());
        auto *group = dialog.findChild<QTreeWidget *>(QStringLiteral("duplicateFinderResults"))->topLevelItem(0);
        QVERIFY(group);
        group->child(0)->setCheckState(0, Qt::Checked);
        QVERIFY(write(second, "else"));
        QSignalSpy requested(&dialog, &DuplicateFinderDialog::trashRequested);
        dialog.findChild<QPushButton *>(QStringLiteral("duplicateFinderTrash"))->click();
        QTRY_VERIFY(dialog.findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text().startsWith(QStringLiteral("Could not recheck")));
        QCOMPARE(requested.size(), 0);
        QVERIFY(QFile::exists(first) && QFile::exists(second));
    }
};

QTEST_MAIN(DuplicateFinderDialogTest)
#include "duplicatefinderdialogtest.moc"
