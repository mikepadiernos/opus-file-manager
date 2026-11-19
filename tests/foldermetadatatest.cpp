// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldermetadata.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class FolderMetadataTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void metadataRoundTripsAndClears()
    {
        QVERIFY(FolderMetadata::setLabel(m_dir.path(), QStringLiteral("photo.jpg"), QStringLiteral("blue")));
        QVERIFY(FolderMetadata::setColor(m_dir.path(), QStringLiteral("photo.jpg"), QStringLiteral("#abc")));
        QVERIFY(FolderMetadata::setFolderColor(m_dir.path(), QStringLiteral("#123456")));
        QVERIFY(FolderMetadata::setRating(m_dir.path(), QStringLiteral("photo.jpg"), 5));
        QVERIFY(FolderMetadata::setNote(m_dir.path(), QStringLiteral("photo.jpg"), QStringLiteral("  favorite  ")));
        QVERIFY(FolderMetadata::togglePin(m_dir.path(), QStringLiteral("photo.jpg")));

        const QJsonObject metadata = FolderMetadata::read(m_dir.path());
        QCOMPARE(metadata.value(QStringLiteral("labels")).toObject().value(QStringLiteral("photo.jpg")).toString(), QStringLiteral("blue"));
        QCOMPARE(metadata.value(QStringLiteral("colors")).toObject().value(QStringLiteral("photo.jpg")).toString(), QStringLiteral("#abc"));
        QCOMPARE(metadata.value(QStringLiteral("color")).toString(), QStringLiteral("#123456"));
        QCOMPARE(metadata.value(QStringLiteral("ratings")).toObject().value(QStringLiteral("photo.jpg")).toInt(), 5);
        QCOMPARE(metadata.value(QStringLiteral("notes")).toObject().value(QStringLiteral("photo.jpg")).toString(), QStringLiteral("favorite"));
        QCOMPARE(metadata.value(QStringLiteral("pins")).toArray().at(0).toString(), QStringLiteral("photo.jpg"));

        QVERIFY(FolderMetadata::setRating(m_dir.path(), QStringLiteral("photo.jpg"), 0));
        QVERIFY(FolderMetadata::setNote(m_dir.path(), QStringLiteral("photo.jpg"), QString()));
        QVERIFY(FolderMetadata::togglePin(m_dir.path(), QStringLiteral("photo.jpg")));
        QVERIFY(FolderMetadata::setLabel(m_dir.path(), QStringLiteral("photo.jpg"), QString()));
        QVERIFY(FolderMetadata::setColor(m_dir.path(), QStringLiteral("photo.jpg"), QString()));
        QVERIFY(FolderMetadata::setFolderColor(m_dir.path(), QString()));
        QVERIFY(!QFile::exists(m_dir.filePath(QStringLiteral(".mfm.json"))));
    }

    void rejectsUnsafeNamesAndInvalidValues()
    {
        QString error;
        QVERIFY(!FolderMetadata::setLabel(m_dir.path(), QStringLiteral("../escape"), QStringLiteral("red"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!FolderMetadata::setLabel(m_dir.path(), QStringLiteral("file"), QStringLiteral("unknown")));
        QVERIFY(!FolderMetadata::setColor(m_dir.path(), QStringLiteral("file"), QStringLiteral("red")));
        QVERIFY(!FolderMetadata::setRating(m_dir.path(), QStringLiteral("file"), 6));
        QVERIFY(!FolderMetadata::setNote(m_dir.path(), QStringLiteral("file"), QString(501, u'x')));
    }

    void formatRoundTrips()
    {
        FolderMetadata::Format expected;
        expected.view = QStringLiteral("grid");
        expected.columns = {QStringLiteral("size"), QStringLiteral("rating")};
        expected.sortKey = QStringLiteral("size");
        expected.sortAscending = false;
        expected.group = QStringLiteral("type");
        QVERIFY(FolderMetadata::setFormat(m_dir.path(), expected));

        FolderMetadata::Format actual;
        QVERIFY(FolderMetadata::format(FolderMetadata::read(m_dir.path()), &actual));
        QCOMPARE(actual.view, expected.view);
        QCOMPARE(actual.columns, expected.columns);
        QCOMPARE(actual.sortKey, expected.sortKey);
        QCOMPARE(actual.sortAscending, expected.sortAscending);
        QCOMPARE(actual.group, expected.group);
    }

    void sanitizesLoadedMetadata()
    {
        const QJsonObject input{
            {QStringLiteral("labels"), QJsonObject{{QStringLiteral("good"), QStringLiteral("green")}, {QStringLiteral("../bad"), QStringLiteral("red")}}},
            {QStringLiteral("ratings"), QJsonObject{{QStringLiteral("good"), 9}, {QStringLiteral("bad"), 2}}},
            {QStringLiteral("colors"), QJsonObject{{QStringLiteral("good"), QStringLiteral("blue")}}},
            {QStringLiteral("hidden"), true},
            {QStringLiteral("icon"), QStringLiteral("/usr/share/icons/folder.svg")},
            {QStringLiteral("invalidIcon"), QStringLiteral("relative/icon.svg")},
        };
        QFile file(m_dir.filePath(QStringLiteral(".mfm.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(input).toJson());
        file.close();

        const QJsonObject actual = FolderMetadata::read(m_dir.path());
        QCOMPARE(actual.value(QStringLiteral("labels")).toObject().size(), 1);
        QCOMPARE(actual.value(QStringLiteral("ratings")).toObject().size(), 1);
        QVERIFY(actual.value(QStringLiteral("colors")).toObject().isEmpty());
        QVERIFY(actual.value(QStringLiteral("hidden")).toBool());
        QCOMPARE(actual.value(QStringLiteral("icon")).toString(), QStringLiteral("/usr/share/icons/folder.svg"));
    }
};

QTEST_GUILESS_MAIN(FolderMetadataTest)

#include "foldermetadatatest.moc"
