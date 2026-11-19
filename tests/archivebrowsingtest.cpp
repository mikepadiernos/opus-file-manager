// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KSharedConfig>
#include <KZip>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

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

    void extractToOtherPaneNeverOverwrites()
    {
        MainWindow window;
        window.openUrls({m_source, m_destination});
        FolderView *view = window.activePane()->view();
        // The view's model fills after the lister, which a cached listing fills at once.
        QTRY_COMPARE(view->itemCount(), 1);
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
