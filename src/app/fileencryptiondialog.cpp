// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileencryptiondialog.h"

#include "symbolicicon.h"

#include <KLocalizedString>
#include <KNewPasswordWidget>
#include <KPasswordLineEdit>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace FileEncryption;

namespace
{
bool usesAesCrypt(Operation operation)
{
    return operation == Operation::AesEncrypt || operation == Operation::AesDecrypt;
}

QString operationTitle(Operation operation)
{
    switch (operation) {
    case Operation::Encrypt:
        return i18nc("@title:window", "Encrypt Files");
    case Operation::Sign:
        return i18nc("@title:window", "Sign Files");
    case Operation::Decrypt:
        return i18nc("@title:window", "Decrypt Files");
    case Operation::Verify:
        return i18nc("@title:window", "Verify Signatures");
    case Operation::AesEncrypt:
        return i18nc("@title:window", "Encrypt with AES Crypt");
    case Operation::AesDecrypt:
        return i18nc("@title:window", "Decrypt with AES Crypt");
    }
    return {};
}

QString introText(Operation operation, int count)
{
    switch (operation) {
    case Operation::Encrypt:
        return i18ncp("@info",
                      "Encrypt %1 file for the chosen keys or with a password. Encrypted copies are saved next to the originals, which are kept.",
                      "Encrypt %1 files for the chosen keys or with a password. Encrypted copies are saved next to the originals, which are kept.",
                      count);
    case Operation::Sign:
        return i18ncp("@info",
                      "Sign %1 file. Each signature is saved as a separate .sig or .asc file next to the signed file, which is not changed.",
                      "Sign %1 files. Each signature is saved as a separate .sig or .asc file next to the signed file, which is not changed.",
                      count);
    case Operation::Decrypt:
        return i18ncp("@info",
                      "Decrypting %1 file. Decrypted copies are saved next to the encrypted files, which are kept. GnuPG asks for passwords itself.",
                      "Decrypting %1 files. Decrypted copies are saved next to the encrypted files, which are kept. GnuPG asks for passwords itself.",
                      count);
    case Operation::Verify:
        return i18ncp("@info", "Checking the signature of %1 file.", "Checking the signatures of %1 files.", count);
    case Operation::AesEncrypt:
        return i18ncp("@info",
                      "Encrypt %1 file with AES Crypt. Encrypted copies are saved next to the originals as .aes files, which are kept. Without "
                      "the password, the encrypted copies cannot be recovered.",
                      "Encrypt %1 files with AES Crypt. Encrypted copies are saved next to the originals as .aes files, which are kept. Without "
                      "the password, the encrypted copies cannot be recovered.",
                      count);
    case Operation::AesDecrypt:
        return i18ncp("@info",
                      "Decrypt %1 AES Crypt file. Decrypted copies are saved next to the encrypted files, which are kept.",
                      "Decrypt %1 AES Crypt files. Decrypted copies are saved next to the encrypted files, which are kept.",
                      count);
    }
    return {};
}

QString startText(Operation operation)
{
    switch (operation) {
    case Operation::Encrypt:
    case Operation::AesEncrypt:
        return i18nc("@action:button", "Encrypt");
    case Operation::Sign:
        return i18nc("@action:button", "Sign");
    case Operation::Decrypt:
    case Operation::AesDecrypt:
        return i18nc("@action:button", "Decrypt");
    case Operation::Verify:
        return i18nc("@action:button", "Verify");
    }
    return {};
}

bool trusted(const Signature &signature)
{
    return signature.status == Signature::Status::Good && (signature.trust == Signature::Trust::Full || signature.trust == Signature::Trust::Ultimate);
}

QString signatureIcon(const Signature &signature)
{
    if (trusted(signature)) {
        return QStringLiteral("dialog-ok-apply");
    }
    return signature.status == Signature::Status::Bad ? QStringLiteral("dialog-error") : QStringLiteral("dialog-warning");
}
}

