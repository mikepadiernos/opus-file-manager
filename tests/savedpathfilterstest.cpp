// SPDX-License-Identifier: GPL-3.0-or-later

#include "savedpathfilters.h"

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SavedPathFiltersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void persistsAndValidatesPresets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("filtersrc")), KConfig::SimpleConfig);
        const auto group = config->group(QStringLiteral("SavedPathFilters"));
        const PathFilter filter{QStringLiteral("path"), QStringLiteral("parent"), {dir.filePath(QStringLiteral("excluded"))}, 3};
        QString error;
        QVERIFY(SavedPathFilters::save(group, {{QStringLiteral("Work"), filter}}, &error));
        config->reparseConfiguration();
        const auto loaded = SavedPathFilters::load(group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().name, QStringLiteral("Work"));
        QVERIFY(loaded.first().filter == filter);
        QVERIFY(!SavedPathFilters::save(group, {{QStringLiteral("Work"), filter}, {QStringLiteral(" work "), filter}}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!SavedPathFilters::save(group, {{QStringLiteral("Invalid"), {{}, {}, {QStringLiteral("relative")}, -1}}}, &error));
        QCOMPARE(SavedPathFilters::load(group, &error).size(), 1);
        QVERIFY(error.isEmpty());
    }

    void rejectsMalformedPresets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("filtersrc")), KConfig::SimpleConfig);
        auto group = config->group(QStringLiteral("SavedPathFilters"));
        for (const QByteArray &bad : {QByteArray("{broken"),
                                      QByteArray(R"([{"name":"bad","filter":{"path":"","parent":"","excluded":[],"depth":1.5}}])"),
                                      QByteArray(R"([{"name":"bad","filter":{"path":"","parent":"","excluded":["relative"],"depth":0}}])")}) {
            group.writeEntry("Filters", bad);
            QString error;
            QVERIFY(SavedPathFilters::load(group, &error).isEmpty());
            QVERIFY(!error.isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(SavedPathFiltersTest)
#include "savedpathfilterstest.moc"
