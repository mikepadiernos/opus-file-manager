// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "kdeconnectshare.h"

#include <QDialog>
#include <QPointer>

class KdeConnectClient;
class KMessageWidget;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

/// Sends selected local files to a paired KDE Connect device the user chooses.
///
/// The dialog lists the files it will send and the items it cannot, and the
/// paired devices with why any of them cannot receive files. It sends only
/// when the user presses Send, and it never pairs devices.
class KdeConnectShareDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param client stays owned by the caller
    KdeConnectShareDialog(KdeConnectClient *client, const QList<QUrl> &urls, QWidget *parent = nullptr);

    /// Reads the devices again, keeping the chosen one when it is still listed.
    void refresh();
    /// Sends the files to the chosen device.
    void send();

    QString selectedDeviceId() const;

private:
    void updateState();

    QPointer<KdeConnectClient> m_client;
    KdeConnectShare::Selection m_selection;
    QList<KdeConnectShare::Device> m_devices;
    KMessageWidget *m_message;
    QListWidget *m_items;
    QLabel *m_summary;
    QComboBox *m_device;
    QLabel *m_deviceState;
    QPushButton *m_refresh;
    QPushButton *m_send;
    /// The device the files were last sent to, so they are not sent twice by accident.
    QString m_sentTo;
};
