// SPDX-License-Identifier: GPL-3.0-or-later

#include "lansharedialog.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

class LanShareDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        QFile file(m_dir.filePath(QStringLiteral("hello.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Hello");
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("LanShare"));
        KSharedConfig::openStateConfig()->group(QStringLiteral("LanShare")).writeEntry("Https", false);
    }

    void checksOnlyLocalAddresses()
    {
        LanShareDialog dialog({m_dir.path()});
        dialog.setAddresses({QHostAddress(QStringLiteral("192.168.1.2")), QHostAddress(QStringLiteral("8.8.8.8")), QHostAddress(QHostAddress::LocalHost)});
        QCOMPARE(dialog.checkedAddresses(), (QList<QHostAddress>{QHostAddress(QStringLiteral("192.168.1.2")), QHostAddress(QHostAddress::LocalHost)}));
        auto *addresses = dialog.findChild<QListWidget *>(QStringLiteral("addresses"));
        QVERIFY(addresses->item(1)->text().contains(QLatin1String("8.8.8.8")));
        QVERIFY(addresses->item(1)->text() != QLatin1String("8.8.8.8"));

        dialog.setAddresses({});
        QVERIFY(dialog.checkedAddresses().isEmpty());
        QVERIFY(!dialog.startSharing().isEmpty());
    }

    void sharesUntilClosed()
    {
        LanShareDialog dialog({m_dir.path()});
        dialog.setAddresses({QHostAddress(QHostAddress::LocalHost)});
        QCOMPARE(dialog.options().timeoutSeconds, 30 * 60);
        QVERIFY(dialog.options().uploadFolder.isEmpty());
        QVERIFY(!dialog.options().tls);

        QCOMPARE(dialog.startSharing(), QString());
        QVERIFY(dialog.server()->isRunning());
        auto *urls = dialog.findChild<QListWidget *>(QStringLiteral("urls"));
        QCOMPARE(urls->count(), 1);
        QVERIFY(urls->item(0)->text().startsWith(QStringLiteral("http://127.0.0.1:")));

        QTcpSocket socket;
        socket.connectToHost(QHostAddress(QHostAddress::LocalHost), dialog.server()->port());
        QVERIFY(socket.waitForConnected(5000));
        const QByteArray path = QUrl(urls->item(0)->text()).path(QUrl::FullyEncoded).toUtf8();
        socket.write("GET " + path + " HTTP/1.1\r\n\r\n");
        auto *sessions = dialog.findChild<QTreeWidget *>(QStringLiteral("sessions"));
        QTRY_COMPARE(sessions->topLevelItemCount(), 1);
        QCOMPARE(sessions->topLevelItem(0)->text(0), QStringLiteral("127.0.0.1"));

        dialog.reject();
        QVERIFY(!dialog.server()->isRunning());
    }

    void requiresAnUploadFolder()
    {
        LanShareDialog dialog({m_dir.filePath(QStringLiteral("hello.txt"))});
        dialog.setAddresses({QHostAddress(QHostAddress::LocalHost)});
        dialog.findChild<QCheckBox *>(QStringLiteral("uploads"))->setChecked(true);
        auto *folder = dialog.findChild<QLineEdit *>(QStringLiteral("uploadFolder"));
        folder->clear();
        QVERIFY(!dialog.startSharing().isEmpty());
        QVERIFY(!dialog.server()->isRunning());

        folder->setText(m_dir.path());
        QCOMPARE(dialog.options().uploadFolder, m_dir.path());
        QCOMPARE(dialog.startSharing(), QString());
        QCOMPARE(dialog.server()->roots().size(), 2);
        dialog.stopSharing();
        QCOMPARE(KSharedConfig::openStateConfig()->group(QStringLiteral("LanShare")).readEntry("UploadFolder"), m_dir.path());
    }

    void showsTheCertificateFingerprint()
    {
        if (const QString reason = LanShare::tlsUnavailableReason(); !reason.isEmpty()) {
            QSKIP(qPrintable(reason));
        }
        KSharedConfig::openStateConfig()->group(QStringLiteral("LanShare")).writeEntry("Https", true);
        LanShareDialog dialog({m_dir.path()});
        dialog.setAddresses({QHostAddress(QHostAddress::LocalHost)});
        QVERIFY(dialog.options().tls);
        QCOMPARE(dialog.startSharing(), QString());
        QVERIFY(dialog.server()->tls());
        const QString fingerprint = dialog.findChild<QLabel *>(QStringLiteral("fingerprint"))->text();
        QCOMPARE(fingerprint, LanShare::certificateFingerprint(dialog.server()->certificate()));
        QVERIFY(!fingerprint.isEmpty());
    }

    void windowActionIsRegistered()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        QAction *action = window.actionCollection()->action(QStringLiteral("share_on_network"));
        QVERIFY(action);
        QVERIFY(!action->text().isEmpty());
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(LanShareDialogTest)
#include "lansharedialogtest.moc"
