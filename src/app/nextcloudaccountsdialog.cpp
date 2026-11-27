// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudaccountsdialog.h"

#include "symbolicicon.h"

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KMessageBox>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

using NextcloudAccounts::Account;

namespace
{
QLabel *makeLabel(const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    return label;
}

bool sameAccount(const Account &a, const Account &b)
{
    return a.server == b.server && a.user == b.user;
}
}

NextcloudAccountsDialog::NextcloudAccountsDialog(const KConfigGroup &group,
                                                 Nextcloud::CredentialStore *store,
                                                 QWidget *parent,
                                                 RemoteProfiles::ProtocolCheck check)
    : QDialog(parent)
    , m_group(group)
    , m_store(store)
    , m_check(std::move(check))
    , m_client(new NextcloudClient(this))
    , m_list(new QListWidget(this))
    , m_status(makeLabel(QStringLiteral("nextcloudStatus"), this))
    , m_notice(makeLabel(QStringLiteral("nextcloudNotice"), this))
    , m_server(new QLineEdit(this))
    , m_signIn(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("im-user-online")), i18nc("@action:button", "Sign In…"), this))
    , m_cancelSignIn(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("dialog-cancel")), i18nc("@action:button", "Cancel Sign-In"), this))
    , m_loginStatus(makeLabel(QStringLiteral("nextcloudLoginStatus"), this))
    , m_checkButton(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "Check"), this))
    , m_remove(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove…"), this))
{
    setWindowTitle(i18nc("@title:window", "Nextcloud Accounts"));
    m_client->setCredentialStore(m_store);

    auto *intro = new QLabel(i18nc("@info",
                                   "Browse the files of a Nextcloud server in either pane. Signing in creates an app password that KDE's "
                                   "password server keeps, in KDE Wallet when it is enabled. Plasma Opus never saves it in its settings."),
                             this);
    intro->setWordWrap(true);

    m_list->setObjectName(QStringLiteral("nextcloudAccounts"));
    m_server->setObjectName(QStringLiteral("nextcloudServer"));
    m_server->setPlaceholderText(i18nc("@info:placeholder", "cloud.example.com"));
    m_server->setClearButtonEnabled(true);
    m_signIn->setObjectName(QStringLiteral("nextcloudSignIn"));
    m_signIn->setToolTip(i18nc("@info:tooltip", "Sign in with your web browser and grant Plasma Opus access with an app password"));
    m_cancelSignIn->setObjectName(QStringLiteral("nextcloudCancelSignIn"));
    m_cancelSignIn->hide();
    m_loginStatus->setOpenExternalLinks(true);
    m_checkButton->setObjectName(QStringLiteral("nextcloudCheck"));
    m_checkButton->setToolTip(i18nc("@info:tooltip", "Check the sign-in, the server's version and which Nextcloud apps it offers"));
    m_remove->setObjectName(QStringLiteral("nextcloudRemove"));

    auto *accountButtons = new QHBoxLayout;
    accountButtons->addWidget(m_checkButton);
    accountButtons->addWidget(m_remove);
    accountButtons->addStretch();

    auto *add = new QGroupBox(i18nc("@title:group", "Add an Account"), this);
    auto *addLayout = new QVBoxLayout(add);
    auto *serverRow = new QHBoxLayout;
    auto *serverLabel = new QLabel(i18nc("@label:textbox", "Server:"), add);
    serverLabel->setBuddy(m_server);
    serverRow->addWidget(serverLabel);
    serverRow->addWidget(m_server, 1);
    serverRow->addWidget(m_signIn);
    serverRow->addWidget(m_cancelSignIn);
    addLayout->addLayout(serverRow);
    addLayout->addWidget(m_loginStatus);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_open = buttons->addButton(i18nc("@action:button", "Open"), QDialogButtonBox::ActionRole);
    m_openOther = buttons->addButton(i18nc("@action:button", "Open in Other Pane"), QDialogButtonBox::ActionRole);
    m_open->setIcon(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")));
    m_openOther->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-split-left-right")));
    m_open->setObjectName(QStringLiteral("nextcloudOpen"));
    m_openOther->setObjectName(QStringLiteral("nextcloudOpenOther"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_status);
    layout->addLayout(accountButtons);
    layout->addWidget(m_notice);
    layout->addWidget(add);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this] {
        showStatus();
        updateState();
    });
    connect(m_server, &QLineEdit::textChanged, this, &NextcloudAccountsDialog::updateState);
    connect(m_server, &QLineEdit::returnPressed, this, &NextcloudAccountsDialog::signIn);
    connect(m_signIn, &QPushButton::clicked, this, &NextcloudAccountsDialog::signIn);
    connect(m_cancelSignIn, &QPushButton::clicked, this, [this] {
        m_client->cancelLogin();
        m_loginStatus->setText(i18nc("@info", "Sign-in cancelled."));
        updateState();
    });
    connect(m_checkButton, &QPushButton::clicked, this, &NextcloudAccountsDialog::check);
    connect(m_remove, &QPushButton::clicked, this, &NextcloudAccountsDialog::remove);
    for (QPushButton *button : {m_open, m_openOther}) {
        connect(button, &QPushButton::clicked, this, [this, button] {
            if (const std::optional<Account> account = selected()) {
                Q_EMIT openRequested(*account, button == m_openOther);
                accept();
            }
        });
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::finished, m_client, &NextcloudClient::cancelLogin);
    connect(m_client, &NextcloudClient::loginStarted, this, &NextcloudAccountsDialog::loginStarted);
    connect(m_client, &NextcloudClient::loginFinished, this, &NextcloudAccountsDialog::loginFinished);
    connect(m_client, &NextcloudClient::loginFailed, this, &NextcloudAccountsDialog::loginFailed);
    connect(m_client, &NextcloudClient::checked, this, [this](const QString &id, const Nextcloud::Status &status) {
        m_statuses.insert(id, status);
        if (m_checking == id) {
            m_checking.clear();
        }
        reload();
    });
    connect(m_client, &NextcloudClient::revoked, this, [this](const QString &, const QString &error) {
        m_notice->setText(error.isEmpty() ? i18nc("@info", "The server revoked the app password of the removed account.")
                                          : i18nc("@info %1 is the error",
                                                  "The app password of the removed account could not be revoked: %1 Revoke it in Nextcloud under "
                                                  "Settings › Security.",
                                                  error));
    });

    resize(560, 520);
    reload();
}

void NextcloudAccountsDialog::setOnlineAccounts(const QList<Account> &accounts, const QString &error)
{
    m_online = accounts;
    m_onlineError = error;
    reload();
}

void NextcloudAccountsDialog::setOtherPaneAvailable(bool available)
{
    m_otherPaneAvailable = available;
    updateState();
}

void NextcloudAccountsDialog::setRemovalConfirmation(RemovalConfirmation confirmation)
{
    m_confirmation = std::move(confirmation);
}

void NextcloudAccountsDialog::setPageOpener(PageOpener opener)
{
    m_pageOpener = std::move(opener);
}

void NextcloudAccountsDialog::selectAccount(const QString &id)
{
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toString() == id) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

NextcloudClient *NextcloudAccountsDialog::client() const
{
    return m_client;
}

QList<Account> NextcloudAccountsDialog::accounts() const
{
    return NextcloudAccounts::merged(m_saved, m_online);
}

void NextcloudAccountsDialog::reload()
{
    const QString current = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
    m_saved = NextcloudAccounts::load(m_group, &m_loadError);
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const QList<Account> all = accounts();
    for (const Account &account : all) {
        QString text = NextcloudAccounts::title(account);
        if (account.source == NextcloudAccounts::Source::OnlineAccounts) {
            text = i18nc("@item %1 is an account name", "%1 (KDE Online Accounts)", text);
        }
        const Nextcloud::Status status = m_statuses.value(account.id);
        const bool problem = m_statuses.contains(account.id) && (status.accessExpired || !status.error.isEmpty());
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(problem ? QStringLiteral("dialog-warning") : QStringLiteral("folder-cloud")), text, m_list);
        item->setData(Qt::UserRole, account.id);
        item->setToolTip(NextcloudAccounts::filesUrl(account).toDisplayString());
    }
    int row = 0;
    for (int index = 0; index < m_list->count(); ++index) {
        if (m_list->item(index)->data(Qt::UserRole).toString() == current) {
            row = index;
        }
    }
    m_list->setCurrentRow(m_list->count() > 0 ? row : -1);
    showStatus();
    updateState();
}

