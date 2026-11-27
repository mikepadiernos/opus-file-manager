// SPDX-License-Identifier: GPL-3.0-or-later

#include "mountmanagerdialog.h"
#include "settings.h"
#include "symbolicicon.h"

#include <KFormat>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegateFactory>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KService>
#include <KWindowStateSaver>

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
const QString PartitionManager = QStringLiteral("org.kde.partitionmanager");

/// Draws the usage column as a bar, keeping the percentage as the item's text for accessibility.
class UsageDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        bool ok = false;
        const int percent = index.data(Qt::UserRole).toInt(&ok);
        if (!ok || percent < 0) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        item.text.clear();
        const QWidget *widget = option.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        QStyleOptionProgressBar bar;
        bar.rect = option.rect.adjusted(2, 2, -2, -2);
        bar.minimum = 0;
        bar.maximum = 100;
        bar.progress = percent;
        bar.text = index.data(Qt::DisplayRole).toString();
        bar.textVisible = true;
        bar.textAlignment = Qt::AlignCenter;
        bar.state = option.state | QStyle::State_Horizontal;
        bar.palette = option.palette;
        style->drawControl(QStyle::CE_ProgressBar, &bar, painter, widget);
    }
};

QString stateText(const MountDevice &device, bool running)
{
    if (running) {
        return i18nc("@item:intable mount state", "In progress…");
    }
    return device.mounted ? i18nc("@item:intable mount state", "Mounted") : i18nc("@item:intable mount state", "Not mounted");
}
}

MountManagerDialog::MountManagerDialog(MountBackend *backend, QWidget *parent)
    : QDialog(parent)
    , m_backend(backend)
{
    setObjectName(QStringLiteral("mountManagerDialog"));
    setWindowTitle(i18nc("@title:window", "Mount Manager"));
    auto *layout = new QVBoxLayout(this);

    m_message = new KMessageWidget(this);
    m_message->setObjectName(QStringLiteral("mountManagerMessage"));
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(true);
    m_message->hide();
    layout->addWidget(m_message);

    m_removableOnly = new QCheckBox(i18nc("@option:check", "Show only removable devices"), this);
    m_removableOnly->setObjectName(QStringLiteral("mountManagerRemovableOnly"));
    m_removableOnly->setToolTip(i18nc("@info:tooltip", "Choose whether this starts checked under Preferences › KDE Plasma › Mount Manager"));
    m_removableOnly->setChecked(Settings::mountRemovableOnly());
    layout->addWidget(m_removableOnly);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("mountManagerList"));
    m_tree->setAccessibleName(i18nc("@info:accessibility", "File systems"));
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setHeaderLabels({
        i18nc("@title:column", "Name"),
        i18nc("@title:column mount state", "State"),
        i18nc("@title:column", "Device"),
        i18nc("@title:column file system type", "Type"),
        i18nc("@title:column", "Mount Point"),
        i18nc("@title:column", "Size"),
        i18nc("@title:column", "Free"),
        i18nc("@title:column", "Usage"),
    });
    m_tree->setItemDelegateForColumn(UsageColumn, new UsageDelegate(m_tree));
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(UsageColumn, QHeaderView::Stretch);
    layout->addWidget(m_tree, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    const auto addButton = [buttons](const QString &name, const QString &text, const QString &icon) {
        QPushButton *button = buttons->addButton(text, QDialogButtonBox::ActionRole);
        button->setObjectName(name);
        button->setIcon(SymbolicIcon::fromTheme(icon));
        return button;
    };
    m_open = addButton(QStringLiteral("mountManagerOpen"), i18nc("@action:button", "Open"), QStringLiteral("document-open-folder"));
    m_mount = addButton(QStringLiteral("mountManagerMount"), i18nc("@action:button", "Mount"), QStringLiteral("media-mount"));
    m_unmount = addButton(QStringLiteral("mountManagerUnmount"), i18nc("@action:button", "Unmount"), QStringLiteral("media-eject"));
    m_eject = addButton(QStringLiteral("mountManagerEject"), i18nc("@action:button", "Eject"), QStringLiteral("media-eject"));
    m_refresh = addButton(QStringLiteral("mountManagerRefresh"), i18nc("@action:button", "Refresh"), QStringLiteral("view-refresh"));
    m_partitionManager =
        addButton(QStringLiteral("mountManagerPartitionManager"), i18nc("@action:button", "Partition Manager…"), QStringLiteral("partitionmanager"));
    m_partitionManager->setToolTip(i18nc("@info:tooltip", "Partition, format and edit file systems in KDE Partition Manager"));
    m_partitionManager->setVisible(KService::serviceByDesktopName(PartitionManager));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_open, &QPushButton::clicked, this, &MountManagerDialog::activateCurrent);
    connect(m_mount, &QPushButton::clicked, this, [this] {
        startOperation(MountOperation::Mount);
    });
    connect(m_unmount, &QPushButton::clicked, this, [this] {
        startOperation(MountOperation::Unmount);
    });
    connect(m_eject, &QPushButton::clicked, this, [this] {
        startOperation(MountOperation::Eject);
    });
    connect(m_refresh, &QPushButton::clicked, this, [this] {
        m_backend->refresh();
        m_backend->refreshSizes();
    });
    connect(m_partitionManager, &QPushButton::clicked, this, [this] {
        auto *job = new KIO::ApplicationLauncherJob(KService::serviceByDesktopName(PartitionManager));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
    connect(m_removableOnly, &QCheckBox::toggled, this, &MountManagerDialog::populate);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &MountManagerDialog::updateButtons);
    connect(m_tree, &QTreeWidget::itemActivated, this, &MountManagerDialog::activateCurrent);
    connect(m_backend, &MountBackend::changed, this, &MountManagerDialog::populate);
    connect(m_backend, &MountBackend::finished, this, &MountManagerDialog::onFinished);

    // The kernel does not notify about mounts made by other programs, so poll while open.
    m_poll = new QTimer(this);
    m_poll->setInterval(3000);
    connect(m_poll, &QTimer::timeout, m_backend, &MountBackend::refresh);
    m_poll->start();

    resize(900, 420);
    new KWindowStateSaver(this, QStringLiteral("MountManagerDialog"));
    m_backend->refresh();
    m_backend->refreshSizes();
    populate();
}

