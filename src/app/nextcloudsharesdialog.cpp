// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudsharesdialog.h"

#include "sendbyemaildialog.h"
#include "symbolicicon.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <utility>

namespace
{
enum Column {
    ItemColumn,
    WithColumn,
    PermissionsColumn,
    ExpiresColumn,
    PasswordColumn,
};
constexpr int IndexRole = Qt::UserRole;

using Nextcloud::SharePermission;
using Nextcloud::SharePermissions;
using Nextcloud::ShareType;

KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudSharesDialog"));
}

bool askConfirmation(QWidget *parent, const QString &title, const QString &text, const QString &action)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Cancel, parent);
    QPushButton *accept = box.addButton(action, QMessageBox::AcceptRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(box.button(QMessageBox::Cancel)));
    box.exec();
    return box.clickedButton() == accept;
}

std::optional<NextcloudShares::ShareDraft> editShare(QWidget *parent, const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &context)
{
    NextcloudShareEditor editor(draft, context, parent);
    if (editor.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return editor.draft();
}

QString dateText(const QDate &date)
{
    return date.isValid() ? QLocale().toString(date, QLocale::ShortFormat) : i18nc("@item share expiry", "Never");
}

/// Who @p share gives access to, such as "Bob (user)" or "Anyone with the link".
QString recipient(const Nextcloud::Share &share)
{
    if (share.type == ShareType::PublicLink) {
        return share.label.isEmpty() ? i18nc("@item share recipient", "Anyone with the link")
                                     : i18nc("@item share recipient, %1 is the link's label", "Anyone with the link “%1”", share.label);
    }
    const QString name = share.shareWithDisplayName.isEmpty() ? share.shareWith : share.shareWithDisplayName;
    return i18nc("@item share recipient, %1 is a name, %2 a kind of share such as User", "%1 (%2)", name, Nextcloud::shareTypeName(share.type));
}

QString expiryLine(const QDate &date)
{
    return date.isValid() ? i18nc("@info %1 is a date", "Access ends on %1.", dateText(date)) : i18nc("@info", "Access doesn't expire.");
}
}

QString NextcloudShares::itemName(const Nextcloud::Share &share)
{
    const QString name = share.path.section(QLatin1Char('/'), -1);
    return name.isEmpty() ? share.path : name;
}

QString NextcloudShares::createQuestion(const ShareDraft &draft, const QString &itemName, bool folder)
{
    QStringList lines;
    const QString permissions = Nextcloud::describePermissions(draft.permissions, folder);
    switch (draft.type) {
    case ShareType::PublicLink:
        lines.append(i18nc("@info %1 is a file or folder name", "Create a public link to “%1”?", itemName));
        lines.append(i18nc("@info %1 describes permissions, such as View, edit",
                           "Anyone who gets the link can open it without signing in, with this access: %1.",
                           permissions));
        lines.append(draft.password.isEmpty() ? i18nc("@info", "The link has no password.")
                                              : i18nc("@info", "The link is protected by the password you entered, which Plasma Opus doesn't keep."));
        break;
    case ShareType::Group:
        lines.append(i18nc("@info %1 is a file or folder name, %2 a group", "Share “%1” with every member of the group “%2”?", itemName, draft.shareWith));
        lines.append(i18nc("@info %1 describes permissions, such as View, edit", "They get this access: %1.", permissions));
        break;
    default:
        lines.append(i18nc("@info %1 is a file or folder name, %2 a user", "Share “%1” with the user “%2”?", itemName, draft.shareWith));
        lines.append(i18nc("@info %1 describes permissions, such as View, edit", "They get this access: %1.", permissions));
        break;
    }
    lines.append(expiryLine(draft.expiration));
    return lines.join(QLatin1Char('\n'));
}

QString NextcloudShares::broadenQuestion(const Nextcloud::Share &share, const Nextcloud::ShareUpdate &update)
{
    QStringList lines{
        i18nc("@info %1 is a file or folder name, %2 who it is shared with", "Give more access to “%1” for %2?", itemName(share), recipient(share))};
    if (update.permissions && *update.permissions != share.permissions) {
        lines.append(i18nc("@info %1 and %2 describe permissions",
                           "Access changes from “%1” to “%2”.",
                           Nextcloud::describePermissions(share.permissions, share.folder),
                           Nextcloud::describePermissions(*update.permissions, share.folder)));
    }
    if (update.expiration && *update.expiration != share.expiration) {
        lines.append(update.expiration->isValid()
                         ? i18nc("@info %1 and %2 are dates", "Access ends on %2 instead of %1.", dateText(share.expiration), dateText(*update.expiration))
                         : i18nc("@info %1 is a date", "Access no longer ends on %1; it doesn't expire.", dateText(share.expiration)));
    }
    if (update.password && update.password->isEmpty() && share.hasPassword) {
        lines.append(i18nc("@info", "The password is removed: anyone who gets the link can open it."));
    }
    return lines.join(QLatin1Char('\n'));
}

QString NextcloudShares::revokeQuestion(const Nextcloud::Share &share)
{
    QStringList lines{
        i18nc("@info %1 is a file or folder name, %2 who it is shared with", "Revoke the share of “%1” with %2?", itemName(share), recipient(share))};
    lines.append(share.type == ShareType::PublicLink ? i18nc("@info", "The link stops working for everyone who has it. A new link would be different.")
                                                     : i18nc("@info", "They lose access to it. The file or folder itself is not deleted."));
    return lines.join(QLatin1Char('\n'));
}

