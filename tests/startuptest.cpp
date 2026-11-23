// SPDX-License-Identifier: GPL-3.0-or-later

#include "startup.h"

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
};

QTEST_GUILESS_MAIN(StartupTest)

#include "startuptest.moc"