QString MountManagerDialog::currentId() const
{
    const QTreeWidgetItem *item = m_tree->currentItem();
    return item ? item->data(NameColumn, Qt::UserRole).toString() : QString();
}

void MountManagerDialog::selectDevice(const QString &id)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        if (item->data(NameColumn, Qt::UserRole).toString() == id) {
            m_tree->setCurrentItem(item);
            return;
        }
    }
}

std::optional<MountDevice> MountManagerDialog::deviceById(const QString &id) const
{
    const QList<MountDevice> devices = m_backend->devices();
    for (const MountDevice &device : devices) {
        if (device.id == id) {
            return device;
        }
    }
    return std::nullopt;
}

void MountManagerDialog::populate()
{
    const QString current = currentId();
    const bool removableOnly = m_removableOnly->isChecked();
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();
    const QList<MountDevice> devices = m_backend->devices();
    KFormat format;
    for (const MountDevice &device : devices) {
        if (removableOnly && !device.removable) {
            continue;
        }
        auto *item = new QTreeWidgetItem(m_tree);
        item->setData(NameColumn, Qt::UserRole, device.id);
        item->setIcon(NameColumn, QIcon::fromTheme(device.iconName, QIcon::fromTheme(QStringLiteral("drive-harddisk"))));
        item->setText(NameColumn, device.name);
        item->setText(StateColumn, stateText(device, m_backend->isRunning(device.id)));
        item->setText(DeviceColumn, device.device);
        item->setText(TypeColumn, device.type);
        item->setText(MountPointColumn, device.mountPoint);
        if (device.mounted && device.size >= 0) {
            const int percent = MountTable::usedPercent(device.size, device.available);
            item->setText(SizeColumn, format.formatByteSize(double(device.size)));
            item->setText(FreeColumn, format.formatByteSize(double(device.available)));
            item->setData(UsageColumn, Qt::UserRole, percent);
            item->setText(UsageColumn, percent >= 0 ? i18nc("@item:intable used space", "%1%", percent) : QString());
        }
        item->setTextAlignment(SizeColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(FreeColumn, Qt::AlignRight | Qt::AlignVCenter);
        QStringList notes;
        if (device.inFstab) {
            notes.append(i18nc("@info:tooltip", "Defined in /etc/fstab"));
        }
        if (device.removable) {
            notes.append(i18nc("@info:tooltip", "Removable device"));
        }
        if (device.system) {
            notes.append(i18nc("@info:tooltip", "System file system, which Plasma Opus does not mount or unmount"));
        }
        if (!notes.isEmpty()) {
            item->setToolTip(NameColumn, notes.join(QLatin1Char('\n')));
        }
        if (!device.mounted) {
            for (int column = 0; column < m_tree->columnCount(); ++column) {
                item->setForeground(column, palette().brush(QPalette::Disabled, QPalette::Text));
            }
        }
    }
    selectDevice(current);
    if (!m_tree->currentItem() && m_tree->topLevelItemCount() > 0) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
    }
    updateButtons();
}

