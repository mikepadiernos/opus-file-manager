// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fileencryption.h"

#include <QDialog>

class KNewPasswordWidget;
class KPasswordLineEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * Encrypts, signs, decrypts or verifies local files, writing results next
 * to the originals. Decrypting and verifying start at once; the other
 * operations first ask for keys or an AES Crypt password.
 */
class FileEncryptionDialog : public QDialog
{
    Q_OBJECT

public:
    FileEncryptionDialog(FileEncryption::Operation operation, const QStringList &paths, QWidget *parent = nullptr);

    FileEncryptionRunner *runner() const;

Q_SIGNALS:
    /// Emitted when every file has been processed or the run was canceled.
    void finished();

private:
    void buildOptions(QWidget *options);
    void showKeys(const QList<FileEncryption::Key> &keys, const QString &error);
    void updateStart();
    void start();
    void showOutcome(int index, const FileEncryption::Outcome &outcome);

    FileEncryption::Operation m_operation;
    QStringList m_paths;
    FileEncryptionRunner *m_runner;
    QLabel *m_status;
    QTreeWidget *m_results;
    QPushButton *m_start;
    QPushButton *m_cancel;
    QTreeWidget *m_keys = nullptr;
    QComboBox *m_signer = nullptr;
    QCheckBox *m_password = nullptr;
    QCheckBox *m_armor = nullptr;
    KNewPasswordWidget *m_newPassword = nullptr;
    KPasswordLineEdit *m_passwordEdit = nullptr;
    QList<QTreeWidgetItem *> m_rows;
    int m_failures = 0;
};
