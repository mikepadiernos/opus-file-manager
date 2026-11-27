// SPDX-License-Identifier: GPL-3.0-or-later

#include "kdeconnectsharedialog.h"
#include "kdeconnectclient.h"
#include "symbolicicon.h"

#include <KLocalizedString>
#include <KMessageWidget>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
QString displayName(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toDisplayString(QUrl::PreferLocalFile);
}
}

KdeConnectShareDialog::KdeConnectShareDialog(KdeConnectClient *client, const QList<QUrl> &urls, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
    , m_selection(KdeConnectShare::classify(urls))
{
    setObjectName(QStringLiteral("kdeConnectShareDialog"));
    setWindowTitle(i18nc("@title:window", "Send to Device"));
    auto *layout = new QVBoxLayout(this);

    m_message = new KMessageWidget(this);
    m_message->setObjectName(QStringLiteral("kdeConnectMessage"));
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->hide();
    layout->addWidget(m_message);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("kdeConnectSummary"));
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    m_items = new QListWidget(this);
    m_items->setObjectName(QStringLiteral("kdeConnectItems"));
    m_items->setAccessibleName(i18nc("@info:accessibility", "Selected items"));
    m_items->setSelectionMode(QAbstractItemView::NoSelection);
    for (const QUrl &file : std::as_const(m_selection.files)) {
        auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("document-send")), displayName(file), m_items);
        item->setData(Qt::UserRole, true);
    }
    for (const KdeConnectShare::Rejected &rejected : std::as_const(m_selection.rejected)) {
        auto *item = new QListWidgetItem(
            QIcon::fromTheme(QStringLiteral("dialog-cancel")),
            i18nc("@item:inlistbox item, why it cannot be sent", "%1 (%2)", displayName(rejected.url), KdeConnectShare::reasonText(rejected.reason)),
            m_items);
        item->setData(Qt::UserRole, false);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    layout->addWidget(m_items, 1);

    auto *form = new QFormLayout;
    m_device = new QComboBox(this);
    m_device->setObjectName(QStringLiteral("kdeConnectDevice"));
    form->addRow(i18nc("@label:listbox", "Device:"), m_device);
    m_deviceState = new QLabel(this);
    m_deviceState->setObjectName(QStringLiteral("kdeConnectDeviceState"));
    m_deviceState->setWordWrap(true);
    form->addRow(QString(), m_deviceState);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_refresh = buttons->addButton(i18nc("@action:button", "Refresh"), QDialogButtonBox::ActionRole);
    m_refresh->setObjectName(QStringLiteral("kdeConnectRefresh"));
    m_refresh->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-refresh")));
    m_send = buttons->addButton(i18nc("@action:button", "Send"), QDialogButtonBox::ActionRole);
    m_send->setObjectName(QStringLiteral("kdeConnectSend"));
    m_send->setIcon(SymbolicIcon::fromTheme(QStringLiteral("document-send")));
    m_send->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_refresh, &QPushButton::clicked, this, &KdeConnectShareDialog::refresh);
    connect(m_send, &QPushButton::clicked, this, &KdeConnectShareDialog::send);
    connect(m_device, &QComboBox::currentIndexChanged, this, [this] {
        m_message->hide();
        updateState();
    });
    if (m_client) {
        connect(m_client, &KdeConnectClient::changed, this, &KdeConnectShareDialog::refresh);
    }

    resize(560, 420);
    refresh();
}

QString KdeConnectShareDialog::selectedDeviceId() const
{
    return m_device->currentData().toString();
}

