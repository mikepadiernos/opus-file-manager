// SPDX-License-Identifier: GPL-3.0-or-later

#include "lansharedialog.h"

#include "symbolicicon.h"

#include <KFormat>
#include <KLocalizedString>
#include <KMessageWidget>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>

#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMimeDatabase>
#include <QPushButton>
#include <QSpinBox>

#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#ifdef OPUSFM_HAVE_PRISON
#include <Prison/Barcode>
#endif

namespace
{
constexpr int qrCodeSize = 160;
}

LanShareDialog::LanShareDialog(const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_paths(paths)
    , m_defaults(LanShare::defaultsFromSettings())
    , m_server(new LanShareServer(this))
    , m_message(new KMessageWidget(this))
    , m_items(new QListWidget(this))
    , m_addresses(new QListWidget(this))
    , m_port(new QSpinBox(this))
    , m_protocol(new QComboBox(this))
    , m_tls(new QCheckBox(i18nc("@option:check", "Encrypt with HTTPS"), this))
    , m_uploads(new QCheckBox(i18nc("@option:check", "Allow uploads to:"), this))
    , m_uploadFolder(new QLineEdit(this))
    , m_browseUploads(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), QString(), this))
    , m_minutes(new QSpinBox(this))
    , m_settings(new QWidget(this))
    , m_running(new QWidget(this))
    , m_urls(new QListWidget(this))
    , m_qrCode(new QLabel(this))
    , m_fingerprint(new QLabel(this))
    , m_remaining(new QLabel(this))
    , m_sessions(new QTreeWidget(this))
    , m_start(new QPushButton(this))
    , m_clock(new QTimer(this))
    , m_tlsUnavailable(LanShare::tlsUnavailableReason())
{
    setWindowTitle(i18nc("@title:window", "Share on Local Network"));
    setMinimumWidth(520);
    m_addresses->setObjectName(QStringLiteral("addresses"));
    m_port->setObjectName(QStringLiteral("port"));
    m_protocol->setObjectName(QStringLiteral("protocol"));
    m_tls->setObjectName(QStringLiteral("tls"));
    m_uploads->setObjectName(QStringLiteral("uploads"));
    m_uploadFolder->setObjectName(QStringLiteral("uploadFolder"));
    m_minutes->setObjectName(QStringLiteral("minutes"));
    m_urls->setObjectName(QStringLiteral("urls"));
    m_fingerprint->setObjectName(QStringLiteral("fingerprint"));
    m_sessions->setObjectName(QStringLiteral("sessions"));

    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->hide();

    const QMimeDatabase mimeDatabase;
    for (const QString &path : std::as_const(m_paths)) {
        auto *item = new QListWidgetItem(QIcon::fromTheme(mimeDatabase.mimeTypeForFile(path).iconName()), QFileInfo(path).fileName(), m_items);
        item->setToolTip(path);
    }
    m_items->setMaximumHeight(m_items->sizeHintForRow(0) * 4 + 2 * m_items->frameWidth() + 4);

    m_addresses->setMaximumHeight(m_items->maximumHeight());
    m_addresses->setToolTip(i18nc("@info:tooltip", "The network addresses that serve the shared items. Public addresses can be reached from the internet."));
    connect(m_addresses, &QListWidget::itemChanged, this, &LanShareDialog::updateState);

    m_port->setRange(0, 65535);
    m_port->setSpecialValueText(i18nc("@item:inrange port chosen automatically", "Automatic"));
    m_port->setValue(m_defaults.port);
    m_port->setToolTip(i18nc("@info:tooltip", "The port that clients connect to. Choose a fixed port to open it in a firewall."));

    m_protocol->addItem(i18nc("@item:inlistbox", "Browsers and file managers (WebDAV)"), int(LanShare::Protocol::BrowsersAndWebDav));
    m_protocol->addItem(i18nc("@item:inlistbox", "Browsers only"), int(LanShare::Protocol::Browsers));
    m_protocol->addItem(i18nc("@item:inlistbox", "File managers only (WebDAV)"), int(LanShare::Protocol::WebDav));
    m_protocol->setCurrentIndex(m_protocol->findData(int(m_defaults.protocol)));
    m_protocol->setToolTip(i18nc("@info:tooltip",
                                 "Browsers get a page listing each folder; file managers and other WebDAV clients can browse, download and "
                                 "upload like in a network folder"));

    m_tls->setChecked(m_tlsUnavailable.isEmpty() && m_defaults.tls);
    m_tls->setEnabled(m_tlsUnavailable.isEmpty());
    m_tls->setToolTip(m_tlsUnavailable.isEmpty() ? i18nc("@info:tooltip",
                                                         "Encrypt transfers with a certificate made for this share. Browsers warn that it isn't trusted; "
                                                         "compare its fingerprint to continue.")
                                                 : m_tlsUnavailable);

    m_uploads->setToolTip(i18nc("@info:tooltip", "Let clients upload files to this folder. Uploads never replace existing files."));
    m_uploads->setChecked(m_defaults.allowUploads);
    m_uploadFolder->setText(m_defaults.uploadFolder);
    m_browseUploads->setToolTip(i18nc("@info:tooltip", "Choose the folder that receives uploads"));
    connect(m_uploads, &QCheckBox::toggled, this, &LanShareDialog::updateState);
    connect(m_browseUploads, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose the Upload Folder"), m_uploadFolder->text());
        if (!folder.isEmpty()) {
            m_uploadFolder->setText(folder);
        }
    });
    auto *uploadRow = new QHBoxLayout;
    uploadRow->addWidget(m_uploads);
    uploadRow->addWidget(m_uploadFolder, 1);
    uploadRow->addWidget(m_browseUploads);

    m_minutes->setRange(1, 24 * 60);
    m_minutes->setValue(m_defaults.timeoutMinutes);
    const auto updateSuffix = [this](int minutes) {
        m_minutes->setSuffix(i18ncp("@item:valuesuffix", " minute", " minutes", minutes));
    };
    updateSuffix(m_minutes->value());
    connect(m_minutes, &QSpinBox::valueChanged, this, updateSuffix);

    auto *note = new QLabel(i18nc("@info",
                                  "Plasma Opus doesn't open firewall ports. To send files to a phone, KDE Connect may be simpler; "
                                  "to share folders permanently, use Samba sharing in the folder's properties. Preferences › Remote and "
                                  "Cloud › LAN Sharing sets the choices this window starts with."),
                            this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);

    auto *settingsForm = new QFormLayout(m_settings);
    settingsForm->setContentsMargins({});
    settingsForm->addRow(i18nc("@label:listbox", "Addresses:"), m_addresses);
    settingsForm->addRow(i18nc("@label:spinbox", "Port:"), m_port);
    settingsForm->addRow(i18nc("@label:listbox", "Serve to:"), m_protocol);
    settingsForm->addRow(QString(), m_tls);
    settingsForm->addRow(QString(), uploadRow);
    settingsForm->addRow(i18nc("@label:spinbox", "Stop after:"), m_minutes);
    settingsForm->addRow(note);

    auto *copy = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action:button", "Copy Link"), this);
    connect(copy, &QPushButton::clicked, this, [this] {
        if (const QListWidgetItem *item = m_urls->currentItem()) {
            QApplication::clipboard()->setText(item->text());
        }
    });
    connect(m_urls, &QListWidget::currentRowChanged, this, &LanShareDialog::updateQrCode);
    m_urls->setMaximumHeight(m_items->maximumHeight());
    m_qrCode->setFixedSize(qrCodeSize, qrCodeSize);
    m_qrCode->setAlignment(Qt::AlignCenter);
