// SPDX-License-Identifier: GPL-3.0-or-later

#include "plasmavaultmenu.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KActionMenu>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class FakeVaultClient : public VaultClient
{
    Q_OBJECT

public:
    using VaultClient::VaultClient;

    QList<PlasmaVaults::Vault> list;
    QString error;
    QStringList requests;

    QList<PlasmaVaults::Vault> vaults(QString *errorMessage) const override
    {
        if (errorMessage) {
            *errorMessage = error;
        }
        return error.isEmpty() ? list : QList<PlasmaVaults::Vault>();
    }

    void request(Request request, const QString &device) override
    {
        static const QStringList names{QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("configure"), QStringLiteral("create")};
        requests.append(QString(names.at(int(request)) + u' ' + device).trimmed());
    }

    void emitChanged(const PlasmaVaults::Vault &vault)
    {
        Q_EMIT vaultChanged(vault);
    }
};

class PlasmaVaultMenuTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_mount;

    PlasmaVaults::Vault vault(const QString &name, PlasmaVaults::Status status) const
    {
        PlasmaVaults::Vault result;
        result.name = name;
        result.device = m_dir.path() + QStringLiteral("/") + name + QStringLiteral(".enc");
        result.mountPoint = m_mount;
        result.status = status;
        return result;
    }

    static QAction *find(QMenu *menu, const QString &name)
    {
        for (QAction *action : menu->actions()) {
            if (action->objectName() == name) {
                return action;
            }
            if (action->menu()) {
                if (QAction *found = find(action->menu(), name)) {
                    return found;
                }
            }
        }
        return nullptr;
    }

    static QMenu *vaultsMenu(MainWindow &window)
    {
        auto *action = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("plasma_vaults")));
        return action ? action->menu() : nullptr;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_mount = QDir(m_dir.path()).canonicalPath() + QStringLiteral("/Vaults/Work");
        QVERIFY(QDir().mkpath(m_mount));
    }

    void reportsAnUnavailableService()
    {
        MainWindow window;
        auto *client = new FakeVaultClient;
        client->error = QStringLiteral("Plasma Vault is not available.");
        window.setVaultClient(client);
        QCOMPARE(window.plasmaVaultMenu()->client(), client);
        QMenu *menu = vaultsMenu(window);
        QVERIFY(menu);
        window.plasmaVaultMenu()->populate();
        QAction *unavailable = find(menu, QStringLiteral("vaultsUnavailable"));
        QVERIFY(unavailable);
        QVERIFY(!unavailable->isEnabled());
        QVERIFY(!find(menu, QStringLiteral("createVault")));
    }

    void opensAndShowsAVault()
    {
        MainWindow window;
        auto *client = new FakeVaultClient;
        const PlasmaVaults::Vault closed = vault(QStringLiteral("Work"), PlasmaVaults::Status::Closed);
        client->list = {closed};
        window.setVaultClient(client);
        QMenu *menu = vaultsMenu(window);
        window.plasmaVaultMenu()->populate();
        QVERIFY(!find(menu, QStringLiteral("closeVault")));
        QVERIFY(find(menu, QStringLiteral("configureVault")));
        find(menu, QStringLiteral("openVault"))->trigger();
        QCOMPARE(client->requests, QStringList{QStringLiteral("open ") + closed.device});

        PlasmaVaults::Vault opening = closed;
        opening.status = PlasmaVaults::Status::Opening;
        client->emitChanged(opening);
        QVERIFY(window.activePane()->url() != QUrl::fromLocalFile(m_mount));
        PlasmaVaults::Vault opened = closed;
        opened.status = PlasmaVaults::Status::Opened;
        client->emitChanged(opened);
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(m_mount));

        // Only the vault opened from Plasma Opus is shown, and only once.
        window.activePane()->setUrl(QUrl::fromLocalFile(m_dir.path()));
        client->emitChanged(opened);
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(m_dir.path()));

        find(menu, QStringLiteral("createVault"))->trigger();
        QCOMPARE(client->requests.last(), QStringLiteral("create"));
    }

    void failedOpeningIsReported()
    {
        MainWindow window;
        auto *client = new FakeVaultClient;
        const PlasmaVaults::Vault closed = vault(QStringLiteral("Work"), PlasmaVaults::Status::Closed);
        window.setVaultClient(client);
        QSignalSpy messages(window.plasmaVaultMenu(), &PlasmaVaultMenu::statusMessage);
        window.plasmaVaultMenu()->openVault(closed);
        PlasmaVaults::Vault failed = closed;
        failed.status = PlasmaVaults::Status::Error;
        failed.message = QStringLiteral("Wrong password");
        client->emitChanged(failed);
        QCOMPARE(messages.size(), 1);
        QVERIFY(messages.at(0).at(0).toString().contains(QStringLiteral("Wrong password")));
        failed.status = PlasmaVaults::Status::Opened;
        failed.message.clear();
        client->emitChanged(failed);
        QVERIFY(window.activePane()->url() != QUrl::fromLocalFile(m_mount));
    }

    void closingAVaultInUseAsksFirst()
    {
        MainWindow window;
        auto *client = new FakeVaultClient;
        const PlasmaVaults::Vault opened = vault(QStringLiteral("Work"), PlasmaVaults::Status::Opened);
        client->list = {opened};
        window.setVaultClient(client);
        PlasmaVaultMenu *controller = window.plasmaVaultMenu();

        const QString proc = m_dir.path() + QStringLiteral("/proc");
        QVERIFY(QDir().mkpath(proc + QStringLiteral("/5/fd")));
        controller->setProcRoot(proc);
        controller->populate();
        QMenu *menu = vaultsMenu(window);
        QVERIFY(find(menu, QStringLiteral("showVault")));
        find(menu, QStringLiteral("closeVault"))->trigger();
        QCOMPARE(client->requests, QStringList{QStringLiteral("close ") + opened.device});

        QVERIFY(QFile::link(m_mount, proc + QStringLiteral("/5/cwd")));
        QStringList asked;
        bool answer = false;
        controller->setConfirmClose([&](const PlasmaVaults::Vault &, const QStringList &users) {
            asked = users;
            return answer;
        });
        controller->closeVault(opened);
        QCOMPARE(asked.size(), 1);
        QVERIFY(asked.first().contains(QStringLiteral("5")));
        QCOMPARE(client->requests.size(), 1);
        answer = true;
        controller->closeVault(opened);
        QCOMPARE(client->requests.size(), 2);
    }
};

QTEST_MAIN(PlasmaVaultMenuTest)

#include "plasmavaultmenutest.moc"