std::optional<Account> NextcloudAccountsDialog::selected() const
{
    if (!m_list->currentItem()) {
        return std::nullopt;
    }
    const QString id = m_list->currentItem()->data(Qt::UserRole).toString();
    const QList<Account> all = accounts();
    for (const Account &account : all) {
        if (account.id == id) {
            return account;
        }
    }
    return std::nullopt;
}

void NextcloudAccountsDialog::signIn()
{
    if (m_client->isLoggingIn()) {
        return;
    }
    QString error;
    const QUrl server = NextcloudAccounts::serverUrl(m_server->text(), &error);
    if (server.isEmpty()) {
        m_loginStatus->setText(error);
        return;
    }
    m_server->setText(server.toString());
    m_loginStatus->setText(i18nc("@info", "Contacting the server…"));
    m_client->startLogin(server);
    updateState();
}

void NextcloudAccountsDialog::loginStarted(const QUrl &url)
{
    if (m_pageOpener) {
        m_pageOpener(url);
    } else {
        auto *job = new KIO::OpenUrlJob(url, QStringLiteral("text/html"));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    }
    m_loginStatus->setText(i18nc("@info %1 is a link",
                                 "Sign in and grant access in your web browser. Plasma Opus waits until you do. If no browser opened, "
                                 "<a href=\"%1\">open the sign-in page</a>.",
                                 url.toString().toHtmlEscaped()));
    updateState();
}

