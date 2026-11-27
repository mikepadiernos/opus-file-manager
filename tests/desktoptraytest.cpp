// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktoptray.h"
#include "settings.h"

#include <KActionCollection>
#include <KStatusNotifierItem>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QWidget>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

class DesktopTrayTest : public QObject
{
    Q_OBJECT

    struct Fixture {
        QPointer<QWidget> window;
        DesktopTray *tray = nullptr;
        Fixture() = default;
        Fixture(const Fixture &) = delete;
        Fixture &operator=(const Fixture &) = delete;
        ~Fixture()
        {
            delete window;
        }
    };

    /// A window that deletes itself when it really closes, shown unless @p show is false.
    static void makeWindow(Fixture &fixture, bool hostAvailable, bool show = true)
    {
        fixture.window = new QWidget;
        fixture.window->setAttribute(Qt::WA_DeleteOnClose);
        auto *actions = new KActionCollection(fixture.window.data());
        fixture.tray = new DesktopTray(fixture.window, actions);
        fixture.tray->setHostProbe([hostAvailable] {
            return hostAvailable;
        });
        fixture.tray->applySettings();
        fixture.window->resize(300, 200);
        if (show) {
            fixture.window->show();
        }
    }

    static void requestCloseFromWindowManager(QWidget *window)
    {
        QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(window->windowHandle());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Settings::self()->setDefaults();
        QApplication::setQuitOnLastWindowClosed(true);
    }

