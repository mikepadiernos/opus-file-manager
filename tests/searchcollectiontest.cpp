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
};

QTEST_GUILESS_MAIN(SearchCollectionTest)
#include "searchcollectiontest.moc"
