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
};

QTEST_GUILESS_MAIN(DraftStoreTest)
#include "draftstoretest.moc"