void NextcloudAccountsDialog::loginFinished(Account account, const QString &password, const Nextcloud::Status &status)
{
    QList<Account> saved = m_saved;
    bool replaced = false;
    for (Account &existing : saved) {
        if (existing.source == NextcloudAccounts::Source::Login && sameAccount(existing, account)) {
            existing.loginName = account.loginName;
            existing.realm = account.realm;
            account = existing;
            replaced = true;
        }
    }
    if (!replaced) {
        saved.append(account);
    }
    QString error;
    if (!NextcloudAccounts::save(m_group, saved, &error)) {
        m_loginStatus->setText(error);
        updateState();
        return;
    }
    if (!replaced) {
        account = saved.last();
    }
    if (m_store) {
        m_store->store(account, password);
    }
    m_statuses.insert(account.id, status);
    m_server->clear();
    m_loginStatus->setText(replaced ? i18nc("@info %1 is an account name", "Signed in to %1 again.", NextcloudAccounts::title(account))
                                    : i18nc("@info %1 is an account name", "Added %1.", NextcloudAccounts::title(account)));
    reload();
    selectAccount(account.id);
    Q_EMIT accountsChanged();
}

void NextcloudAccountsDialog::loginFailed(const QString &error)
{
    m_loginStatus->setText(error);
    updateState();
}

void NextcloudAccountsDialog::check()
{
    const std::optional<Account> account = selected();
    if (!account || !m_checking.isEmpty()) {
        return;
    }
    m_checking = account->id;
    m_statuses.remove(account->id);
    showStatus();
    updateState();
    m_client->check(*account);
}

