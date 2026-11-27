// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "remoteprofiles.h"

#include <KConfigGroup>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

/**
 * Adds, edits and removes saved remote connections. Changes are saved as soon
 * as Save or Remove is chosen. Used by the Remote Connections dialog and the
 * Connections tab in Preferences.
 */
class RemoteProfilesEditor : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteProfilesEditor(const KConfigGroup &group, QWidget *parent = nullptr, RemoteProfiles::ProtocolCheck check = RemoteProfiles::isKnownProtocol);

    /// The connection defaults chosen under Preferences › Remote and Cloud › Connections.
    static RemoteProfiles::Profile defaultsFromSettings();

    /// The profile described by the form, which may be unsaved.
    RemoteProfiles::Profile formProfile() const;
    /// The protocol, user and sign-in method that new profiles start with.
    void setNewProfileDefaults(const RemoteProfiles::Profile &defaults);
    /// Reads the saved profiles again when they changed elsewhere, keeping unsaved edits otherwise.
    void refresh();

Q_SIGNALS:
    /// The form changed, so actions on the form's profile may need updating.
    void formChanged();
    /// The saved profiles changed.
    void profilesSaved();

private:
    void reload(const QString &selectedId = {});
    void showProfile(int row);
    void addProfile();
    void saveProfile();
    void removeProfile();
    void updateState();

    KConfigGroup m_group;
    RemoteProfiles::ProtocolCheck m_check;
    QList<RemoteProfiles::Profile> m_profiles;
    RemoteProfiles::Profile m_defaults;
    QListWidget *m_list;
    QLineEdit *m_name;
    QComboBox *m_protocol;
    QLineEdit *m_host;
    QSpinBox *m_port;
    QLineEdit *m_user;
    QLineEdit *m_path;
    QComboBox *m_authentication;
    QLineEdit *m_identityFile;
    QLabel *m_status;
    QPushButton *m_remove;
    QPushButton *m_save;
    QString m_loadError;
};
