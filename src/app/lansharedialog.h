// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lanshare.h"

#include <QDialog>
#include <QHostAddress>

class KMessageWidget;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;
class QTreeWidget;
class QWidget;

/// Shares local files and folders with browsers and WebDAV clients on the
/// local network until the dialog closes or the time limit is reached.
///
/// Before sharing, the dialog lists the shared items and lets the user choose
/// the addresses, port, clients, HTTPS, an optional upload folder and the time
/// limit, starting from the defaults in Preferences › Remote and Cloud › LAN
/// Sharing. While sharing, it shows the links, a QR code when Prison is
/// available, the certificate fingerprint and the clients that connected.
class LanShareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LanShareDialog(const QStringList &paths, QWidget *parent = nullptr);
    ~LanShareDialog() override;

    /// Replaces the offered network addresses, checking the private ones.
    void setAddresses(const QList<QHostAddress> &addresses);
    /// Replaces the offered network addresses, checking the private ones that
    /// aren't on an interface excluded in the defaults.
    void setAddresses(const QList<LanShare::InterfaceAddress> &addresses);
    QList<QHostAddress> checkedAddresses() const;
    LanShareServer::Options options() const;

    /// Starts sharing, returning an error message or an empty string.
    QString startSharing();
    void stopSharing();

    LanShareServer *server() const;

    /// Stops sharing before closing.
    void done(int result) override;

private:
    void updateState();
    void updateSessions();
    void updateRemaining();
    void updateQrCode();

    QStringList m_paths;
    LanShare::Defaults m_defaults;
    LanShareServer *m_server;
    KMessageWidget *m_message;
    QListWidget *m_items;
    QListWidget *m_addresses;
    QSpinBox *m_port;
    QComboBox *m_protocol;
    QCheckBox *m_tls;
    QCheckBox *m_uploads;
    QLineEdit *m_uploadFolder;
    QPushButton *m_browseUploads;
    QSpinBox *m_minutes;
    QWidget *m_settings;
    QWidget *m_running;
    QListWidget *m_urls;
    QLabel *m_qrCode;
    QLabel *m_fingerprint;
    QLabel *m_remaining;
    QTreeWidget *m_sessions;
    QPushButton *m_start;
    QTimer *m_clock;
    QString m_tlsUnavailable;
};
