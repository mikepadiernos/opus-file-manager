// SPDX-License-Identifier: GPL-3.0-or-later

#include "mountmanagerdialog.h"
#include "mainwindow.h"
#include "mountbackend.h"
#include "pane.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QAction>
#include <QCheckBox>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

namespace
{
class FakeMountBackend : public MountBackend
{
public:
    QList<MountDevice> list;
    QSet<QString> running;
    QList<std::pair<QString, MountOperation>> started;
    int refreshes = 0;
    int sizeRefreshes = 0;

    QList<MountDevice> devices() const override
    {
        return list;
    }
    void refresh() override
    {
        ++refreshes;
    }
    void refreshSizes() override
    {
        ++sizeRefreshes;
    }
    void start(const QString &id, MountOperation operation) override
    {
        started.append({id, operation});
        running.insert(id);
    }
    bool isRunning(const QString &id) const override
    {
        return running.contains(id);
    }

    /// Finishes the running operation on @p id, updating its state on success.
    void complete(const QString &id, MountOperation operation, const QString &error = {}, const QString &mountPoint = {})
    {
        running.remove(id);
        if (error.isEmpty()) {
            for (MountDevice &device : list) {
                if (device.id == id) {
                    device.mounted = operation == MountOperation::Mount;
                    device.canMount = !device.mounted;
                    device.canUnmount = device.mounted;
                    if (!mountPoint.isEmpty()) {
                        device.mountPoint = mountPoint;
                    }
                }
            }
            Q_EMIT changed();
        }
        Q_EMIT finished(id, operation, error, mountPoint);
    }
};

MountDevice rootDevice()
{
    MountDevice device;
    device.id = QStringLiteral("/");
    device.name = QStringLiteral("Root");
    device.device = QStringLiteral("/dev/nvme0n1p2");
    device.mountPoint = QStringLiteral("/");
    device.type = QStringLiteral("ext4");
    device.iconName = QStringLiteral("drive-harddisk");
    device.mounted = true;
    device.inFstab = true;
    device.size = 1000;
    device.available = 250;
    return device;
}

MountDevice stick(const QString &mountPoint)
{
    MountDevice device;
    device.id = QStringLiteral("/org/freedesktop/UDisks2/block_devices/sdb1");
    device.udi = device.id;
    device.name = QStringLiteral("Stick");
    device.device = QStringLiteral("/dev/sdb1");
    device.mountPoint = mountPoint;
    device.type = QStringLiteral("exfat");
    device.iconName = QStringLiteral("drive-removable-media-usb");
    device.removable = true;
    device.canMount = true;
    return device;
}

MountDevice backup()
{
    MountDevice device;
    device.id = QStringLiteral("/mnt/backup");
    device.name = QStringLiteral("backup");
    device.device = QStringLiteral("/dev/sdc1");
    device.mountPoint = QStringLiteral("/mnt/backup");
    device.type = QStringLiteral("ext4");
    device.inFstab = true;
    device.mounted = true;
    device.canUnmount = true;
    device.size = 2000;
    device.available = 2000;
    return device;
}

MountDevice disc()
{
    MountDevice device;
    device.id = QStringLiteral("/org/freedesktop/UDisks2/block_devices/sr0");
    device.udi = device.id;
    device.name = QStringLiteral("Disc");
    device.device = QStringLiteral("/dev/sr0");
    device.type = QStringLiteral("iso9660");
    device.removable = true;
    device.canMount = true;
    device.canEject = true;
    return device;
}

QTreeWidgetItem *rowOf(QTreeWidget *tree, const QString &id)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->data(MountManagerDialog::NameColumn, Qt::UserRole).toString() == id) {
            return tree->topLevelItem(i);
        }
    }
    return nullptr;
}
}

class MountManagerDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("MountManager"));
        QVERIFY(m_dir.isValid());
    }

    void listsFileSystemsWithTheirUsage()
    {
        FakeMountBackend backend;
        backend.list = {rootDevice(), stick(m_dir.path()), disc()};
        MountManagerDialog dialog(&backend);
        QCOMPARE(backend.refreshes, 1);
        QCOMPARE(backend.sizeRefreshes, 1);
        auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("mountManagerList"));
        QCOMPARE(tree->topLevelItemCount(), 3);
        QTreeWidgetItem *root = rowOf(tree, QStringLiteral("/"));
        QCOMPARE(root->text(MountManagerDialog::NameColumn), QStringLiteral("Root"));
        QCOMPARE(root->text(MountManagerDialog::DeviceColumn), QStringLiteral("/dev/nvme0n1p2"));
        QCOMPARE(root->text(MountManagerDialog::TypeColumn), QStringLiteral("ext4"));
        QCOMPARE(root->text(MountManagerDialog::MountPointColumn), QStringLiteral("/"));
        QVERIFY(!root->text(MountManagerDialog::SizeColumn).isEmpty());
        QCOMPARE(root->data(MountManagerDialog::UsageColumn, Qt::UserRole).toInt(), 75);
        QVERIFY(root->text(MountManagerDialog::UsageColumn).contains(QStringLiteral("75")));
        QTreeWidgetItem *usb = rowOf(tree, stick({}).id);
        QVERIFY(usb->text(MountManagerDialog::SizeColumn).isEmpty());
        QVERIFY(!usb->data(MountManagerDialog::UsageColumn, Qt::UserRole).isValid());
        QVERIFY(usb->text(MountManagerDialog::StateColumn) != root->text(MountManagerDialog::StateColumn));

        backend.list.removeLast();
        Q_EMIT backend.changed();
        QCOMPARE(tree->topLevelItemCount(), 2);
    }

    void showsOnlyRemovableDevicesOnRequest()
    {
        FakeMountBackend backend;
        backend.list = {rootDevice(), stick(m_dir.path()), backup()};
        {
            MountManagerDialog dialog(&backend);
            auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("mountManagerList"));
            dialog.findChild<QCheckBox *>(QStringLiteral("mountManagerRemovableOnly"))->setChecked(true);
            QCOMPARE(tree->topLevelItemCount(), 1);
            QVERIFY(rowOf(tree, stick({}).id));
        }
        MountManagerDialog reopened(&backend);
        QVERIFY(reopened.findChild<QCheckBox *>(QStringLiteral("mountManagerRemovableOnly"))->isChecked());
        QCOMPARE(reopened.findChild<QTreeWidget *>(QStringLiteral("mountManagerList"))->topLevelItemCount(), 1);
    }

    void enablesOnlyPossibleOperations()
    {
        FakeMountBackend backend;
        backend.list = {rootDevice(), stick(m_dir.path()), backup(), disc()};
        MountManagerDialog dialog(&backend);
        auto *open = dialog.findChild<QPushButton *>(QStringLiteral("mountManagerOpen"));
        auto *mount = dialog.findChild<QPushButton *>(QStringLiteral("mountManagerMount"));
        auto *unmount = dialog.findChild<QPushButton *>(QStringLiteral("mountManagerUnmount"));
        auto *eject = dialog.findChild<QPushButton *>(QStringLiteral("mountManagerEject"));

        dialog.selectDevice(QStringLiteral("/"));
        QCOMPARE(dialog.currentId(), QStringLiteral("/"));
        QVERIFY(open->isEnabled());
        QVERIFY(!mount->isEnabled());
        QVERIFY(!unmount->isEnabled());
        QVERIFY(!eject->isEnabled());

        dialog.selectDevice(backup().id);
        QVERIFY(open->isEnabled());
        QVERIFY(!mount->isEnabled());
        QVERIFY(unmount->isEnabled());

        dialog.selectDevice(stick({}).id);
        QVERIFY(open->isEnabled());
        QVERIFY(mount->isEnabled());
        QVERIFY(!unmount->isEnabled());
        QVERIFY(!eject->isEnabled());

        dialog.selectDevice(disc().id);
        QVERIFY(eject->isEnabled());

        // Operations that are not possible are ignored.
        dialog.selectDevice(QStringLiteral("/"));
        dialog.startOperation(MountOperation::Unmount);
        QVERIFY(backend.started.isEmpty());
    }

    void mountsAndOpensOnActivation()
    {
        FakeMountBackend backend;
        backend.list = {rootDevice(), stick(QString())};
        MountManagerDialog dialog(&backend);
        QSignalSpy opened(&dialog, &MountManagerDialog::openRequested);
        dialog.selectDevice(stick({}).id);
        dialog.activateCurrent();
        QCOMPARE(backend.started.size(), 1);
        QCOMPARE(backend.started.first(), std::make_pair(stick({}).id, MountOperation::Mount));
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mountManagerMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Information);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("mountManagerMount"))->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("mountManagerOpen"))->isEnabled());

        // Activating again while it mounts does not start a second operation.
        dialog.activateCurrent();
        QCOMPARE(backend.started.size(), 1);

        backend.complete(stick({}).id, MountOperation::Mount, {}, m_dir.path());
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.first().first().toUrl(), QUrl::fromLocalFile(m_dir.path()));
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(m_dir.path()));
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("mountManagerUnmount"))->isEnabled());

        // A mounted file system opens directly.
        dialog.activateCurrent();
        QCOMPARE(opened.size(), 2);
        QCOMPARE(backend.started.size(), 1);
    }

    void reportsFailures()
    {
        FakeMountBackend backend;
        backend.list = {backup()};
        MountManagerDialog dialog(&backend);
        QSignalSpy opened(&dialog, &MountManagerDialog::openRequested);
        dialog.selectDevice(backup().id);
        dialog.startOperation(MountOperation::Unmount);
        QCOMPARE(backend.started.first(), std::make_pair(backup().id, MountOperation::Unmount));
        const QString error = mountErrorText(MountOperation::Unmount, 2, QStringLiteral("target is busy"));
        backend.complete(backup().id, MountOperation::Unmount, error);
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mountManagerMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(error));
        QVERIFY(opened.isEmpty());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("mountManagerUnmount"))->isEnabled());
    }

    void describesErrors()
    {
        QVERIFY(mountErrorText(MountOperation::Mount, 0, QStringLiteral("ignored")).isEmpty());
        const QString denied = mountErrorText(MountOperation::Mount, 1, {});
        QVERIFY(!denied.isEmpty());
        const QString busy = mountErrorText(MountOperation::Unmount, 2, QStringLiteral("umount: target is busy."));
        QVERIFY(busy.contains(QStringLiteral("target is busy")));
        QVERIFY(mountErrorText(MountOperation::Eject, 4, QStringLiteral("dismissed")) != mountErrorText(MountOperation::Eject, 3, QStringLiteral("dismissed")));
        QCOMPARE(mountProcessErrorType(QStringLiteral("umount: /mnt/backup: target is busy.")), 2);
        QCOMPARE(mountProcessErrorType(QStringLiteral("mount: /mnt/data: must be superuser to use mount.")), 1);
        QCOMPARE(mountProcessErrorType(QStringLiteral("mount: /mnt/data: can't find in /etc/fstab.")), 3);
    }

    void windowMenuOpensAndMountsFileSystems()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        auto *backend = new FakeMountBackend;
        backend->list = {rootDevice(), stick(QString())};
        window.setMountBackend(backend);
        QCOMPARE(window.mountBackend(), backend);
        QVERIFY(window.actionCollection()->action(QStringLiteral("mount_manager")));
        auto *menuAction = window.actionCollection()->action(QStringLiteral("mount_points"));
        QVERIFY(menuAction);
        QMenu *menu = menuAction->menu();
        QVERIFY(menu);

        window.populateMountPoints();
        QCOMPARE(backend->refreshes, 1);
        const QList<QAction *> entries = menu->actions();
        // Two file systems, a separator and the Mount Manager action.
        QCOMPARE(entries.size(), 4);
        QVERIFY(entries.at(0)->isChecked());
        QVERIFY(!entries.at(1)->isChecked());
        QVERIFY(entries.at(1)->isEnabled());
        QCOMPARE(entries.last(), window.actionCollection()->action(QStringLiteral("mount_manager")));

        entries.at(1)->trigger();
        QCOMPARE(backend->started.size(), 1);
        backend->complete(stick({}).id, MountOperation::Mount, {}, m_dir.path());
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(m_dir.path()));

        window.showMountManager();
        auto *dialog = window.findChild<MountManagerDialog *>();
        QVERIFY(dialog);
        window.showMountManager();
        QCOMPARE(window.findChildren<MountManagerDialog *>().size(), 1);
        dialog->close();
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(MountManagerDialogTest)
#include "mountmanagerdialogtest.moc"
