// SPDX-License-Identifier: GPL-3.0-or-later

#include "kwinintegration.h"
#include "dropdownwindow.h"
#include "mainwindow.h"
#include "settingsdialog.h"

#include <KActionCollection>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTest>

namespace
{
struct FakeKWin {
    bool running = true;
    std::optional<bool> loaded = false;
    int installed = -1;
    bool enabled = false;
    int starts = 0;
    int unloads = 0;
    int processRequests = 0;
    bool forgotten = false;
    QString installedFrom;
    bool installedAsUpdate = false;
    bool sourceComplete = false;
    KWinBackend::Callback pendingInstall;
    KWinBackend::Callback pendingRemove;
};

class FakeKWinBackend : public KWinBackend
{
public:
    explicit FakeKWinBackend(std::shared_ptr<FakeKWin> state)
        : m(std::move(state))
    {
    }

    bool kwinRunning() const override
    {
        return m->running;
    }
    std::optional<bool> scriptLoaded() const override
    {
        return m->loaded;
    }
    int installedVersion() const override
    {
        return m->installed;
    }
    bool pluginEnabled() const override
    {
        return m->enabled;
    }
    void setPluginEnabled(bool enabled) override
    {
        m->enabled = enabled;
    }
    void forgetPluginEnabled() override
    {
        m->enabled = false;
        m->forgotten = true;
    }
    void startScripts() override
    {
        ++m->starts;
        if (m->enabled && m->installed >= 0) {
            m->loaded = true;
        }
    }
    void unloadScript() override
    {
        ++m->unloads;
        m->loaded = false;
    }
    void invokeProcessShortcut() override
    {
        ++m->processRequests;
    }
    void installPackage(const QString &sourceDirectory, bool update, const Callback &done) override
    {
        m->installedFrom = sourceDirectory;
        m->installedAsUpdate = update;
        m->sourceComplete =
            QFile::exists(sourceDirectory + QLatin1String("/metadata.json")) && QFile::exists(sourceDirectory + QLatin1String("/contents/code/main.js"));
        m->pendingInstall = done;
    }
    void removePackage(const Callback &done) override
    {
        m->pendingRemove = done;
    }

private:
    std::shared_ptr<FakeKWin> m;
};

QJsonObject parse(const QString &reply)
{
    return QJsonDocument::fromJson(reply.toUtf8()).object();
}

QString placementJson(int x)
{
    return QStringLiteral(
               R"({"screen":"DP-1","desktops":["d1"],"onAllDesktops":false,"geometry":{"x":%1,"y":0,"width":800,"height":600},"keepAbove":false,"tile":null})")
        .arg(x);
}
}

class KWinIntegrationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        m_config = KSharedConfig::openConfig(QStringLiteral("kwinintegrationtestrc"), KConfig::SimpleConfig);
        m_state = KSharedConfig::openConfig(QStringLiteral("kwinintegrationteststaterc"), KConfig::SimpleConfig);
        m_config->deleteGroup(QStringLiteral("KWinScript"));
        m_config->deleteGroup(QStringLiteral("KWinVirtualDesktops"));
        m_state->deleteGroup(QStringLiteral("KWinWindowPlacements"));
        m_fake = std::make_shared<FakeKWin>();
    }

    void bundlesTheScriptPackage()
    {
        QCOMPARE(KWinIntegration::bundledVersion(), 1);
        QTemporaryDir dir;
        const QString root = KWinIntegration::extractPackage(dir.path());
        QCOMPARE(root, dir.path() + QLatin1String("/opusfilemanager"));
        QFile script(root + QLatin1String("/contents/code/main.js"));
        QVERIFY(script.open(QIODevice::ReadOnly));
        QVERIFY(script.readAll().contains("org.kde.opusfilemanager.KWin1"));
    }

    void syncAnswersWithThePidAndPendingCommandsOnce()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        kwin.setWorkspaceProvider([] {
            return QStringLiteral("w1");
        });
        QVERIFY(!kwin.restoreWorkspace(QStringLiteral("w1")));
        kwin.WindowChanged(placementJson(25));
        QVERIFY(kwin.placement(QStringLiteral("w1")));
        QCOMPARE(kwin.placement(QStringLiteral("w1"))->geometry.x(), 25);

        QVERIFY(kwin.restoreWorkspace(QStringLiteral("w1")));
        const QJsonObject first = parse(kwin.Sync(QStringLiteral("1")));
        QCOMPARE(first.value(QStringLiteral("pid")).toInteger(), QCoreApplication::applicationPid());
        const QJsonArray commands = first.value(QStringLiteral("commands")).toArray();
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands.first().toObject().value(QStringLiteral("type")).toString(), QStringLiteral("restore"));
        QVERIFY(parse(kwin.Sync(QStringLiteral("1"))).value(QStringLiteral("commands")).toArray().isEmpty());

        kwin.setOptions({.restoreWindows = false});
        QVERIFY(!kwin.restoreWorkspace(QStringLiteral("w1")));
    }

    void ignoresMalformedPlacementsAndMissingWorkspaces()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        kwin.WindowChanged(placementJson(1));
        QVERIFY(m_state->group(QStringLiteral("KWinWindowPlacements")).keyList().isEmpty());
        kwin.setWorkspaceProvider([] {
            return QStringLiteral("w1");
        });
        kwin.WindowChanged(QStringLiteral("{}"));
        QVERIFY(!kwin.placement(QStringLiteral("w1")));
    }

    void requestsSyncOnlyWhenTheScriptIsEnabled()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        kwin.requestSync();
        QTest::qWait(10);
        QCOMPARE(m_fake->processRequests, 0);
        m_fake->enabled = true;
        kwin.requestSync();
        kwin.requestSync();
        QTRY_COMPARE(m_fake->processRequests, 1);
        m_fake->running = false;
        kwin.requestSync();
        QTest::qWait(10);
        QCOMPARE(m_fake->processRequests, 1);
    }

    void followsVirtualDesktopsWhenAsked()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        QSignalSpy desktops(&kwin, &KWinIntegration::desktopsChanged);
        kwin.DesktopsChanged(QStringLiteral(R"([{"id":"d1","name":"Work"}])"));
        kwin.DesktopsChanged(QStringLiteral(R"([{"id":"d1","name":"Work"}])"));
        QCOMPARE(desktops.size(), 1);
        QCOMPARE(kwin.desktops().first().name, QStringLiteral("Work"));

        QSignalSpy requested(&kwin, &KWinIntegration::desktopWorkspaceRequested);
        kwin.setDesktopWorkspaces({{QStringLiteral("d1"), QStringLiteral("w1")}});
        kwin.CurrentDesktopChanged(QStringLiteral("d1"));
        QCOMPARE(requested.size(), 0);
        kwin.setOptions({.followDesktops = true});
        kwin.CurrentDesktopChanged(QStringLiteral("d2"));
        kwin.CurrentDesktopChanged(QStringLiteral("d1"));
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toString(), QStringLiteral("w1"));

        QSignalSpy edge(&kwin, &KWinIntegration::edgeActivated);
        kwin.EdgeActivated();
        QCOMPARE(edge.size(), 1);
    }

    void reportsItsStatus()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        QCOMPARE(kwin.status(), KWinScript::Status::NotInstalled);
        m_fake->loaded = std::nullopt;
        QCOMPARE(kwin.status(), KWinScript::Status::ScriptingUnavailable);
        m_fake->running = false;
        QCOMPARE(kwin.status(), KWinScript::Status::KWinUnavailable);
        QVERIFY(!kwin.statusText().isEmpty());
        m_fake->running = true;
        m_fake->loaded = true;
        m_fake->installed = 1;
        m_fake->enabled = true;
        QCOMPARE(kwin.status(), KWinScript::Status::NotRunning);
        QVERIFY(!kwin.scriptActive());
        kwin.Sync(QStringLiteral("1"));
        QCOMPARE(kwin.status(), KWinScript::Status::Running);
        QVERIFY(kwin.scriptActive());
    }

    void installsEnablesAndRemovesTheScript()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        QSignalSpy finished(&kwin, &KWinIntegration::operationFinished);
        kwin.install();
        QVERIFY(m_fake->sourceComplete);
        QVERIFY(!m_fake->installedAsUpdate);
        QVERIFY(m_fake->pendingInstall);
        m_fake->installed = 1;
        std::exchange(m_fake->pendingInstall, {})(QString());
        QCOMPARE(finished.size(), 1);
        QVERIFY(finished.first().first().toString().isEmpty());
        QVERIFY(m_fake->enabled);
        QCOMPARE(m_fake->unloads, 1);
        QCOMPARE(m_fake->starts, 1);
        QCOMPARE(kwin.status(), KWinScript::Status::NotRunning);
        QTRY_COMPARE(m_fake->processRequests, 1);

        kwin.install();
        QVERIFY(m_fake->installedAsUpdate);
        std::exchange(m_fake->pendingInstall, {})(QStringLiteral("broken"));
        QCOMPARE(finished.last().first().toString(), QStringLiteral("broken"));

        kwin.setEnabled(false);
        QVERIFY(!m_fake->enabled);
        QCOMPARE(kwin.status(), KWinScript::Status::Disabled);
        kwin.setEnabled(true);
        QCOMPARE(kwin.status(), KWinScript::Status::NotRunning);

        kwin.remove();
        QVERIFY(m_fake->forgotten);
        QVERIFY(m_fake->pendingRemove);
        m_fake->installed = -1;
        std::exchange(m_fake->pendingRemove, {})(QString());
        QCOMPARE(kwin.status(), KWinScript::Status::NotInstalled);
    }

    void dropDownWindowTogglesTheWindow()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        QWidget window;
        DropDownWindow dropDown(&window, &kwin);
        QSignalSpy notices(&dropDown, &DropDownWindow::notice);
        dropDown.toggle();
        QCOMPARE(notices.size(), 1);
        QVERIFY(!window.isVisible());

        KWinScript::DropDown options;
        options.enabled = true;
        options.workspaceId = QStringLiteral("w1");
        kwin.setDropDown(options);
        QSignalSpy workspaces(&dropDown, &DropDownWindow::workspaceRequested);
        dropDown.toggle();
        QVERIFY(window.isVisible());
        QCOMPARE(notices.size(), 2);
        QCOMPARE(workspaces.size(), 1);
        QVERIFY(QTest::qWaitForWindowActive(&window));

        dropDown.toggle();
        QVERIFY(!window.isVisible());
        QVERIFY(dropDown.isHidden());
        QVERIFY(!QApplication::quitOnLastWindowClosed());
        dropDown.toggle();
        QVERIFY(window.isVisible());
        QVERIFY(!dropDown.isHidden());
        QVERIFY(QApplication::quitOnLastWindowClosed());
        QCOMPARE(notices.size(), 2);
    }

    void dropDownWindowStaysForItsOwnWindows()
    {
        KWinIntegration kwin(std::make_unique<FakeKWinBackend>(m_fake), m_config, m_state);
        KWinScript::DropDown options;
        options.enabled = true;
        options.hideOnFocusLoss = true;
        kwin.setDropDown(options);
        QWidget window;
        DropDownWindow dropDown(&window, &kwin);
        dropDown.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(!DropDownWindow::attentionNeeded(&window));

        QWidget other;
        other.show();
        other.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&other));
        QVERIFY(DropDownWindow::attentionNeeded(&window));
        QTest::qWait(400);
        QVERIFY(window.isVisible());
    }

    void mainWindowOffersTheIntegration()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("KWinScript"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("KWinVirtualDesktops"));
        MainWindow window;
        QVERIFY(window.kwinIntegration());
        QVERIFY(window.dropDownWindow());
        window.setKWinBackend(std::make_unique<FakeKWinBackend>(m_fake));
        QCOMPARE(window.kwinIntegration()->status(), KWinScript::Status::NotInstalled);
        QVERIFY(window.actionCollection()->action(QStringLiteral("toggle_drop_down")));

        // Configure KWin Integration… opens Preferences at KDE Plasma › KWin.
        window.actionCollection()->action(QStringLiteral("kwin_integration"))->trigger();
        auto *dialog = qobject_cast<SettingsDialog *>(KConfigDialog::exists(QLatin1String(SettingsDialog::Name)));
        QVERIFY(dialog);
        auto *page = dialog->findChild<QWidget *>(QStringLiteral("kwinIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->currentWidget(), page);
        QVERIFY(page->isVisible());
        window.showKWinIntegration();
        QCOMPARE(KConfigDialog::exists(QLatin1String(SettingsDialog::Name)), dialog);
        page->findChild<QPushButton *>(QStringLiteral("kwinInstall"))->click();
        QVERIFY(m_fake->sourceComplete);
        dialog->close();

        window.kwinIntegration()->setOptions({.followDesktops = true});
        window.kwinIntegration()->setDesktopWorkspaces({{QStringLiteral("d1"), QStringLiteral("missing")}});
        window.kwinIntegration()->CurrentDesktopChanged(QStringLiteral("d1"));
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("The requested workspace no longer exists."));
    }

private:
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_state;
    std::shared_ptr<FakeKWin> m_fake;
};

QTEST_MAIN(KWinIntegrationTest)
#include "kwinintegrationtest.moc"
