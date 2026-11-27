// SPDX-License-Identifier: GPL-3.0-or-later

#include "remoteprofilesdialog.h"

#include "remoteprofileseditor.h"
#include "symbolicicon.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

using RemoteProfiles::Profile;

RemoteProfilesDialog::RemoteProfilesDialog(const KConfigGroup &group, QWidget *parent, RemoteProfiles::ProtocolCheck check)
    : QDialog(parent)
    , m_check(check)
    , m_editor(new RemoteProfilesEditor(group, this, std::move(check)))
{
    setWindowTitle(i18nc("@title:window", "Remote Connections"));

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
    layout->addWidget(m_editor);
    layout->addWidget(buttons);

    connect(m_editor, &RemoteProfilesEditor::formChanged, this, &RemoteProfilesDialog::updateState);
    connect(m_open, &QPushButton::clicked, this, [this] {
        open(false);
    });
    connect(m_openOther, &QPushButton::clicked, this, [this] {
        open(true);
    });
    connect(m_ssh, &QPushButton::clicked, this, &RemoteProfilesDialog::openSsh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateState();
}

void RemoteProfilesDialog::setOtherPaneAvailable(bool available)
{
    m_otherPaneAvailable = available;
    updateState();
}

Profile RemoteProfilesDialog::formProfile() const
{
    return m_editor->formProfile();
}

void RemoteProfilesDialog::setNewProfileDefaults(const Profile &defaults)
{
    m_editor->setNewProfileDefaults(defaults);
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
    // The editor reports changes while it is built, before the buttons exist.
    if (!m_open) {
        return;
    }
    const Profile profile = formProfile();
    const bool valid = RemoteProfiles::validationError(profile).isEmpty();
    const bool available = RemoteProfiles::unavailableReason(profile.protocol, m_check).isEmpty();
    m_open->setEnabled(valid && available);
    m_openOther->setEnabled(valid && available && m_otherPaneAvailable);
    m_ssh->setEnabled(valid && RemoteProfiles::supportsSsh(profile.protocol));
}
