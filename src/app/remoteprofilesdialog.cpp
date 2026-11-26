// SPDX-License-Identifier: GPL-3.0-or-later

#include "remoteprofilesdialog.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

using RemoteProfiles::Authentication;
using RemoteProfiles::Profile;
using RemoteProfiles::Protocol;

RemoteProfilesDialog::RemoteProfilesDialog(const KConfigGroup &group, QWidget *parent, RemoteProfiles::ProtocolCheck check)
    : QDialog(parent)
    , m_group(group)
    , m_check(std::move(check))
    , m_list(new QListWidget(this))
    , m_name(new QLineEdit(this))
    , m_protocol(new QComboBox(this))
    , m_host(new QLineEdit(this))
    , m_port(new QSpinBox(this))
    , m_user(new QLineEdit(this))
    , m_path(new QLineEdit(this))
    , m_authentication(new QComboBox(this))
    , m_identityFile(new QLineEdit(this))
    , m_status(new QLabel(this))
{
    setWindowTitle(i18nc("@title:window", "Remote Connections"));
    m_list->setObjectName(QStringLiteral("remoteProfileList"));
    m_name->setObjectName(QStringLiteral("remoteProfileName"));
    m_protocol->setObjectName(QStringLiteral("remoteProfileProtocol"));
    m_host->setObjectName(QStringLiteral("remoteProfileHost"));
    m_port->setObjectName(QStringLiteral("remoteProfilePort"));
    m_user->setObjectName(QStringLiteral("remoteProfileUser"));
    m_path->setObjectName(QStringLiteral("remoteProfilePath"));
    m_authentication->setObjectName(QStringLiteral("remoteProfileAuthentication"));
    m_identityFile->setObjectName(QStringLiteral("remoteProfileIdentityFile"));
    m_status->setObjectName(QStringLiteral("remoteProfileStatus"));

    for (Protocol protocol : RemoteProfiles::protocols()) {
        m_protocol->addItem(RemoteProfiles::name(protocol), static_cast<int>(protocol));
    }
    m_port->setRange(0, 65535);
    m_port->setSpecialValueText(i18nc("@item:inrange port", "Default"));
    m_host->setPlaceholderText(i18nc("@info:placeholder", "server.example.org"));
    m_user->setPlaceholderText(i18nc("@info:placeholder", "Optional"));
    m_path->setPlaceholderText(i18nc("@info:placeholder", "/ or the home folder"));
    m_authentication->addItem(i18nc("@item:inlistbox sign-in method", "Password, asked when needed"), static_cast<int>(Authentication::Password));
    m_authentication->addItem(i18nc("@item:inlistbox sign-in method", "SSH key or agent"), static_cast<int>(Authentication::KeyOrAgent));
    m_identityFile->setPlaceholderText(i18nc("@info:placeholder", "SSH configuration and agent"));
    QAction *browse = m_identityFile->addAction(SymbolicIcon::fromTheme(QStringLiteral("document-open")), QLineEdit::TrailingPosition);
    browse->setToolTip(i18nc("@info:tooltip", "Choose a private key"));
    connect(browse, &QAction::triggered, this, [this] {
        const QString file =
            QFileDialog::getOpenFileName(this, i18nc("@title:window", "Choose Private Key"), QDir::home().filePath(QStringLiteral(".ssh")), {}, nullptr);
        if (!file.isEmpty()) {
            m_identityFile->setText(file);
        }
    });
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "Name:"), m_name);
    form->addRow(i18nc("@label:listbox", "Protocol:"), m_protocol);
    form->addRow(i18nc("@label:textbox", "Host:"), m_host);
    form->addRow(i18nc("@label:spinbox", "Port:"), m_port);
    form->addRow(i18nc("@label:textbox", "User:"), m_user);
    form->addRow(i18nc("@label:textbox", "Folder:"), m_path);
    form->addRow(i18nc("@label:listbox", "Sign in with:"), m_authentication);
    form->addRow(i18nc("@label:textbox", "Private key:"), m_identityFile);
    form->addRow(m_status);

    auto *add = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add"), this);
    m_remove = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove"), this);
    m_save = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-save")), i18nc("@action:button", "Save"), this);
    add->setObjectName(QStringLiteral("remoteProfileAdd"));
    m_remove->setObjectName(QStringLiteral("remoteProfileRemove"));
    m_save->setObjectName(QStringLiteral("remoteProfileSave"));
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(add);
    listButtons->addWidget(m_remove);
    listButtons->addStretch();
    listButtons->addWidget(m_save);

    auto *listColumn = new QVBoxLayout;
    listColumn->addWidget(m_list);
    auto *body = new QHBoxLayout;
    body->addLayout(listColumn, 1);
    body->addLayout(form, 2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_open = buttons->addButton(i18nc("@action:button", "Open"), QDialogButtonBox::ActionRole);
    m_openOther = buttons->addButton(i18nc("@action:button", "Open in Other Pane"), QDialogButtonBox::ActionRole);
    m_ssh = buttons->addButton(i18nc("@action:button", "Open SSH Terminal"), QDialogButtonBox::ActionRole);
    m_open->setIcon(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")));
    m_openOther->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-split-left-right")));
    m_ssh->setIcon(SymbolicIcon::fromTheme(QStringLiteral("utilities-terminal")));
    m_open->setObjectName(QStringLiteral("remoteProfileOpen"));
    m_openOther->setObjectName(QStringLiteral("remoteProfileOpenOther"));
    m_ssh->setObjectName(QStringLiteral("remoteProfileSsh"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(body);
    layout->addLayout(listButtons);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, &RemoteProfilesDialog::showProfile);
    connect(add, &QPushButton::clicked, this, &RemoteProfilesDialog::addProfile);
    connect(m_remove, &QPushButton::clicked, this, &RemoteProfilesDialog::removeProfile);
    connect(m_save, &QPushButton::clicked, this, &RemoteProfilesDialog::saveProfile);
    connect(m_open, &QPushButton::clicked, this, [this] {
        open(false);
    });
    connect(m_openOther, &QPushButton::clicked, this, [this] {
        open(true);
    });
    connect(m_ssh, &QPushButton::clicked, this, &RemoteProfilesDialog::openSsh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (QLineEdit *edit : {m_name, m_host, m_user, m_path, m_identityFile}) {
        connect(edit, &QLineEdit::textChanged, this, &RemoteProfilesDialog::updateState);
    }
    connect(m_port, &QSpinBox::valueChanged, this, &RemoteProfilesDialog::updateState);
    connect(m_protocol, &QComboBox::currentIndexChanged, this, &RemoteProfilesDialog::updateState);
    connect(m_authentication, &QComboBox::currentIndexChanged, this, &RemoteProfilesDialog::updateState);

    reload();
    if (m_profiles.isEmpty()) {
        addProfile();
    }
}

void RemoteProfilesDialog::setOtherPaneAvailable(bool available)
{
    m_otherPaneAvailable = available;
    updateState();
}

Profile RemoteProfilesDialog::formProfile() const
{
    Profile profile;
    const int row = m_list->currentRow();
    if (row >= 0 && row < m_profiles.size()) {
        profile.id = m_profiles.at(row).id;
    }
    profile.name = m_name->text();
    profile.protocol = static_cast<Protocol>(m_protocol->currentData().toInt());
    profile.host = m_host->text().trimmed();
    profile.port = m_port->value();
    profile.user = m_user->text().trimmed();
    profile.path = m_path->text().trimmed();
    profile.authentication = static_cast<Authentication>(m_authentication->currentData().toInt());
    if (profile.authentication == Authentication::KeyOrAgent) {
        profile.identityFile = m_identityFile->text().trimmed();
    }
    return profile;
}

void RemoteProfilesDialog::reload(const QString &selectedId)
{
    m_profiles = RemoteProfiles::load(m_group, &m_loadError);
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    int selected = m_profiles.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_profiles.size(); ++i) {
        auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("folder-network")), m_profiles.at(i).name, m_list);
        item->setToolTip(RemoteProfiles::url(m_profiles.at(i)).toDisplayString());
        if (m_profiles.at(i).id == selectedId) {
            selected = i;
        }
    }
    m_list->setCurrentRow(selected);
    showProfile(selected);
}

