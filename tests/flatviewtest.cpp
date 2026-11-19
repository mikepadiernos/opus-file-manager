// SPDX-License-Identifier: GPL-3.0-or-later

#include "flatview.h"

#include <QTest>

class FlatViewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void listingUrlRoundTrips()
    {
        const QUrl folder = QUrl::fromLocalFile(QStringLiteral("/data/my photos"));
        const QUrl listing = FlatView::listingUrl(folder);
        QCOMPARE(listing.scheme(), QStringLiteral("filenamesearch"));
        QCOMPARE(FlatView::folderOf(listing), folder);
        QVERIFY(FlatView::folderOf(folder).isEmpty());
    }

    void relativeFolders()
    {
        const QUrl base = QUrl::fromLocalFile(QStringLiteral("/data/flat"));
        QCOMPARE(FlatView::relativeFolder(base, QUrl::fromLocalFile(QStringLiteral("/data/flat/x/y/deep.txt"))), QStringLiteral("x/y"));
        QCOMPARE(FlatView::relativeFolder(base, QUrl::fromLocalFile(QStringLiteral("/data/flat/top.txt"))), QString());
        QCOMPARE(FlatView::relativeFolder(QUrl::fromLocalFile(QStringLiteral("/data/flat/")), QUrl::fromLocalFile(QStringLiteral("/data/flat/x/a.txt"))),
                 QStringLiteral("x"));
    }
};

QTEST_GUILESS_MAIN(FlatViewTest)

#include "flatviewtest.moc"
