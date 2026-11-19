// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercomparison.h"

#include <KIO/UDSEntry>

#include <QStandardPaths>
#include <QTest>
#include <sys/stat.h>

namespace
{
KFileItem item(const QString &name, mode_t type, qint64 size = -1, qint64 modified = -1, const QString &link = {})
{
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
    if (size >= 0) {
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, size);
    }
    if (modified >= 0) {
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, modified);
    }
    if (!link.isEmpty()) {
        entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, link);
    }
    return KFileItem(entry, QUrl::fromLocalFile(QStringLiteral("/test")), true, true);
}
}

class FolderComparisonTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void classifiesWithoutAssumingContentEquality()
    {
        using S = FolderComparison::Status;
        const KFileItemList left{
            item(QStringLiteral("only-left"), S_IFREG, 1, 100),
            item(QStringLiteral("different-size"), S_IFREG, 1, 100),
            item(QStringLiteral("different-time"), S_IFREG, 2, 100),
            item(QStringLiteral("metadata-match"), S_IFREG, 3, 100),
            item(QStringLiteral("missing-size"), S_IFREG, -1, 100),
            item(QStringLiteral("missing-time"), S_IFREG, 1, -1),
            item(QStringLiteral("folder"), S_IFDIR),
            item(QStringLiteral("type"), S_IFDIR),
            item(QStringLiteral("link"), S_IFREG, 0, 100, QStringLiteral("target-a")),
        };
        const KFileItemList right{
            item(QStringLiteral("only-right"), S_IFREG, 1, 100),
            item(QStringLiteral("different-size"), S_IFREG, 2, 100),
            item(QStringLiteral("different-time"), S_IFREG, 2, 101),
            item(QStringLiteral("metadata-match"), S_IFREG, 3, 100),
            item(QStringLiteral("missing-size"), S_IFREG, 1, 100),
            item(QStringLiteral("missing-time"), S_IFREG, 1, 100),
            item(QStringLiteral("folder"), S_IFDIR),
            item(QStringLiteral("type"), S_IFREG, 1, 100),
            item(QStringLiteral("link"), S_IFREG, 0, 100, QStringLiteral("target-b")),
        };
        const auto result = FolderComparison::compare(left, right);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.rows.size(), 10);
        QHash<QString, S> statuses;
        for (const auto &row : result.rows) {
            statuses.insert(row.name, row.status);
        }
        QCOMPARE(statuses.value(QStringLiteral("only-left")), S::LeftOnly);
        QCOMPARE(statuses.value(QStringLiteral("only-right")), S::RightOnly);
        QCOMPARE(statuses.value(QStringLiteral("different-size")), S::SizeDifferent);
        QCOMPARE(statuses.value(QStringLiteral("different-time")), S::TimeDifferent);
        QCOMPARE(statuses.value(QStringLiteral("metadata-match")), S::MetadataMatches);
        QCOMPARE(statuses.value(QStringLiteral("missing-size")), S::Unknown);
        QCOMPARE(statuses.value(QStringLiteral("missing-time")), S::Unknown);
        QCOMPARE(statuses.value(QStringLiteral("folder")), S::BothFolders);
        QCOMPARE(statuses.value(QStringLiteral("type")), S::TypeDifferent);
        QCOMPARE(statuses.value(QStringLiteral("link")), S::LinkDifferent);
        QVERIFY(FolderComparison::isDifference(S::Unknown));
        QVERIFY(!FolderComparison::isDifference(S::MetadataMatches));
        QVERIFY(!FolderComparison::compare({left.first(), left.first()}, {}).error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FolderComparisonTest)
#include "foldercomparisontest.moc"
