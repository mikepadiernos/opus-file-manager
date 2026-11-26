// SPDX-License-Identifier: GPL-3.0-or-later

#include "kdeconnectsharedialog.h"
#include "kdeconnectclient.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using KdeConnectShare::Device;

namespace
{
class FakeKdeConnect : public KdeConnectClient
{
public:
    bool available = true;
    QList<Device> list;
    QString error;
    QList<std::pair<QString, QList<QUrl>>> sent;

    bool isAvailable() const override
    {
        return available;
    }
    QList<Device> devices() const override
    {
        return available ? list : QList<Device>();
    }
    QString share(const QString &deviceId, const QList<QUrl> &files) override
    {
        if (!error.isEmpty()) {
            return error;
        }
        sent.append({deviceId, files});
        return {};
    }
    void change()
    {
        Q_EMIT changed();
    }
};

const Device Phone{QStringLiteral("phone"), QStringLiteral("Phone"), true, true, true};
const Device Tablet{QStringLiteral("tablet"), QStringLiteral("Tablet"), false, true, true};
}

class KdeConnectShareDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("KdeConnectShare"));
        QVERIFY(m_dir.isValid());
        m_file = m_dir.filePath(QStringLiteral("notes.txt"));
        QFile out(m_file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("notes");
    }

    void reportsAMissingService()
    {
        FakeKdeConnect client;
        client.available = false;
        KdeConnectShareDialog dialog(&client, {QUrl::fromLocalFile(m_file)});
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("kdeConnectMessage"));
        QVERIFY(!message->isHidden());
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("kdeConnectSend"))->isEnabled());

        client.available = true;
        client.list = {Phone};
        client.change();
        QVERIFY(message->isHidden());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("kdeConnectSend"))->isEnabled());
    }

    void reportsNoPairedOrReachableDevice()
    {
        FakeKdeConnect client;
        KdeConnectShareDialog dialog(&client, {QUrl::fromLocalFile(m_file)});
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("kdeConnectMessage"));
        QVERIFY(!message->isHidden());
        QCOMPARE(message->messageType(), KMessageWidget::Information);

        client.list = {Tablet};
        dialog.refresh();
        QCOMPARE(message->messageType(), KMessageWidget::Warning);
        auto *device = dialog.findChild<QComboBox *>(QStringLiteral("kdeConnectDevice"));
        QCOMPARE(device->count(), 1);
        QVERIFY(device->currentText().contains(QStringLiteral("Tablet")));
        QVERIFY(!qobject_cast<QStandardItemModel *>(device->model())->item(0)->isEnabled());
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("kdeConnectDeviceState"))->isHidden());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("kdeConnectSend"))->isEnabled());
    }

    void listsUnsupportedItemsAndSendsOnlyFilesOnRequest()
    {
        FakeKdeConnect client;
        client.list = {Tablet, Phone};
        const QUrl remote(QStringLiteral("smb://server/share/report.pdf"));
        KdeConnectShareDialog dialog(&client, {QUrl::fromLocalFile(m_file), QUrl::fromLocalFile(m_dir.path()), remote});
        auto *items = dialog.findChild<QListWidget *>(QStringLiteral("kdeConnectItems"));
        QCOMPARE(items->count(), 3);
        QVERIFY(items->item(0)->flags() & Qt::ItemIsEnabled);
        QVERIFY(!(items->item(1)->flags() & Qt::ItemIsEnabled));
        QVERIFY(!(items->item(2)->flags() & Qt::ItemIsEnabled));
        QVERIFY(client.sent.isEmpty());

        QCOMPARE(dialog.selectedDeviceId(), Phone.id);
        auto *send = dialog.findChild<QPushButton *>(QStringLiteral("kdeConnectSend"));
        QVERIFY(send->isEnabled());
        send->click();
        QCOMPARE(client.sent.size(), 1);
        QCOMPARE(client.sent.first().first, Phone.id);
        QCOMPARE(client.sent.first().second, QList<QUrl>{QUrl::fromLocalFile(m_file)});

        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("kdeConnectMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(QStringLiteral("Phone")));
        QVERIFY(!send->isEnabled());
        client.change();
        QVERIFY(!message->isHidden());
        QCOMPARE(KSharedConfig::openStateConfig()->group(QStringLiteral("KdeConnectShare")).readEntry("LastDevice"), Phone.id);
    }

    void reportsAFailedRequest()
    {
        FakeKdeConnect client;
        client.list = {Phone};
        client.error = QStringLiteral("No such object");
        KdeConnectShareDialog dialog(&client, {QUrl::fromLocalFile(m_file)});
        dialog.send();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("kdeConnectMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(client.error));
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("kdeConnectSend"))->isEnabled());
    }

    void remembersTheLastDevice()
    {
        const Device laptop{QStringLiteral("laptop"), QStringLiteral("Laptop"), true, true, true};
        KSharedConfig::openStateConfig()->group(QStringLiteral("KdeConnectShare")).writeEntry("LastDevice", Phone.id);
        FakeKdeConnect client;
        client.list = {laptop, Phone};
        KdeConnectShareDialog dialog(&client, {QUrl::fromLocalFile(m_file)});
        QCOMPARE(dialog.selectedDeviceId(), Phone.id);
    }

    void windowActionOpensTheDialog()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        auto *client = new FakeKdeConnect;
        client->list = {Phone};
        window.setKdeConnectClient(client);
        QCOMPARE(window.kdeConnectClient(), client);
        QAction *action = window.actionCollection()->action(QStringLiteral("kdeconnect_send"));
        QVERIFY(action);
        QVERIFY(!action->text().isEmpty());
        window.sendToDevice();
        QVERIFY(!window.findChild<KdeConnectShareDialog *>());
    }

private:
    QTemporaryDir m_dir;
    QString m_file;
};

QTEST_MAIN(KdeConnectShareDialogTest)
#include "kdeconnectsharedialogtest.moc"
