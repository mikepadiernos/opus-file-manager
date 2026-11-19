// SPDX-License-Identifier: GPL-3.0-or-later

#include "draftstore.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class DraftStoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripAndIndependentDrafts()
    {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("private"));
        DraftStore store(path);
        const QUrl source = QUrl::fromLocalFile(directory.filePath(QStringLiteral("source")));
        const QString one = QStringLiteral("00000000-0000-0000-0000-000000000001");
        const QString two = QStringLiteral("00000000-0000-0000-0000-000000000002");
        QVERIFY(store.write({one, QStringLiteral("first"), source, 10}).isEmpty());
        QVERIFY(store.write({two, QStringLiteral("second"), source, 20}).isEmpty());
        QVERIFY(!(QFileInfo(path).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
        QString error;
        auto drafts = store.drafts(source, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(drafts.size(), 2);
        QCOMPARE(drafts.at(0).text, QStringLiteral("second"));
        QVERIFY(store.write({one, QStringLiteral("changed"), source, 30}).isEmpty());
        drafts = store.drafts(source, &error);
        QCOMPARE(drafts.at(0).text, QStringLiteral("changed"));
        QVERIFY(store.remove(one).isEmpty());
        QCOMPARE(store.drafts(source, &error).size(), 1);
        QCOMPARE(store.drafts(QUrl::fromLocalFile(QStringLiteral("/other")), &error).size(), 0);
        QVERIFY(!store.write({two, QString(8 * 1024 * 1024 + 1, QLatin1Char('x')), source, 40}).isEmpty());
        QCOMPARE(store.drafts(source, &error).at(0).text, QStringLiteral("second"));
        QVERIFY(!store.remove(QStringLiteral("../invalid")).isEmpty());
    }

    void malformedDraftIsReported()
    {
        QTemporaryDir directory;
        DraftStore store(directory.path());
        QFile bad(directory.filePath(QStringLiteral("00000000-0000-0000-0000-000000000001.json")));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("{");
        bad.close();
        QString error;
        QVERIFY(store.drafts(QUrl(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void discardRejectsChangedDraft()
    {
        QTemporaryDir directory;
        DraftStore writer(directory.path());
        DraftStore reader(directory.path());
        const QUrl source = QUrl::fromLocalFile(directory.filePath(QStringLiteral("source")));
        const QString id = QStringLiteral("00000000-0000-0000-0000-000000000001");
        QVERIFY(writer.write({id, QStringLiteral("first"), source, 10}).isEmpty());
        QString error;
        const auto draft = reader.drafts(source, &error).first();
        QVERIFY(writer.write({id, QStringLiteral("newer"), source, 20}).isEmpty());
        writer.release(id);
        QVERIFY(!reader.remove(id, &draft).isEmpty());
        QCOMPARE(reader.drafts(source, &error).first().text, QStringLiteral("newer"));
    }

    void retentionPreservesInUseDrafts()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("drafts"));
        const QUrl source = QUrl::fromLocalFile(directory.filePath(QStringLiteral("source")));
        const QString id = QStringLiteral("00000000-0000-0000-0000-000000000001");
        const QString recent = QStringLiteral("00000000-0000-0000-0000-000000000002");
        DraftStore active(path);
        DraftStore other(path);
        QVERIFY(active.write({id, QStringLiteral("unsaved"), source, 1000}).isEmpty());
        QVERIFY(active.write({recent, QStringLiteral("recent"), source, 190000000}).isEmpty());
        active.release(recent);
        QVERIFY(other.prune(1, 200000000).isEmpty());
        QString error;
        QCOMPARE(other.drafts(source, &error).size(), 2);
        QVERIFY(!other.remove(id).isEmpty());
        active.release(id);
        QVERIFY(other.prune(0, 200000000).isEmpty());
        QCOMPARE(other.drafts(source, &error).size(), 2);
        QVERIFY(other.prune(1, 200000000).isEmpty());
        QCOMPARE(other.drafts(source, &error).size(), 1);
        QCOMPARE(other.drafts(source, &error).first().text, QStringLiteral("recent"));
        QVERIFY(error.isEmpty());
    }

    void retentionPreservesLegacyDrafts()
    {
        QTemporaryDir directory;
        const QString fileName = directory.filePath(QStringLiteral("00000000-0000-0000-0000-000000000001.json"));
        QFile legacy(fileName);
        QVERIFY(legacy.open(QIODevice::WriteOnly));
        legacy.write("{\"source\":\"file:///old\",\"text\":\"unsaved\",\"timestamp\":1000}");
        legacy.close();
        DraftStore store(directory.path());
        QVERIFY(store.prune(1, 200000000).isEmpty());
        QVERIFY(QFile::exists(fileName));
    }
};

QTEST_GUILESS_MAIN(DraftStoreTest)
#include "draftstoretest.moc"