void KdeConnectShareDialog::refresh()
{
    const QString previous = m_device->count() ? selectedDeviceId() : KdeConnectShare::rememberedDevice();
    m_devices = m_client && m_client->isAvailable() ? m_client->devices() : QList<KdeConnectShare::Device>();
    std::stable_sort(m_devices.begin(), m_devices.end(), [](const KdeConnectShare::Device &a, const KdeConnectShare::Device &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    const QSignalBlocker blocker(m_device);
    m_device->clear();
    auto *model = qobject_cast<QStandardItemModel *>(m_device->model());
    for (const KdeConnectShare::Device &device : std::as_const(m_devices)) {
        const QString reason = KdeConnectShare::unavailableReason(device);
        m_device->addItem(reason.isEmpty() ? device.name : i18nc("@item:inlistbox device name, why it cannot receive files", "%1 (%2)", device.name, reason),
                          device.id);
        if (!reason.isEmpty() && model) {
            model->item(m_device->count() - 1)->setEnabled(false);
        }
    }
    const QList<KdeConnectShare::Device> eligible = KdeConnectShare::eligible(m_devices);
    m_device->setCurrentIndex(m_device->findData(KdeConnectShare::preferredDevice(m_devices, previous)));

    if (!m_client || !m_client->isAvailable()) {
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(i18nc("@info", "KDE Connect is not running. Install or start KDE Connect, then press Refresh."));
        m_message->show();
    } else if (m_devices.isEmpty()) {
        m_message->setMessageType(KMessageWidget::Information);
        m_message->setText(i18nc("@info", "No device is paired with KDE Connect. Pair one in KDE Connect, then press Refresh."));
        m_message->show();
    } else if (eligible.isEmpty()) {
        m_message->setMessageType(KMessageWidget::Warning);
        m_message->setText(i18nc("@info", "No paired device can receive files now. Make sure it is online and file sharing is turned on in KDE Connect."));
        m_message->show();
    } else if (m_message->messageType() != KMessageWidget::Positive || m_sentTo.isEmpty()) {
        m_message->hide();
    }
    updateState();
}

void KdeConnectShareDialog::updateState()
{
    const int sendable = m_selection.files.size();
    const int rejected = m_selection.rejected.size();
    if (sendable == 0) {
        m_summary->setText(i18nc("@info", "None of the selected items can be sent. KDE Connect sends local files only."));
    } else if (rejected == 0) {
        m_summary->setText(i18ncp("@info", "One file will be sent.", "%1 files will be sent.", sendable));
    } else {
        m_summary->setText(i18ncp("@info %2 is a count of files",
                                  "%2 will be sent; one item cannot be sent.",
                                  "%2 will be sent; %1 items cannot be sent.",
                                  rejected,
                                  i18ncp("@info", "One file", "%1 files", sendable)));
    }

    const QString id = selectedDeviceId();
    const auto device = std::find_if(m_devices.cbegin(), m_devices.cend(), [&id](const KdeConnectShare::Device &candidate) {
        return candidate.id == id;
    });
    const QString reason = device == m_devices.cend() ? QString() : KdeConnectShare::unavailableReason(*device);
    m_deviceState->setText(reason);
    m_deviceState->setVisible(!reason.isEmpty());
    m_send->setEnabled(m_client && sendable > 0 && device != m_devices.cend() && reason.isEmpty() && id != m_sentTo);
}

void KdeConnectShareDialog::send()
{
    if (!m_send->isEnabled() || !m_client) {
        return;
    }
    const QString id = selectedDeviceId();
    const QString name = m_device->currentText();
    const QString error = m_client->share(id, m_selection.files);
    if (!error.isEmpty()) {
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(i18nc("@info %1 is a device name", "Could not send the files to %1: %2", name, error));
        m_message->show();
        return;
    }
    m_sentTo = id;
    KdeConnectShare::rememberDevice(id);
    m_message->setMessageType(KMessageWidget::Positive);
    m_message->setText(i18ncp("@info %2 is a device name",
                              "KDE Connect is sending one file to %2. Its notifications show the transfer's progress and result.",
                              "KDE Connect is sending %1 files to %2. Its notifications show the transfer's progress and result.",
                              m_selection.files.size(),
                              name));
    m_message->show();
    updateState();
}
