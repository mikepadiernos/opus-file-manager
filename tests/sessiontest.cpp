// SPDX-License-Identifier: GPL-3.0-or-later

#include "session.h"

#include <KConfig>
#include <KConfigGroup>

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class SessionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void roundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("staterc"));

        WindowSession session;
        session.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/home/me")), QUrl::fromLocalFile(QStringLiteral("/tmp/with space"))};
        session.left.currentTab = 1;
        session.right.tabs = {QUrl(QStringLiteral("trash:/")), QUrl(QStringLiteral("sftp://host/dir"))};
        session.right.currentTab = 0;
        session.activePane = 1;
        session.stacked = true;

        {
            KConfig config(path, KConfig::SimpleConfig);
            KConfigGroup group = config.group(QStringLiteral("Session"));
            Session::save(session, group);
            config.sync();
        }

        KConfig config(path, KConfig::SimpleConfig);
        const WindowSession loaded = Session::load(config.group(QStringLiteral("Session")));
        QCOMPARE(loaded, session);
    }

    void loadMissingGroupIsEmpty()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        const WindowSession loaded = Session::load(config.group(QStringLiteral("Session")));
        QVERIFY(loaded.isEmpty());
        QCOMPARE(loaded.activePane, 0);
        QVERIFY(!loaded.stacked);
    }

    void sanitizedFillsGapsAndClamps()
    {
        const QUrl home = QUrl::fromLocalFile(QStringLiteral("/home/me"));

        WindowSession session;
        session.left.tabs = {QUrl(), QUrl::fromLocalFile(QStringLiteral("/a")), QUrl(QStringLiteral("not a url"), QUrl::StrictMode)};
        session.left.currentTab = 7;
        session.right.currentTab = -3;
        session.activePane = 5;
        session.stacked = true;

        const WindowSession clean = session.sanitized(home);
        QCOMPARE(clean.left.tabs, (QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/a"))}));
        QCOMPARE(clean.left.currentTab, 0);
        QCOMPARE(clean.right.tabs, (QList<QUrl>{home}));
        QCOMPARE(clean.right.currentTab, 0);
        QCOMPARE(clean.activePane, 0);
        QVERIFY(clean.stacked);

        WindowSession rightActive;
        rightActive.activePane = 1;
        QCOMPARE(rightActive.sanitized(home).activePane, 1);
    }

    void tabStateRoundTripsAndSurvivesSanitizing()
    {
        QTemporaryDir dir;
        const QUrl a = QUrl::fromLocalFile(QStringLiteral("/a"));
        const QUrl b = QUrl::fromLocalFile(QStringLiteral("/b"));
        const QUrl c = QUrl::fromLocalFile(QStringLiteral("/c"));
        WindowSession session;
        session.left.tabs = {a, QUrl(), b, c};
        session.left.pinnedCount = 2;
        session.left.lockedTabs = {2, 3};
        session.left.names = {QStringLiteral("Pinned A"), QStringLiteral("gone"), QString(), QStringLiteral("C")};
        session.right.tabs = {a};

        {
            KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
            KConfigGroup group = config.group(QStringLiteral("Session"));
            Session::save(session, group);
            config.sync();
        }
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        const WindowSession loaded = Session::load(config.group(QStringLiteral("Session")));
        QCOMPARE(loaded.left.pinnedCount, 2);
        QCOMPARE(loaded.left.lockedTabs, (QList<int>{2, 3}));
        QCOMPARE(loaded.left.names, session.left.names);

        const PaneSession clean = loaded.sanitized(a).left;
        QCOMPARE(clean.tabs, (QList<QUrl>{a, b, c}));
        QCOMPARE(clean.pinnedCount, 1);
        QCOMPARE(clean.lockedTabs, (QList<int>{1, 2}));
        QCOMPARE(clean.names, (QStringList{QStringLiteral("Pinned A"), QString(), QStringLiteral("C")}));
    }
};

QTEST_GUILESS_MAIN(SessionTest)

#include "sessiontest.moc"