void RemoteProfilesDialog::showProfile(int row)
{
    const Profile profile = row >= 0 && row < m_profiles.size() ? m_profiles.at(row) : Profile();
    m_name->setText(profile.name);
    m_protocol->setCurrentIndex(m_protocol->findData(static_cast<int>(profile.protocol)));
    m_host->setText(profile.host);
    m_port->setValue(profile.port);
    m_user->setText(profile.user);
    m_path->setText(profile.path);
    m_authentication->setCurrentIndex(m_authentication->findData(static_cast<int>(profile.authentication)));
    m_identityFile->setText(profile.identityFile);
    updateState();
}

void RemoteProfilesDialog::addProfile()
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentRow(-1);
        m_list->clearSelection();
    }
    showProfile(-1);
    m_name->setFocus();
}

void RemoteProfilesDialog::saveProfile()
{
    const Profile profile = formProfile();
    QList<Profile> profiles = m_profiles;
    const int row = m_list->currentRow();
    if (row >= 0 && row < profiles.size()) {
        profiles[row] = profile;
    } else {
        profiles.append(profile);
    }
    QString error;
    if (!RemoteProfiles::save(m_group, profiles, &error)) {
        m_status->setText(error);
        m_status->setVisible(true);
        return;
    }
    const QString id = row >= 0 && row < profiles.size() ? profiles.at(row).id : profiles.constLast().id;
    reload(id);
}