QString NextcloudShares::emailReview(const Nextcloud::Share &share)
{
    QStringList lines{i18nc("@info %1 is a file or folder name", "Write a message with the public link to “%1”?", itemName(share))};
    lines.append(i18nc("@info %1 is a link", "Link: %1", share.url.toString()));
    lines.append(i18nc("@info %1 describes permissions, such as View, edit",
                       "Anyone who gets the message can open it with this access: %1.",
                       Nextcloud::describePermissions(share.permissions, share.folder)));
    lines.append(expiryLine(share.expiration));
    lines.append(share.hasPassword ? i18nc("@info", "The link is protected by a password, which the message doesn't include. Send the password separately.")
                                   : i18nc("@info", "The link has no password."));
    lines.append(i18nc("@info", "The message opens in your mail client for you to review and send."));
    return lines.join(QLatin1Char('\n'));
}

std::pair<QString, QString> NextcloudShares::emailMessage(const Nextcloud::Share &share)
{
    const QString name = itemName(share);
    QStringList lines{i18nc("@info email text, %1 is a file or folder name", "I shared “%1” with you:", name), share.url.toString(), QString()};
    lines.append(i18nc("@info email text, %1 describes permissions", "Access: %1", Nextcloud::describePermissions(share.permissions, share.folder)));
    if (share.expiration.isValid()) {
        lines.append(i18nc("@info email text, %1 is a date", "The link works until %1.", dateText(share.expiration)));
    }
    if (share.hasPassword) {
        lines.append(i18nc("@info email text", "The link is protected by a password, which I'll send you separately."));
    }
    return {i18nc("@info email subject, %1 is a file or folder name", "Shared with you: %1", name), lines.join(QLatin1Char('\n'))};
}

