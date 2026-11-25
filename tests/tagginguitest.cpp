// SPDX-License-Identifier: GPL-3.0-or-later

#include "librarymovetracker.h"
#include "libraryrefreshdialog.h"
#include "libraryregistry.h"
#include "libraryreviewdialog.h"
#include "libraryscan.h"
#include "tageditdialog.h"
#include "taglibrary.h"
#include "tagmanagerwidget.h"

#include <KColorButton>
#include <KIO/CopyJob>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

class TaggingUiTest : public QObject
{
    Q_OBJECT

    static void write(const QString &path)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
    }

    static void refresh(const QString &root, TagLibrary::ScanChanges *changes, QString *message, bool cancel = false)
    {
        LibraryRefreshDialog dialog(root);
        QSignalSpy finished(&dialog, &LibraryRefreshDialog::refreshFinished);
        QSignalSpy changed(&dialog, &LibraryRefreshDialog::libraryChanged);
        dialog.start();
        if (cancel) {
            dialog.cancel();
        }
        QVERIFY(finished.wait(10000) || finished.count() == 1);
        QVERIFY(!dialog.isRunning());
        QCOMPARE(changed.count(), 1);
        *changes = dialog.changes();
        *message = dialog.message();
    }

    /// Indexes @p root and assigns a tag to the entries at @p tagged.
    static void indexWithTag(const QString &root, const QStringList &tagged)
    {
        TagLibrary::Library library(root);
        library.applyScan(LibraryScan::scan(root).items, true);
        QJsonObject document = library.toJson();
        document[QStringLiteral("tags")] =
            QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("t1")}, {QStringLiteral("name"), QStringLiteral("Tag")}}};
        QJsonArray entries = document.value(QStringLiteral("entries")).toArray();
        for (qsizetype i = 0; i < entries.size(); ++i) {
            QJsonObject entry = entries.at(i).toObject();
            if (tagged.contains(entry.value(QStringLiteral("path")).toString())) {
                entry[QStringLiteral("tags")] = QJsonArray{QStringLiteral("t1")};
            }
            entries[i] = entry;
        }
        document[QStringLiteral("entries")] = entries;
        QFile file(library.filePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(document).toJson());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        for (const QString &root : LibraryRegistry::roots()) {
            LibraryRegistry::remove(root);
        }
    }

    void refreshIndexesAndMarksMissingEntries()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("photos")));
        write(dir.filePath(QStringLiteral("notes.txt")));
        write(dir.filePath(QStringLiteral("photos/a.jpg")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());

        TagLibrary::ScanChanges changes;
        QString message;
        refresh(dir.path(), &changes, &message);
        QCOMPARE(changes.added, 3);
        QCOMPARE(changes.missing, 0);
        QVERIFY(!message.isEmpty());

        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("notes.txt"))));
        refresh(dir.path(), &changes, &message);
        QCOMPARE(changes.added, 0);
        QCOMPARE(changes.missing, 1);
        const TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QVERIFY(library.entry(QStringLiteral("notes.txt")));
        QVERIFY(library.entry(QStringLiteral("notes.txt"))->missing);
    }

    void canceledRefreshMarksNothingMissing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("keep.txt")));
        for (int i = 0; i < 40; ++i) {
            const QString folder = QStringLiteral("folder%1").arg(i);
            QVERIFY(QDir(dir.path()).mkpath(folder));
            for (int j = 0; j < 10; ++j) {
                write(dir.filePath(QStringLiteral("%1/file%2").arg(folder).arg(j)));
            }
        }
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        TagLibrary::ScanChanges changes;
        QString message;
        refresh(dir.path(), &changes, &message);
        QCOMPARE(changes.added, 441);

        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("keep.txt"))));
        refresh(dir.path(), &changes, &message, true);
        QCOMPARE(changes.missing, 0);
        QVERIFY(!TagLibrary::Library::load(dir.path()).entry(QStringLiteral("keep.txt"))->missing);
    }

    void tracksMovesAndRenamesPlasmaOpusMakes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Inbox")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Archive")));
        write(dir.filePath(QStringLiteral("Inbox/a.txt")));
        write(dir.filePath(QStringLiteral("b.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {QStringLiteral("Inbox/a.txt"), QStringLiteral("b.txt")});
        const QString aId = TagLibrary::Library::load(dir.path()).entry(QStringLiteral("Inbox/a.txt"))->id;
        const QString bId = TagLibrary::Library::load(dir.path()).entry(QStringLiteral("b.txt"))->id;

        LibraryMoveTracker *tracker = LibraryMoveTracker::instance();
        QSignalSpy changed(tracker, &LibraryMoveTracker::librariesChanged);
        KIO::CopyJob *job = KIO::move(QUrl::fromLocalFile(dir.filePath(QStringLiteral("Inbox"))),
                                      QUrl::fromLocalFile(dir.filePath(QStringLiteral("Archive"))),
                                      KIO::HideProgressInfo);
        tracker->trackCopyJob(job);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().toStringList(), QStringList{dir.path()});
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.entry(QStringLiteral("Archive/Inbox/a.txt"))->id, aId);
        QCOMPARE(library.entry(QStringLiteral("Archive/Inbox/a.txt"))->tags, QStringList{QStringLiteral("t1")});
        QVERIFY(!library.entry(QStringLiteral("Inbox")));

        // Copies aren't tracked.
        KIO::CopyJob *copy = KIO::copy(QUrl::fromLocalFile(dir.filePath(QStringLiteral("b.txt"))),
                                       QUrl::fromLocalFile(dir.filePath(QStringLiteral("Archive"))),
                                       KIO::HideProgressInfo);
        tracker->trackCopyJob(copy);
        QVERIFY(copy->exec());
        QTest::qWait(50);
        QCOMPARE(changed.count(), 1);

        // An inline rename is applied once the renamed file is confirmed.
        tracker->expectRename(dir.filePath(QStringLiteral("b.txt")), dir.filePath(QStringLiteral("c.txt")));
        QVERIFY(QFile::rename(dir.filePath(QStringLiteral("b.txt")), dir.filePath(QStringLiteral("c.txt"))));
        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(TagLibrary::Library::load(dir.path()).entry(QStringLiteral("c.txt"))->id, bId);
    }

    void reviewRelinksOnlyChosenItems()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("photo.jpg")));
        write(dir.filePath(QStringLiteral("note.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {QStringLiteral("photo.jpg"), QStringLiteral("note.txt")});
        const QString photoId = TagLibrary::Library::load(dir.path()).entry(QStringLiteral("photo.jpg"))->id;

        // Moved by another program.
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Moved")));
        QVERIFY(QFile::rename(dir.filePath(QStringLiteral("photo.jpg")), dir.filePath(QStringLiteral("Moved/photo.jpg"))));
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("note.txt"))));
        {
            LibraryRefreshDialog refreshDialog(dir.path());
            QSignalSpy finished(&refreshDialog, &LibraryRefreshDialog::refreshFinished);
            refreshDialog.show();
            refreshDialog.start();
            QVERIFY(finished.wait(10000) || finished.count() == 1);
            QCOMPARE(refreshDialog.missingEntries(), 2);
            auto *reviewButton = refreshDialog.findChild<QPushButton *>(QStringLiteral("libraryReviewMissing"));
            QVERIFY(reviewButton && reviewButton->isVisible());
            QSignalSpy review(&refreshDialog, &LibraryRefreshDialog::reviewRequested);
            reviewButton->click();
            QCOMPARE(review.count(), 1);
        }
        // Refreshing never attaches the tag to the moved file by itself.
        QVERIFY(TagLibrary::Library::load(dir.path()).entry(QStringLiteral("Moved/photo.jpg"))->tags.isEmpty());

        LibraryReviewDialog dialog(dir.path());
        QSignalSpy changed(&dialog, &LibraryReviewDialog::libraryChanged);
        auto *missing = dialog.findChild<QTreeWidget *>(QStringLiteral("libraryMissingList"));
        auto *candidates = dialog.findChild<QTreeWidget *>(QStringLiteral("libraryCandidateList"));
        auto *relink = dialog.findChild<QPushButton *>(QStringLiteral("libraryRelink"));
        QCOMPARE(missing->topLevelItemCount(), 2);
        missing->setCurrentItem(missing->findItems(QStringLiteral("photo.jpg"), Qt::MatchExactly).first());
        QCOMPARE(candidates->topLevelItemCount(), 1);
        QCOMPARE(candidates->topLevelItem(0)->text(0), QStringLiteral("Moved/photo.jpg"));
        QVERIFY(!relink->isEnabled());
        candidates->setCurrentItem(candidates->topLevelItem(0));
        QVERIFY(relink->isEnabled());
        relink->click();
        QCOMPARE(changed.count(), 1);
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.entry(QStringLiteral("Moved/photo.jpg"))->id, photoId);
        QCOMPARE(library.entry(QStringLiteral("Moved/photo.jpg"))->tags, QStringList{QStringLiteral("t1")});
        QCOMPARE(missing->topLevelItemCount(), 1);

        // The note has no suggestion; an item outside the library is refused.
        missing->setCurrentItem(missing->topLevelItem(0));
        QCOMPARE(candidates->topLevelItem(0)->flags(), Qt::NoItemFlags);
        QTemporaryDir outside;
        write(outside.filePath(QStringLiteral("note.txt")));
        QVERIFY(!dialog.relinkTo(outside.filePath(QStringLiteral("note.txt"))));
        QVERIFY(!dialog.message().isEmpty());
        QVERIFY(dialog.removeSelected());
        QCOMPARE(missing->topLevelItemCount(), 0);
        QVERIFY(!TagLibrary::Library::load(dir.path()).entry(QStringLiteral("note.txt")));
    }

    void managesTagsInTheTagManager()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {});

        TagManagerWidget manager;
        QSignalSpy changed(&manager, &TagManagerWidget::libraryChanged);
        manager.setLibrary(dir.path());
        auto *list = manager.findChild<QTreeWidget *>(QStringLiteral("tagList"));
        QCOMPARE(list->topLevelItemCount(), 1);

        // A new tag below the existing one, with a duplicate name.
        TagEditDialog *dialog = manager.createTag();
        QVERIFY(dialog);
        auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled());
        dialog->findChild<QLineEdit *>(QStringLiteral("tagName"))->setText(QStringLiteral("tag"));
        dialog->findChild<QPlainTextEdit *>(QStringLiteral("tagAliases"))->setPlainText(QStringLiteral("Label\n\nlabel"));
        dialog->findChild<QLineEdit *>(QStringLiteral("tagShorthand"))->setText(QStringLiteral("t 2"));
        QVERIFY(!ok->isEnabled());
        QVERIFY(!dialog->problem().isEmpty());
        dialog->findChild<QLineEdit *>(QStringLiteral("tagShorthand"))->setText(QStringLiteral("t2"));
        QVERIFY(ok->isEnabled());
        QCOMPARE(dialog->duplicateNote().contains(QStringLiteral("tag (t2)")), true);
        auto *parents = dialog->findChild<QListWidget *>(QStringLiteral("tagParents"));
        QCOMPARE(parents->count(), 1);
        parents->item(0)->setCheckState(Qt::Checked);
        QCOMPARE(dialog->duplicateNote().contains(QStringLiteral("tag (Tag)")), true);
        const QString childId = dialog->tag().id;
        ok->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(manager.currentTag(), childId);

        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.tag(childId)->aliases, QStringList{QStringLiteral("Label")});
        QCOMPARE(library.tag(childId)->parents, QStringList{QStringLiteral("t1")});
        QCOMPARE(library.displayName(childId), QStringLiteral("tag (Tag)"));

        // The parent can't become a child of its own child.
        dialog = manager.editTag(QStringLiteral("t1"));
        QVERIFY(dialog);
        parents = dialog->findChild<QListWidget *>(QStringLiteral("tagParents"));
        QCOMPARE(parents->count(), 1);
        QVERIFY(!(parents->item(0)->flags() & Qt::ItemIsEnabled));
        dialog->findChild<QCheckBox *>(QStringLiteral("tagUseColor"))->setChecked(true);
        dialog->findChild<KColorButton *>(QStringLiteral("tagColor"))->setColor(QColor(QStringLiteral("#112233")));
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(TagLibrary::Library::load(dir.path()).tag(QStringLiteral("t1"))->color, QStringLiteral("#112233"));

        // Searching matches aliases and shorthand.
        auto *filter = manager.findChild<QLineEdit *>(QStringLiteral("tagFilter"));
        filter->setText(QStringLiteral("labe"));
        int visible = 0;
        for (int i = 0; i < list->topLevelItemCount(); ++i) {
            visible += list->topLevelItem(i)->isHidden() ? 0 : 1;
        }
        QCOMPARE(visible, 1);
        filter->clear();

        // Deleting the parent keeps the child.
        QVERIFY(manager.removeTag(QStringLiteral("t1")));
        library = TagLibrary::Library::load(dir.path());
        QVERIFY(!library.tag(QStringLiteral("t1")));
        QVERIFY(library.tag(childId)->parents.isEmpty());
        QCOMPARE(library.displayName(childId), QStringLiteral("tag"));
        QCOMPARE(list->topLevelItemCount(), 1);
    }
};

QTEST_MAIN(TaggingUiTest)

#include "tagginguitest.moc"
