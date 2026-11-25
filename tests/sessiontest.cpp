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
        session.left.filterText = QStringLiteral("*.cpp");
        session.left.filterScope = 2;
        session.left.yaziMode = true;
        session.left.yaziPreview = 0;
        session.right.tabs = {QUrl(QStringLiteral("trash:/")), QUrl(QStringLiteral("sftp://host/dir"))};
        session.right.currentTab = 0;
        session.activePane = 1;
        session.dual = false;
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
        QVERIFY(loaded.dual);
        QVERIFY(loaded.stacked);
        KConfigGroup group = config.group(QStringLiteral("Session"));
        group.writeEntry("Stacked", false);
        QVERIFY(!Session::load(group).stacked);
    }

    void missingViewModeUsesTheDefault()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Session"));
        QCOMPARE(Session::load(group, 1).left.viewMode, 1);
        QCOMPARE(Session::load(group, 1).right.viewMode, 1);

        group.group(QStringLiteral("LeftPane")).writeEntry("ViewMode", 2);
        group.group(QStringLiteral("RightPane")).writeEntry("ViewMode", 0);
        const WindowSession saved = Session::load(group, 1);
        QCOMPARE(saved.left.viewMode, 2);
        QCOMPARE(saved.right.viewMode, 0);
    }

    void sanitizedFillsGapsAndClamps()
    {
        const QUrl home = QUrl::fromLocalFile(QStringLiteral("/home/me"));

        WindowSession session;
        session.left.tabs = {QUrl(), QUrl::fromLocalFile(QStringLiteral("/a")), QUrl(QStringLiteral("not a url"), QUrl::StrictMode)};
        session.left.currentTab = 7;
        session.right.currentTab = -3;
        session.activePane = 5;
        session.dual = false;
        session.stacked = true;

        const WindowSession clean = session.sanitized(home);
        QCOMPARE(clean.left.tabs, (QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/a"))}));
        QCOMPARE(clean.left.currentTab, 0);
        QCOMPARE(clean.right.tabs, (QList<QUrl>{home}));
        QCOMPARE(clean.right.currentTab, 0);
        QCOMPARE(clean.activePane, 0);
        QVERIFY(!clean.dual);
        QVERIFY(clean.stacked);

        WindowSession rightActive;
        rightActive.activePane = 1;
        QCOMPARE(rightActive.sanitized(home).activePane, 1);

        WindowSession invalidFilter;
        invalidFilter.left.filterScope = 5;
        QCOMPARE(invalidFilter.sanitized(home).left.filterScope, 2);
        QCOMPARE(WindowSession().sanitized(home).left.filterScope, -1);
        QCOMPARE(WindowSession().sanitized(home).left.locationPresentation, -1);
        invalidFilter.left.locationPresentation = 7;
        QCOMPARE(invalidFilter.sanitized(home).left.locationPresentation, 2);

        WindowSession cards;
        cards.left.viewMode = 3;
        cards.right.viewMode = 9;
        QCOMPARE(cards.sanitized(home).left.viewMode, 3);
        QCOMPARE(cards.sanitized(home).right.viewMode, PaneSession::MaximumViewMode);
    }

    void yaziModeIsOffUnlessSavedAndSurvivesSanitizing()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Session"));
        QVERIFY(!Session::load(group).left.yaziMode);
        QVERIFY(!Session::load(group).right.yaziMode);

        group.group(QStringLiteral("RightPane")).writeEntry("YaziMode", true);
        const WindowSession loaded = Session::load(group);
        QVERIFY(!loaded.left.yaziMode);
        QVERIFY(loaded.right.yaziMode);

        const WindowSession clean = loaded.sanitized(QUrl::fromLocalFile(QStringLiteral("/home/me")));
        QVERIFY(!clean.left.yaziMode);
        QVERIFY(clean.right.yaziMode);
    }

    void yaziPreviewIsUnsetUnlessSavedAndSurvivesSanitizing()
    {
        QTemporaryDir dir;
        KConfig config(dir.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Session"));
        QCOMPARE(Session::load(group).left.yaziPreview, -1);
        QCOMPARE(Session::load(group).right.yaziPreview, -1);

        group.group(QStringLiteral("RightPane")).writeEntry("YaziPreview", false);
        WindowSession loaded = Session::load(group);
        QCOMPARE(loaded.left.yaziPreview, -1);
        QCOMPARE(loaded.right.yaziPreview, 0);

        loaded.left.yaziPreview = 7;
        const WindowSession clean = loaded.sanitized(QUrl::fromLocalFile(QStringLiteral("/home/me")));
        QCOMPARE(clean.left.yaziPreview, 1);
        QCOMPARE(clean.right.yaziPreview, 0);

        // Saving an unset choice leaves no entry for older versions to trip on.
        WindowSession unset = loaded;
        unset.left.yaziPreview = -1;
        Session::save(unset, group);
        QVERIFY(!group.group(QStringLiteral("LeftPane")).hasKey("YaziPreview"));
        QCOMPARE(Session::load(group).left.yaziPreview, -1);
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