NextcloudShareEditor::NextcloudShareEditor(const NextcloudShares::ShareDraft &draft, const NextcloudShares::EditorContext &context, QWidget *parent)
    : QDialog(parent)
    , m_context(context)
    , m_type(new QComboBox(this))
    , m_shareWith(new QLineEdit(this))
    , m_read(new QCheckBox(i18nc("@option:check share permission", "View"), this))
    , m_update(new QCheckBox(i18nc("@option:check share permission", "Edit"), this))
    , m_create(new QCheckBox(i18nc("@option:check share permission", "Upload"), this))
    , m_delete(new QCheckBox(i18nc("@option:check share permission", "Delete"), this))
    , m_share(new QCheckBox(i18nc("@option:check share permission", "Reshare"), this))
    , m_expires(new QCheckBox(i18nc("@option:check", "Expires on:"), this))
    , m_expiration(new QDateEdit(this))
    , m_password(new QLineEdit(this))
    , m_removePassword(new QCheckBox(i18nc("@option:check", "Remove the password"), this))
    , m_problem(new QLabel(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle(context.creating ? i18nc("@title:window %1 is a file or folder name", "Share “%1”", context.itemName)
                                    : i18nc("@title:window %1 is a file or folder name", "Change Share of “%1”", context.itemName));

    m_type->setObjectName(QStringLiteral("shareType"));
    m_type->addItem(SymbolicIcon::fromTheme(QStringLiteral("link")), Nextcloud::shareTypeName(ShareType::PublicLink), int(ShareType::PublicLink));
    m_type->addItem(SymbolicIcon::fromTheme(QStringLiteral("user")), Nextcloud::shareTypeName(ShareType::User), int(ShareType::User));
    m_type->addItem(SymbolicIcon::fromTheme(QStringLiteral("group")), Nextcloud::shareTypeName(ShareType::Group), int(ShareType::Group));
    if (context.capabilities && !context.capabilities->groupSharing) {
        m_type->removeItem(m_type->findData(int(ShareType::Group)));
    }
    if (context.capabilities && !context.capabilities->publicLinks && draft.type != ShareType::PublicLink) {
        m_type->removeItem(m_type->findData(int(ShareType::PublicLink)));
    }
    if (m_type->findData(int(draft.type)) < 0) {
        m_type->addItem(Nextcloud::shareTypeName(draft.type), int(draft.type));
    }
    m_type->setCurrentIndex(m_type->findData(int(draft.type)));
    m_type->setEnabled(context.creating);

    m_shareWith->setObjectName(QStringLiteral("shareWith"));
    m_shareWith->setText(draft.shareWith);
    m_shareWith->setReadOnly(!context.creating);
    m_shareWith->setToolTip(i18nc("@info:tooltip", "The user or group ID on the server. Names aren't suggested yet."));

    m_read->setObjectName(QStringLiteral("shareRead"));
    m_update->setObjectName(QStringLiteral("shareUpdate"));
    m_create->setObjectName(QStringLiteral("shareCreate"));
    m_delete->setObjectName(QStringLiteral("shareDelete"));
    m_share->setObjectName(QStringLiteral("shareReshare"));
    m_read->setChecked(draft.permissions.testFlag(SharePermission::Read));
    m_update->setChecked(draft.permissions.testFlag(SharePermission::Update));
    m_create->setChecked(draft.permissions.testFlag(SharePermission::Create));
    m_delete->setChecked(draft.permissions.testFlag(SharePermission::Delete));
    m_share->setChecked(draft.permissions.testFlag(SharePermission::Share));
    m_create->setVisible(context.folder);
    m_delete->setVisible(context.folder);
    m_create->setToolTip(i18nc("@info:tooltip", "Add files to the folder. A public link with only this permission is a file drop."));

    m_expires->setObjectName(QStringLiteral("shareExpires"));
    m_expiration->setObjectName(QStringLiteral("shareExpiration"));
    m_expiration->setCalendarPopup(true);
    const QDate tomorrow = QDate::currentDate().addDays(1);
    m_expiration->setMinimumDate(tomorrow);
    m_expires->setChecked(draft.expiration.isValid());
    m_expiration->setDate(draft.expiration.isValid() ? draft.expiration : QDate::currentDate().addDays(7));

    m_password->setObjectName(QStringLiteral("sharePassword"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(context.hadPassword ? i18nc("@info:placeholder", "Leave empty to keep the current password")
                                                       : i18nc("@info:placeholder", "No password"));
    m_removePassword->setObjectName(QStringLiteral("shareRemovePassword"));
    m_removePassword->setVisible(context.hadPassword);
    m_removePassword->setChecked(draft.removePassword);

    m_problem->setObjectName(QStringLiteral("shareProblem"));
    m_problem->setWordWrap(true);

    auto *permissions = new QHBoxLayout;
    for (QCheckBox *box : {m_read, m_update, m_create, m_delete, m_share}) {
        permissions->addWidget(box);
    }
    permissions->addStretch();
    auto *expiry = new QHBoxLayout;
    expiry->addWidget(m_expires);
    expiry->addWidget(m_expiration);
    expiry->addStretch();

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:listbox", "Share with:"), m_type);
    form->addRow(i18nc("@label:textbox", "User or group:"), m_shareWith);
    form->addRow(i18nc("@label", "Access:"), permissions);
    form->addRow(i18nc("@label", "Expiry:"), expiry);
    form->addRow(i18nc("@label:textbox", "Password:"), m_password);
    form->addRow(QString(), m_removePassword);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_problem);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_type, &QComboBox::currentIndexChanged, this, [this] {
        // New user and group shares start with the server's default permissions, links with viewing only.
        SharePermissions defaults = SharePermission::Read;
        if (type() != ShareType::PublicLink && m_context.capabilities) {
            defaults = m_context.capabilities->defaultPermissions | SharePermission::Read;
        }
        m_read->setChecked(defaults.testFlag(SharePermission::Read));
        m_update->setChecked(defaults.testFlag(SharePermission::Update));
        m_create->setChecked(m_context.folder && defaults.testFlag(SharePermission::Create));
        m_delete->setChecked(m_context.folder && defaults.testFlag(SharePermission::Delete));
        m_share->setChecked(type() != ShareType::PublicLink && defaults.testFlag(SharePermission::Share));
        updateState();
    });
    for (QCheckBox *box : {m_read, m_update, m_create, m_delete, m_share, m_expires, m_removePassword}) {
        connect(box, &QCheckBox::toggled, this, &NextcloudShareEditor::updateState);
    }
    connect(m_shareWith, &QLineEdit::textChanged, this, &NextcloudShareEditor::updateState);
    connect(m_password, &QLineEdit::textChanged, this, &NextcloudShareEditor::updateState);
    connect(m_expiration, &QDateEdit::dateChanged, this, &NextcloudShareEditor::updateState);
    updateState();
}

NextcloudShareEditor::~NextcloudShareEditor() = default;

ShareType NextcloudShareEditor::type() const
{
    return ShareType(m_type->currentData().toInt());
}

NextcloudShares::ShareDraft NextcloudShareEditor::draft() const
{
    NextcloudShares::ShareDraft draft;
    draft.type = type();
    draft.shareWith = m_shareWith->text().trimmed();
    SharePermissions permissions;
    permissions.setFlag(SharePermission::Read, m_read->isChecked());
    permissions.setFlag(SharePermission::Update, m_update->isChecked());
    permissions.setFlag(SharePermission::Create, m_context.folder && m_create->isChecked());
    permissions.setFlag(SharePermission::Delete, m_context.folder && m_delete->isChecked());
    permissions.setFlag(SharePermission::Share, draft.type != ShareType::PublicLink && m_share->isChecked());
    draft.permissions = permissions;
    draft.expiration = m_expires->isChecked() ? m_expiration->date() : QDate();
    const bool link = draft.type == ShareType::PublicLink;
    draft.removePassword = link && m_context.hadPassword && m_removePassword->isChecked();
    draft.password = link && !draft.removePassword ? m_password->text() : QString();
    return draft;
}

QString NextcloudShareEditor::problem() const
{
    const NextcloudShares::ShareDraft current = draft();
    const bool link = current.type == ShareType::PublicLink;
    if (!link && current.shareWith.isEmpty()) {
        return current.type == ShareType::Group ? i18nc("@info", "Enter the group to share with.") : i18nc("@info", "Enter the user to share with.");
    }
    const bool fileDrop = link && m_context.folder && current.permissions == SharePermission::Create;
    if (!current.permissions.testFlag(SharePermission::Read) && !fileDrop) {
        return link && m_context.folder ? i18nc("@info", "Allow viewing, or only uploading for a file drop.") : i18nc("@info", "Allow at least viewing.");
    }
    const std::optional<Nextcloud::ShareCapabilities> &capabilities = m_context.capabilities;
    if (link && capabilities) {
        if (capabilities->passwordEnforced && (current.removePassword || (m_context.creating && current.password.isEmpty()))) {
            return i18nc("@info", "The server requires a password for public links.");
        }
        if (capabilities->expireEnforced && !current.expiration.isValid()) {
            return i18nc("@info", "The server requires public links to expire.");
        }
        if (capabilities->expireEnforced && capabilities->expireDays > 0 && current.expiration > QDate::currentDate().addDays(capabilities->expireDays)) {
            return i18ncp("@info",
                          "The server requires public links to expire within %1 day.",
                          "The server requires public links to expire within %1 days.",
                          capabilities->expireDays);
        }
        if (!capabilities->publicUpload && (current.permissions & (SharePermission::Create | SharePermission::Delete | SharePermission::Update))
            && m_context.folder) {
            return i18nc("@info", "The server doesn't allow uploads through public links.");
        }
    }
    return {};
}

void NextcloudShareEditor::updateState()
{
    const bool link = type() == ShareType::PublicLink;
    const std::optional<Nextcloud::ShareCapabilities> &capabilities = m_context.capabilities;
    m_shareWith->setEnabled(!link);
    // Shares with users and groups always allow viewing.
    if (!link && !m_read->isChecked()) {
        m_read->setChecked(true);
    }
    m_read->setEnabled(link && m_context.folder);
    m_share->setVisible(!link);
    m_share->setEnabled(!capabilities || capabilities->resharing);
    const bool expiryEnforced = link && capabilities && capabilities->expireEnforced;
    if (expiryEnforced && !m_expires->isChecked()) {
        m_expires->setChecked(true);
    }
    m_expires->setEnabled(!expiryEnforced);
    m_expiration->setMaximumDate(expiryEnforced && capabilities->expireDays > 0 ? QDate::currentDate().addDays(capabilities->expireDays) : QDate(9999, 12, 31));
    m_expiration->setEnabled(m_expires->isChecked());
    m_password->setEnabled(link && !m_removePassword->isChecked());
    m_removePassword->setEnabled(link && !(capabilities && capabilities->passwordEnforced));
    m_password->setToolTip(link ? i18nc("@info:tooltip", "Sent to the server only. Plasma Opus never keeps share passwords.")
                                : i18nc("@info:tooltip", "Only public links have passwords."));
    const QString problem = this->problem();
    m_problem->setText(problem);
    m_problem->setVisible(!problem.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty());
}

NextcloudSharesDialog::NextcloudSharesDialog(const NextcloudAccounts::Account &account,
                                             Nextcloud::CredentialStore *store,
                                             const std::optional<QUrl> &item,
                                             bool itemIsFolder,
                                             QWidget *parent)
    : QDialog(parent)
    , m_account(account)
    , m_item(item)
    , m_itemPath(item ? Nextcloud::sharePath(account, *item) : QString())
    , m_itemIsFolder(itemIsFolder)
    , m_client(new NextcloudClient(this))
    , m_confirm(askConfirmation)
    , m_editor(editShare)
    , m_scope(new QComboBox(this))
    , m_shares(new QTreeWidget(this))
    , m_status(new QLabel(this))
    , m_createLink(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("link")), i18nc("@action:button", "Create Public Link…"), this))
    , m_create(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("im-user")), i18nc("@action:button", "Share with User or Group…"), this))
    , m_edit(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "Change…"), this))
    , m_revoke(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-delete-remove")), i18nc("@action:button", "Revoke…"), this))
    , m_copy(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action:button", "Copy Link"), this))
    , m_email(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("mail-message-new")), i18nc("@action:button", "Send Link by Email…"), this))
    , m_show(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:button", "Show in Pane"), this))
    , m_reload(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "Reload"), this))
{
    setWindowTitle(i18nc("@title:window %1 is an account name", "Nextcloud Shares — %1", NextcloudAccounts::title(account)));
    m_client->setCredentialStore(store);
    m_composer = [this](const QString &subject, const QString &body) {
        SendByEmailDialog::composeMessage(subject, body, this, [this](const QString &error) {
            if (!error.isEmpty()) {
                m_message = i18nc("@info:status %1 is an error message", "The message couldn't be opened: %1", error);
                updateState();
            }
        });
    };
    resize(820, 440);

    auto *intro = new QLabel(i18nc("@info",
                                   "Shares on the Nextcloud server. Plasma Opus asks before it gives anyone more access or revokes a share, "
                                   "and never keeps share passwords."),
                             this);
    intro->setWordWrap(true);

    m_scope->setObjectName(QStringLiteral("sharesScope"));
    if (!m_itemPath.isEmpty()) {
        m_scope->addItem(SymbolicIcon::fromTheme(m_itemIsFolder ? QStringLiteral("folder") : QStringLiteral("text-x-generic")),
                         i18nc("@item:inlistbox %1 is a file or folder name", "Shares of “%1”", m_itemPath.section(QLatin1Char('/'), -1)),
                         int(Nextcloud::ShareScope::Item));
    }
    m_scope->addItem(SymbolicIcon::fromTheme(QStringLiteral("folder-publicshare")),
                     i18nc("@item:inlistbox", "Shared by me"),
                     int(Nextcloud::ShareScope::SharedByMe));
    m_scope->addItem(SymbolicIcon::fromTheme(QStringLiteral("folder-remote")),
                     i18nc("@item:inlistbox", "Shared with me"),
                     int(Nextcloud::ShareScope::SharedWithMe));
    m_reload->setObjectName(QStringLiteral("sharesReload"));

    m_shares->setObjectName(QStringLiteral("sharesList"));
    m_shares->setHeaderLabels({i18nc("@title:column", "Item"),
                               i18nc("@title:column", "With"),
                               i18nc("@title:column", "Access"),
                               i18nc("@title:column", "Expires"),
                               i18nc("@title:column", "Password")});
    m_shares->setRootIsDecorated(false);
    m_shares->setUniformRowHeights(true);
    m_shares->setSelectionMode(QAbstractItemView::SingleSelection);
    m_shares->header()->setSectionResizeMode(ItemColumn, QHeaderView::Stretch);
    m_shares->header()->setStretchLastSection(false);
    m_status->setObjectName(QStringLiteral("sharesStatus"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_createLink->setObjectName(QStringLiteral("sharesCreateLink"));
    m_create->setObjectName(QStringLiteral("sharesCreate"));
    m_edit->setObjectName(QStringLiteral("sharesEdit"));
    m_revoke->setObjectName(QStringLiteral("sharesRevoke"));
    m_copy->setObjectName(QStringLiteral("sharesCopyLink"));
    m_email->setObjectName(QStringLiteral("sharesEmail"));
    m_show->setObjectName(QStringLiteral("sharesShow"));
    m_createLink->setVisible(!m_itemPath.isEmpty());
    m_create->setVisible(!m_itemPath.isEmpty());
    m_email->setVisible(false);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(i18nc("@label:listbox", "Show:"), this));
    top->addWidget(m_scope, 1);
    top->addWidget(m_reload);

    auto *createRow = new QHBoxLayout;
    createRow->addWidget(m_createLink);
    createRow->addWidget(m_create);
    createRow->addStretch();

    auto *actions = new QHBoxLayout;
    for (QPushButton *button : {m_edit, m_revoke, m_copy, m_email, m_show}) {
        actions->addWidget(button);
    }
    actions->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(top);
    layout->addLayout(createRow);
    layout->addWidget(m_shares, 1);
    layout->addWidget(m_status);
    layout->addLayout(actions);
    layout->addWidget(buttons);

    if (m_itemPath.isEmpty()) {
        const int remembered = stateGroup().readEntry("Scope", int(Nextcloud::ShareScope::SharedByMe));
        m_scope->setCurrentIndex(std::max(0, m_scope->findData(remembered)));
    }

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this] {
        if (scope() != Nextcloud::ShareScope::Item) {
            stateGroup().writeEntry("Scope", int(scope()));
        }
        m_message.clear();
        reload();
    });
    connect(m_reload, &QPushButton::clicked, this, &NextcloudSharesDialog::reload);
    connect(m_shares, &QTreeWidget::itemSelectionChanged, this, &NextcloudSharesDialog::updateState);
    connect(m_shares, &QTreeWidget::itemDoubleClicked, this, &NextcloudSharesDialog::editSelected);
    connect(m_createLink, &QPushButton::clicked, this, [this] {
        create(ShareType::PublicLink);
    });
    connect(m_create, &QPushButton::clicked, this, [this] {
        create(ShareType::User);
    });
    connect(m_edit, &QPushButton::clicked, this, &NextcloudSharesDialog::editSelected);
    connect(m_revoke, &QPushButton::clicked, this, &NextcloudSharesDialog::revokeSelected);
    connect(m_copy, &QPushButton::clicked, this, &NextcloudSharesDialog::copySelected);
    connect(m_email, &QPushButton::clicked, this, &NextcloudSharesDialog::emailSelected);
    connect(m_show, &QPushButton::clicked, this, &NextcloudSharesDialog::showSelected);
    connect(m_client, &NextcloudClient::shareCapabilitiesFetched, this, &NextcloudSharesDialog::capabilitiesFetched);
    connect(m_client, &NextcloudClient::sharesListed, this, &NextcloudSharesDialog::sharesListed);
    connect(m_client, &NextcloudClient::shareCreated, this, [this](const QString &, const Nextcloud::Share &share, const QString &error) {
        if (!m_creating) {
            return;
        }
        m_creating = false;
        if (!error.isEmpty()) {
            m_message = i18nc("@info:status %1 is an error message", "The share couldn't be created: %1", error);
            updateState();
            return;
        }
        m_message = share.type == ShareType::PublicLink && share.url.isValid()
            ? i18nc("@info:status %1 is a link", "Created the public link %1.", share.url.toString())
            : i18nc("@info:status %1 is a file or folder name, %2 who it is shared with",
                    "Shared “%1” with %2.",
                    NextcloudShares::itemName(share),
                    recipient(share));
        m_selectAfterReload = share.id;
        {
            const QSignalBlocker blocker(m_scope);
            setScope(Nextcloud::ShareScope::Item);
        }
        reload();
    });
    connect(m_client, &NextcloudClient::shareUpdated, this, [this](const QString &id, const Nextcloud::Share &, const QString &error) {
        if (!m_pending.removeOne(id)) {
            return;
        }
        m_message = error.isEmpty() ? i18nc("@info:status", "Changed the share.")
                                    : i18nc("@info:status %1 is an error message", "The share couldn't be changed: %1", error);
        // The server's new state is listed again, as KIO doesn't pass on the response to a change.
        m_selectAfterReload = id;
        reload();
    });
    connect(m_client, &NextcloudClient::shareDeleted, this, [this](const QString &id, const QString &error) {
        if (!m_pending.removeOne(id)) {
            return;
        }
        m_message = error.isEmpty() ? i18nc("@info:status", "Revoked the share.")
                                    : i18nc("@info:status %1 is an error message", "The share couldn't be revoked: %1", error);
        reload();
    });

    m_client->fetchShareCapabilities(m_account);
    reload();
}