void RemoteProfilesDialog::removeProfile()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_profiles.size()) {
        return;
    }
    QList<Profile> profiles = m_profiles;
    profiles.removeAt(row);
    QString error;
    if (!RemoteProfiles::save(m_group, profiles, &error)) {
        m_status->setText(error);
        m_status->setVisible(true);
        return;
    }
    reload(profiles.isEmpty() ? QString() : profiles.at(std::min(row, int(profiles.size()) - 1)).id);
    if (m_profiles.isEmpty()) {
        addProfile();
    }
}

void RemoteProfilesDialog::open(bool otherPane)
{
    const Profile profile = formProfile();
    if (!RemoteProfiles::validationError(profile).isEmpty()) {
        updateState();
        return;
    }
    Q_EMIT openRequested(RemoteProfiles::url(profile), otherPane);
    accept();
}

void RemoteProfilesDialog::openSsh()
{
    const Profile profile = formProfile();
    if (!RemoteProfiles::validationError(profile).isEmpty() || !RemoteProfiles::supportsSsh(profile.protocol)) {
        updateState();
        return;
    }
    Q_EMIT sshRequested(profile);
    accept();
}

void RemoteProfilesDialog::updateState()
{
    const auto protocol = static_cast<Protocol>(m_protocol->currentData().toInt());
    const bool ssh = RemoteProfiles::supportsSsh(protocol);
    if (!ssh && m_authentication->currentData().toInt() != static_cast<int>(Authentication::Password)) {
        const QSignalBlocker blocker(m_authentication);
        m_authentication->setCurrentIndex(m_authentication->findData(static_cast<int>(Authentication::Password)));
    }
    m_authentication->setEnabled(ssh);
    m_identityFile->setEnabled(ssh && m_authentication->currentData().toInt() == static_cast<int>(Authentication::KeyOrAgent));
    m_port->setSpecialValueText(i18nc("@item:inrange %1 is a port number", "Default (%1)", RemoteProfiles::defaultPort(protocol)));

    const Profile profile = formProfile();
    const QString error = RemoteProfiles::validationError(profile);
    const bool editing = m_list->currentRow() >= 0;
    const bool emptyNew = !editing && profile.name.isEmpty() && profile.host.isEmpty();
    QStringList messages;
    if (!m_loadError.isEmpty()) {
        messages.append(m_loadError);
    }
    if (!error.isEmpty() && !emptyNew) {
        messages.append(error);
    }
    const QString unavailable = RemoteProfiles::unavailableReason(protocol, m_check);
    if (!unavailable.isEmpty()) {
        messages.append(unavailable);
    }
    const QString warning = RemoteProfiles::securityWarning(profile);
    if (!warning.isEmpty()) {
        messages.append(warning);
    }
    m_status->setText(messages.join(QLatin1Char('\n')));
    m_status->setVisible(!messages.isEmpty());

    m_remove->setEnabled(editing);
    m_save->setEnabled(error.isEmpty());
    m_open->setEnabled(error.isEmpty() && unavailable.isEmpty());
    m_openOther->setEnabled(error.isEmpty() && unavailable.isEmpty() && m_otherPaneAvailable);
    m_ssh->setEnabled(error.isEmpty() && ssh);
}