#ifndef OPUSFM_HAVE_PRISON
    m_qrCode->hide();
#endif
    m_fingerprint->setWordWrap(true);
    m_fingerprint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_fingerprint->setToolTip(i18nc("@info:tooltip", "Compare this with the fingerprint the browser shows before accepting the certificate"));
    m_remaining->setWordWrap(true);

    m_sessions->setRootIsDecorated(false);
    m_sessions->setHeaderLabels({i18nc("@title:column", "Client"),
                                 i18nc("@title:column", "Requests"),
                                 i18nc("@title:column", "Sent"),
                                 i18nc("@title:column", "Received"),
                                 i18nc("@title:column", "Last Seen")});
    m_sessions->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_sessions->setToolTip(i18nc("@info:tooltip", "The devices that opened a link, by network address"));

    auto *linkColumn = new QVBoxLayout;
    linkColumn->addWidget(m_urls);
    linkColumn->addWidget(copy, 0, Qt::AlignLeft);
    linkColumn->addStretch();
    auto *linkRow = new QHBoxLayout;
    linkRow->addLayout(linkColumn, 1);
    linkRow->addWidget(m_qrCode);
    auto *runningForm = new QFormLayout(m_running);
    runningForm->setContentsMargins({});
    runningForm->addRow(i18nc("@label:listbox", "Links:"), linkRow);
    runningForm->addRow(i18nc("@label", "Certificate:"), m_fingerprint);
    runningForm->addRow(m_remaining);
    runningForm->addRow(i18nc("@label:listbox", "Clients:"), m_sessions);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_start, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_start, &QPushButton::clicked, this, [this] {
        if (m_server->isRunning()) {
            stopSharing();
        } else {
            startSharing();
        }
    });
    auto *closing = new QLabel(i18nc("@info", "Closing this window stops sharing."), this);
    closing->setForegroundRole(QPalette::PlaceholderText);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_message);
    layout->addWidget(new QLabel(i18nc("@label:listbox", "Shared items:"), this));
    layout->addWidget(m_items);
    layout->addWidget(m_settings);
    layout->addWidget(m_running, 1);
    layout->addWidget(closing);
    layout->addWidget(buttons);

    m_clock->setInterval(1000);
    connect(m_clock, &QTimer::timeout, this, &LanShareDialog::updateRemaining);
    connect(m_server, &LanShareServer::sessionsChanged, this, &LanShareDialog::updateSessions);
    connect(m_server, &LanShareServer::uploaded, this, [this](const QString &path) {
        m_message->setMessageType(KMessageWidget::Positive);
        m_message->setText(i18nc("@info %1 is a file name", "Received “%1”.", QFileInfo(path).fileName()));
        m_message->animatedShow();
    });
    connect(m_server, &LanShareServer::stopped, this, [this](const QString &reason) {
        if (!reason.isEmpty()) {
            m_message->setMessageType(KMessageWidget::Information);
            m_message->setText(reason);
            m_message->animatedShow();
        } else {
            m_message->hide();
        }
        updateState();
    });

    setAddresses(LanShare::interfaceAddresses());
}