NextcloudSharesDialog::~NextcloudSharesDialog() = default;

NextcloudClient *NextcloudSharesDialog::client() const
{
    return m_client;
}

void NextcloudSharesDialog::setConfirmation(const NextcloudShares::Confirmation &confirmation)
{
    m_confirm = confirmation;
}

void NextcloudSharesDialog::setShareEditor(const NextcloudShares::ShareEditor &editor)
{
    m_editor = editor;
}

void NextcloudSharesDialog::setMailComposer(const NextcloudShares::MailComposer &composer)
{
    m_composer = composer;
}

void NextcloudSharesDialog::setSendByEmailEnabled(bool enabled)
{
    m_sendByEmail = enabled;
    m_email->setVisible(enabled);
    updateState();
}

void NextcloudSharesDialog::setScope(Nextcloud::ShareScope scope)
{
    m_scope->setCurrentIndex(std::max(0, m_scope->findData(int(scope))));
}

Nextcloud::ShareScope NextcloudSharesDialog::scope() const
{
    return Nextcloud::ShareScope(m_scope->currentData().toInt());
}

void NextcloudSharesDialog::reload()
{
    m_shareList.clear();
    {
        const QSignalBlocker blocker(m_shares);
        m_shares->clear();
    }
    m_listingError.clear();
    m_request = m_client->listShares(m_account, scope(), scope() == Nextcloud::ShareScope::Item ? m_itemPath : QString());
    updateState();
}