    void registersTheGlobalShortcutAction()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        auto *actions = fixture.window->findChild<KActionCollection *>();
        QAction *toggle = actions->action(QStringLiteral("toggle_window_visibility"));
        QVERIFY(toggle);
        QCOMPARE(toggle, fixture.tray->toggleAction());
        QCOMPARE(toggle->text(), QStringLiteral("Show or Hide Plasma Opus"));
    }

    void trayIconFollowsTheSetting()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        QVERIFY(fixture.tray->trayIconShown());
        QMenu *menu = fixture.tray->contextMenu();
        QVERIFY(menu);
        for (const char *name : {"trayShowHide", "trayOpenHome", "trayTransfers", "trayCloudSync", "trayCloudActivity"}) {
            QVERIFY2(menu->findChild<QAction *>(QLatin1String(name)), name);
        }

        Settings::setShowTrayIcon(false);
        fixture.tray->applySettings();
        QVERIFY(!fixture.tray->trayIconShown());
        QVERIFY(!fixture.tray->contextMenu());
        QVERIFY(!fixture.tray->state().trayIconEnabled);
    }

    void closeToTrayHidesOnWindowManagerClose()
    {
        Settings::setCloseToTray(true);
        Fixture fixture;
        makeWindow(fixture, true);
        QVERIFY(QTest::qWaitForWindowExposed(fixture.window));
        requestCloseFromWindowManager(fixture.window);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(fixture.window);
        QVERIFY(!fixture.window->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());

        DesktopTray::bringToFront(fixture.window);
        QVERIFY(fixture.window->isVisible());
        QVERIFY(QApplication::quitOnLastWindowClosed());
    }

    void quitStillClosesWithCloseToTray()
    {
        Settings::setCloseToTray(true);
        Fixture fixture;
        makeWindow(fixture, true);
        QVERIFY(QTest::qWaitForWindowExposed(fixture.window));
        // File › Quit and the tray's Quit close the window programmatically.
        QVERIFY(fixture.window->close());
        QTRY_VERIFY(!fixture.window);
    }

    void closesWithoutTrayHostOrSetting_data()
    {
        QTest::addColumn<bool>("closeToTray");
        QTest::addColumn<bool>("host");
        QTest::addColumn<bool>("icon");
        QTest::newRow("setting off") << false << true << true;
        QTest::newRow("no tray host") << true << false << true;
        QTest::newRow("no tray icon") << true << true << false;
    }

    void closesWithoutTrayHostOrSetting()
    {
        QFETCH(bool, closeToTray);
        QFETCH(bool, host);
        QFETCH(bool, icon);
        Settings::setCloseToTray(closeToTray);
        Settings::setShowTrayIcon(icon);
        Fixture fixture;
        makeWindow(fixture, host);
        QVERIFY(QTest::qWaitForWindowExposed(fixture.window));
        requestCloseFromWindowManager(fixture.window);
        QTRY_VERIFY(!fixture.window);
    }

    void minimizeToTrayHides()
    {
        Settings::setMinimizeToTray(true);
        Fixture fixture;
        makeWindow(fixture, true);
        QVERIFY(QTest::qWaitForWindowExposed(fixture.window));
        fixture.window->showMinimized();
        QTRY_VERIFY(!fixture.window->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());

        DesktopTray::bringToFront(fixture.window);
        QVERIFY(fixture.window->isVisible());
        QVERIFY(!fixture.window->isMinimized());
    }

    void toggleHidesAnActiveWindowAndShowsItAgain()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        fixture.window->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(fixture.window));
        fixture.tray->toggleAction()->trigger();
        QVERIFY(!fixture.window->isVisible());
        fixture.tray->toggleAction()->trigger();
        QVERIFY(fixture.window->isVisible());
    }

    void toggleMinimizesWithoutATrayHost()
    {
        Fixture fixture;
        makeWindow(fixture, false);
        fixture.window->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(fixture.window));
        QVERIFY(!fixture.tray->hideToTray());
        fixture.tray->toggleAction()->trigger();
        QVERIFY(fixture.window->isMinimized());
        QVERIFY(QApplication::quitOnLastWindowClosed());
    }

    void trayMenuOpensHomeAndTransfers()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        QSignalSpy open(fixture.tray, &DesktopTray::openRequested);
        QSignalSpy transfers(fixture.tray, &DesktopTray::transfersRequested);
        QMenu *menu = fixture.tray->contextMenu();
        menu->findChild<QAction *>(QStringLiteral("trayOpenHome"))->trigger();
        QCOMPARE(open.count(), 1);
        QCOMPARE(open.first().first().toUrl(), QUrl::fromLocalFile(QDir::homePath()));
        menu->findChild<QAction *>(QStringLiteral("trayTransfers"))->trigger();
        QCOMPARE(transfers.count(), 1);

        QAction *showHide = menu->findChild<QAction *>(QStringLiteral("trayShowHide"));
        Q_EMIT menu->aboutToShow();
        QCOMPARE(showHide->text(), QStringLiteral("Hide Plasma Opus"));
        showHide->trigger();
        QVERIFY(!fixture.window->isVisible());
        Q_EMIT menu->aboutToShow();
        QCOMPARE(showHide->text(), QStringLiteral("Show Plasma Opus"));
        showHide->trigger();
        QVERIFY(fixture.window->isVisible());
    }

    void trayMenuOpensCloudSync()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        QSignalSpy cloudSync(fixture.tray, &DesktopTray::cloudSyncRequested);
        fixture.tray->contextMenu()->findChild<QAction *>(QStringLiteral("trayCloudSync"))->trigger();
        QCOMPARE(cloudSync.count(), 1);
        QSignalSpy cloudActivity(fixture.tray, &DesktopTray::cloudActivityRequested);
        fixture.tray->contextMenu()->findChild<QAction *>(QStringLiteral("trayCloudActivity"))->trigger();
        QCOMPARE(cloudActivity.count(), 1);
    }

    void trayQuitCanBeCanceled()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        int checks = 0;
        fixture.tray->setQuitCheck([&checks] {
            ++checks;
            return false;
        });
        QAction *quit = nullptr;
        // The item adds its standard Quit action when the menu is about to show.
        Q_EMIT fixture.tray->contextMenu()->aboutToShow();
        const auto menuActions = fixture.tray->contextMenu()->actions();
        for (QAction *action : menuActions) {
            if (action->text().remove(QLatin1Char('&')) == QLatin1String("Quit")) {
                quit = action;
            }
        }
        QVERIFY(quit);
        QSignalSpy aboutToQuit(QCoreApplication::instance(), &QCoreApplication::aboutToQuit);
        quit->trigger();
        QTRY_COMPARE(checks, 1);
        QTest::qWait(50);
        QCOMPARE(aboutToQuit.count(), 0);
        QVERIFY(fixture.window->isVisible());
    }

    void backgroundStartKeepsTheTrayIconWithoutAWindow()
    {
        Settings::setShowTrayIcon(false);
        Settings::setCloseToTray(false);
        Fixture fixture;
        fixture.window = new QWidget;
        fixture.window->setAttribute(Qt::WA_DeleteOnClose);
        auto *actions = new KActionCollection(fixture.window.data());
        fixture.tray = new DesktopTray(fixture.window, actions);
        fixture.tray->setHostProbe([] {
            return true;
        });
        fixture.tray->applySettings();
        QVERIFY(!fixture.tray->trayIconShown());

        fixture.tray->startInBackground();
        QVERIFY(fixture.tray->trayIconShown());
        QVERIFY(!fixture.window->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());
        auto *item = fixture.tray->findChild<KStatusNotifierItem *>();
        QVERIFY(item);
        QCOMPARE(item->status(), KStatusNotifierItem::Active);
        // The settings no longer remove the icon background sync relies on.
        fixture.tray->applySettings();
        QVERIFY(fixture.tray->trayIconShown());

        fixture.tray->toggleWindow();
        QVERIFY(fixture.window->isVisible());
        QVERIFY(QApplication::quitOnLastWindowClosed());
        // Closing hides to the tray, so synchronization keeps running.
        QVERIFY(QTest::qWaitForWindowExposed(fixture.window));
        requestCloseFromWindowManager(fixture.window);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(fixture.window);
        QVERIFY(!fixture.window->isVisible());
    }

    void attentionShowsInTheTray()
    {
        Fixture fixture;
        makeWindow(fixture, true);
        auto *item = fixture.tray->findChild<KStatusNotifierItem *>();
        QVERIFY(item);
        QCOMPARE(item->status(), KStatusNotifierItem::Passive);
        fixture.tray->setAttention(QStringLiteral("Waiting for review"));
        QCOMPARE(item->status(), KStatusNotifierItem::NeedsAttention);
        QCOMPARE(item->toolTipSubTitle(), QStringLiteral("Waiting for review"));
        fixture.tray->setAttention({});
        QCOMPARE(item->status(), KStatusNotifierItem::Passive);
        QVERIFY(item->toolTipSubTitle().isEmpty());
    }

    void startsHiddenWhenTheSettingAsks()
    {
        Settings::setStartHiddenInTray(true);
        Fixture fixture;
        makeWindow(fixture, true, false);
        // Folders or a workspace to open show the window.
        QVERIFY(!fixture.tray->startHidden(true));
        QVERIFY(QApplication::quitOnLastWindowClosed());

        QVERIFY(fixture.tray->startHidden(false));
        QVERIFY(!fixture.window->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());
        auto *item = fixture.tray->findChild<KStatusNotifierItem *>();
        QVERIFY(item);
        QCOMPARE(item->status(), KStatusNotifierItem::Active);

        fixture.tray->toggleWindow();
        QVERIFY(fixture.window->isVisible());
        QVERIFY(QApplication::quitOnLastWindowClosed());
        QCOMPARE(item->status(), KStatusNotifierItem::Passive);
    }

    void startsShownWithoutTheSettingOrATray_data()
    {
        QTest::addColumn<bool>("setting");
        QTest::addColumn<bool>("host");
        QTest::addColumn<bool>("icon");
        QTest::newRow("setting off") << false << true << true;
        QTest::newRow("no tray host") << true << false << true;
        QTest::newRow("no tray icon") << true << true << false;
    }

    void startsShownWithoutTheSettingOrATray()
    {
        QFETCH(bool, setting);
        QFETCH(bool, host);
        QFETCH(bool, icon);
        Settings::setStartHiddenInTray(setting);
        Settings::setShowTrayIcon(icon);
        Fixture fixture;
        makeWindow(fixture, host, false);
        QVERIFY(!fixture.tray->startHidden(false));
        QVERIFY(QApplication::quitOnLastWindowClosed());
    }
};

QTEST_MAIN(DesktopTrayTest)

#include "desktoptraytest.moc"