LanShareDialog::~LanShareDialog()
{
    disconnect(m_server, nullptr, this, nullptr);
    m_server->stop();
}

void LanShareDialog::setAddresses(const QList<QHostAddress> &addresses)
{
    QList<LanShare::InterfaceAddress> withInterfaces;
    for (const QHostAddress &address : addresses) {
        withInterfaces.append({address, QString()});
    }
    setAddresses(withInterfaces);
}

void LanShareDialog::setAddresses(const QList<LanShare::InterfaceAddress> &addresses)
{
    const QSignalBlocker blocker(m_addresses);
    m_addresses->clear();
    bool excludedPrivate = false;
    bool chosen = false;
    for (const LanShare::InterfaceAddress &entry : addresses) {
        const QHostAddress &address = entry.address;
        const bool local = LanShare::isPrivateAddress(address) || address.isLoopback();
        const bool check = LanShare::chosenByDefault(entry, m_defaults.excludedInterfaces);
        excludedPrivate = excludedPrivate || (local && !check);
        chosen = chosen || check;
        QString text = local ? address.toString() : i18nc("@item %1 is a network address", "%1 (public)", address.toString());
        if (!entry.interface.isEmpty()) {
            text = i18nc("@item %1 is a network address, %2 a network interface", "%1 on %2", text, entry.interface);
        }
        auto *item = new QListWidgetItem(text, m_addresses);
        item->setData(Qt::UserRole, address.toString());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(check ? Qt::Checked : Qt::Unchecked);
        if (!local) {
            item->setToolTip(i18nc("@info:tooltip", "This address may be reachable from the internet"));
        } else if (!check) {
            item->setToolTip(i18nc("@info:tooltip", "Preferences › Remote and Cloud › LAN Sharing leaves out this network interface"));
        }
    }
    if (addresses.isEmpty()) {
        auto *item = new QListWidgetItem(i18nc("@item", "No network connection"), m_addresses);
        item->setFlags(Qt::NoItemFlags);
    }
    if (excludedPrivate && !chosen && !m_server->isRunning()) {
        m_message->setMessageType(KMessageWidget::Information);
        m_message->setText(i18nc("@info",
                                 "Preferences › Remote and Cloud › LAN Sharing leaves out the network interfaces that are connected. "
                                 "Check an address to share on it anyway."));
        m_message->animatedShow();
    }
    updateState();
}

QList<QHostAddress> LanShareDialog::checkedAddresses() const
{
    QList<QHostAddress> result;
    for (int i = 0; i < m_addresses->count(); ++i) {
        const QListWidgetItem *item = m_addresses->item(i);
        if (item->checkState() == Qt::Checked && !item->data(Qt::UserRole).toString().isEmpty()) {
            result.append(QHostAddress(item->data(Qt::UserRole).toString()));
        }
    }
    return result;
}

LanShareServer::Options LanShareDialog::options() const
{
    LanShareServer::Options options;
    options.paths = m_paths;
    options.addresses = checkedAddresses();
    options.port = quint16(m_port->value());
    options.tls = m_tls->isEnabled() && m_tls->isChecked();
    options.protocol = LanShare::Protocol(m_protocol->currentData().toInt());
    if (m_uploads->isChecked()) {
        options.uploadFolder = m_uploadFolder->text().trimmed();
    }
    options.timeoutSeconds = m_minutes->value() * 60;
    return options;
}