QList<Nextcloud::Share> NextcloudSharesDialog::shownShares() const
{
    QList<Nextcloud::Share> shares;
    for (int i = 0; i < m_shares->topLevelItemCount(); ++i) {
        shares.append(m_shareList.value(m_shares->topLevelItem(i)->data(ItemColumn, IndexRole).toInt()));
    }
    return shares;
}

void NextcloudSharesDialog::capabilitiesFetched(const QString &accountId, const Nextcloud::ShareCapabilities &capabilities, const QString &error)
{
    if (accountId != m_account.id) {
        return;
    }
    // Without capabilities, nothing is restricted in advance and the server reports what it refuses.
    m_capabilities = error.isEmpty() ? std::optional(capabilities) : std::nullopt;
    m_capabilitiesError = error;
    updateState();
}

void NextcloudSharesDialog::sharesListed(int request, const Nextcloud::ShareListing &listing)
{
    if (request != m_request) {
        return;
    }
    m_request = 0;
    m_shareList = listing.shares;
    m_listingError = listing.error;
    m_unavailable = listing.unavailable;
    const bool received = listing.scope == Nextcloud::ShareScope::SharedWithMe;
    QTreeWidgetItem *select = nullptr;
    {
        const QSignalBlocker blocker(m_shares);
        for (int i = 0; i < m_shareList.size(); ++i) {
            const Nextcloud::Share &share = m_shareList.at(i);
            const QString with = received
                ? i18nc("@item %1 is who shared an item", "From %1", share.ownerDisplayName.isEmpty() ? share.owner : share.ownerDisplayName)
                : recipient(share);
            QString password;
            if (share.type == ShareType::PublicLink) {
                password = share.hasPassword ? i18nc("@item the share has a password", "Yes") : i18nc("@item the share has no password", "No");
            }
            auto *item =
                new QTreeWidgetItem(m_shares,
                                    {share.path, with, Nextcloud::describePermissions(share.permissions, share.folder), dateText(share.expiration), password});
            item->setData(ItemColumn, IndexRole, i);
            item->setIcon(ItemColumn, SymbolicIcon::fromTheme(share.folder ? QStringLiteral("folder") : QStringLiteral("text-x-generic")));
            if (share.url.isValid()) {
                item->setToolTip(WithColumn, share.url.toString());
            }
            if (!received && !manageable(share)) {
                item->setToolTip(ItemColumn, i18nc("@info:tooltip", "Change this kind of share in the web interface."));
            }
            if (!m_selectAfterReload.isEmpty() && share.id == m_selectAfterReload) {
                select = item;
            }
        }
    }
    m_selectAfterReload.clear();
    if (select) {
        m_shares->setCurrentItem(select);
    }
    updateState();
}