void NextcloudAccountsDialog::remove()
{
    const std::optional<Account> account = selected();
    if (!account || account->source != NextcloudAccounts::Source::Login) {
        return;
    }
    Removal removal = Removal::Cancel;
    if (m_confirmation) {
        removal = m_confirmation(*account);
    } else {
        const auto answer = KMessageBox::questionTwoActionsCancel(
            this,
            i18nc("@info %1 is an account name",
                  "Remove %1 from Plasma Opus?<nl/><nl/>Revoking also deletes the account's app password on the server, so that it stops "
                  "working everywhere. Otherwise it keeps working until you revoke it in Nextcloud under Settings › Security.",
                  NextcloudAccounts::title(*account)),
            i18nc("@title:window", "Remove Nextcloud Account"),
            KGuiItem(i18nc("@action:button", "Revoke and Remove"), QStringLiteral("edit-delete")),
            KGuiItem(i18nc("@action:button", "Remove Only"), QStringLiteral("list-remove")),
            KStandardGuiItem::cancel(),
            QString(),
            KMessageBox::Dangerous);
        removal = answer == KMessageBox::PrimaryAction ? Removal::RevokeAndRemove : answer == KMessageBox::SecondaryAction ? Removal::Remove : Removal::Cancel;
    }
    if (removal == Removal::Cancel) {
        return;
    }
    if (removal == Removal::RevokeAndRemove) {
        // The app password is read now, before it is forgotten.
        m_notice->setText(i18nc("@info", "Revoking the app password…"));
        m_client->revokeAppPassword(*account);
    } else {
        m_notice->setText(i18nc("@info", "Removed. The app password keeps working until you revoke it in Nextcloud under Settings › Security."));
    }
    QList<Account> saved = m_saved;
    saved.removeIf([&account](const Account &existing) {
        return existing.id == account->id;
    });
    QString error;
    if (!NextcloudAccounts::save(m_group, saved, &error)) {
        m_notice->setText(error);
        return;
    }
    if (m_store) {
        m_store->forget(*account);
    }
    m_statuses.remove(account->id);
    reload();
    Q_EMIT accountsChanged();
}

void NextcloudAccountsDialog::showStatus()
{
    QStringList lines;
    if (!m_loadError.isEmpty()) {
        lines.append(m_loadError);
    }
    if (!m_onlineError.isEmpty()) {
        lines.append(m_onlineError);
    }
    const std::optional<Account> account = selected();
    if (!account) {
        if (m_list->count() == 0) {
            lines.append(i18nc("@info", "No Nextcloud accounts. Enter a server address below and sign in to add one."));
        }
    } else {
        if (account->source == NextcloudAccounts::Source::OnlineAccounts) {
            lines.append(i18nc("@info",
                               "From KDE Online Accounts, where it is managed. KIO asks for the password when you first open it; "
                               "sign in here to use an app password instead."));
        }
        if (m_checking == account->id) {
            lines.append(i18nc("@info", "Checking…"));
        } else if (m_statuses.contains(account->id)) {
            lines.append(Nextcloud::describe(m_statuses.value(account->id)));
        } else {
            lines.append(i18nc("@info", "Not checked yet."));
        }
    }
    m_status->setText(lines.join(QLatin1Char('\n')));
}

void NextcloudAccountsDialog::updateState()
{
    const std::optional<Account> account = selected();
    const QString unavailable = RemoteProfiles::unavailableReason(RemoteProfiles::Protocol::WebDavs, m_check);
    const bool loggingIn = m_client->isLoggingIn();
    m_open->setEnabled(account && unavailable.isEmpty());
    m_openOther->setEnabled(account && unavailable.isEmpty() && m_otherPaneAvailable);
    m_open->setToolTip(unavailable);
    m_openOther->setToolTip(unavailable);
    m_checkButton->setEnabled(account && m_checking.isEmpty());
    const bool removable = account && account->source == NextcloudAccounts::Source::Login;
    m_remove->setEnabled(removable);
    m_remove->setToolTip(account && !removable ? i18nc("@info:tooltip", "Remove accounts from KDE Online Accounts in System Settings")
                                               : i18nc("@info:tooltip", "Remove the account from Plasma Opus, and optionally revoke its app password"));
    m_server->setEnabled(!loggingIn);
    m_signIn->setVisible(!loggingIn);
    m_signIn->setEnabled(!m_server->text().trimmed().isEmpty());
    m_cancelSignIn->setVisible(loggingIn);
}
