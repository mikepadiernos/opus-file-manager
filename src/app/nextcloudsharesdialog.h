// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudclient.h"

#include <QDate>
#include <QDialog>
#include <QList>
#include <QUrl>

#include <functional>
#include <optional>
#include <utility>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace NextcloudShares
{
/// Asks the user to confirm @p text; returns whether they did. The default asks with a message box.
using Confirmation = std::function<bool(QWidget *parent, const QString &title, const QString &text, const QString &action)>;

/// What the user chose in the share editor. The password is only passed on to the server.
struct ShareDraft {
    Nextcloud::ShareType type = Nextcloud::ShareType::PublicLink;
    /// The user or group, for user and group shares.
    QString shareWith;
    Nextcloud::SharePermissions permissions = Nextcloud::SharePermission::Read;
    /// An invalid date for no expiry.
    QDate expiration;
    /// A new password; empty keeps the current one, or sets none.
    QString password;
    /// Whether to remove the current password.
    bool removePassword = false;
};

/// What the share editor edits.
struct EditorContext {
    /// Whether a new share is being created, rather than an existing one changed.
    bool creating = true;
    bool folder = false;
    /// Whether the existing share has a password.
    bool hadPassword = false;
    QString itemName;
    /// What the server allows, when known.
    std::optional<Nextcloud::ShareCapabilities> capabilities;
};

/// Asks the user for a share; nullopt when they cancel. The default is a NextcloudShareEditor.
using ShareEditor = std::function<std::optional<ShareDraft>(QWidget *parent, const ShareDraft &draft, const EditorContext &context)>;
/// Opens a new message with @p subject and @p body for the user to complete. The default uses the mail client.
using MailComposer = std::function<void(const QString &subject, const QString &body)>;

/// The question to ask before creating a share of @p itemName as @p draft describes.
QString createQuestion(const ShareDraft &draft, const QString &itemName, bool folder);
/// The question to ask before @p update gives more access to @p share.
QString broadenQuestion(const Nextcloud::Share &share, const Nextcloud::ShareUpdate &update);
/// The question to ask before revoking @p share.
QString revokeQuestion(const Nextcloud::Share &share);
/// What the user reviews before a message with the link of @p share is composed. Never includes a password.
QString emailReview(const Nextcloud::Share &share);
/// The subject and body of a message with the link of @p share. Never includes a password.
std::pair<QString, QString> emailMessage(const Nextcloud::Share &share);
/// The name of the item that @p share shares.
QString itemName(const Nextcloud::Share &share);
}

/**
 * Edits a new or existing share: who it is with, what they may do, when it
 * expires and its password. Enforces what the server's capabilities require.
 */
class NextcloudShareEditor : public QDialog
{
    Q_OBJECT

public:
    NextcloudShareEditor(const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &context, QWidget *parent = nullptr);
    ~NextcloudShareEditor() override;

    /// The share as edited.
    NextcloudShares::ShareDraft draft() const;
    /// Why the share can't be saved as it is, or an empty string.
    QString problem() const;

private:
    Nextcloud::ShareType type() const;
    void updateState();

    NextcloudShares::EditorContext m_context;
    QComboBox *m_type;
    QLineEdit *m_shareWith;
    QCheckBox *m_read;
    QCheckBox *m_update;
    QCheckBox *m_create;
    QCheckBox *m_delete;
    QCheckBox *m_share;
    QCheckBox *m_expires;
    QDateEdit *m_expiration;
    QLineEdit *m_password;
    QCheckBox *m_removePassword;
    QLabel *m_problem;
    QDialogButtonBox *m_buttons;
};

/**
 * Lists the Nextcloud shares of an item, those the user made and those
 * others made with the user, and creates, changes and revokes them after the
 * user confirms anything that gives more access or revokes it. Share
 * passwords are only sent to the server, never kept.
 */
class NextcloudSharesDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param store where the account's app password is kept; not owned
    /// @param item a file or folder in the account's files whose shares to list first
    /// @param itemIsFolder whether @p item is a folder
    NextcloudSharesDialog(const NextcloudAccounts::Account &account,
                          Nextcloud::CredentialStore *store,
                          const std::optional<QUrl> &item = std::nullopt,
                          bool itemIsFolder = false,
                          QWidget *parent = nullptr);
    ~NextcloudSharesDialog() override;

    NextcloudClient *client() const;
    void setConfirmation(const NextcloudShares::Confirmation &confirmation);
    void setShareEditor(const NextcloudShares::ShareEditor &editor);
    void setMailComposer(const NextcloudShares::MailComposer &composer);
    /// Offers sending a link by email; off by default, as the Send by Email setting decides.
    void setSendByEmailEnabled(bool enabled);
    void setScope(Nextcloud::ShareScope scope);
    Nextcloud::ShareScope scope() const;
    /// Lists the shares of the current scope again.
    void reload();
    /// The shares shown, in the order shown.
    QList<Nextcloud::Share> shownShares() const;

Q_SIGNALS:
    /// The user asked to show the item @p url in the active pane.
    void showRequested(const QUrl &url, bool folder);

private:
    void capabilitiesFetched(const QString &accountId, const Nextcloud::ShareCapabilities &capabilities, const QString &error);
    void sharesListed(int request, const Nextcloud::ShareListing &listing);
    void create(Nextcloud::ShareType type);
    void editSelected();
    void revokeSelected();
    void copySelected();
    void emailSelected();
    void showSelected();
    std::optional<Nextcloud::Share> selectedShare() const;
    bool manageable(const Nextcloud::Share &share) const;
    void updateState();

    NextcloudAccounts::Account m_account;
    std::optional<QUrl> m_item;
    QString m_itemPath;
    bool m_itemIsFolder = false;
    NextcloudClient *m_client;
    NextcloudShares::Confirmation m_confirm;
    NextcloudShares::ShareEditor m_editor;
    NextcloudShares::MailComposer m_composer;
    bool m_sendByEmail = false;
    QComboBox *m_scope;
    QTreeWidget *m_shares;
    QLabel *m_status;
    QPushButton *m_createLink;
    QPushButton *m_create;
    QPushButton *m_edit;
    QPushButton *m_revoke;
    QPushButton *m_copy;
    QPushButton *m_email;
    QPushButton *m_show;
    QPushButton *m_reload;
    QList<Nextcloud::Share> m_shareList;
    std::optional<Nextcloud::ShareCapabilities> m_capabilities;
    QString m_capabilitiesError;
    int m_request = 0;
    /// Shares being changed or revoked, and whether one is being created.
    QStringList m_pending;
    bool m_creating = false;
    QString m_message;
    QString m_listingError;
    bool m_unavailable = false;
    QString m_selectAfterReload;
};
