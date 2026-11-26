// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "remoteprofiles.h"

#include <KConfigGroup>
#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

/**
 * Adds, edits and removes saved SSH, SFTP and WebDAV connections, and opens
 * one in a pane or an SSH terminal.
 */
class RemoteProfilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteProfilesDialog(const KConfigGroup &group, QWidget *parent = nullptr);

    /// Whether "Open in Other Pane" is offered.
    void setOtherPaneAvailable(bool available);
    /// The profile described by the form, which may be unsaved.
    RemoteProfiles::Profile formProfile() const;

Q_SIGNALS:
    void openRequested(const QUrl &url, bool otherPane);
    void sshRequested(const RemoteProfiles::Profile &profile);

private:
    void reload(const QString &selectedId = {});
    void showProfile(int row);
    void addProfile();
    void saveProfile();
    void removeProfile();
    void open(bool otherPane);
    void openSsh();
    void updateState();

    KConfigGroup m_group;
    QList<RemoteProfiles::Profile> m_profiles;
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
    QPushButton *m_open;
    QPushButton *m_openOther;
    QPushButton *m_ssh;
    bool m_otherPaneAvailable = true;
    QString m_loadError;
};
