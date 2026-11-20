// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchcollection.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SearchCollectionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void persistsReferencesAndRejectsInvalidUrls()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        const KConfigGroup group = config->group(QStringLiteral("SearchCollection"));
        const QUrl a = QUrl::fromLocalFile(dir.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(dir.filePath(QStringLiteral("other/b")));
        QString error;
        QVERIFY(SearchCollection::save(group, {a, b}, &error));
        config->reparseConfiguration();
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a, b}));
        QVERIFY(error.isEmpty());
        QVERIFY(!SearchCollection::save(group, {a, a}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!SearchCollection::save(group, {QUrl(QStringLiteral("sftp://example.invalid/path"))}, &error));
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a, b}));
    }

    void managesNamedCollectionsWithoutChangingTheWorkingCollection()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("searchesrc")), KConfig::SimpleConfig);
        auto group = config->group(QStringLiteral("SearchCollection"));
        const QUrl a = QUrl::fromLocalFile(dir.filePath(QStringLiteral("a, #1")));
        const QUrl b = QUrl::fromLocalFile(dir.filePath(QStringLiteral("b")));
        QString error;
        QVERIFY(SearchCollection::save(group, {a}, &error));
        QVERIFY(SearchCollection::loadSaved(group, &error).isEmpty());
        QVERIFY(error.isEmpty());
        QVERIFY(SearchCollection::saveSaved(group, {{QStringLiteral("First"), {a, b}}, {QStringLiteral("Empty"), {}}}, &error));
        config->reparseConfiguration();
        const auto saved = SearchCollection::loadSaved(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(saved.size(), 2);
        QCOMPARE(saved.first().urls, (QList<QUrl>{a, b}));
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a}));
        QVERIFY(!SearchCollection::saveSaved(group, {{QStringLiteral("First"), {a}}, {QStringLiteral(" first "), {}}}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!SearchCollection::saveSaved(group, {{QStringLiteral("Bad"), {a, a}}}, &error));
        QCOMPARE(SearchCollection::loadSaved(group, &error).size(), 2);
        group.writeEntry("Collections", QByteArray("[{\"name\":\"bad\",\"urls\":[\"sftp://example.invalid/a\"]}]"));
        QVERIFY(SearchCollection::loadSaved(group, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QCOMPARE(SearchCollection::load(group, &error), (QList<QUrl>{a}));
    }
};

QTEST_GUILESS_MAIN(SearchCollectionTest)
#include "searchcollectiontest.moc"
