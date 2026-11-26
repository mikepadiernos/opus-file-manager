// SPDX-License-Identifier: GPL-3.0-or-later

#include "startup.h"

#include <QScopeGuard>
#include <QStandardPaths>
#include <QTest>

using Mode = Settings::EnumStartupMode;
using Dual = Settings::EnumDualPaneAtStartup;

class StartupTest : public QObject
{
    Q_OBJECT

private:
    const QUrl m_home = QUrl::fromLocalFile(QStringLiteral("/home/me"));
    const QUrl m_a = QUrl::fromLocalFile(QStringLiteral("/a"));
    const QUrl m_b = QUrl::fromLocalFile(QStringLiteral("/b"));

    WindowSession saved() const
    {
        WindowSession session;
        session.left.tabs = {m_a, m_b};
        session.left.currentTab = 1;
        session.right.tabs = {m_b};
        session.activePane = 1;
        session.dual = false;
        session.stacked = true;
        return session;
    }

    StartupOptions options(Mode::type mode, Dual::type dual = Dual::AsLastUsed) const
    {
        return {mode, dual, m_home, m_a, m_b};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void restoreSessionKeepsSavedTabs()
    {
        const WindowSession session = Startup::resolve(saved(), options(Mode::RestoreSession));
        QCOMPARE(session.left.tabs, saved().left.tabs);
        QCOMPARE(session.left.currentTab, 1);
        QCOMPARE(session.right.tabs, saved().right.tabs);
        QCOMPARE(session.activePane, 1);
    }

    void restoreSessionFallsBackToHome()
    {
        const WindowSession session = Startup::resolve(WindowSession(), options(Mode::RestoreSession));
        QCOMPARE(session.left.tabs, QList<QUrl>{m_home});
        QCOMPARE(session.right.tabs, QList<QUrl>{m_home});
    }

    void homeFolderOpensHomeInBothPanes()
    {
        const WindowSession session = Startup::resolve(saved(), options(Mode::HomeFolder));
        QCOMPARE(session.left.tabs, QList<QUrl>{m_home});
        QCOMPARE(session.right.tabs, QList<QUrl>{m_home});
        QCOMPARE(session.activePane, 0);
        QVERIFY(session.stacked);
    }

    void fixedFoldersOpenConfiguredFolders()
    {
        const WindowSession session = Startup::resolve(saved(), options(Mode::FixedFolders));
        QCOMPARE(session.left.tabs, QList<QUrl>{m_a});
        QCOMPARE(session.right.tabs, QList<QUrl>{m_b});
        QCOMPARE(session.activePane, 0);
    }

    void fixedFoldersUseDefaultViewMode()
    {
        const auto previous = Settings::defaultViewMode();
        Settings::setDefaultViewMode(Settings::EnumDefaultViewMode::Icons);
        const WindowSession session = Startup::resolve(saved(), options(Mode::FixedFolders));
        Settings::setDefaultViewMode(previous);
        QCOMPARE(session.left.viewMode, int(Settings::EnumDefaultViewMode::Icons));
        QCOMPARE(session.right.viewMode, int(Settings::EnumDefaultViewMode::Icons));
    }

    void fixedFoldersFallBackToHomeWhenUnset()
    {
        StartupOptions unset = options(Mode::FixedFolders);
        unset.leftFolder = QUrl();
        const WindowSession session = Startup::resolve(saved(), unset);
        QCOMPARE(session.left.tabs, QList<QUrl>{m_home});
        QCOMPARE(session.right.tabs, QList<QUrl>{m_b});
    }

    void dualPanePolicy()
    {
        QVERIFY(!Startup::resolve(saved(), options(Mode::RestoreSession, Dual::AsLastUsed)).dual);
        QVERIFY(Startup::resolve(saved(), options(Mode::RestoreSession, Dual::Always)).dual);
        WindowSession dual = saved();
        dual.dual = true;
        QVERIFY(!Startup::resolve(dual, options(Mode::HomeFolder, Dual::Never)).dual);
    }

    void restoredSessionKeepsEachPaneYaziMode()
    {
        WindowSession session = saved();
        session.right.yaziMode = true;
        session.right.yaziPreview = 0;
        const WindowSession resolved = Startup::resolve(session, options(Mode::RestoreSession));
        QVERIFY(!resolved.left.yaziMode);
        QVERIFY(resolved.right.yaziMode);
        QCOMPARE(resolved.right.yaziPreview, 0);

        // A restored choice wins over the default.
        StartupOptions defaults = options(Mode::RestoreSession);
        defaults.leftYazi.defaultMode = true;
        QVERIFY(!Startup::resolve(session, defaults).left.yaziMode);
    }

    void notRestoringUsesEachPaneDefault()
    {
        WindowSession session = saved();
        session.left.yaziMode = true;
        session.right.yaziMode = true;
        session.right.yaziParentColumn = 0;
        StartupOptions start = options(Mode::RestoreSession);
        start.leftYazi = {true, false};
        start.rightYazi = {false, false};
        const WindowSession resolved = Startup::resolve(session, start);
        QVERIFY(resolved.left.yaziMode);
        QVERIFY(!resolved.right.yaziMode);
        // Only the mode changes; tabs and the other Yazi choices are restored.
        QCOMPARE(resolved.left.tabs, saved().left.tabs);
        QCOMPARE(resolved.left.currentTab, 1);
        QCOMPARE(resolved.right.yaziParentColumn, 0);
    }

    void panesWithoutASessionUseTheirDefault_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("empty");
        QTest::newRow("first start") << int(Mode::RestoreSession) << true;
        QTest::newRow("home folder") << int(Mode::HomeFolder) << false;
        QTest::newRow("fixed folders") << int(Mode::FixedFolders) << false;
    }

    void panesWithoutASessionUseTheirDefault()
    {
        QFETCH(int, mode);
        QFETCH(bool, empty);
        WindowSession session = empty ? WindowSession() : saved();
        session.left.yaziMode = !empty;
        session.right.yaziMode = !empty;
        StartupOptions start = options(static_cast<Mode::type>(mode));
        QVERIFY(!Startup::resolve(session, start).left.yaziMode);
        QVERIFY(!Startup::resolve(session, start).right.yaziMode);
        start.rightYazi.defaultMode = true;
        QVERIFY(!Startup::resolve(session, start).left.yaziMode);
        QVERIFY(Startup::resolve(session, start).right.yaziMode);
    }

    void optionsFollowTheSettings()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
        });
        Settings::self()->setDefaults();
        StartupOptions start = Startup::fromSettings();
        QCOMPARE(start.leftYazi, YaziMode::Activation({false, true}));
        QCOMPARE(start.rightYazi, YaziMode::Activation({false, true}));
        Settings::setYaziLeftPaneRestore(false);
        Settings::setYaziRightPaneDefault(true);
        start = Startup::fromSettings();
        QCOMPARE(start.leftYazi, YaziMode::Activation({false, false}));
        QCOMPARE(start.rightYazi, YaziMode::Activation({true, true}));
    }
};

QTEST_GUILESS_MAIN(StartupTest)

#include "startuptest.moc"