FileEncryptionDialog::FileEncryptionDialog(Operation operation, const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_operation(operation)
    , m_paths(paths)
    , m_runner(new FileEncryptionRunner(this))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_start(new QPushButton(startText(operation), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel"), this))
{
    setObjectName(QStringLiteral("fileEncryptionDialog"));
    setWindowTitle(operationTitle(operation));
    resize(650, 450);
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(introText(operation, paths.size()), this);
    intro->setObjectName(QStringLiteral("encryptionIntro"));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *options = new QWidget(this);
    options->setObjectName(QStringLiteral("encryptionOptions"));
    buildOptions(options);
    layout->addWidget(options);

    m_results->setObjectName(QStringLiteral("encryptionResults"));
    m_results->setColumnCount(2);
    m_results->setHeaderLabels({i18nc("@title:column", "File"), i18nc("@title:column", "Result")});
    m_results->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_results->header()->setStretchLastSection(true);
    m_results->setWordWrap(true);
    layout->addWidget(m_results, 1);

    m_status->setObjectName(QStringLiteral("encryptionStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_start->setObjectName(QStringLiteral("encryptionStart"));
    m_cancel->setObjectName(QStringLiteral("encryptionCancel"));
    m_cancel->setEnabled(false);
    buttons->addButton(m_start, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_start, &QPushButton::clicked, this, &FileEncryptionDialog::start);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling…"));
        m_runner->cancel();
    });
    layout->addWidget(buttons);

    connect(m_runner, &FileEncryptionRunner::keysListed, this, &FileEncryptionDialog::showKeys);
    connect(m_runner, &FileEncryptionRunner::fileStarted, this, [this](int index, const QString &path) {
        m_status->setText(i18nc("@info %1 is a file name", "Processing: %1", QFileInfo(path).fileName()));
        if (QTreeWidgetItem *row = m_rows.value(index)) {
            row->setText(1, i18nc("@info:status", "Working…"));
        }
    });
    connect(m_runner, &FileEncryptionRunner::fileFinished, this, &FileEncryptionDialog::showOutcome);
    connect(m_runner, &FileEncryptionRunner::finished, this, [this](bool canceled) {
        m_cancel->setEnabled(false);
        if (canceled) {
            for (QTreeWidgetItem *row : std::as_const(m_rows)) {
                if (!row->data(1, Qt::UserRole).toBool()) {
                    row->setText(1, i18nc("@info:status", "Skipped"));
                }
            }
            m_status->setText(i18nc("@info", "Canceled. Files already written are kept."));
        } else if (m_failures > 0) {
            m_status->setText(i18ncp("@info", "Finished: %1 of %2 file failed.", "Finished: %1 of %2 files failed.", m_failures, m_paths.size()));
        } else {
            m_status->setText(i18nc("@info", "Finished."));
        }
        Q_EMIT finished();
    });

    const Tool tool = usesAesCrypt(operation) ? Tool::AesCrypt : Tool::GnuPG;
    if (findExecutable(tool).isEmpty()) {
        m_status->setText(missingText(tool));
        options->setEnabled(false);
        m_start->setEnabled(false);
        return;
    }
    if (operation == Operation::Encrypt || operation == Operation::Sign) {
        m_status->setText(i18nc("@info", "Loading keys…"));
        m_runner->listKeys(operation == Operation::Sign);
    }
    updateStart();
    if (operation == Operation::Decrypt || operation == Operation::Verify) {
        QTimer::singleShot(0, this, &FileEncryptionDialog::start);
    }
}

FileEncryptionRunner *FileEncryptionDialog::runner() const
{
    return m_runner;
}

void FileEncryptionDialog::buildOptions(QWidget *options)
{
    auto *layout = new QFormLayout(options);
    layout->setContentsMargins({});
    switch (m_operation) {
    case Operation::Encrypt: {
        m_keys = new QTreeWidget(options);
        m_keys->setObjectName(QStringLiteral("encryptionKeys"));
        m_keys->setRootIsDecorated(false);
        m_keys->setHeaderLabels({i18nc("@title:column", "Key"), i18nc("@title:column", "Fingerprint")});
        m_keys->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        connect(m_keys, &QTreeWidget::itemChanged, this, &FileEncryptionDialog::updateStart);
        layout->addRow(i18nc("@label", "Encrypt for:"), m_keys);
        m_password = new QCheckBox(i18nc("@option:check", "Also allow decrypting with a password, which GnuPG asks for"), options);
        m_password->setObjectName(QStringLiteral("encryptionPassword"));
        connect(m_password, &QCheckBox::toggled, this, &FileEncryptionDialog::updateStart);
        layout->addRow(QString(), m_password);
        break;
    }
    case Operation::Sign:
        m_signer = new QComboBox(options);
        m_signer->setObjectName(QStringLiteral("encryptionSigner"));
        connect(m_signer, &QComboBox::currentIndexChanged, this, &FileEncryptionDialog::updateStart);
        layout->addRow(i18nc("@label:listbox", "Sign with:"), m_signer);
        break;
    case Operation::AesEncrypt:
        m_newPassword = new KNewPasswordWidget(options);
        m_newPassword->setObjectName(QStringLiteral("encryptionNewPassword"));
        m_newPassword->setAllowEmptyPasswords(false);
        connect(m_newPassword, &KNewPasswordWidget::passwordStatusChanged, this, &FileEncryptionDialog::updateStart);
        layout->addRow(m_newPassword);
        break;
    case Operation::AesDecrypt:
        m_passwordEdit = new KPasswordLineEdit(options);
        m_passwordEdit->setObjectName(QStringLiteral("encryptionPasswordEdit"));
        connect(m_passwordEdit, &KPasswordLineEdit::passwordChanged, this, &FileEncryptionDialog::updateStart);
        layout->addRow(i18nc("@label:textbox", "Password:"), m_passwordEdit);
        break;
    case Operation::Decrypt:
    case Operation::Verify:
        options->hide();
        return;
    }
    if (m_operation == Operation::Encrypt || m_operation == Operation::Sign) {
        m_armor = new QCheckBox(i18nc("@option:check", "Save as text (.asc) instead of binary"), options);
        m_armor->setObjectName(QStringLiteral("encryptionArmor"));
        m_armor->setToolTip(i18nc("@info:tooltip", "Text files can be pasted into emails and messages"));
        layout->addRow(QString(), m_armor);
    }
}

void FileEncryptionDialog::showKeys(const QList<Key> &keys, const QString &error)
{
    m_status->setText(error);
    if (m_operation == Operation::Encrypt) {
        m_keys->clear();
        int usable = 0;
        for (const Key &key : keys) {
            if (!key.canEncrypt || key.unusable) {
                continue;
            }
            auto *item = new QTreeWidgetItem(m_keys, {keyText(key), key.fingerprint});
            item->setData(0, Qt::UserRole, key.fingerprint);
            item->setToolTip(0, key.userIds.join(QLatin1Char('\n')));
            if (isValid(key)) {
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(0, Qt::Unchecked);
                ++usable;
            } else {
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                item->setToolTip(
                    0,
                    i18nc("@info:tooltip", "GnuPG encrypts only for keys certified as belonging to their owner. Certify this key in Kleopatra first."));
            }
        }
        if (error.isEmpty() && usable == 0) {
            m_status->setText(i18nc("@info", "No certified keys to encrypt for were found. Import or certify keys in Kleopatra, or encrypt with a password."));
        }
    } else if (m_operation == Operation::Sign) {
        m_signer->clear();
        for (const Key &key : keys) {
            if (key.canSign && !key.unusable) {
                m_signer->addItem(keyText(key), key.fingerprint);
            }
        }
        if (error.isEmpty() && m_signer->count() == 0) {
            m_status->setText(i18nc("@info", "You have no key to sign with. Create one in Kleopatra or with “gpg --full-generate-key”."));
        }
    }
    updateStart();
}

void FileEncryptionDialog::updateStart()
{
    if (!m_rows.isEmpty()) {
        m_start->setEnabled(false);
        return;
    }
    bool ready = true;
    switch (m_operation) {
    case Operation::Encrypt: {
        bool anyKey = false;
        for (int i = 0; i < m_keys->topLevelItemCount(); ++i) {
            anyKey = anyKey || m_keys->topLevelItem(i)->checkState(0) == Qt::Checked;
        }
        ready = anyKey || m_password->isChecked();
        break;
    }
    case Operation::Sign:
        ready = !m_signer->currentData().toString().isEmpty();
        break;
    case Operation::AesEncrypt:
        ready = m_newPassword->passwordStatus() == KNewPasswordWidget::WeakPassword || m_newPassword->passwordStatus() == KNewPasswordWidget::StrongPassword;
        break;
    case Operation::AesDecrypt:
        ready = !m_passwordEdit->password().isEmpty();
        break;
    case Operation::Decrypt:
    case Operation::Verify:
        break;
    }
    m_start->setEnabled(ready);
}

void FileEncryptionDialog::start()
{
    if (!m_rows.isEmpty() || m_runner->isRunning()) {
        return;
    }
    Options options;
    QString password;
    switch (m_operation) {
    case Operation::Encrypt:
        for (int i = 0; i < m_keys->topLevelItemCount(); ++i) {
            if (m_keys->topLevelItem(i)->checkState(0) == Qt::Checked) {
                options.recipients.append(m_keys->topLevelItem(i)->data(0, Qt::UserRole).toString());
            }
        }
        options.password = m_password->isChecked();
        break;
    case Operation::Sign:
        options.signer = m_signer->currentData().toString();
        break;
    case Operation::AesEncrypt:
        password = m_newPassword->password();
        break;
    case Operation::AesDecrypt:
        password = m_passwordEdit->password();
        m_passwordEdit->clear();
        break;
    case Operation::Decrypt:
    case Operation::Verify:
        break;
    }
    if (m_armor) {
        options.armor = m_armor->isChecked();
    }
    if (QWidget *optionsWidget = findChild<QWidget *>(QStringLiteral("encryptionOptions"))) {
        optionsWidget->setEnabled(false);
    }
    for (const QString &path : std::as_const(m_paths)) {
        auto *row = new QTreeWidgetItem(m_results, {QFileInfo(path).fileName(), i18nc("@info:status", "Waiting")});
        row->setToolTip(0, path);
        m_rows.append(row);
    }
    m_failures = 0;
    m_start->setEnabled(false);
    m_cancel->setEnabled(true);
    m_status->setText(i18nc("@info", "Starting…"));
    m_runner->start(m_operation, m_paths, options, password);
}

void FileEncryptionDialog::showOutcome(int index, const Outcome &outcome)
{
    QTreeWidgetItem *row = m_rows.value(index);
    if (!row) {
        return;
    }
    m_failures += !outcome.success;
    row->setData(1, Qt::UserRole, true);
    QString icon = outcome.success ? QStringLiteral("dialog-ok-apply") : QStringLiteral("dialog-error");
    if (m_operation == Operation::Verify) {
        icon = outcome.signatures.isEmpty() ? QStringLiteral("dialog-error") : signatureIcon(outcome.signatures.first());
        for (const Signature &signature : outcome.signatures) {
            if (!trusted(signature) && icon == QStringLiteral("dialog-ok-apply")) {
                icon = signatureIcon(signature);
            }
        }
    }
    row->setIcon(1, SymbolicIcon::fromTheme(icon));
    row->setText(1, outcome.message);
    row->setToolTip(1, outcome.output.isEmpty() ? outcome.message : outcome.output);
    // Signatures found while decrypting are listed under the file.
    if (m_operation == Operation::Decrypt) {
        for (const Signature &signature : outcome.signatures) {
            auto *child = new QTreeWidgetItem(row, {QString(), signatureText(signature)});
            child->setIcon(1, SymbolicIcon::fromTheme(signatureIcon(signature)));
        }
        row->setExpanded(true);
    }
    for (const Signature &signature : outcome.signatures) {
        if (signature.created.isValid() || !signature.fingerprint.isEmpty()) {
            const QString details = i18nc("@info:tooltip %1 is a date, %2 a key fingerprint",
                                          "Signed on %1\nKey fingerprint: %2",
                                          QLocale().toString(signature.created, QLocale::LongFormat),
                                          signature.fingerprint);
            row->setToolTip(1, outcome.message + QStringLiteral("\n\n") + details);
            break;
        }
    }
}
