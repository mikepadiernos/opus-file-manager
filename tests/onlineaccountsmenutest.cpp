// SPDX-License-Identifier: GPL-3.0-or-later

#include "onlineaccountsmenu.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KActionMenu>

#include <QAction>
#include <QMenu>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace OnlineAccounts;

namespace
{
QList<Account> sampleAccounts()
{
    Account google;
    google.id = 1;
    google.displayName = QStringLiteral("someone@gmail.com");
    google.provider = QStringLiteral("google");
    google.providerDisplayName = QStringLiteral("Google");
    google.enabled = true;
    google.services = {{QStringLiteral("google-drive"), QStringLiteral("google-drive"), QStringLiteral("Drive"), {}, true, {}, {}}};

    Account nextcloud;
    nextcloud.id = 2;
    nextcloud.displayName = QStringLiteral("alex");
    nextcloud.provider = QStringLiteral("nextcloud");
    nextcloud.providerDisplayName = QStringLiteral("Nextcloud");
    nextcloud.enabled = true;
    nextcloud.services = {{QStringLiteral("nextcloud-storage"),
                           QStringLiteral("dav-storage"),
                           QStringLiteral("Storage"),
                           {},
                           false,
                           QStringLiteral("cloud.example.org"),
                           QStringLiteral("/remote.php/dav/files/alex")}};
    return {google, nextcloud};
}

bool allProtocols(const QString &)
{
    return true;
}

QList<QAction *> entries(QMenu *menu)
{
    QList<QAction *> result;
    const QList<QAction *> actions = menu->actions();
    for (QAction *action : actions) {
        if (!action->isSeparator() || !action->text().isEmpty()) {
            result.append(action);
        }
    }
    return result;
}
}

class OnlineAccountsMenuTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_accountsDir.isValid());
        // Keep libaccounts away from the real account database.
        qputenv("ACCOUNTS", m_accountsDir.path().toUtf8());
        qputenv("AG_PROVIDERS", m_accountsDir.path().toUtf8());
        qputenv("AG_SERVICES", m_accountsDir.path().toUtf8());
    }

    void listsAccountsAndOpensFolders()
    {
        QMenu menu;
        QAction manage(QStringLiteral("Manage"));
        QObject context;
        QList<QPair<QUrl, bool>> opened;
        OnlineAccountsMenu::populate(&menu, sampleAccounts(), QString(), allProtocols, true, &manage, &context, [&opened](const QUrl &url, bool otherPane) {
            opened.append({url, otherPane});
        });

        const QList<QAction *> items = entries(&menu);
        QCOMPARE(items.size(), 5);
        QVERIFY(items.at(0)->isSeparator());
        QCOMPARE(items.at(0)->text(), QStringLiteral("someone@gmail.com (Google)"));

        QMenu *drive = items.at(1)->menu();
        QVERIFY(drive);
        QCOMPARE(items.at(1)->text(), QStringLiteral("Drive"));
        QCOMPARE(items.at(1)->toolTip(), QStringLiteral("gdrive:/someone@gmail.com/"));
        QCOMPARE(drive->actions().size(), 2);
        QVERIFY(drive->actions().at(1)->isEnabled());
        drive->actions().at(0)->trigger();
        drive->actions().at(1)->trigger();
        QCOMPARE(opened.size(), 2);
        QCOMPARE(opened.at(0).first, QUrl(QStringLiteral("gdrive:/someone@gmail.com/")));
        QVERIFY(!opened.at(0).second);
        QVERIFY(opened.at(1).second);

        QVERIFY(items.at(2)->isSeparator());
        QCOMPARE(items.at(2)->text(), QStringLiteral("alex (Nextcloud)"));
        QVERIFY(!items.at(3)->menu());
        QVERIFY(!items.at(3)->isEnabled());
        QVERIFY(!items.at(3)->toolTip().isEmpty());
        QCOMPARE(items.at(4), &manage);
    }

    void otherPaneDisabledInSinglePane()
    {
        QMenu menu;
        QObject context;
        OnlineAccountsMenu::populate(&menu, sampleAccounts(), QString(), allProtocols, false, nullptr, &context, [](const QUrl &, bool) { });
        QMenu *drive = entries(&menu).at(1)->menu();
        QVERIFY(drive);
        QVERIFY(drive->actions().at(0)->isEnabled());
        QVERIFY(!drive->actions().at(1)->isEnabled());
    }

    void emptyAndErrorStates()
    {
        QMenu menu;
        QAction manage(QStringLiteral("Manage"));
        QObject context;
        OnlineAccountsMenu::populate(&menu, {}, QString(), allProtocols, true, &manage, &context, [](const QUrl &, bool) { });
        QList<QAction *> items = entries(&menu);
        QCOMPARE(items.size(), 2);
        QVERIFY(!items.at(0)->isEnabled());
        QCOMPARE(items.at(1), &manage);

        OnlineAccountsMenu::populate(&menu, {}, QStringLiteral("Broken & bad"), allProtocols, true, &manage, &context, [](const QUrl &, bool) { });
        items = entries(&menu);
        QCOMPARE(items.size(), 2);
        QCOMPARE(items.at(0)->text(), QStringLiteral("Broken && bad"));
        QVERIFY(!items.at(0)->isEnabled());
    }

    void goMenuOffersOnlineAccounts()
    {
        MainWindow window;
        auto *accounts = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("online_accounts")));
        QVERIFY(accounts);
        QAction *manage = window.actionCollection()->action(QStringLiteral("manage_online_accounts"));
        QVERIFY(manage);

        Q_EMIT accounts->menu()->aboutToShow();
        const QList<QAction *> items = entries(accounts->menu());
        QCOMPARE(items.size(), 2);
        QVERIFY(!items.first()->isEnabled());
        QCOMPARE(items.last(), manage);
    }

private:
    QTemporaryDir m_accountsDir;
};

QTEST_MAIN(OnlineAccountsMenuTest)
#include "onlineaccountsmenutest.moc"
