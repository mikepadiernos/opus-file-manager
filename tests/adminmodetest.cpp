// SPDX-License-Identifier: GPL-3.0-or-later

#include "adminmode.h"

#include <QTest>

class AdminModeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void convertsLocalFoldersToAdminUrls();
    void convertsAdminUrlsBack();
    void leavesOtherLocationsAlone();
};

void AdminModeTest::convertsLocalFoldersToAdminUrls()
{
    const QUrl admin = AdminMode::toAdmin(QUrl::fromLocalFile(QStringLiteral("/etc/ssh")));
    QCOMPARE(admin.scheme(), QStringLiteral("admin"));
    QCOMPARE(admin.path(), QStringLiteral("/etc/ssh"));
    QVERIFY(AdminMode::isAdminUrl(admin));
    QCOMPARE(AdminMode::toAdmin(admin), admin);
    QVERIFY(AdminMode::canToggle(QUrl::fromLocalFile(QStringLiteral("/"))));
    QVERIFY(AdminMode::canToggle(admin));
}

void AdminModeTest::convertsAdminUrlsBack()
{
    const QUrl folder = QUrl::fromLocalFile(QStringLiteral("/etc/a folder"));
    QCOMPARE(AdminMode::toUser(AdminMode::toAdmin(folder)), folder);
    QCOMPARE(AdminMode::toUser(QUrl(QStringLiteral("admin:/usr/share"))), QUrl::fromLocalFile(QStringLiteral("/usr/share")));
    QVERIFY(AdminMode::toUser(QUrl(QStringLiteral("admin:/usr/share"))).isLocalFile());
}

void AdminModeTest::leavesOtherLocationsAlone()
{
    const QUrl remote(QStringLiteral("sftp://host/etc"));
    QVERIFY(AdminMode::toAdmin(remote).isEmpty());
    QCOMPARE(AdminMode::toUser(remote), remote);
    QVERIFY(!AdminMode::canToggle(remote));
    QVERIFY(!AdminMode::canToggle(QUrl(QStringLiteral("trash:/"))));
    QVERIFY(!AdminMode::isAdminUrl(QUrl::fromLocalFile(QStringLiteral("/etc"))));
}

QTEST_GUILESS_MAIN(AdminModeTest)
#include "adminmodetest.moc"