void MountManagerDialog::updateButtons()
{
    const std::optional<MountDevice> device = deviceById(currentId());
    const bool idle = device && !m_backend->isRunning(device->id);
    m_open->setEnabled(idle && (device->mounted || device->canMount));
    m_mount->setEnabled(idle && device->canMount);
    m_unmount->setEnabled(idle && device->canUnmount);
    m_eject->setEnabled(idle && device->canEject);
}

void MountManagerDialog::activateCurrent()
{
    const std::optional<MountDevice> device = deviceById(currentId());
    if (!device || m_backend->isRunning(device->id)) {
        return;
    }
    if (device->mounted) {
        Q_EMIT openRequested(QUrl::fromLocalFile(device->mountPoint));
    } else if (device->canMount) {
        m_openAfterMount = device->id;
        startOperation(MountOperation::Mount);
    }
}

void MountManagerDialog::startOperation(MountOperation operation)
{
    const std::optional<MountDevice> device = deviceById(currentId());
    if (!device || m_backend->isRunning(device->id)) {
        return;
    }
    const bool allowed = (operation == MountOperation::Mount && device->canMount) || (operation == MountOperation::Unmount && device->canUnmount)
        || (operation == MountOperation::Eject && device->canEject);
    if (!allowed) {
        return;
    }
    if (operation != MountOperation::Mount && m_openAfterMount == device->id) {
        m_openAfterMount.clear();
    }
    m_message->setMessageType(KMessageWidget::Information);
    switch (operation) {
    case MountOperation::Mount:
        m_message->setText(i18nc("@info %1 is a file system name", "Mounting %1…", device->name));
        break;
    case MountOperation::Unmount:
        m_message->setText(i18nc("@info %1 is a file system name", "Unmounting %1…", device->name));
        break;
    case MountOperation::Eject:
        m_message->setText(i18nc("@info %1 is a file system name", "Ejecting %1…", device->name));
        break;
    }
    m_message->animatedShow();
    m_backend->start(device->id, operation);
    populate();
}

void MountManagerDialog::onFinished(const QString &id, MountOperation operation, const QString &error, const QString &mountPoint)
{
    const std::optional<MountDevice> device = deviceById(id);
    const QString name = device ? device->name : (mountPoint.isEmpty() ? id : mountPoint);
    const bool openAfter = m_openAfterMount == id;
    if (openAfter) {
        m_openAfterMount.clear();
    }
    if (!error.isEmpty()) {
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(i18nc("@info %1 is a file system name, %2 the reason", "%1: %2", name, error));
    } else {
        m_message->setMessageType(KMessageWidget::Positive);
        switch (operation) {
        case MountOperation::Mount:
            m_message->setText(mountPoint.isEmpty() ? i18nc("@info", "%1 was mounted.", name)
                                                    : i18nc("@info %1 is a file system name", "%1 was mounted at %2.", name, mountPoint));
            break;
        case MountOperation::Unmount:
            m_message->setText(device && device->removable ? i18nc("@info", "%1 was unmounted. It is safe to remove the device.", name)
                                                           : i18nc("@info", "%1 was unmounted.", name));
            break;
        case MountOperation::Eject:
            m_message->setText(i18nc("@info", "%1 was ejected.", name));
            break;
        }
        if (openAfter && operation == MountOperation::Mount && !mountPoint.isEmpty()) {
            Q_EMIT openRequested(QUrl::fromLocalFile(mountPoint));
        }
    }
    m_message->animatedShow();
    populate();
}
