// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexstatus.h"

#include <QTest>

using namespace IndexStatus;

namespace
{
Config config()
{
    Config result;
    result.available = true;
    result.enabled = true;
    result.includeFolders = {QStringLiteral("/home/user"), QStringLiteral("/home/user/build/keep")};
    result.excludeFolders = {QStringLiteral("/home/user/build"), QStringLiteral("/home/user/both")};
    result.excludeFilters = {QStringLiteral("*.o"), QStringLiteral("node_modules")};
    result.excludeMimeTypes = {QStringLiteral("text/x-log")};
    return result;
}
}

class IndexStatusTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void classifiesPaths_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("mimeType");
        QTest::addColumn<int>("coverage");
        QTest::addColumn<QString>("detail");
        QTest::newRow("root") << QStringLiteral("/home/user") << QString() << int(Coverage::Configured) << QStringLiteral("/home/user");
        QTest::newRow("file") << QStringLiteral("/home/user/notes/a.txt") << QStringLiteral("text/plain") << int(Coverage::Configured)
                              << QStringLiteral("/home/user");
        QTest::newRow("unclean") << QStringLiteral("/home/user/notes/../b.txt") << QString() << int(Coverage::Configured) << QStringLiteral("/home/user");
        QTest::newRow("outside") << QStringLiteral("/srv/data") << QString() << int(Coverage::OutsideFolders) << QString();
        QTest::newRow("prefix") << QStringLiteral("/home/username") << QString() << int(Coverage::OutsideFolders) << QString();
        QTest::newRow("relative") << QStringLiteral("notes") << QString() << int(Coverage::OutsideFolders) << QString();
        QTest::newRow("excluded") << QStringLiteral("/home/user/build/x") << QString() << int(Coverage::ExcludedFolder) << QStringLiteral("/home/user/build");
        QTest::newRow("reincluded") << QStringLiteral("/home/user/build/keep/a") << QString() << int(Coverage::Configured)
                                    << QStringLiteral("/home/user/build/keep");
        QTest::newRow("hidden") << QStringLiteral("/home/user/.cache/a") << QString() << int(Coverage::Hidden) << QStringLiteral(".cache");
        QTest::newRow("filter") << QStringLiteral("/home/user/src/main.o") << QString() << int(Coverage::ExcludedFilter) << QStringLiteral("*.o");
        QTest::newRow("filter folder") << QStringLiteral("/home/user/web/node_modules/x/y.js") << QString() << int(Coverage::ExcludedFilter)
                                       << QStringLiteral("node_modules");
        QTest::newRow("type") << QStringLiteral("/home/user/a.log") << QStringLiteral("text/x-log") << int(Coverage::ExcludedType)
                              << QStringLiteral("text/x-log");
    }

    void classifiesPaths()
    {
        QFETCH(QString, path);
        QFETCH(QString, mimeType);
        QFETCH(int, coverage);
        QFETCH(QString, detail);
        const PathCoverage result = classify(path, config(), mimeType);
        QCOMPARE(int(result.coverage), coverage);
        QCOMPARE(result.detail, detail);
        QVERIFY(!coverageText(result).isEmpty());
    }

    void exclusionWinsTies()
    {
        Config settings = config();
        settings.includeFolders.append(QStringLiteral("/home/user/both"));
        QCOMPARE(classify(QStringLiteral("/home/user/both/a"), settings).coverage, Coverage::ExcludedFolder);
    }

    void honoursHiddenSetting()
    {
        Config settings = config();
        settings.indexHidden = true;
        QCOMPARE(classify(QStringLiteral("/home/user/.config/a"), settings).coverage, Coverage::Configured);
    }

    void reportsUnavailableAndDisabled()
    {
        QCOMPARE(classify(QStringLiteral("/home/user"), Config()).coverage, Coverage::Unavailable);
        Config settings = config();
        settings.enabled = false;
        QCOMPARE(classify(QStringLiteral("/home/user"), settings).coverage, Coverage::Disabled);

        const QStringList unavailable = warnings(Config(), Service(), QStringLiteral("/home/user"));
        QCOMPARE(unavailable.size(), 1);
        QVERIFY(unavailable.first().contains(QStringLiteral("without Baloo")));
        const QStringList disabled = warnings(settings, Service(), QStringLiteral("/home/user"));
        QCOMPARE(disabled.size(), 1);
        QVERIFY(disabled.first().contains(QStringLiteral("turned off")));
    }

    void warnsAboutStaleOrIncompleteResults()
    {
        Service service;
        service.running = true;
        service.state = int(State::Idle);
        QVERIFY(warnings(config(), service, QStringLiteral("/home/user/notes")).isEmpty());
        QVERIFY(warnings(config(), service).isEmpty());

        service.state = int(State::Suspended);
        QVERIFY(warnings(config(), service).join(QLatin1Char(' ')).contains(QStringLiteral("suspended")));
        service.state = int(State::ContentIndexing);
        QVERIFY(warnings(config(), service).join(QLatin1Char(' ')).contains(QStringLiteral("incomplete")));
        service.state = int(State::LowPowerIdle);
        QVERIFY(warnings(config(), service).isEmpty());

        QVERIFY(warnings(config(), Service()).join(QLatin1Char(' ')).contains(QStringLiteral("not running")));

        Config basic = config();
        basic.onlyBasic = true;
        service.state = int(State::Idle);
        QVERIFY(warnings(basic, service).join(QLatin1Char(' ')).contains(QStringLiteral("contents and metadata")));

        const QStringList excluded = warnings(config(), service, QStringLiteral("/home/user/build"));
        QCOMPARE(excluded.size(), 1);
        QVERIFY(excluded.first().contains(QStringLiteral("/home/user/build")));
    }

    void describesStates()
    {
        QCOMPARE(stateText(int(State::Idle)), QStringLiteral("Idle"));
        QCOMPARE(stateText(int(State::Startup)), QStringLiteral("Starting"));
        QCOMPARE(stateText(42), QStringLiteral("Unknown"));
        QVERIFY(isBusy(int(State::FirstRun)));
        QVERIFY(!isBusy(int(State::Suspended)));
        QVERIFY(!isBusy(-1));

        Service service;
        QCOMPARE(serviceText(config(), service), QStringLiteral("Not running"));
        service.running = true;
        QCOMPARE(serviceText(config(), service), QStringLiteral("Running; state unknown"));
        service.state = int(State::NewFiles);
        service.remainingMs = 120000;
        QVERIFY(serviceText(config(), service).startsWith(QStringLiteral("Indexing new files, about ")));
        service.state = int(State::Idle);
        QCOMPARE(serviceText(config(), service), QStringLiteral("Idle"));
    }

    void findsMissingExtractors()
    {
        QVERIFY(expectsMetadata(QStringLiteral("application/pdf")));
        QVERIFY(expectsMetadata(QStringLiteral("image/png")));
        QVERIFY(expectsMetadata(QStringLiteral("application/vnd.oasis.opendocument.text")));
        QVERIFY(!expectsMetadata(QStringLiteral("application/octet-stream")));
        QVERIFY(!expectsMetadata(QStringLiteral("inode/directory")));

        int lookups = 0;
        const QStringList missing = missingExtractors({QStringLiteral("image/png"),
                                                       QStringLiteral("text/plain"),
                                                       QStringLiteral("application/pdf"),
                                                       QStringLiteral("image/png"),
                                                       QStringLiteral("application/octet-stream")},
                                                      [&lookups](const QString &type) {
                                                          ++lookups;
                                                          return type == QStringLiteral("text/plain");
                                                      });
        QCOMPARE(missing, (QStringList{QStringLiteral("application/pdf"), QStringLiteral("image/png")}));
        QCOMPARE(lookups, 3);
    }
};

QTEST_GUILESS_MAIN(IndexStatusTest)

#include "indexstatustest.moc"
