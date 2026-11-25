// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazimode.h"

#include <KConfig>
#include <KConfigGroup>

#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class YaziModeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void sessionEntryRoundTripsAndDefaultsToOff()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("staterc"));
        {
            KConfig config(path, KConfig::SimpleConfig);
            KConfigGroup group = config.group(QStringLiteral("Pane"));
            QVERIFY(!YaziMode::readSession(group));
            YaziMode::writeSession(group, true);
            QVERIFY(YaziMode::readSession(group));
            config.sync();
        }
        KConfig config(path, KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Pane"));
        QVERIFY(YaziMode::readSession(group));
        QCOMPARE(group.readEntry("YaziMode", QString()), QStringLiteral("true"));
        YaziMode::writeSession(group, false);
        QVERIFY(!YaziMode::readSession(group));
    }

    void workspaceEntryRoundTrips()
    {
        QJsonObject object;
        YaziMode::writeWorkspace(object, true);
        QCOMPARE(object.value(QStringLiteral("yaziMode")), QJsonValue(true));
        bool enabled = false;
        QVERIFY(YaziMode::readWorkspace(object, enabled));
        QVERIFY(enabled);

        YaziMode::writeWorkspace(object, false);
        QVERIFY(YaziMode::readWorkspace(object, enabled));
        QVERIFY(!enabled);
    }

    void missingWorkspaceEntryIsOff()
    {
        bool enabled = true;
        QVERIFY(YaziMode::readWorkspace(QJsonObject(), enabled));
        QVERIFY(!enabled);
    }

    void invalidWorkspaceEntryIsRejected_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("number") << QJsonValue(1);
        QTest::newRow("string") << QJsonValue(QStringLiteral("true"));
        QTest::newRow("null") << QJsonValue(QJsonValue::Null);
        QTest::newRow("object") << QJsonValue(QJsonObject());
    }

    void invalidWorkspaceEntryIsRejected()
    {
        QFETCH(QJsonValue, value);
        const QJsonObject object{{QStringLiteral("yaziMode"), value}};
        bool enabled = true;
        QVERIFY(!YaziMode::readWorkspace(object, enabled));
        QVERIFY(enabled);
    }

    void parentFolder_data()
    {
        QTest::addColumn<QUrl>("current");
        QTest::addColumn<QUrl>("parent");
        QTest::newRow("local") << QUrl::fromLocalFile(QStringLiteral("/home/me/Documents")) << QUrl::fromLocalFile(QStringLiteral("/home/me/"));
        QTest::newRow("trailing slash") << QUrl::fromLocalFile(QStringLiteral("/home/me/")) << QUrl::fromLocalFile(QStringLiteral("/home/"));
        QTest::newRow("top level") << QUrl::fromLocalFile(QStringLiteral("/tmp")) << QUrl::fromLocalFile(QStringLiteral("/"));
        QTest::newRow("root") << QUrl::fromLocalFile(QStringLiteral("/")) << QUrl();
        QTest::newRow("remote") << QUrl(QStringLiteral("sftp://host/dir/sub")) << QUrl(QStringLiteral("sftp://host/dir/"));
        QTest::newRow("remote root") << QUrl(QStringLiteral("sftp://host/")) << QUrl();
        QTest::newRow("empty") << QUrl() << QUrl();
    }

    void parentFolder()
    {
        QFETCH(QUrl, current);
        QFETCH(QUrl, parent);
        QCOMPARE(YaziMode::parentFolder(current).adjusted(QUrl::StripTrailingSlash), parent.adjusted(QUrl::StripTrailingSlash));
        QCOMPARE(YaziMode::parentFolder(current).isEmpty(), parent.isEmpty());
    }
};

QTEST_GUILESS_MAIN(YaziModeTest)

#include "yazimodetest.moc"
