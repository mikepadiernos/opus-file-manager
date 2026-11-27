// SPDX-License-Identifier: GPL-3.0-or-later

#include "remoteprofileseditor.h"

#include "settings.h"
#include "symbolicicon.h"

#include <KLocalizedString>

#include <QComboBox>
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

#include <algorithm>

using RemoteProfiles::Authentication;
using RemoteProfiles::Profile;
using RemoteProfiles::Protocol;

RemoteProfilesEditor::RemoteProfilesEditor(const KConfigGroup &group, QWidget *parent, RemoteProfiles::ProtocolCheck check)
    : QWidget(parent)
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

    auto *body = new QHBoxLayout;
    body->addWidget(m_list, 1);
    body->addLayout(form, 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addLayout(body);
    layout->addLayout(listButtons);

    connect(m_list, &QListWidget::currentRowChanged, this, &RemoteProfilesEditor::showProfile);
    connect(add, &QPushButton::clicked, this, &RemoteProfilesEditor::addProfile);
    connect(m_remove, &QPushButton::clicked, this, &RemoteProfilesEditor::removeProfile);
    connect(m_save, &QPushButton::clicked, this, &RemoteProfilesEditor::saveProfile);
    for (QLineEdit *edit : {m_name, m_host, m_user, m_path, m_identityFile}) {
        connect(edit, &QLineEdit::textChanged, this, &RemoteProfilesEditor::updateState);
    }
    connect(m_port, &QSpinBox::valueChanged, this, &RemoteProfilesEditor::updateState);
    connect(m_protocol, &QComboBox::currentIndexChanged, this, &RemoteProfilesEditor::updateState);
    connect(m_authentication, &QComboBox::currentIndexChanged, this, &RemoteProfilesEditor::updateState);

    reload();
    if (m_profiles.isEmpty()) {
        addProfile();
    }
}

Profile RemoteProfilesEditor::defaultsFromSettings()
{
    const QList<Protocol> protocols = RemoteProfiles::protocols();
    // The choices of RemoteDefaultProtocol follow RemoteProfiles::protocols().
    const Protocol protocol = protocols.value(Settings::remoteDefaultProtocol(), Protocol::Sftp);
    const Authentication authentication = Settings::remoteDefaultAuthentication() == Settings::EnumRemoteDefaultAuthentication::KeyOrAgent
        ? Authentication::KeyOrAgent
        : Authentication::Password;
    return RemoteProfiles::newProfile(protocol, Settings::remoteDefaultUser(), authentication);
}

Profile RemoteProfilesEditor::formProfile() const
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

void RemoteProfilesEditor::setNewProfileDefaults(const Profile &defaults)
{
    const Profile previous = m_defaults;
    m_defaults = RemoteProfiles::newProfile(defaults.protocol, defaults.user, defaults.authentication);
    // A new profile the user hasn't changed yet follows the new defaults.
    if (m_list->currentRow() < 0 && formProfile() == previous) {
        showProfile(-1);
    }
}

void RemoteProfilesEditor::refresh()
{
    QString error;
    const QList<Profile> saved = RemoteProfiles::load(m_group, &error);
    if (saved == m_profiles && error == m_loadError) {
        return;
    }
    const int row = m_list->currentRow();
    const QString selectedId = row >= 0 && row < m_profiles.size() ? m_profiles.at(row).id : QString();
    reload(selectedId);
    if (m_profiles.isEmpty()) {
        addProfile();
    }
}

void RemoteProfilesEditor::reload(const QString &selectedId)
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

void RemoteProfilesEditor::showProfile(int row)
{
    const Profile profile = row >= 0 && row < m_profiles.size() ? m_profiles.at(row) : m_defaults;
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

void RemoteProfilesEditor::addProfile()
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentRow(-1);
        m_list->clearSelection();
    }
    showProfile(-1);
    m_name->setFocus();
}

void RemoteProfilesEditor::saveProfile()
{
    const Profile profile = formProfile();
    // Start from the saved list, so connections saved in another window meanwhile are kept.
    QList<Profile> profiles = RemoteProfiles::load(m_group);
    qsizetype index = -1;
    for (qsizetype i = 0; i < profiles.size() && !profile.id.isEmpty(); ++i) {
        if (profiles.at(i).id == profile.id) {
            index = i;
        }
    }
    if (index >= 0) {
        profiles[index] = profile;
    } else {
        profiles.append(profile);
        index = profiles.size() - 1;
    }
    QString error;
    if (!RemoteProfiles::save(m_group, profiles, &error)) {
        m_status->setText(error);
        m_status->setVisible(true);
        return;
    }
    reload(profiles.at(index).id);
    Q_EMIT profilesSaved();
}

void RemoteProfilesEditor::removeProfile()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_profiles.size()) {
        return;
    }
    const QString id = m_profiles.at(row).id;
    QList<Profile> profiles = RemoteProfiles::load(m_group);
    const auto removed = std::find_if(profiles.begin(), profiles.end(), [&id](const Profile &profile) {
        return profile.id == id;
    });
    const qsizetype index = removed - profiles.begin();
    if (removed != profiles.end()) {
        profiles.erase(removed);
        QString error;
        if (!RemoteProfiles::save(m_group, profiles, &error)) {
            m_status->setText(error);
            m_status->setVisible(true);
            return;
        }
    }
    reload(profiles.isEmpty() ? QString() : profiles.at(std::min(index, profiles.size() - 1)).id);
    if (m_profiles.isEmpty()) {
        addProfile();
    }
    Q_EMIT profilesSaved();
}

void RemoteProfilesEditor::updateState()
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
    Q_EMIT formChanged();
}
