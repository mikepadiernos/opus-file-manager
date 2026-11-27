// SPDX-License-Identifier: GPL-3.0-or-later

#include "filemanagerrequest.h"
#include "appidentity.h"

#include <QTest>

class FileManagerRequestTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesUrisAndPaths()
    {
        QStringList rejected;
        const QList<QUrl> urls = FileManagerRequest::parseUris({QStringLiteral("file:///home/user/a%20b.txt"),
                                                                QStringLiteral("/home/user/folder/"),
                                                                QStringLiteral("/home/user/../user/c"),
                                                                QStringLiteral("sftp://host/srv/data"),
                                                                QStringLiteral("file:///home/user/a%20b.txt"),
                                                                QStringLiteral("relative/path"),
                                                                QStringLiteral(""),
                                                                QStringLiteral("file:///")},
                                                               &rejected);
        QCOMPARE(urls,
                 QList<QUrl>({QUrl::fromLocalFile(QStringLiteral("/home/user/a b.txt")),
                              QUrl::fromLocalFile(QStringLiteral("/home/user/folder")),
                              QUrl::fromLocalFile(QStringLiteral("/home/user/c")),
                              QUrl(QStringLiteral("sftp://host/srv/data")),
                              QUrl::fromLocalFile(QStringLiteral("/"))}));
        QCOMPARE(rejected, QStringList({QStringLiteral("relative/path"), QString()}));
    }

    void groupsItemsByFolder()
    {
        const QUrl a = QUrl::fromLocalFile(QStringLiteral("/data/one/a.txt"));
        const QUrl b = QUrl::fromLocalFile(QStringLiteral("/data/two/b.txt"));
        const QUrl c = QUrl::fromLocalFile(QStringLiteral("/data/one/c.txt"));
        const QUrl top = QUrl::fromLocalFile(QStringLiteral("/data"));
        const QUrl root = QUrl::fromLocalFile(QStringLiteral("/"));
        const QUrl remote(QStringLiteral("sftp://host/srv/file"));

        const QList<FileManagerRequest::Reveal> reveals = FileManagerRequest::groupByFolder({a, b, c, a, top, root, remote});
        QCOMPARE(reveals.size(), 4);
        QCOMPARE(reveals.at(0).folder, QUrl::fromLocalFile(QStringLiteral("/data/one")));
        QCOMPARE(reveals.at(0).items, QList<QUrl>({a, c}));
        QCOMPARE(reveals.at(1).folder, QUrl::fromLocalFile(QStringLiteral("/data/two")));
        QCOMPARE(reveals.at(1).items, QList<QUrl>({b}));
        // The root folder has no parent, so it adds no selection to the entry
        // it shares with /data.
        QCOMPARE(reveals.at(2).folder, root);
        QCOMPARE(reveals.at(2).items, QList<QUrl>({top}));
        QCOMPARE(reveals.at(3).folder, QUrl(QStringLiteral("sftp://host/srv")));
        QCOMPARE(reveals.at(3).items, QList<QUrl>({remote}));
    }

    void rootOpensWithoutSelection()
    {
        const QUrl root = QUrl::fromLocalFile(QStringLiteral("/"));
        const QList<FileManagerRequest::Reveal> reveals = FileManagerRequest::groupByFolder({root});
        QCOMPARE(reveals.size(), 1);
        QCOMPARE(reveals.first().folder, root);
        QVERIFY(reveals.first().items.isEmpty());
    }

    void claimsOnlyAsDefaultFolderHandler()
    {
        QVERIFY(FileManagerRequest::shouldClaim(AppIdentity::desktopFileName()));
        QVERIFY(FileManagerRequest::shouldClaim(AppIdentity::desktopFileName() + QStringLiteral(".desktop")));
        QVERIFY(!FileManagerRequest::shouldClaim(QStringLiteral("org.kde.dolphin.desktop")));
        QVERIFY(!FileManagerRequest::shouldClaim(QString()));
    }
};

QTEST_GUILESS_MAIN(FileManagerRequestTest)

#include "filemanagerrequesttest.moc"
