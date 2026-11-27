// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudclient.h"
#include "remoteprofiles.h"

#include <KConfigGroup>

#include <QDialog>
#include <QHash>

#include <functional>
#include <optional>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * Adds Nextcloud accounts with Login Flow, checks what their servers offer,
 * removes them and opens their files. Accounts from KDE Online Accounts are
 * listed but managed in System Settings.
 */
class NextcloudAccountsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Removal {
        Cancel,
        Remove,
        /// Also delete the app password on the server.
        RevokeAndRemove,
    };
    using RemovalConfirmation = std::function<Removal(const NextcloudAccounts::Account &account)>;
    using PageOpener = std::function<void(const QUrl &url)>;

    /**
     * @param group where the accounts are saved
     * @param store where app passwords are kept; not owned
     */
    NextcloudAccountsDialog(const KConfigGroup &group,
                            Nextcloud::CredentialStore *store,
                            QWidget *parent = nullptr,
                            RemoteProfiles::ProtocolCheck check = RemoteProfiles::isKnownProtocol);

    /// Accounts from KDE Online Accounts to list alongside the saved ones.
    void setOnlineAccounts(const QList<NextcloudAccounts::Account> &accounts, const QString &error = QString());
    /// Whether "Open in Other Pane" is offered.
    void setOtherPaneAvailable(bool available);
    /// Replaces the question asked before removing an account, for tests.
    void setRemovalConfirmation(RemovalConfirmation confirmation);
    /// Replaces how the sign-in page is opened in a web browser, for tests.
    void setPageOpener(PageOpener opener);
    /// Selects the account with @p id.
    void selectAccount(const QString &id);
    NextcloudClient *client() const;
    /// The saved accounts followed by those from Online Accounts.
    QList<NextcloudAccounts::Account> accounts() const;

Q_SIGNALS:
    void openRequested(const NextcloudAccounts::Account &account, bool otherPane);
    /// An account was added, signed in again or removed.
    void accountsChanged();

private:
    void reload();
    std::optional<NextcloudAccounts::Account> selected() const;
    void signIn();
    void loginStarted(const QUrl &url);
    void loginFinished(NextcloudAccounts::Account account, const QString &password, const Nextcloud::Status &status);
    void loginFailed(const QString &error);
    void check();
    void remove();
    void showStatus();
    void updateState();

    KConfigGroup m_group;
    Nextcloud::CredentialStore *m_store;
    RemoteProfiles::ProtocolCheck m_check;
    NextcloudClient *m_client;
    RemovalConfirmation m_confirmation;
    PageOpener m_pageOpener;
    QList<NextcloudAccounts::Account> m_saved;
    QList<NextcloudAccounts::Account> m_online;
    QString m_loadError;
    QString m_onlineError;
    QHash<QString, Nextcloud::Status> m_statuses;

    QString m_checking;
    bool m_otherPaneAvailable = true;

    QListWidget *m_list;
    QLabel *m_status;
    QLabel *m_notice;
    QLineEdit *m_server;
    QPushButton *m_signIn;
    QPushButton *m_cancelSignIn;
    QLabel *m_loginStatus;
    QPushButton *m_checkButton;
    QPushButton *m_remove;
    QPushButton *m_open;
    QPushButton *m_openOther;
};