std::optional<Nextcloud::Share> NextcloudSharesDialog::selectedShare() const
{
    const QList<QTreeWidgetItem *> selected = m_shares->selectedItems();
    if (selected.size() != 1) {
        return std::nullopt;
    }
    const int index = selected.first()->data(ItemColumn, IndexRole).toInt();
    if (index < 0 || index >= m_shareList.size()) {
        return std::nullopt;
    }
    return m_shareList.at(index);
}

bool NextcloudSharesDialog::manageable(const Nextcloud::Share &share) const
{
    return scope() != Nextcloud::ShareScope::SharedWithMe
        && (share.type == ShareType::PublicLink || share.type == ShareType::User || share.type == ShareType::Group);
}

void NextcloudSharesDialog::create(Nextcloud::ShareType type)
{
    if (m_itemPath.isEmpty() || m_creating) {
        return;
    }
    NextcloudShares::ShareDraft draft;
    draft.type = type;
    if (type != ShareType::PublicLink && m_capabilities) {
        draft.permissions = m_capabilities->defaultPermissions | SharePermission::Read;
    }
    if (!m_itemIsFolder) {
        draft.permissions &= SharePermissions(SharePermission::Read) | SharePermission::Update | SharePermission::Share;
    }
    if (type == ShareType::PublicLink && m_capabilities && m_capabilities->expireEnforced && m_capabilities->expireDays > 0) {
        draft.expiration = QDate::currentDate().addDays(m_capabilities->expireDays);
    }
    NextcloudShares::EditorContext context;
    context.creating = true;
    context.folder = m_itemIsFolder;
    context.itemName = m_itemPath.section(QLatin1Char('/'), -1);
    context.capabilities = m_capabilities;
    std::optional<NextcloudShares::ShareDraft> chosen = m_editor(this, draft, context);
    if (!chosen) {
        return;
    }
    const bool link = chosen->type == ShareType::PublicLink;
    if (!m_confirm(this,
                   link ? i18nc("@title:window", "Create Public Link") : i18nc("@title:window", "Share"),
                   NextcloudShares::createQuestion(*chosen, context.itemName, m_itemIsFolder),
                   link ? i18nc("@action:button", "Create Link") : i18nc("@action:button", "Share"))) {
        return;
    }
    Nextcloud::NewShare share;
    share.path = m_itemPath;
    share.type = chosen->type;
    share.shareWith = chosen->shareWith;
    share.permissions = chosen->permissions;
    share.expiration = chosen->expiration;
    // The password goes to the server once; neither the draft nor the dialog keeps it.
    share.password = std::exchange(chosen->password, QString());
    m_creating = true;
    m_message.clear();
    m_client->createShare(m_account, share);
    updateState();
}

