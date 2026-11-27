// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "remotemounts.h"

#include <KConfigGroup>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

/**
 * Chooses how a saved remote connection is mounted as a local folder: the
 * backend, read-only access, the folder and whether to mount at startup.
 */
class RemoteMountDialog : public QDialog
{
    Q_OBJECT

public:
    RemoteMountDialog(const RemoteProfiles::Profile &profile,
                      const KConfigGroup &group,
                      QWidget *parent = nullptr,
                      RemoteMounts::ExecutableLookup find = RemoteMounts::findExecutable);

    /// The options described by the form, which may be unsaved.
    RemoteMounts::Options options() const;

Q_SIGNALS:
    /// The options were saved and the connection should be mounted with them.
    void mountRequested();

private:
    bool save();
    void updateState();

    RemoteProfiles::Profile m_profile;
    KConfigGroup m_group;
    RemoteMounts::ExecutableLookup m_find;
    QComboBox *m_backend;
    QCheckBox *m_readOnly;
    QLineEdit *m_location;
    QCheckBox *m_atStartup;
    QLabel *m_hint;
    QLabel *m_status;
    QPushButton *m_save;
    QPushButton *m_mount;
};
