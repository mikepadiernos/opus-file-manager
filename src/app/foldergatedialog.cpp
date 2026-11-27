// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldergatedialog.h"

#include "foldergates.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KNewPasswordDialog>
#include <KPasswordDialog>

#include <QApplication>
#include <QDir>
#include <QFileInfo>

#include <functional>

namespace
{
/// Returns an error message for a wrong password, or an empty string when it is accepted.
using PasswordCheck = std::function<QString(const QString &password)>;

class GatePasswordDialog : public KPasswordDialog
{
public:
    GatePasswordDialog(QWidget *parent, PasswordCheck check)
        : KPasswordDialog(parent)
        , m_check(std::move(check))
    {
    }

protected:
    bool checkPassword() override
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const QString error = m_check(password());
        QApplication::restoreOverrideCursor();
        if (!error.isEmpty()) {
            showErrorMessage(error, KPasswordDialog::PasswordError);
            return false;
        }
        return true;
    }

private:
    PasswordCheck m_check;
};

class GateNewPasswordDialog : public KNewPasswordDialog
{
public:
    GateNewPasswordDialog(QWidget *parent, PasswordCheck apply)
        : KNewPasswordDialog(parent)
        , m_apply(std::move(apply))
    {
        setAllowEmptyPasswords(false);
    }

protected:
    bool checkPassword(const QString &password) override
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const QString error = m_apply(password);
        QApplication::restoreOverrideCursor();
        if (!error.isEmpty()) {
            KMessageBox::error(this, error);
            return false;
        }
        return true;
    }

private:
    PasswordCheck m_apply;
};

QString folderName(const QString &folder)
{
    const QString name = QFileInfo(folder).fileName();
    return name.isEmpty() ? QDir::toNativeSeparators(folder) : name;
}

QString checkError(FolderGates::Check check)
{
    switch (check) {
    case FolderGates::Check::Unlocked:
        return {};
    case FolderGates::Check::Wrong:
        return i18nc("@info:status password-gated folders", "The password is incorrect.");
    case FolderGates::Check::Damaged:
        break;
    }
    return i18nc("@info:status password-gated folders", "The stored password for this folder is damaged, so the folder stays locked.");
}

void show(QDialog *dialog, const QString &objectName, const QString &title)
{
    dialog->setObjectName(objectName);
    dialog->setWindowTitle(title);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->open();
}

KPasswordDialog *passwordDialog(QWidget *parent, const QString &prompt, PasswordCheck check)
{
    auto *dialog = new GatePasswordDialog(parent, std::move(check));
    dialog->setIcon(QIcon::fromTheme(QStringLiteral("folder-locked")));
    dialog->setPrompt(prompt);
    return dialog;
}

KNewPasswordDialog *newPasswordDialog(QWidget *parent, const QString &prompt, PasswordCheck apply)
{
    auto *dialog = new GateNewPasswordDialog(parent, std::move(apply));
    dialog->setIcon(QIcon::fromTheme(QStringLiteral("folder-locked")));
    dialog->setPrompt(prompt);
    return dialog;
}
}

void FolderGateDialog::protect(QWidget *parent, const QString &folder)
{
    const QString prompt = xi18nc("@info",
                                  "Choose a password for <filename>%1</filename>. Plasma Opus will show its contents only after the password "
                                  "is entered.<nl/><nl/><warning>This is not encryption. Other applications, terminals and file sharing can still "
                                  "open the folder, and its files and permissions do not change. The password cannot be recovered.</warning>",
                                  folderName(folder));
    auto *dialog = newPasswordDialog(parent, prompt, [folder](const QString &password) {
        return FolderGates::instance()->protect(folder, password);
    });
    show(dialog, QStringLiteral("folderGateProtectDialog"), i18nc("@title:window", "Protect Folder with Password"));
}

void FolderGateDialog::unlock(QWidget *parent, const QString &gate)
{
    const QString prompt = xi18nc("@info", "Enter the password for <filename>%1</filename>.", folderName(gate));
    auto *dialog = passwordDialog(parent, prompt, [gate](const QString &password) {
        return checkError(FolderGates::instance()->unlock(gate, password));
    });
    show(dialog, QStringLiteral("folderGateUnlockDialog"), i18nc("@title:window", "Unlock Folder"));
}

void FolderGateDialog::changePassword(QWidget *parent, const QString &gate)
{
    const QString prompt = xi18nc("@info", "Enter the current password for <filename>%1</filename>.", folderName(gate));
    auto *dialog = passwordDialog(parent, prompt, [gate](const QString &password) {
        return FolderGates::instance()->checkPassword(gate, password);
    });
    QObject::connect(dialog, &KPasswordDialog::gotPassword, parent, [parent, gate](const QString &oldPassword) {
        const QString newPrompt = xi18nc("@info", "Choose a new password for <filename>%1</filename>.", folderName(gate));
        auto *next = newPasswordDialog(parent, newPrompt, [gate, oldPassword](const QString &password) {
            return FolderGates::instance()->changePassword(gate, oldPassword, password);
        });
        show(next, QStringLiteral("folderGateNewPasswordDialog"), i18nc("@title:window", "Change Folder Password"));
    });
    show(dialog, QStringLiteral("folderGateChangeDialog"), i18nc("@title:window", "Change Folder Password"));
}

void FolderGateDialog::removeProtection(QWidget *parent, const QString &gate)
{
    const QString prompt = xi18nc("@info", "Enter the password for <filename>%1</filename> to remove its password protection.", folderName(gate));
    auto *dialog = passwordDialog(parent, prompt, [gate](const QString &password) {
        return FolderGates::instance()->removeProtection(gate, password);
    });
    show(dialog, QStringLiteral("folderGateRemoveDialog"), i18nc("@title:window", "Remove Password Protection"));
}