void NextcloudSharesDialog::editSelected()
{
    const std::optional<Nextcloud::Share> share = selectedShare();
    if (!share || !manageable(*share) || !share->canEdit || m_pending.contains(share->id)) {
        return;
    }
    NextcloudShares::ShareDraft draft;
    draft.type = share->type;
    draft.shareWith = share->shareWithDisplayName.isEmpty() ? share->shareWith : share->shareWithDisplayName;
    draft.permissions = share->permissions;
    draft.expiration = share->expiration;
    NextcloudShares::EditorContext context;
    context.creating = false;
    context.folder = share->folder;
    context.hadPassword = share->hasPassword;
    context.itemName = NextcloudShares::itemName(*share);
    context.capabilities = m_capabilities;
    std::optional<NextcloudShares::ShareDraft> chosen = m_editor(this, draft, context);
    if (!chosen) {
        return;
    }
    Nextcloud::ShareUpdate update;
    if (chosen->permissions != share->permissions) {
        update.permissions = chosen->permissions;
    }
    if (chosen->expiration != share->expiration) {
        update.expiration = chosen->expiration;
    }
    if (share->type == ShareType::PublicLink) {
        if (chosen->removePassword && share->hasPassword) {
            update.password = QString();
        } else if (!chosen->password.isEmpty()) {
            update.password = std::exchange(chosen->password, QString());
        }
    }
    if (update.isEmpty()) {
        m_message = i18nc("@info:status", "Nothing changed.");
        updateState();
        return;
    }
    // Less access needs no confirmation; more access, a longer one or no password does.
    if (Nextcloud::broadensAccess(*share, update)
        && !m_confirm(this,
                      i18nc("@title:window", "Give More Access"),
                      NextcloudShares::broadenQuestion(*share, update),
                      i18nc("@action:button", "Give More Access"))) {
        return;
    }
    m_pending.append(share->id);
    m_message.clear();
    m_client->updateShare(m_account, *share, update);
    updateState();
}

void NextcloudSharesDialog::revokeSelected()
{
    const std::optional<Nextcloud::Share> share = selectedShare();
    if (!share || scope() == Nextcloud::ShareScope::SharedWithMe || !share->canDelete || m_pending.contains(share->id)) {
        return;
    }
    if (!m_confirm(this, i18nc("@title:window", "Revoke Share"), NextcloudShares::revokeQuestion(*share), i18nc("@action:button", "Revoke"))) {
        return;
    }
    m_pending.append(share->id);
    m_message.clear();
    m_client->deleteShare(m_account, *share);
    updateState();
}

