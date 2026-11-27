// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "remoteprofiles.h"

#include <KConfigGroup>
#include <QDialog>

class QPushButton;
class RemoteProfilesEditor;

/**
 * Adds, edits and removes saved remote connections, and opens
 * one in a pane or an SSH terminal.
 */
class RemoteProfilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteProfilesDialog(const KConfigGroup &group, QWidget *parent = nullptr, RemoteProfiles::ProtocolCheck check = RemoteProfiles::isKnownProtocol);

    /// Whether "Open in Other Pane" is offered.
    void setOtherPaneAvailable(bool available);
    /// The profile described by the form, which may be unsaved.
    RemoteProfiles::Profile formProfile() const;
    /// The protocol, user and sign-in method that new profiles start with.
    void setNewProfileDefaults(const RemoteProfiles::Profile &defaults);

Q_SIGNALS:
    void openRequested(const QUrl &url, bool otherPane);
    void sshRequested(const RemoteProfiles::Profile &profile);

private:
    void open(bool otherPane);
    void openSsh();
    void updateState();

    RemoteProfiles::ProtocolCheck m_check;
    RemoteProfilesEditor *m_editor;
    QPushButton *m_open = nullptr;
    QPushButton *m_openOther = nullptr;
    QPushButton *m_ssh = nullptr;
    bool m_otherPaneAvailable = true;
};
