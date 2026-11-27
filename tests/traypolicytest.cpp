// SPDX-License-Identifier: GPL-3.0-or-later

#include "traypolicy.h"

#include <QTest>

class TrayPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hidesOnlyWithAnIconAndHost()
    {
        QVERIFY(TrayPolicy::canHide({.trayIconEnabled = true, .trayHostAvailable = true}));
        QVERIFY(!TrayPolicy::canHide({.trayIconEnabled = true, .trayHostAvailable = false}));
        QVERIFY(!TrayPolicy::canHide({.trayIconEnabled = false, .trayHostAvailable = true}));
    }

    void closeHidesOnlyForWindowManagerRequests()
    {
        const TrayPolicy::State tray{.trayIconEnabled = true, .trayHostAvailable = true};
        QVERIFY(TrayPolicy::hideOnClose(tray, true, true));
        // File › Quit, the tray's Quit and logging out close the window.
        QVERIFY(!TrayPolicy::hideOnClose(tray, true, false));
        QVERIFY(!TrayPolicy::hideOnClose(tray, false, true));
        QVERIFY(!TrayPolicy::hideOnClose({.trayIconEnabled = true, .trayHostAvailable = false}, true, true));
    }

    void minimizeHidesWhenEnabled()
    {
        const TrayPolicy::State tray{.trayIconEnabled = true, .trayHostAvailable = true};
        QVERIFY(TrayPolicy::hideOnMinimize(tray, true));
        QVERIFY(!TrayPolicy::hideOnMinimize(tray, false));
        QVERIFY(!TrayPolicy::hideOnMinimize({}, true));
    }

    void startsHiddenOnlyWithoutARequest()
    {
        const TrayPolicy::State tray{.trayIconEnabled = true, .trayHostAvailable = true};
        QVERIFY(TrayPolicy::startHidden(tray, true, false));
        QVERIFY(!TrayPolicy::startHidden(tray, false, false));
        // Folders or a workspace on the command line show the window.
        QVERIFY(!TrayPolicy::startHidden(tray, true, true));
        // Without a tray to bring it back, the window is shown.
        QVERIFY(!TrayPolicy::startHidden({.trayIconEnabled = true, .trayHostAvailable = false}, true, false));
        QVERIFY(!TrayPolicy::startHidden({.trayIconEnabled = false, .trayHostAvailable = true}, true, false));
    }

    void shortcutStatusFollowsTheSettingServiceAndKey()
    {
        using TrayPolicy::ShortcutStatus;
        QCOMPARE(TrayPolicy::shortcutStatus(true, true, true), ShortcutStatus::Assigned);
        // KDE leaves the shortcut without a key when the default is taken.
        QCOMPARE(TrayPolicy::shortcutStatus(true, true, false), ShortcutStatus::Unassigned);
        QCOMPARE(TrayPolicy::shortcutStatus(true, false, true), ShortcutStatus::Unavailable);
        QCOMPARE(TrayPolicy::shortcutStatus(true, false, false), ShortcutStatus::Unavailable);
        QCOMPARE(TrayPolicy::shortcutStatus(false, true, true), ShortcutStatus::Off);
        QCOMPARE(TrayPolicy::shortcutStatus(false, false, false), ShortcutStatus::Off);
    }

    void toggleShowsHidesOrMinimizes()
    {
        const TrayPolicy::State tray{.trayIconEnabled = true, .trayHostAvailable = true};
        QCOMPARE(TrayPolicy::toggle(tray, false, false, false), TrayPolicy::Toggle::Show);
        QCOMPARE(TrayPolicy::toggle(tray, true, true, false), TrayPolicy::Toggle::Show);
        // A window behind other windows comes to the front first.
        QCOMPARE(TrayPolicy::toggle(tray, true, false, false), TrayPolicy::Toggle::Show);
        QCOMPARE(TrayPolicy::toggle(tray, true, false, true), TrayPolicy::Toggle::Hide);
        QCOMPARE(TrayPolicy::toggle({}, true, false, true), TrayPolicy::Toggle::Minimize);
    }
};

QTEST_GUILESS_MAIN(TrayPolicyTest)

#include "traypolicytest.moc"