void NextcloudSharesDialog::copySelected()
{
    const std::optional<Nextcloud::Share> share = selectedShare();
    if (!share || !share->url.isValid()) {
        return;
    }
    QGuiApplication::clipboard()->setText(share->url.toString());
    m_message = i18nc("@info:status", "Copied the link.");
    updateState();
}

void NextcloudSharesDialog::emailSelected()
{
    const std::optional<Nextcloud::Share> share = selectedShare();
    if (!m_sendByEmail || !share || share->type != ShareType::PublicLink || !share->url.isValid()) {
        return;
    }
    if (!m_confirm(this, i18nc("@title:window", "Send Link by Email"), NextcloudShares::emailReview(*share), i18nc("@action:button", "Write Message"))) {
        return;
    }
    const auto [subject, body] = NextcloudShares::emailMessage(*share);
    m_composer(subject, body);
}

void NextcloudSharesDialog::showSelected()
{
    const std::optional<Nextcloud::Share> share = selectedShare();
    const QUrl url = share ? Nextcloud::shareFileUrl(m_account, *share) : QUrl();
    if (url.isEmpty()) {
        return;
    }
    Q_EMIT showRequested(url, share->folder);
}

void NextcloudSharesDialog::updateState()
{
    const bool busy = m_request != 0;
    const Nextcloud::ShareScope current = scope();
    const std::optional<Nextcloud::Share> share = selectedShare();

    QStringList status;
    if (busy) {
        status.append(i18nc("@info:status", "Loading…"));
    } else if (!m_listingError.isEmpty()) {
        status.append(m_listingError);
    } else if (m_shareList.isEmpty()) {
        switch (current) {
        case Nextcloud::ShareScope::Item:
            status.append(i18nc("@info:status", "This item isn't shared."));
            break;
        case Nextcloud::ShareScope::SharedByMe:
            status.append(i18nc("@info:status", "You haven't shared anything."));
            break;
        case Nextcloud::ShareScope::SharedWithMe:
            status.append(i18nc("@info:status", "Nothing is shared with you."));
            break;
        }
    } else {
        status.append(i18ncp("@info:status", "%1 share.", "%1 shares.", m_shareList.size()));
    }
    if (current == Nextcloud::ShareScope::SharedWithMe && !busy) {
        status.append(i18nc("@info:status", "Shares with you can only be changed by their owners."));
    }
    if (m_capabilities && !m_capabilities->api) {
        status.append(i18nc("@info:status", "Sharing is turned off on this server."));
    }
    if (!m_message.isEmpty()) {
        status.prepend(m_message);
    }
    m_status->setText(status.join(QLatin1Char(' ')));

    QString createReason;
    if (m_unavailable) {
        createReason = m_listingError;
    } else if (m_capabilities && !m_capabilities->api) {
        createReason = i18nc("@info:tooltip", "Sharing is turned off on this server.");
    }
    m_create->setEnabled(createReason.isEmpty() && !m_creating);
    m_create->setToolTip(createReason.isEmpty() ? i18nc("@info:tooltip", "Share with a user or group on the server") : createReason);
    if (createReason.isEmpty() && m_capabilities && !m_capabilities->publicLinks) {
        createReason = i18nc("@info:tooltip", "Public links are turned off on this server.");
    }
    m_createLink->setEnabled(createReason.isEmpty() && !m_creating);
    m_createLink->setToolTip(createReason.isEmpty() ? i18nc("@info:tooltip", "Create a link that anyone who gets it can open") : createReason);

    const bool pending = share && m_pending.contains(share->id);
    const bool received = current == Nextcloud::ShareScope::SharedWithMe;
    QString editReason;
    if (share && received) {
        editReason = i18nc("@info:tooltip", "Only the owner can change a share with you.");
    } else if (share && !manageable(*share)) {
        editReason = i18nc("@info:tooltip", "Change this kind of share in the web interface.");
    } else if (share && !share->canEdit) {
        editReason = i18nc("@info:tooltip", "The server doesn't allow you to change this share.");
    }
    m_edit->setEnabled(share && editReason.isEmpty() && !pending);
    m_edit->setToolTip(editReason.isEmpty() ? i18nc("@info:tooltip", "Change the access, expiry or password") : editReason);
    QString revokeReason;
    if (share && received) {
        revokeReason = i18nc("@info:tooltip", "Only the owner can revoke a share with you.");
    } else if (share && !share->canDelete) {
        revokeReason = i18nc("@info:tooltip", "The server doesn't allow you to revoke this share.");
    }
    m_revoke->setEnabled(share && revokeReason.isEmpty() && !pending);
    m_revoke->setToolTip(revokeReason.isEmpty() ? i18nc("@info:tooltip", "Revoke the share; the item itself is not deleted") : revokeReason);

    const bool link = share && share->type == ShareType::PublicLink && share->url.isValid();
    m_copy->setEnabled(link);
    m_email->setEnabled(m_sendByEmail && link);
    m_email->setToolTip(i18nc("@info:tooltip", "Review the link, its access and expiry, then write a message with it in your mail client"));
    m_show->setEnabled(share && !Nextcloud::shareFileUrl(m_account, *share).isEmpty());
}
