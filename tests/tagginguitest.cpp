// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryrefreshdialog.h"
#include "libraryregistry.h"
#include "taglibrary.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

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
};

QTEST_MAIN(TaggingUiTest)

#include "tagginguitest.moc"
