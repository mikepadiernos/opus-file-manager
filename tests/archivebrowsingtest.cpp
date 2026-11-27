// SPDX-License-Identifier: GPL-3.0-or-later

#include "compressdialog.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KSharedConfig>
#include <KZip>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class ArchiveBrowsingTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_source;
    QUrl m_destination;
    QUrl m_archive;

    static KFileItem itemNamed(FolderView *view, const QString &name)
    {
        const KFileItemList items = view->items();
        for (const KFileItem &item : items) {
            if (item.name() == name) {
                return item;
            }
        }
        return {};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("source")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("destination")));
        KZip zip(m_root.filePath(QStringLiteral("source/photos.zip")));
        QVERIFY(zip.open(QIODevice::WriteOnly));
        QVERIFY(zip.writeFile(QStringLiteral("inner/readme.txt"), QByteArrayLiteral("hello")));
        QVERIFY(zip.close());
        m_source = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("source")));
        m_destination = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("destination")));
        m_archive = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("source/photos.zip")));
        QFile note(m_root.filePath(QStringLiteral("source/note.txt")));
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("hello");
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void openingAnArchiveBrowsesIt()
    {
        MainWindow window;
        window.openUrls({m_source, m_destination});
        FolderView *view = window.activePane()->view();
        QTRY_VERIFY(!itemNamed(view, QStringLiteral("photos.zip")).isNull());

        Q_EMIT view->itemActivated(itemNamed(view, QStringLiteral("photos.zip")));
        QVERIFY(window.activePane()->url().scheme() != QStringLiteral("file"));
        QVERIFY(window.activePane()->url().path().startsWith(m_archive.path()));
        QTRY_VERIFY(!itemNamed(view, QStringLiteral("inner")).isNull());
    }

    void goingUpLeavesTheArchive()
    {
        MainWindow window;
        window.openUrls({m_source, m_destination});
        FolderView *view = window.activePane()->view();
        QTRY_VERIFY(!itemNamed(view, QStringLiteral("photos.zip")).isNull());
        Q_EMIT view->itemActivated(itemNamed(view, QStringLiteral("photos.zip")));
        QTRY_VERIFY(!itemNamed(view, QStringLiteral("inner")).isNull());

        window.activePane()->goUp();
        QTRY_COMPARE(window.activePane()->url().adjusted(QUrl::StripTrailingSlash), m_source);
    }

    void compressDialogDefaultsAndValidation()
    {
        CompressDialog dialog(QStringLiteral("note"), false, nullptr);
        QCOMPARE(dialog.archiveName(), QStringLiteral("note"));
        QCOMPARE(dialog.format(), ArchiveWriter::Format::Zip);
        QVERIFY(!dialog.toOtherPane());
        QVERIFY(!dialog.findChild<QComboBox *>(QStringLiteral("archiveLocation"))->isEnabled());

        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("archiveName"));
        QPushButton *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        name->setText(QStringLiteral("  "));
        QVERIFY(!ok->isEnabled());
        name->setText(QStringLiteral("a/b"));
        QVERIFY(!ok->isEnabled());
        name->setText(QStringLiteral(" Backup "));
        QVERIFY(ok->isEnabled());
        QCOMPARE(dialog.archiveName(), QStringLiteral("Backup"));
        auto *password = dialog.findChild<QLineEdit *>(QStringLiteral("archivePassword"));
        auto *confirmation = dialog.findChild<QLineEdit *>(QStringLiteral("archivePasswordConfirmation"));
        password->setText(QStringLiteral("secret"));
        QVERIFY(!ok->isEnabled());
        confirmation->setText(QStringLiteral("secret"));
        QVERIFY(ok->isEnabled());
        QCOMPARE(dialog.password(), QStringLiteral("secret"));
        dialog.findChild<QComboBox *>(QStringLiteral("archiveFormat"))->setCurrentIndex(ArchiveWriter::formats().indexOf(ArchiveWriter::Format::TarGz));
        QVERIFY(!password->isEnabled());
        QVERIFY(!ok->isEnabled());
    }

    void compressDialogSplitsAndHandsOffToArk()
    {
        CompressDialog plain(QStringLiteral("note"), false, nullptr);
        QVERIFY(!plain.findChild<QPushButton *>(QStringLiteral("compressWithArk"))->isEnabled());
        QCOMPARE(plain.volumeSize(), 0);
        auto *split = plain.findChild<QCheckBox *>(QStringLiteral("archiveSplit"));
        auto *size = plain.findChild<QSpinBox *>(QStringLiteral("archiveVolumeSize"));
        QVERIFY(split && size && !size->isEnabled());
        split->setChecked(true);
        QVERIFY(size->isEnabled());
        size->setValue(5);
        QCOMPARE(plain.volumeSize(), qint64(5) * 1024 * 1024);
        plain.findChild<QComboBox *>(QStringLiteral("archiveFormat"))->setCurrentIndex(ArchiveWriter::formats().indexOf(ArchiveWriter::Format::SevenZip));
        QCOMPARE(plain.format(), ArchiveWriter::Format::SevenZip);
        QVERIFY(!plain.findChild<QLineEdit *>(QStringLiteral("archivePassword"))->isEnabled());

        CompressDialog withArk(QStringLiteral("note"), false, nullptr, ArchiveWriter::Format::Zip, false, true);
        auto *ark = withArk.findChild<QPushButton *>(QStringLiteral("compressWithArk"));
        QVERIFY(ark->isEnabled());
        QTimer::singleShot(0, ark, &QPushButton::click);
        QCOMPARE(withArk.exec(), CompressDialog::UseArk);
    }

    void compressSplitsIntoVolumes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("source")));
        QFile input(root.filePath(QStringLiteral("source/data.bin")));
        QVERIFY(input.open(QIODevice::WriteOnly));
        const QByteArray data(3 * 1024 * 1024 + 512 * 1024, 'x');
        QVERIFY(input.write(data) == data.size());
        input.close();
        QFile existing(root.filePath(QStringLiteral("source/data.zip")));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("source")))});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({QUrl::fromLocalFile(input.fileName())});
        QCOMPARE(view->selectedItems().size(), 1);
        auto *poll = new QTimer(&window);
        QObject::connect(poll, &QTimer::timeout, poll, [poll] {
            auto *dialog = qobject_cast<CompressDialog *>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            poll->deleteLater();
            dialog->findChild<QComboBox *>(QStringLiteral("archiveFormat"))
                ->setCurrentIndex(ArchiveWriter::formats().indexOf(ArchiveWriter::Format::ZipStored));
            dialog->findChild<QCheckBox *>(QStringLiteral("archiveSplit"))->setChecked(true);
            dialog->findChild<QSpinBox *>(QStringLiteral("archiveVolumeSize"))->setValue(1);
            dialog->accept();
        });
        poll->start(20);
        window.actionCollection()->action(QStringLiteral("compress"))->trigger();
        const QDir folder(root.filePath(QStringLiteral("source")));
        QTRY_COMPARE(folder.entryList({QStringLiteral("*.zip.0*")}).size(), 4);
        const QStringList volumes = folder.entryList({QStringLiteral("*.zip.0*")});
        QVERIFY2(volumes.first().endsWith(QStringLiteral(".zip.001")) && volumes.first() != QStringLiteral("data.zip.001"), qPrintable(volumes.join(u',')));
        QCOMPARE(folder.entryList({QStringLiteral("*.zip")}), QStringList{QStringLiteral("data.zip")});
        QCOMPARE(QFileInfo(folder.filePath(volumes.first())).size(), 1024 * 1024);
        QCOMPARE(existing.size(), 0);
    }

    void compressWritesAnArchiveAndNeverOverwrites()
    {
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("pack")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("packed")));
        QFile note(m_root.filePath(QStringLiteral("pack/note.txt")));
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.close();
        const QUrl pack = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("pack")));

        MainWindow window;
        window.openUrls({pack, QUrl::fromLocalFile(m_root.filePath(QStringLiteral("packed")))});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(note.fileName())});
        QCOMPARE(view->selectedItems().size(), 1);

        // Picks a format and location in the dialog once it is shown, then accepts it.
        const auto answer = [&window](ArchiveWriter::Format format, bool otherPane) {
            auto *poll = new QTimer(&window);
            QObject::connect(poll, &QTimer::timeout, poll, [poll, format, otherPane] {
                auto *dialog = qobject_cast<CompressDialog *>(QApplication::activeModalWidget());
                if (!dialog) {
                    return;
                }
                poll->deleteLater();
                dialog->findChild<QComboBox *>(QStringLiteral("archiveFormat"))->setCurrentIndex(ArchiveWriter::formats().indexOf(format));
                dialog->findChild<QComboBox *>(QStringLiteral("archiveLocation"))->setCurrentIndex(otherPane ? 1 : 0);
                dialog->accept();
            });
            poll->start(20);
        };
        QAction *compress = window.actionCollection()->action(QStringLiteral("compress"));
        QVERIFY(compress);

        answer(ArchiveWriter::Format::TarGz, false);
        compress->trigger();
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("pack/note.tar.gz"))));

        answer(ArchiveWriter::Format::TarGz, false);
        compress->trigger();
        QTRY_COMPARE(QDir(pack.toLocalFile()).entryList({QStringLiteral("*.tar.gz")}).size(), 2);

        answer(ArchiveWriter::Format::Zip, true);
        compress->trigger();
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("packed/note.zip"))));
    }

    void defaultOtherPaneDestinationPreservesExistingArchive()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("source")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
        QFile input(root.filePath(QStringLiteral("source/note.txt")));
        QVERIFY(input.open(QIODevice::WriteOnly));
        QCOMPARE(input.write("new"), 3);
        input.close();
        QFile existing(root.filePath(QStringLiteral("destination/note.zip")));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("existing"), 8);
        existing.close();
        Settings::setArchiveOtherPaneByDefault(true);
        const auto restore = qScopeGuard([] {
            Settings::setArchiveOtherPaneByDefault(false);
        });
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("source"))), QUrl::fromLocalFile(root.filePath(QStringLiteral("destination")))});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(input.fileName())});
        QCOMPARE(view->selectedItems().size(), 1);
        bool choseOtherPane = false;
        auto *poll = new QTimer(&window);
        connect(poll, &QTimer::timeout, &window, [poll, &choseOtherPane] {
            auto *dialog = qobject_cast<CompressDialog *>(QApplication::activeModalWidget());
            if (dialog) {
                choseOtherPane = dialog->toOtherPane();
                poll->deleteLater();
                dialog->accept();
            }
        });
        poll->start(20);
        window.actionCollection()->action(QStringLiteral("compress"))->trigger();
        QVERIFY(choseOtherPane);
        QTRY_COMPARE(QDir(root.filePath(QStringLiteral("destination"))).entryList({QStringLiteral("*.zip")}).size(), 2);
        const auto written = [&root] {
            const QDir destination(root.filePath(QStringLiteral("destination")));
            const QStringList names = destination.entryList({QStringLiteral("*.zip")});
            for (const QString &name : names) {
                if (name != QLatin1String("note.zip")) {
                    KZip archive(destination.filePath(name));
                    if (archive.open(QIODevice::ReadOnly) && archive.directory()->file(QStringLiteral("note.txt"))) {
                        return true;
                    }
                }
            }
            return false;
        };
        QTRY_VERIFY(written());
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArrayLiteral("existing"));
    }

    void extractToOtherPaneNeverOverwrites()
    {
        MainWindow window;
        window.openUrls({m_source, m_destination});
        FolderView *view = window.activePane()->view();
        // The view's model fills after the lister, which a cached listing fills at once.
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({m_archive});
        QCOMPARE(view->selectedItems().size(), 1);

        QAction *extract = window.actionCollection()->action(QStringLiteral("extract_to_other_pane"));
        QVERIFY(extract);
        extract->trigger();
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("destination/photos/inner/readme.txt"))));

        extract->trigger();
        QTRY_COMPARE(QDir(m_destination.toLocalFile()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 2);
    }
};

QTEST_MAIN(ArchiveBrowsingTest)

#include "archivebrowsingtest.moc"
