// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddedmetadata.h"
#include "fieldeditdialog.h"
#include "informationpanel.h"
#include "libraryarchive.h"
#include "libraryautorefresh.h"
#include "libraryexportdialog.h"
#include "librarymovetracker.h"
#include "libraryrefreshdialog.h"
#include "libraryregistry.h"
#include "libraryrestoredialog.h"
#include "libraryreviewdialog.h"
#include "libraryscan.h"
#include "localizeddates.h"
#include "metadataimportdialog.h"
#include "operationjob.h"
#include "settings.h"
#include "tagassignment.h"
#include "tageditdialog.h"
#include "tagfieldmanagerwidget.h"
#include "taggingcontroller.h"
#include "taggingsettings.h"
#include "taggroupdialog.h"
#include "taggroupmanagerwidget.h"
#include "taglibrary.h"
#include "tagmanagerwidget.h"
#include "tagpanel.h"

#include <KActionCollection>
#include <KColorButton>
#include <KIO/CopyJob>
#include <KJobTrackerInterface>
#include <KMessageWidget>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QUrl>

/// Records the jobs a dialog reports to Plasma and how they ended.
class RecordingTracker : public KJobTrackerInterface
{
public:
    QStringList titles;
    QList<int> errors;
    int registered = 0;

    void registerJob(KJob *job) override
    {
        ++registered;
        KJobTrackerInterface::registerJob(job);
    }
    void unregisterJob(KJob *job) override
    {
        errors.append(job->error());
        KJobTrackerInterface::unregisterJob(job);
    }

protected:
    void description(KJob *, const QString &title, const std::pair<QString, QString> &, const std::pair<QString, QString> &) override
    {
        titles.append(title);
    }
};

class TaggingUiTest : public QObject
{
    Q_OBJECT