QString LanShareDialog::startSharing()
{
    LanShareServer::Options shareOptions = options();
    if (m_uploads->isChecked() && shareOptions.uploadFolder.isEmpty()) {
        const QString error = i18nc("@info", "Choose a folder for uploads.");
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(error);
        m_message->animatedShow();
        return error;
    }
    if (shareOptions.tls) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        shareOptions.certificate = LanShare::createCertificate(shareOptions.addresses);
        QApplication::restoreOverrideCursor();
    }
    const QString error = m_server->start(shareOptions);
    if (!error.isEmpty()) {
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(error);
        m_message->animatedShow();
        updateState();
        return error;
    }
    if (shareOptions.tls) {
        m_message->setMessageType(KMessageWidget::Information);
        m_message->setText(i18nc("@info",
                                 "Anyone with a link can open the shared items. Browsers warn that the certificate isn't trusted; "
                                 "continue only if its fingerprint matches the one below."));
    } else {
        m_message->setMessageType(KMessageWidget::Warning);
        m_message->setText(i18nc("@info",
                                 "Anyone with a link can open the shared items. The connection isn't encrypted, so others on the network "
                                 "can see the link and what is transferred."));
    }
    m_message->animatedShow();
    updateState();
    return {};
}

void LanShareDialog::stopSharing()
{
    m_server->stop();
}

void LanShareDialog::done(int result)
{
    stopSharing();
    QDialog::done(result);
}

LanShareServer *LanShareDialog::server() const
{
    return m_server;
}

void LanShareDialog::updateState()
{
    const bool running = m_server->isRunning();
    m_settings->setEnabled(!running);
    m_running->setVisible(running);
    m_uploadFolder->setEnabled(m_uploads->isChecked());
    m_browseUploads->setEnabled(m_uploads->isChecked());
    m_start->setText(running ? i18nc("@action:button", "Stop Sharing") : i18nc("@action:button", "Start Sharing"));
    m_start->setIcon(SymbolicIcon::fromTheme(running ? QStringLiteral("process-stop") : QStringLiteral("network-server")));
    m_start->setEnabled(running || (!m_paths.isEmpty() && !checkedAddresses().isEmpty()));

    const QString current = m_urls->currentItem() ? m_urls->currentItem()->text() : QString();
    {
        const QSignalBlocker blocker(m_urls);
        m_urls->clear();
        m_urls->addItems(m_server->urls());
    }
    const auto matches = m_urls->findItems(current, Qt::MatchExactly);
    m_urls->setCurrentRow(matches.isEmpty() ? 0 : m_urls->row(matches.first()));
    updateQrCode();

    const QString fingerprint = running && m_server->tls() ? LanShare::certificateFingerprint(m_server->certificate()) : QString();
    m_fingerprint->setText(fingerprint);
    static_cast<QFormLayout *>(m_running->layout())->setRowVisible(m_fingerprint, !fingerprint.isEmpty());
    updateSessions();
    updateRemaining();
    if (running) {
        m_clock->start();
    } else {
        m_clock->stop();
    }
}

void LanShareDialog::updateSessions()
{
    m_sessions->clear();
    const QLocale locale;
    const auto sessions = m_server->sessions();
    for (const LanShareSession &session : sessions) {
        auto *item = new QTreeWidgetItem(m_sessions,
                                         {session.peer,
                                          locale.toString(session.requests),
                                          locale.formattedDataSize(session.bytesSent),
                                          locale.formattedDataSize(session.bytesReceived),
                                          locale.toString(session.lastSeen.time(), QLocale::ShortFormat)});
        if (session.active > 0) {
            item->setIcon(0, SymbolicIcon::fromTheme(QStringLiteral("network-transmit-receive")));
            item->setToolTip(0, i18ncp("@info:tooltip", "%1 transfer in progress", "%1 transfers in progress", session.active));
        }
    }
}

void LanShareDialog::updateRemaining()
{
    const QDateTime stopsAt = m_server->stopsAt();
    if (!m_server->isRunning() || !stopsAt.isValid()) {
        m_remaining->clear();
        return;
    }
    const qint64 seconds = std::max<qint64>(0, QDateTime::currentDateTime().secsTo(stopsAt));
    m_remaining->setText(i18nc("@info %1 is a duration", "Sharing stops in %1.", KFormat().formatDuration(seconds * 1000)));
}

void LanShareDialog::updateQrCode()
{
#ifdef OPUSFM_HAVE_PRISON
    m_qrCode->clear();
    const QListWidgetItem *item = m_urls->currentItem();
    if (!item) {
        return;
    }
    auto barcode = Prison::Barcode::create(Prison::QRCode);
    if (!barcode) {
        return;
    }
    barcode->setData(item->text());
    const QImage image = barcode->toImage(barcode->preferredSize(devicePixelRatioF()));
    if (image.isNull()) {
        return;
    }
    const int size = qrCodeSize * devicePixelRatioF();
    QPixmap pixmap = QPixmap::fromImage(image.scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_qrCode->setPixmap(pixmap);
    m_qrCode->setToolTip(i18nc("@info:tooltip", "Scan this code to open the link on a phone"));
#endif
}