    static void write(const QString &path)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
    }

    static bool hasExifExtractor()
    {
        const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfilemetadata"));
        return std::any_of(plugins.cbegin(), plugins.cend(), [](const KPluginMetaData &plugin) {
            return plugin.pluginId().contains(QLatin1String("exiv2"));
        });
    }

    /// Writes a JPEG whose EXIF data names the camera maker "Acme", returning its contents.
    static QByteArray writeExifJpeg(const QString &path)
    {
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        if (!image.save(path)) {
            return {};
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        QByteArray jpeg = file.readAll();
        file.close();
        jpeg.insert(2, QByteArray::fromHex("ffe1002745786966000049492a000800000001000f010200050000001a0000000000000041636d6500"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(jpeg) != jpeg.size()) {
            return {};
        }
        return jpeg;
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

    void refreshReportsPlasmaJob()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("notes.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());

        RecordingTracker tracker;
        OperationJob::setDefaultTracker(&tracker);
        const auto resetTracker = qScopeGuard([] {
            OperationJob::setDefaultTracker(nullptr);
        });

        TagLibrary::ScanChanges changes;
        QString message;
        refresh(dir.path(), &changes, &message);
        QCOMPARE(tracker.registered, 1);
        QVERIFY(tracker.titles.contains(QStringLiteral("Refreshing tag library")));
        QTRY_COMPARE(tracker.errors, QList<int>{KJob::NoError});

        refresh(dir.path(), &changes, &message, true);
        QCOMPARE(tracker.registered, 2);
        QTRY_COMPARE(tracker.errors.size(), 2);
        QCOMPARE(tracker.errors.last(), int(KJob::KilledJobError));
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

    void refreshFollowsTheScanSettings()
    {
        const auto reset = qScopeGuard([] {
            Settings::setLibraryScanLimit(0);
            Settings::setLibraryExclusions({});
            Settings::self()->save();
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        write(dir.filePath(QStringLiteral("b.txt")));
        write(dir.filePath(QStringLiteral("c.tmp")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        TagLibrary::ScanChanges changes;
        QString message;
        refresh(dir.path(), &changes, &message);
        QCOMPARE(changes.added, 3);

        // An excluded item keeps its entry, and a refresh stopped by the limit marks nothing missing.
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("a.txt"))));
        Settings::setLibraryExclusions({QStringLiteral("*.tmp")});
        Settings::setLibraryScanLimit(1);
        Settings::self()->save();
        write(dir.filePath(QStringLiteral("d.txt")));
        refresh(dir.path(), &changes, &message);
        QCOMPARE(changes.missing, 0);
        QVERIFY(message.contains(QStringLiteral("Stopped after indexing one item, the limit set in Preferences › Tagging › Libraries.")));
        QVERIFY(!TagLibrary::Library::load(dir.path()).entry(QStringLiteral("a.txt"))->missing);

        Settings::setLibraryScanLimit(0);
        Settings::self()->save();
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("c.tmp"))));
        refresh(dir.path(), &changes, &message);
        QCOMPARE(changes.missing, 1);
        QVERIFY(!message.contains(QStringLiteral("Stopped after")));
        const TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QVERIFY(library.entry(QStringLiteral("a.txt"))->missing);
        QVERIFY(!library.entry(QStringLiteral("c.tmp"))->missing);
        QVERIFY(library.entry(QStringLiteral("d.txt")));
    }

    void refreshesLibrariesAutomatically()
    {
        const auto reset = qScopeGuard([] {
            Settings::setAutomaticLibraryRefresh(Settings::EnumAutomaticLibraryRefresh::Never);
            Settings::setLibraryRefreshInterval(60);
            Settings::self()->save();
        });
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("LibraryRefresh"));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.filePath(QStringLiteral("Photos"));
        QVERIFY(QDir().mkpath(root + QStringLiteral("/trip")));
        write(root + QStringLiteral("/a.jpg"));
        write(root + QStringLiteral("/trip/b.jpg"));
        QCOMPARE(LibraryRegistry::create(root), QString());

        LibraryAutoRefresh *refresher = LibraryAutoRefresh::instance();
        QCOMPARE(LibraryAutoRefresh::instance(), refresher);
        QSignalSpy refreshed(refresher, &LibraryAutoRefresh::refreshed);

        // A library never refreshed before is refreshed at once, and reported.
        QVERIFY(!LibraryAutoRefresh::lastRefresh(root).isValid());
        refresher->enqueue(root);
        QVERIFY(refresher->isPending(root));
        QVERIFY(refreshed.wait(10000));
        QCOMPARE(refreshed.constLast().at(0).toString(), root);
        QCOMPARE(refreshed.constLast().at(1).toString(), QStringLiteral("Refreshed library Photos: 3 added"));
        QVERIFY(refreshed.constLast().at(2).toBool());
        QVERIFY(!refresher->isPending(root));
        QVERIFY(LibraryAutoRefresh::lastRefresh(root).isValid());
        QCOMPARE(TagLibrary::Library::load(root).entries().size(), 3);

        // Within the interval it's skipped, unless forced; an unchanged library reports nothing.
        refresher->enqueue(root);
        QVERIFY(!refresher->isPending(root));
        refresher->enqueue(root, true);
        QVERIFY(refresher->isPending(root));
        QVERIFY(refreshed.wait(10000));
        QCOMPARE(refreshed.size(), 2);
        QVERIFY(refreshed.constLast().at(1).toString().isEmpty());
        QVERIFY(!refreshed.constLast().at(2).toBool());

        // Opening a folder in the library refreshes it only when chosen in the settings.
        LibraryAutoRefresh::recordRefresh(root, QDateTime::currentDateTimeUtc().addSecs(-2 * 60 * 60));
        QVERIFY(QFile::remove(root + QStringLiteral("/a.jpg")));
        refresher->folderOpened(QUrl::fromLocalFile(root + QStringLiteral("/trip")));
        QVERIFY(!refresher->isPending(root));
        Settings::setAutomaticLibraryRefresh(Settings::EnumAutomaticLibraryRefresh::WhenOpened);
        refresher->folderOpened(QUrl::fromLocalFile(dir.path()));
        QVERIFY(!refresher->isPending(root));
        refresher->folderOpened(QUrl::fromLocalFile(root + QStringLiteral("/trip")));
        QVERIFY(refresher->isPending(root));
        QVERIFY(refreshed.wait(10000));
        QCOMPARE(refreshed.constLast().at(1).toString(), QStringLiteral("Refreshed library Photos: 1 missing"));
        QVERIFY(TagLibrary::Library::load(root).entry(QStringLiteral("a.jpg"))->missing);

        // A manual refresh takes precedence, and counts as the latest refresh.
        LibraryAutoRefresh::recordRefresh(root, QDateTime::currentDateTimeUtc().addSecs(-2 * 60 * 60));
        refresher->beginManualRefresh(root);
        refresher->enqueue(root, true);
        QVERIFY(!refresher->isPending(root));
        refresher->endManualRefresh(root, true);
        QVERIFY(LibraryAutoRefresh::lastRefresh(root).secsTo(QDateTime::currentDateTimeUtc()) < 60);

        // Unregistered folders and canceled refreshes report nothing.
        refresher->enqueue(dir.path(), true);
        QVERIFY(!refresher->isPending(dir.path()));
        refresher->enqueue(root, true);
        refresher->cancel();
        QVERIFY(!refresher->isPending(root));
        QTest::qWait(100);
        QCOMPARE(refreshed.size(), 3);
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

    void organizesTagGroups()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {});
        QVERIFY(TagLibrary::update(dir.path(), [](TagLibrary::Library &library, QString *error) {
            return library.setTag({QStringLiteral("t2"), QStringLiteral("Beach"), {}, {}, {}, {}}, error)
                && library.setTag({QStringLiteral("t3"), QStringLiteral("Loose"), {}, {}, {}, {}}, error);
        }));

        TagGroupManagerWidget groups;
        QSignalSpy changed(&groups, &TagGroupManagerWidget::libraryChanged);
        groups.setLibrary(dir.path());
        auto *groupList = groups.findChild<QTreeWidget *>(QStringLiteral("tagGroupList"));
        QCOMPARE(groupList->topLevelItemCount(), 0);
        QVERIFY(!groups.message().isEmpty());

        // A new group with two tags.
        TagGroupDialog *dialog = groups.createGroup();
        QVERIFY(dialog);
        auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled());
        dialog->findChild<QLineEdit *>(QStringLiteral("tagGroupName"))->setText(QStringLiteral("Places"));
        QVERIFY(ok->isEnabled());
        auto *members = dialog->findChild<QListWidget *>(QStringLiteral("tagGroupTags"));
        QCOMPARE(members->count(), 3);
        dialog->findChild<QLineEdit *>(QStringLiteral("tagGroupTagFilter"))->setText(QStringLiteral("bea"));
        int visible = 0;
        for (int i = 0; i < members->count(); ++i) {
            visible += members->item(i)->isHidden() ? 0 : 1;
        }
        QCOMPARE(visible, 1);
        members->findItems(QStringLiteral("Beach"), Qt::MatchExactly).first()->setCheckState(Qt::Checked);
        members->findItems(QStringLiteral("Tag"), Qt::MatchExactly).first()->setCheckState(Qt::Checked);
        const QString places = dialog->group().id;
        ok->click();
        QCOMPARE(changed.count(), 1);
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.group(places)->name, QStringLiteral("Places"));
        QCOMPARE(library.group(places)->tags, (QStringList{QStringLiteral("t2"), QStringLiteral("t1")}));
        QCOMPARE(groups.currentGroup(), places);

        // A second group sharing a tag, moved to the top.
        dialog = groups.createGroup();
        dialog->findChild<QLineEdit *>(QStringLiteral("tagGroupName"))->setText(QStringLiteral("Subjects"));
        dialog->findChild<QListWidget *>(QStringLiteral("tagGroupTags"))
            ->findItems(QStringLiteral("Tag"), Qt::MatchExactly)
            .first()
            ->setCheckState(Qt::Checked);
        const QString subjects = dialog->group().id;
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(groupList->topLevelItemCount(), 2);
        QVERIFY(!groups.findChild<QPushButton *>(QStringLiteral("tagGroupDown"))->isEnabled());
        groups.findChild<QPushButton *>(QStringLiteral("tagGroupUp"))->click();
        library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.groups().first().id, subjects);
        QCOMPARE(library.groupsOf(QStringLiteral("t1")), (QStringList{subjects, places}));
        QCOMPARE(groupList->topLevelItem(0)->text(0), QStringLiteral("Subjects"));
        QVERIFY(!groups.moveGroup(subjects, -1));

        // The tag manager lists each tag's groups.
        TagManagerWidget manager;
        manager.setLibrary(dir.path());
        auto *tagList = manager.findChild<QTreeWidget *>(QStringLiteral("tagList"));
        QCOMPARE(tagList->findItems(QStringLiteral("Tag"), Qt::MatchExactly).first()->text(3), QStringLiteral("Subjects, Places"));

        // The tag panel lists every group in order, then the tags without a group.
        TagPanel panel;
        auto *list = panel.findChild<QTreeWidget *>(QStringLiteral("tagPanelList"));
        auto *filter = panel.findChild<QLineEdit *>(QStringLiteral("tagPanelFilter"));
        panel.setContext(QUrl::fromLocalFile(dir.path()), {KFileItem(QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt"))))});
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("Subjects"));
        QCOMPARE(list->topLevelItem(1)->text(0), QStringLiteral("Places"));
        QCOMPARE(list->topLevelItem(2)->childCount(), 1);
        QCOMPARE(list->topLevelItem(2)->child(0)->text(0), QStringLiteral("Loose"));
        QVERIFY(!(list->topLevelItem(0)->flags() & Qt::ItemIsUserCheckable));
        QVERIFY(list->topLevelItem(0)->isExpanded());
        QTreeWidgetItem *places0 = list->topLevelItem(1);
        QCOMPARE(places0->child(0)->text(0), QStringLiteral("Beach"));

        // Checking a tag in one group checks its copy in the other.
        places0->child(1)->setCheckState(0, Qt::Checked);
        QCOMPARE(panel.assignment(QStringLiteral("t1")), Qt::Checked);
        QCOMPARE(list->topLevelItem(0)->child(0)->checkState(0), Qt::Checked);
        QCOMPARE(TagLibrary::Library::load(dir.path()).entry(QStringLiteral("a.txt"))->tags, QStringList{QStringLiteral("t1")});

        // Searching a group's name shows its tags; other groups without matches are hidden.
        filter->setText(QStringLiteral("places"));
        QVERIFY(list->topLevelItem(0)->isHidden());
        QVERIFY(!places0->isHidden());
        QVERIFY(!places0->child(0)->isHidden() && !places0->child(1)->isHidden());
        filter->setText(QStringLiteral("loo"));
        QVERIFY(list->topLevelItem(0)->isHidden() && list->topLevelItem(1)->isHidden());
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        filter->setFocus();
        QTest::keyClick(filter, Qt::Key_Down);
        QCOMPARE(list->currentItem()->data(0, Qt::UserRole).toString(), QStringLiteral("t3"));
        filter->clear();
        QVERIFY(!list->topLevelItem(0)->isHidden());

        // Deleting a group keeps its tags, which move to the ungrouped section.
        QVERIFY(groups.removeGroup(places));
        library = TagLibrary::Library::load(dir.path());
        QVERIFY(library.tag(QStringLiteral("t2")));
        QCOMPARE(library.entry(QStringLiteral("a.txt"))->tags, QStringList{QStringLiteral("t1")});
        TagAssignment::invalidate(dir.path());
        panel.reload();
        QCOMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(list->topLevelItem(1)->childCount(), 2);

        // Without groups the panel shows a flat list again.
        QVERIFY(groups.removeGroup(subjects));
        TagAssignment::invalidate(dir.path());
        panel.reload();
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(list->topLevelItem(0)->childCount(), 0);
    }

    void resolvesAndAppliesAssignments()
    {
        QTemporaryDir dir;
        QTemporaryDir other;
        QVERIFY(dir.isValid() && other.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        write(other.filePath(QStringLiteral("b.txt")));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("a.txt")), dir.filePath(QStringLiteral("link.txt"))));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        QCOMPARE(LibraryRegistry::create(other.path()), QString());
        indexWithTag(dir.path(), {});

        TagAssignment::Selection selection = TagAssignment::resolve({dir.filePath(QStringLiteral("a.txt"))});
        QCOMPARE(selection.root, dir.path());
        QCOMPARE(selection.relativePaths, QStringList{QStringLiteral("a.txt")});
        QVERIFY(selection.problem.isEmpty());
        selection = TagAssignment::resolve({dir.filePath(QStringLiteral("a.txt")), other.filePath(QStringLiteral("b.txt"))});
        QVERIFY(selection.root.isEmpty());
        QVERIFY(!selection.problem.isEmpty());
        QVERIFY(!TagAssignment::resolve({dir.path()}).problem.isEmpty());
        QVERIFY(!TagAssignment::resolve({}).problem.isEmpty());

        QString error;
        QVERIFY(!TagAssignment::apply(dir.path(), {dir.filePath(QStringLiteral("link.txt"))}, {QStringLiteral("t1")}, {}, nullptr, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!TagAssignment::apply(dir.path(), {other.filePath(QStringLiteral("b.txt"))}, {QStringLiteral("t1")}, {}, nullptr, &error));

        // The cache follows writes in this process.
        QVERIFY(TagAssignment::cachedLibrary(dir.path()).entry(QStringLiteral("a.txt"))->tags.isEmpty());
        int changed = 0;
        QVERIFY2(TagAssignment::apply(dir.path(), {dir.filePath(QStringLiteral("a.txt"))}, {QStringLiteral("t1")}, {}, &changed, &error), qPrintable(error));
        QCOMPARE(changed, 1);
        QCOMPARE(TagAssignment::cachedLibrary(dir.path()).entry(QStringLiteral("a.txt"))->tags, QStringList{QStringLiteral("t1")});
    }

    void assignsTagsInTheTagPanel()
    {
        QTemporaryDir dir;
        QTemporaryDir other;
        QVERIFY(dir.isValid() && other.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        write(dir.filePath(QStringLiteral("b.txt")));
        write(other.filePath(QStringLiteral("c.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        QCOMPARE(LibraryRegistry::create(other.path()), QString());
        indexWithTag(dir.path(), {QStringLiteral("a.txt")});
        // Created after indexing, so assigning a tag indexes it.
        write(dir.filePath(QStringLiteral("new.txt")));
        const auto item = [](const QString &path) {
            return KFileItem(QUrl::fromLocalFile(path));
        };
        const KFileItem a = item(dir.filePath(QStringLiteral("a.txt")));
        const KFileItem b = item(dir.filePath(QStringLiteral("b.txt")));

        TagPanel panel;
        QSignalSpy changed(&panel, &TagPanel::libraryChanged);
        auto *list = panel.findChild<QTreeWidget *>(QStringLiteral("tagPanelList"));
        auto *filter = panel.findChild<QLineEdit *>(QStringLiteral("tagPanelFilter"));
        auto *create = panel.findChild<QPushButton *>(QStringLiteral("tagPanelCreate"));
        panel.setContext(QUrl::fromLocalFile(dir.path()), {a, b});
        QCOMPARE(panel.library(), dir.path());
        QCOMPARE(list->topLevelItemCount(), 1);
        QCOMPARE(panel.assignment(QStringLiteral("t1")), Qt::PartiallyChecked);
        QCOMPARE(list->topLevelItem(0)->checkState(0), Qt::PartiallyChecked);
        QVERIFY(list->topLevelItem(0)->flags() & Qt::ItemIsUserCheckable);

        // Checking a partly assigned tag assigns it to every item.
        list->topLevelItem(0)->setCheckState(0, Qt::Checked);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(panel.assignment(QStringLiteral("t1")), Qt::Checked);
        QCOMPARE(TagLibrary::Library::load(dir.path()).entry(QStringLiteral("b.txt"))->tags, QStringList{QStringLiteral("t1")});
        QVERIFY(panel.setAssigned(QStringLiteral("t1"), false));
        QCOMPARE(list->topLevelItem(0)->checkState(0), Qt::Unchecked);
        QVERIFY(TagLibrary::Library::load(dir.path()).entry(QStringLiteral("a.txt"))->tags.isEmpty());

        // Typing a new name and pressing Enter creates and assigns it; Enter on a match toggles it.
        filter->setText(QStringLiteral("Travel"));
        QVERIFY(create->isEnabled());
        QTest::keyClick(filter, Qt::Key_Return);
        QVERIFY(filter->text().isEmpty());
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.tags().size(), 2);
        const QString travel = library.findTags(QStringLiteral("travel")).value(0);
        QVERIFY(!travel.isEmpty());
        QCOMPARE(library.entry(QStringLiteral("a.txt"))->tags, QStringList{travel});
        QCOMPARE(panel.assignment(travel), Qt::Checked);
        filter->setText(QStringLiteral("travel"));
        QVERIFY(!create->isEnabled());
        QTest::keyClick(filter, Qt::Key_Return);
        QCOMPARE(panel.assignment(travel), Qt::Unchecked);

        // Down moves to the list, where Space toggles the current tag.
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        filter->setFocus();
        filter->setText(QStringLiteral("tag"));
        QTest::keyClick(filter, Qt::Key_Down);
        QCOMPARE(list->currentItem()->data(0, Qt::UserRole).toString(), QStringLiteral("t1"));
        QTest::keyClick(list, Qt::Key_Space);
        QCOMPARE(panel.assignment(QStringLiteral("t1")), Qt::Checked);

        // Unindexed items are indexed when tagged.
        panel.setContext(QUrl::fromLocalFile(dir.path()), {item(dir.filePath(QStringLiteral("new.txt")))});
        QVERIFY(panel.setAssigned(travel, true));
        QCOMPARE(TagLibrary::Library::load(dir.path()).entry(QStringLiteral("new.txt"))->tags, QStringList{travel});

        // Items in different libraries can't be tagged together.
        panel.setContext(QUrl::fromLocalFile(dir.path()), {a, item(other.filePath(QStringLiteral("c.txt")))});
        QVERIFY(panel.message().contains(QStringLiteral("different libraries")));
        QCOMPARE(panel.library(), dir.path());
        QVERIFY(!panel.setAssigned(QStringLiteral("t1"), true));
        QVERIFY(!list->topLevelItem(0)->data(0, Qt::CheckStateRole).isValid());

        // Without a selection the folder's library is shown without check boxes.
        panel.setContext(QUrl::fromLocalFile(dir.path()), {});
        QCOMPARE(list->topLevelItemCount(), 2);
        QVERIFY(!(list->topLevelItem(0)->flags() & Qt::ItemIsUserCheckable));
        QTemporaryDir outside;
        panel.setContext(QUrl::fromLocalFile(outside.path()), {});
        QVERIFY(panel.library().isEmpty());
        QCOMPARE(list->topLevelItemCount(), 0);
        QVERIFY(!panel.message().isEmpty());
    }

    void showsTagsInTheInformationPanel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        write(dir.filePath(QStringLiteral("b.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {QStringLiteral("a.txt")});
        QVERIFY(TagLibrary::update(dir.path(), [](TagLibrary::Library &library, QString *error) {
            return library.setTag({QStringLiteral("t2"), QStringLiteral("Child"), {}, {}, {}, {QStringLiteral("t1")}}, error);
        }));
        TagAssignment::invalidate();
        QVERIFY(TagAssignment::apply(dir.path(), {dir.filePath(QStringLiteral("b.txt"))}, {QStringLiteral("t2")}, {}));

        InformationPanel panel;
        panel.setFields({QStringLiteral("size")});
        auto *tags = panel.findChild<QLabel *>(QStringLiteral("informationTags"));
        QVERIFY(tags);
        panel.setItem(KFileItem(QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt")))));
        QVERIFY(!tags->isHidden());
        QCOMPARE(tags->text(), QStringLiteral("Tag"));
        panel.setItem(KFileItem(QUrl::fromLocalFile(dir.filePath(QStringLiteral("b.txt")))));
        QCOMPARE(tags->text(), QStringLiteral("Child"));
        QVERIFY(tags->toolTip().contains(QStringLiteral("Tag")));

        // Inherited tags can follow the item's own tags, and the tags can be hidden.
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        const KFileItem child(QUrl::fromLocalFile(dir.filePath(QStringLiteral("b.txt"))));
        Settings::setShowInheritedTags(true);
        panel.setItem(child);
        QCOMPARE(tags->text(), QStringLiteral("Child, Tag (inherited)"));
        QVERIFY(tags->toolTip().contains(QStringLiteral("Tag")));
        panel.setItem(KFileItem(QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt")))));
        QCOMPARE(tags->text(), QStringLiteral("Tag"));
        Settings::setShowTagsInInformationPanel(false);
        panel.setItem(child);
        QVERIFY(tags->isHidden());
        Settings::setShowTagsInInformationPanel(true);
        panel.setItem(child);
        QVERIFY(!tags->isHidden());
        Settings::setShowInheritedTags(false);

        QTemporaryDir outside;
        write(outside.filePath(QStringLiteral("c.txt")));
        panel.setItem(KFileItem(QUrl::fromLocalFile(outside.filePath(QStringLiteral("c.txt")))));
        QVERIFY(tags->isHidden());
    }

    void appliesTheTagsSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {});
        const KFileItem a(QUrl::fromLocalFile(dir.filePath(QStringLiteral("a.txt"))));

        // New tags get the default colour, if it is valid.
        Settings::setDefaultTagColor(QStringLiteral("#AA3355"));
        TagPanel panel;
        panel.setContext(QUrl::fromLocalFile(dir.path()), {a});
        QVERIFY(panel.createAndAssign(QStringLiteral("Red")));
        Settings::setDefaultTagColor(QStringLiteral("crimson"));
        QVERIFY(panel.createAndAssign(QStringLiteral("Plain")));
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.tag(library.findTags(QStringLiteral("Red")).constFirst())->color, QStringLiteral("#aa3355"));
        QVERIFY(library.tag(library.findTags(QStringLiteral("Plain")).constFirst())->color.isEmpty());

        Settings::setDefaultTagColor(QStringLiteral("#112233"));
        TagManagerWidget manager;
        manager.setLibrary(dir.path());
        TagEditDialog *dialog = manager.createTag();
        QVERIFY(dialog);
        QVERIFY(dialog->findChild<QCheckBox *>(QStringLiteral("tagUseColor"))->isChecked());
        QCOMPARE(dialog->tag().color, QStringLiteral("#112233"));
        dialog->reject();
        Settings::setDefaultTagColor(QString());
        dialog = manager.createTag();
        QVERIFY(!dialog->findChild<QCheckBox *>(QStringLiteral("tagUseColor"))->isChecked());
        QVERIFY(dialog->tag().color.isEmpty());
        dialog->reject();

        // Hidden colours show a plain tag icon and stay stored.
        const auto centre = [](const QIcon &icon) {
            return icon.pixmap(16, 16).toImage().pixelColor(8, 8);
        };
        QCOMPARE(centre(tagColorIcon(QStringLiteral("#aa3355"))), QColor(QStringLiteral("#aa3355")));
        auto *list = panel.findChild<QTreeWidget *>(QStringLiteral("tagPanelList"));
        const auto iconOf = [list](const QString &name) {
            for (int i = 0; i < list->topLevelItemCount(); ++i) {
                if (list->topLevelItem(i)->text(0) == name) {
                    return list->topLevelItem(i)->icon(0);
                }
            }
            return QIcon();
        };
        QCOMPARE(centre(iconOf(QStringLiteral("Red"))), QColor(QStringLiteral("#aa3355")));
        Settings::setShowTagColors(false);
        QVERIFY(centre(tagColorIcon(QStringLiteral("#aa3355"))) != QColor(QStringLiteral("#aa3355")));
        panel.reload();
        QVERIFY(centre(iconOf(QStringLiteral("Red"))) != QColor(QStringLiteral("#aa3355")));
        library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.tag(library.findTags(QStringLiteral("Red")).constFirst())->color, QStringLiteral("#aa3355"));
        Settings::setShowTagColors(true);
    }

    void editsFieldValues()
    {
        QTemporaryDir dir;
        QTemporaryDir other;
        QVERIFY(dir.isValid() && other.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        write(dir.filePath(QStringLiteral("b.txt")));
        write(other.filePath(QStringLiteral("c.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        QCOMPARE(LibraryRegistry::create(other.path()), QString());
        indexWithTag(dir.path(), {});
        // Created after indexing, so setting a value indexes it.
        write(dir.filePath(QStringLiteral("new.txt")));
        const QString a = dir.filePath(QStringLiteral("a.txt"));
        const QString b = dir.filePath(QStringLiteral("b.txt"));
        const QString fresh = dir.filePath(QStringLiteral("new.txt"));

        // Without fields the editor offers to create them.
        {
            FieldEditDialog dialog(dir.path(), {a});
            QSignalSpy manage(&dialog, &FieldEditDialog::manageFieldsRequested);
            QVERIFY(!dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
            dialog.findChild<QPushButton *>(QStringLiteral("fieldEditManage"))->click();
            QCOMPARE(manage.count(), 1);
        }

        // A text field and a date field; names must be unique.
        TagFieldManagerWidget fields;
        QSignalSpy fieldsChanged(&fields, &TagFieldManagerWidget::libraryChanged);
        fields.setLibrary(dir.path());
        auto *fieldList = fields.findChild<QTreeWidget *>(QStringLiteral("tagFieldList"));
        QCOMPARE(fieldList->topLevelItemCount(), 0);
        TagFieldDialog *fieldDialog = fields.createField();
        QVERIFY(fieldDialog);
        auto *ok = fieldDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled());
        fieldDialog->findChild<QLineEdit *>(QStringLiteral("tagFieldName"))->setText(QStringLiteral("Title"));
        const QString title = fieldDialog->field().id;
        ok->click();
        fieldDialog = fields.createField();
        fieldDialog->findChild<QLineEdit *>(QStringLiteral("tagFieldName"))->setText(QStringLiteral("title"));
        QVERIFY(!fieldDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        fieldDialog->findChild<QLineEdit *>(QStringLiteral("tagFieldName"))->setText(QStringLiteral("Taken"));
        auto *type = fieldDialog->findChild<QComboBox *>(QStringLiteral("tagFieldType"));
        QVERIFY(type->isEnabled());
        type->setCurrentIndex(type->findData(int(TagLibrary::FieldType::Date)));
        const QString taken = fieldDialog->field().id;
        fieldDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(fieldsChanged.count(), 2);
        QCOMPARE(fieldList->topLevelItemCount(), 2);
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.field(taken)->type, TagLibrary::FieldType::Date);
        QCOMPARE(library.field(title)->name, QStringLiteral("Title"));

        // Values are validated, then set on every item, indexing new ones.
        {
            FieldEditDialog dialog(dir.path(), {a, b, fresh});
            QSignalSpy changed(&dialog, &FieldEditDialog::libraryChanged);
            auto *dialogOk = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("fieldEditNote")));
            QTest::keyClicks(dialog.editor(title), QStringLiteral("Whiskers"));
            QTest::keyClicks(dialog.editor(taken), QStringLiteral("2026-02-30"));
            QVERIFY(!dialogOk->isEnabled());
            QVERIFY(!dialog.problem().isEmpty());
            dialog.editor(taken)->selectAll();
            QTest::keyClicks(dialog.editor(taken), QStringLiteral("2026-02-28"));
            QVERIFY(dialogOk->isEnabled());
            QVERIFY(dialog.findChild<QAction *>(QStringLiteral("fieldDatePicker_") + taken));
            QVERIFY(!dialog.findChild<QAction *>(QStringLiteral("fieldDatePicker_") + title));
            dialogOk->click();
            QCOMPARE(changed.count(), 1);
            QCOMPARE(dialog.changedCount(), 3);
        }
        library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.entry(QStringLiteral("a.txt"))->fields.value(title), QStringLiteral("Whiskers"));
        QCOMPARE(library.entry(QStringLiteral("new.txt"))->fields.value(taken), QStringLiteral("2026-02-28"));

        // The information panel shows the values, with dates in the regional format.
        TagAssignment::invalidate();
        InformationPanel panel;
        panel.setFields({QStringLiteral("size")});
        auto *values = panel.findChild<QLabel *>(QStringLiteral("informationLibraryFields"));
        QVERIFY(values);
        panel.setItem(KFileItem(QUrl::fromLocalFile(a)));
        QVERIFY(!values->isHidden());
        QCOMPARE(values->text(), QStringLiteral("Title: Whiskers\nTaken: ") + LocalizedDates::date(QDate(2026, 2, 28)));

        // Mixed values are shown as such and kept unless edited; clearing a value removes it.
        QString error;
        QVERIFY2(TagAssignment::applyFields(dir.path(), {b}, {{title, QStringLiteral("Tom")}}, nullptr, &error), qPrintable(error));
        {
            FieldEditDialog dialog(dir.path(), {a, b});
            QVERIFY(dialog.editor(title)->text().isEmpty());
            QVERIFY(!dialog.editor(title)->placeholderText().isEmpty());
            QCOMPARE(dialog.editor(taken)->text(), QStringLiteral("2026-02-28"));
            dialog.editor(taken)->selectAll();
            QTest::keyClick(dialog.editor(taken), Qt::Key_Backspace);
            QCOMPARE(dialog.edits(), (QMap<QString, QString>{{taken, QString()}}));
            QVERIFY(dialog.apply());
            QCOMPARE(dialog.changedCount(), 2);
        }
        library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.entry(QStringLiteral("a.txt"))->fields, (QMap<QString, QString>{{title, QStringLiteral("Whiskers")}}));
        QCOMPARE(library.entry(QStringLiteral("b.txt"))->fields, (QMap<QString, QString>{{title, QStringLiteral("Tom")}}));
        TagAssignment::invalidate();
        panel.setItem(KFileItem(QUrl::fromLocalFile(b)));
        QCOMPARE(values->text(), QStringLiteral("Title: Tom"));

        // A field with values keeps its type; deleting it removes its values.
        fields.reload();
        QCOMPARE(fieldList->topLevelItem(0)->text(2), QStringLiteral("3"));
        fieldDialog = fields.editField(title);
        QVERIFY(!fieldDialog->findChild<QComboBox *>(QStringLiteral("tagFieldType"))->isEnabled());
        fieldDialog->reject();
        QVERIFY(fields.removeField(title));
        QVERIFY(fields.message().contains(QStringLiteral("3")));
        QVERIFY(!fields.removeField(title));
        QVERIFY(!fields.message().isEmpty());
        library = TagLibrary::Library::load(dir.path());
        QVERIFY(!library.field(title));
        QVERIFY(library.entry(QStringLiteral("a.txt"))->fields.isEmpty());
        TagAssignment::invalidate();
        panel.setItem(KFileItem(QUrl::fromLocalFile(a)));
        QCOMPARE(values->text(), QStringLiteral("None"));

        // The controller opens the editor for the selection and refuses items in different libraries.
        QWidget window;
        KActionCollection actions(&window);
        KFileItemList selection{KFileItem(QUrl::fromLocalFile(a))};
        QString status;
        TaggingController controller(&window,
                                     &actions,
                                     {[&dir] {
                                          return QUrl::fromLocalFile(dir.path());
                                      },
                                      [&selection] {
                                          return selection;
                                      },
                                      [&status](const QString &message) {
                                          status = message;
                                      }});
        controller.updateActions();
        QVERIFY(actions.action(QStringLiteral("edit_fields"))->isEnabled());
        actions.action(QStringLiteral("edit_fields"))->trigger();
        auto *editor = window.findChild<FieldEditDialog *>();
        QVERIFY(editor);
        QVERIFY(editor->editor(taken));
        QSignalSpy controllerChanged(&controller, &TaggingController::libraryChanged);
        QTest::keyClicks(editor->editor(taken), QStringLiteral("2026-03-01"));
        QVERIFY(editor->apply());
        QCOMPARE(controllerChanged.count(), 1);
        QVERIFY(!status.isEmpty());
        status.clear();
        QVERIFY(!controller.editFields({a, other.filePath(QStringLiteral("c.txt"))}));
        QVERIFY(!status.isEmpty());
        selection.clear();
        status.clear();
        actions.action(QStringLiteral("edit_fields"))->trigger();
        QVERIFY(!status.isEmpty());

        // Manage Tags can open on the Fields tab.
        QWidget *manager = controller.manageTags(dir.path(), TaggingController::ManagerPage::Fields);
        QCOMPARE(manager->findChild<QTabWidget *>(QStringLiteral("tagsDialogTabs"))->currentIndex(), 2);
        QVERIFY(manager->findChild<TagFieldManagerWidget *>());
        manager->close();
    }

    void importsEmbeddedMetadata()
    {
        if (!hasExifExtractor()) {
            QSKIP("No EXIF metadata extractor is installed");
        }
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString photo = dir.filePath(QStringLiteral("photo.jpg"));
        const QByteArray jpeg = writeExifJpeg(photo);
        QVERIFY(!jpeg.isEmpty());
        const QString notes = dir.filePath(QStringLiteral("notes.txt"));
        write(notes);
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {});
        QString error;
        QVERIFY2(TagLibrary::update(
                     dir.path(),
                     [](TagLibrary::Library &library, QString *problem) {
                         return library.setField({QStringLiteral("maker"), QStringLiteral("Manufacturer"), TagLibrary::FieldType::Text}, problem);
                     },
                     &error),
                 qPrintable(error));
        const QByteArray original = jpeg;

        {
            MetadataImportDialog dialog(dir.path(), {photo, notes});
            QSignalSpy changed(&dialog, &MetadataImportDialog::libraryChanged);
            QTRY_VERIFY(dialog.isReady());
            auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("metadataImportPreview"));
            auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("metadataImportApply"));
            auto *changes = dialog.findChild<QTreeWidget *>(QStringLiteral("metadataImportChanges"));
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("metadataImportEditEmbedded"))->isVisibleTo(&dialog));
            // A field named like the property is suggested, and the conflict policy defaults to keeping values.
            QComboBox *maker = dialog.mapping(QStringLiteral("manufacturer"));
            QVERIFY(maker);
            QCOMPARE(maker->currentData().toString(), QStringLiteral("maker"));
            QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("metadataImportReplace"))->isChecked());
            QVERIFY(preview->isEnabled());
            QVERIFY(!apply->isEnabled());

            // Importing as tags creates the tag; the text file is reported, not changed.
            maker->setCurrentIndex(maker->findData(MetadataImport::tagsTarget()));
            preview->click();
            QCOMPARE(changes->topLevelItemCount(), 2);
            QCOMPARE(dialog.plan().count(MetadataImport::Action::CreateTag), 1);
            QCOMPARE(dialog.plan().count(MetadataImport::Action::Unsupported), 1);
            QVERIFY(apply->isEnabled());
            QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("metadataImportSummary"))->text().isEmpty());

            // Changing a mapping requires a new preview.
            maker->setCurrentIndex(maker->findData(QStringLiteral("maker")));
            QVERIFY(!apply->isEnabled());
            QCOMPARE(changes->topLevelItemCount(), 0);
            QVERIFY(dialog.preview());
            QCOMPARE(dialog.plan().count(MetadataImport::Action::SetField), 1);
            QVERIFY(dialog.import());
            QCOMPARE(changed.count(), 1);
            QCOMPARE(dialog.importResult().valuesSet, 1);
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
        }
        // The import saves the mappings by field name.
        QCOMPARE(Settings::embeddedMetadataMappings(), QStringList{QStringLiteral("manufacturer=field:Manufacturer")});
        QCOMPARE(KSharedConfig::openConfig()->group(QStringLiteral("Tagging")).readEntry("EmbeddedMetadataMappings", QStringList()),
                 Settings::embeddedMetadataMappings());
        TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.entry(QStringLiteral("photo.jpg"))->fields.value(QStringLiteral("maker")), QStringLiteral("Acme"));
        QVERIFY(library.findTags(QStringLiteral("Acme")).isEmpty());
        QFile check(photo);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), original);
        check.close();

        // A single editable file offers the embedded metadata editor; another preview finds nothing to import.
        {
            MetadataImportDialog dialog(dir.path(), {photo});
            QTRY_VERIFY(dialog.isReady());
            QVERIFY(dialog.preview());
            QVERIFY(!dialog.plan().hasChanges());
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("metadataImportApply"))->isEnabled());
            QVERIFY(!dialog.import());
            // Writing to files needs consent in Preferences, so the editor is offered but turned off by default.
            QVERIFY(!Settings::allowEmbeddedMetadataWrites());
            auto *edit = dialog.findChild<QPushButton *>(QStringLiteral("metadataImportEditEmbedded"));
            auto *note = dialog.findChild<QLabel *>(QStringLiteral("metadataImportEditEmbeddedNote"));
            QVERIFY(edit->isVisibleTo(&dialog));
            QVERIFY(!edit->isEnabled());
            QVERIFY(note->isVisibleTo(&dialog));
            QVERIFY(note->text().contains(QStringLiteral("Privacy")));
            QVERIFY(edit->toolTip().contains(QStringLiteral("Privacy")));
            QSignalSpy requested(&dialog, &MetadataImportDialog::editEmbeddedRequested);
            edit->click();
            QCOMPARE(requested.count(), 0);
        }
        Settings::setAllowEmbeddedMetadataWrites(true);
        {
            MetadataImportDialog dialog(dir.path(), {photo});
            QTRY_VERIFY(dialog.isReady());
            auto *edit = dialog.findChild<QPushButton *>(QStringLiteral("metadataImportEditEmbedded"));
            QVERIFY(edit->isVisibleTo(&dialog));
            QVERIFY(edit->isEnabled());
            QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("metadataImportEditEmbeddedNote"))->isVisibleTo(&dialog));
            QSignalSpy requested(&dialog, &MetadataImportDialog::editEmbeddedRequested);
            edit->click();
            QCOMPARE(requested.count(), 1);
            QCOMPARE(requested.first().first().toString(), photo);
        }
        Settings::setAllowEmbeddedMetadataWrites(false);

        // The controller opens the import for the selection and refuses items outside libraries.
        QWidget window;
        KActionCollection actions(&window);
        KFileItemList selection{KFileItem(QUrl::fromLocalFile(photo))};
        QString status;
        TaggingController controller(&window,
                                     &actions,
                                     {[&dir] {
                                          return QUrl::fromLocalFile(dir.path());
                                      },
                                      [&selection] {
                                          return selection;
                                      },
                                      [&status](const QString &message) {
                                          status = message;
                                      }});
        controller.updateActions();
        QVERIFY(actions.action(QStringLiteral("import_metadata"))->isEnabled());
        actions.action(QStringLiteral("import_metadata"))->trigger();
        auto *opened = window.findChild<MetadataImportDialog *>();
        QVERIFY(opened);
        QTRY_VERIFY(opened->isReady());
        opened->reject();
        QVERIFY(!controller.importMetadata({QDir::tempPath()}));
        QVERIFY(!status.isEmpty());
        selection.clear();
        status.clear();
        actions.action(QStringLiteral("import_metadata"))->trigger();
        QVERIFY(!status.isEmpty());

        // Saved mappings and the conflict policy are preselected; mappings of properties not shown are kept.
        Settings::setEmbeddedMetadataMappings({QStringLiteral("genre=tags"), QStringLiteral("manufacturer=tags")});
        Settings::setEmbeddedMetadataConflicts(Settings::EnumEmbeddedMetadataConflicts::Replace);
        {
            MetadataImportDialog dialog(dir.path(), {photo});
            QTRY_VERIFY(dialog.isReady());
            QCOMPARE(dialog.mapping(QStringLiteral("manufacturer"))->currentData().toString(), MetadataImport::tagsTarget());
            QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("metadataImportReplace"))->isChecked());
            QVERIFY(dialog.preview());
            QCOMPARE(dialog.plan().count(MetadataImport::Action::CreateTag), 1);
            QVERIFY(dialog.import());
        }
        QCOMPARE(Settings::embeddedMetadataMappings(), (QStringList{QStringLiteral("manufacturer=tags"), QStringLiteral("genre=tags")}));
        QCOMPARE(TagLibrary::Library::load(dir.path()).findTags(QStringLiteral("Acme")).size(), 1);

        // Properties that aren't read are not listed.
        Settings::setDisabledEmbeddedMetadataKeys({QStringLiteral("manufacturer")});
        {
            MetadataImportDialog dialog(dir.path(), {photo});
            QTRY_VERIFY(dialog.isReady());
            QVERIFY(!dialog.mapping(QStringLiteral("manufacturer")));
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("metadataImportIntro"))->text().contains(QStringLiteral("turned off")));
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("metadataImportPreview"))->isEnabled());
        }
        Settings::setDisabledEmbeddedMetadataKeys(EmbeddedMetadata::keys());
        {
            MetadataImportDialog dialog(dir.path(), {photo});
            QTRY_VERIFY(dialog.isReady());
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("metadataImportIntro"))->text().contains(QStringLiteral("every property")));
        }
    }

    void importsEmbeddedMetadataOnRefresh()
    {
        if (!hasExifExtractor()) {
            QSKIP("No EXIF metadata extractor is installed");
        }
        const auto restore = qScopeGuard([] {
            LibraryAutoRefresh::instance()->cancel();
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.filePath(QStringLiteral("Photos"));
        QVERIFY(QDir().mkpath(root));
        QVERIFY(!writeExifJpeg(root + QStringLiteral("/old.jpg")).isEmpty());
        QCOMPARE(LibraryRegistry::create(root), QString());
        QString error;
        QVERIFY2(TagLibrary::update(
                     root,
                     [](TagLibrary::Library &library, QString *problem) {
                         return library.setField({QStringLiteral("maker"), QStringLiteral("Camera Maker"), TagLibrary::FieldType::Text}, problem);
                     },
                     &error),
                 qPrintable(error));
        const auto refreshWithImport = [&root](QString *message, int *imported, int *changes) {
            LibraryRefreshDialog dialog(root);
            QSignalSpy finished(&dialog, &LibraryRefreshDialog::refreshFinished);
            QSignalSpy changed(&dialog, &LibraryRefreshDialog::libraryChanged);
            dialog.start();
            QVERIFY(finished.wait(10000) || finished.count() == 1);
            QVERIFY(!dialog.isRunning());
            *message = dialog.message();
            *imported = dialog.importedItems();
            *changes = int(changed.count());
        };
        QString message;
        int imported = -1;
        int changes = 0;

        // Without mappings, refreshing only indexes.
        Settings::setImportEmbeddedMetadataOnRefresh(true);
        QVERIFY(!TaggingSettings::importsEmbeddedMetadataOnRefresh());
        refreshWithImport(&message, &imported, &changes);
        QCOMPARE(imported, 0);
        QCOMPARE(changes, 1);
        QVERIFY(!message.contains(QStringLiteral("embedded metadata")));
        QVERIFY(TagLibrary::Library::load(root).entry(QStringLiteral("old.jpg"))->fields.isEmpty());

        // With mappings, the files a refresh adds are imported, matching the field by name; indexed files are not.
        Settings::setEmbeddedMetadataMappings({QStringLiteral("manufacturer=field:camera maker")});
        QVERIFY(TaggingSettings::importsEmbeddedMetadataOnRefresh());
        QVERIFY(!writeExifJpeg(root + QStringLiteral("/new.jpg")).isEmpty());
        write(root + QStringLiteral("/notes.txt"));
        refreshWithImport(&message, &imported, &changes);
        QCOMPARE(imported, 1);
        QCOMPARE(changes, 2);
        QVERIFY2(message.contains(QStringLiteral("Imported embedded metadata for one item.")), qPrintable(message));
        TagLibrary::Library library = TagLibrary::Library::load(root);
        QCOMPARE(library.entry(QStringLiteral("new.jpg"))->fields.value(QStringLiteral("maker")), QStringLiteral("Acme"));
        QVERIFY(library.entry(QStringLiteral("old.jpg"))->fields.isEmpty());
        QVERIFY(library.entry(QStringLiteral("notes.txt")));

        // A refresh that adds nothing imports nothing.
        refreshWithImport(&message, &imported, &changes);
        QCOMPARE(imported, 0);
        QVERIFY(!message.contains(QStringLiteral("embedded metadata")));

        // Tags created by the import get the default tag colour.
        Settings::setEmbeddedMetadataMappings({QStringLiteral("manufacturer=tags")});
        Settings::setDefaultTagColor(QStringLiteral("#336699"));
        QVERIFY(!writeExifJpeg(root + QStringLiteral("/third.jpg")).isEmpty());
        refreshWithImport(&message, &imported, &changes);
        QCOMPARE(imported, 1);
        library = TagLibrary::Library::load(root);
        const QStringList acme = library.findTags(QStringLiteral("Acme"));
        QCOMPARE(acme.size(), 1);
        QCOMPARE(library.tag(acme.constFirst())->color, QStringLiteral("#336699"));
        QCOMPARE(library.entry(QStringLiteral("third.jpg"))->tags, acme);

        // Automatic refreshes import too, and say so.
        LibraryAutoRefresh *refresher = LibraryAutoRefresh::instance();
        QSignalSpy refreshed(refresher, &LibraryAutoRefresh::refreshed);
        QVERIFY(!writeExifJpeg(root + QStringLiteral("/fourth.jpg")).isEmpty());
        refresher->enqueue(root, true);
        QVERIFY(refreshed.wait(10000));
        QCOMPARE(refreshed.constLast().at(0).toString(), root);
        QCOMPARE(refreshed.constLast().at(1).toString(), QStringLiteral("Refreshed library Photos: 1 added; imported embedded metadata for one item"));
        QVERIFY(refreshed.constLast().at(2).toBool());
        QVERIFY(!refresher->isPending(root));
        QCOMPARE(TagLibrary::Library::load(root).entry(QStringLiteral("fourth.jpg"))->tags, acme);

        // Disabled properties are not imported.
        Settings::setDisabledEmbeddedMetadataKeys({QStringLiteral("manufacturer")});
        QVERIFY(!TaggingSettings::importsEmbeddedMetadataOnRefresh());
        QVERIFY(!writeExifJpeg(root + QStringLiteral("/fifth.jpg")).isEmpty());
        refreshWithImport(&message, &imported, &changes);
        QCOMPARE(imported, 0);
        QVERIFY(TagLibrary::Library::load(root).entry(QStringLiteral("fifth.jpg"))->tags.isEmpty());
    }

    void exportsAndRestoresTheLibrary()
    {
        QTemporaryDir dir;
        QTemporaryDir exports;
        QVERIFY(dir.isValid() && exports.isValid());
        write(dir.filePath(QStringLiteral("a.txt")));
        write(dir.filePath(QStringLiteral("b.txt")));
        QCOMPARE(LibraryRegistry::create(dir.path()), QString());
        indexWithTag(dir.path(), {QStringLiteral("a.txt")});
        const QString exported = exports.filePath(QStringLiteral("tags.json"));
        const auto addTag = [&dir](const QString &id, const QString &name) {
            QVERIFY(TagLibrary::update(dir.path(), [id, name](TagLibrary::Library &library, QString *problem) {
                return library.setTag({id, name, {}, {}, {}, {}}, problem);
            }));
        };

        {
            LibraryExportDialog dialog(dir.path());
            QSignalSpy done(&dialog, &LibraryExportDialog::exported);
            auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("libraryExportApply"));
            auto *status = dialog.findChild<QLabel *>(QStringLiteral("libraryExportDestinationStatus"));
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("libraryExportSummary"))->text().contains(QStringLiteral("1 tag")));
            QVERIFY(dialog.destination().endsWith(QStringLiteral(".json")));

            // Files other than earlier exports are never replaced.
            dialog.setDestination(dir.filePath(QStringLiteral("a.txt")));
            QVERIFY(!apply->isEnabled());
            QVERIFY(!status->text().isEmpty());
            QVERIFY(!dialog.exportLibrary());
            dialog.setDestination(exported);
            QVERIFY(apply->isEnabled());
            QVERIFY(status->text().isEmpty());

            // A library changed while the dialog is open is summarized again before it is exported.
            addTag(QStringLiteral("t2"), QStringLiteral("Extra"));
            QVERIFY(!dialog.exportLibrary());
            QVERIFY(!QFileInfo::exists(exported));
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("libraryExportSummary"))->text().contains(QStringLiteral("2 tags")));
            QVERIFY(dialog.exportLibrary());
            QCOMPARE(done.count(), 1);
            QCOMPARE(done.first().first().toString(), exported);
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
        }
        QVERIFY(QFileInfo::exists(exported));
        QFile file(dir.filePath(QStringLiteral("a.txt")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("data"));
        file.close();

        QVERIFY(TagLibrary::update(dir.path(), [](TagLibrary::Library &library, QString *) {
            library.removeTag(QStringLiteral("t1"));
            return true;
        }));
        {
            LibraryRestoreDialog dialog(dir.path());
            QSignalSpy changed(&dialog, &LibraryRestoreDialog::libraryChanged);
            auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("libraryRestorePreview"));
            auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("libraryRestoreApply"));
            auto *confirm = dialog.findChild<QCheckBox *>(QStringLiteral("libraryRestoreConfirm"));
            auto *changes = dialog.findChild<QTreeWidget *>(QStringLiteral("libraryRestoreChanges"));
            QVERIFY(!preview->isEnabled());
            QVERIFY(!apply->isEnabled());
            dialog.setSource(exported);
            QVERIFY(preview->isEnabled());
            preview->click();
            QVERIFY(dialog.plan().hasChanges());
            QCOMPARE(changes->topLevelItemCount(), 2);
            QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("libraryRestoreDetails"))->text().isEmpty());

            // Restoring needs explicit confirmation after the preview.
            QVERIFY(!apply->isEnabled());
            QVERIFY(!dialog.restore());
            QVERIFY(confirm->isEnabled());
            confirm->setChecked(true);
            QVERIFY(apply->isEnabled());

            // Choosing another export needs another preview and confirmation.
            dialog.setSource(exported + QStringLiteral(".missing"));
            QVERIFY(!apply->isEnabled());
            QVERIFY(!confirm->isChecked());
            QCOMPARE(changes->topLevelItemCount(), 0);
            QVERIFY(!dialog.preview());
            QVERIFY(dialog.findChild<KMessageWidget *>(QStringLiteral("libraryRestoreError"))->isVisibleTo(&dialog));
            dialog.setSource(exported);
            QVERIFY(dialog.preview());
            confirm->setChecked(true);
            QVERIFY(dialog.restore());
            QCOMPARE(changed.count(), 1);
            QVERIFY(!dialog.backupPath().isEmpty());
            QVERIFY(QFileInfo::exists(dialog.backupPath()));
        }
        const TagLibrary::Library library = TagLibrary::Library::load(dir.path());
        QCOMPARE(library.entry(QStringLiteral("a.txt"))->tags, QStringList{QStringLiteral("t1")});
        QVERIFY(library.tag(QStringLiteral("t2")));

        // Another preview finds nothing to restore.
        {
            LibraryRestoreDialog dialog(dir.path());
            dialog.setSource(exported);
            QVERIFY(dialog.preview());
            QVERIFY(!dialog.plan().hasChanges());
            QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("libraryRestoreConfirm"))->isEnabled());
            QVERIFY(!dialog.restore());
        }

        // The controller offers both for the current library only.
        QWidget window;
        KActionCollection actions(&window);
        QUrl folder = QUrl::fromLocalFile(dir.path());
        TaggingController controller(&window,
                                     &actions,
                                     {[&folder] {
                                          return folder;
                                      },
                                      [] {
                                          return KFileItemList();
                                      },
                                      [](const QString &) { }});
        controller.updateActions();
        QVERIFY(actions.action(QStringLiteral("export_library"))->isEnabled());
        QVERIFY(actions.action(QStringLiteral("restore_library"))->isEnabled());
        actions.action(QStringLiteral("export_library"))->trigger();
        auto *exportDialog = window.findChild<LibraryExportDialog *>();
        QVERIFY(exportDialog);
        exportDialog->reject();
        actions.action(QStringLiteral("restore_library"))->trigger();
        auto *restoreDialog = window.findChild<LibraryRestoreDialog *>();
        QVERIFY(restoreDialog);
        restoreDialog->reject();
        folder = QUrl::fromLocalFile(exports.path());
        controller.updateActions();
        QVERIFY(!actions.action(QStringLiteral("export_library"))->isEnabled());
        QVERIFY(!actions.action(QStringLiteral("restore_library"))->isEnabled());
        QDir(LibraryArchive::backupDirectory()).removeRecursively();
    }
};

QTEST_MAIN(TaggingUiTest)

#include "tagginguitest.moc"
