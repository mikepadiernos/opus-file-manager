// SPDX-License-Identifier: GPL-3.0-or-later

#include "plasmavaultmenu.h"

#include "settings.h"
#include "symbolicicon.h"

#include <KLocalizedString>

#include <QAction>
#include <QMenu>

PlasmaVaultMenu::PlasmaVaultMenu(QMenu *menu, VaultClient *client, QObject *parent)
    : QObject(parent)
    , m_menu(menu)
    , m_client(client)
{
    connect(m_client, &VaultClient::vaultChanged, this, &PlasmaVaultMenu::onVaultChanged);
    connect(m_client, &VaultClient::failed, this, &PlasmaVaultMenu::statusMessage);
    if (m_menu) {
        connect(m_menu, &QMenu::aboutToShow, this, &PlasmaVaultMenu::populate);
    }
}

VaultClient *PlasmaVaultMenu::client() const
{
    return m_client;
}

void PlasmaVaultMenu::setConfirmClose(ConfirmClose confirm)
{
    m_confirmClose = std::move(confirm);
}

void PlasmaVaultMenu::setProcRoot(const QString &procRoot)
{
    m_procRoot = procRoot;
}

void PlasmaVaultMenu::populate()
{
    if (!m_menu) {
        return;
    }
    m_menu->clear();
    QString error;
    const QList<PlasmaVaults::Vault> all = m_client->vaults(&error);
    if (!error.isEmpty()) {
        QAction *unavailable = m_menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")), error);
        unavailable->setObjectName(QStringLiteral("vaultsUnavailable"));
        unavailable->setEnabled(false);
        return;
    }
    const QList<PlasmaVaults::Vault> vaults = PlasmaVaults::shownVaults(all, Settings::plasmaVaultHidden());
    const bool showState = Settings::plasmaVaultShowState();
    if (vaults.isEmpty()) {
        QAction *none = all.isEmpty()
            ? m_menu->addAction(i18nc("@item:inmenu", "No Vaults"))
            : m_menu->addAction(i18ncp("@item:inmenu", "%1 Vault Hidden in Preferences", "%1 Vaults Hidden in Preferences", all.size()));
        none->setObjectName(QStringLiteral("noVaults"));
        none->setEnabled(false);
    }
    for (const PlasmaVaults::Vault &vault : vaults) {
        // Without the state, open and closed vaults share a name and icon; problems are still reported.
        const bool stateShown = showState || !PlasmaVaults::isLockState(vault.status);
        QMenu *submenu =
            m_menu->addMenu(SymbolicIcon::fromTheme(stateShown ? PlasmaVaults::iconName(vault) : QStringLiteral("folder-encrypted")),
                            stateShown ? i18nc("@title:menu vault name and status", "%1 (%2)", vault.displayName(), PlasmaVaults::statusText(vault.status))
                                       : vault.displayName());
        submenu->menuAction()->setObjectName(QStringLiteral("vault:") + vault.device);
        if (!vault.message.isEmpty()) {
            QAction *message = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")), vault.message);
            message->setEnabled(false);
            submenu->addSeparator();
        }
        if (vault.status == PlasmaVaults::Status::Closed) {
            QAction *open = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("object-unlocked")), i18nc("@action:inmenu", "Open Vault…"));
            open->setObjectName(QStringLiteral("openVault"));
            connect(open, &QAction::triggered, this, [this, vault] {
                openVault(vault);
            });
        } else if (vault.isOpen()) {
            QAction *show = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-open")), i18nc("@action:inmenu", "Show Vault Contents"));
            show->setObjectName(QStringLiteral("showVault"));
            connect(show, &QAction::triggered, this, [this, vault] {
                Q_EMIT showRequested(QUrl::fromLocalFile(vault.mountPoint));
            });
            QAction *close = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("object-locked")), i18nc("@action:inmenu", "Close Vault"));
            close->setObjectName(QStringLiteral("closeVault"));
            connect(close, &QAction::triggered, this, [this, vault] {
                closeVault(vault);
            });
        } else {
            QAction *status = submenu->addAction(PlasmaVaults::statusText(vault.status));
            status->setEnabled(false);
        }
        if (!vault.isBusy()) {
            QAction *configure = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu", "Configure Vault…"));
            configure->setObjectName(QStringLiteral("configureVault"));
            connect(configure, &QAction::triggered, this, [this, device = vault.device] {
                m_client->request(VaultClient::Request::Configure, device);
            });
        }
    }
    m_menu->addSeparator();
    QAction *create = m_menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:inmenu", "Create Vault…"));
    create->setObjectName(QStringLiteral("createVault"));
    connect(create, &QAction::triggered, this, [this] {
        m_client->request(VaultClient::Request::Create);
    });
}

void PlasmaVaultMenu::openVault(const PlasmaVaults::Vault &vault)
{
    m_pendingShow.insert(vault.device);
    m_client->request(VaultClient::Request::Open, vault.device);
}

void PlasmaVaultMenu::closeVault(const PlasmaVaults::Vault &vault)
{
    m_pendingShow.remove(vault.device);
    const QList<PlasmaVaults::User> users = PlasmaVaults::usersOf(vault.mountPoint, m_procRoot);
    if (!users.isEmpty()) {
        QStringList names;
        for (const PlasmaVaults::User &user : users) {
            names.append(i18nc("@item process name and id", "%1 (process %2)", user.name, user.pid));
        }
        if (!m_confirmClose || !m_confirmClose(vault, names)) {
            return;
        }
    }
    m_client->request(VaultClient::Request::Close, vault.device);
}

void PlasmaVaultMenu::onVaultChanged(const PlasmaVaults::Vault &vault)
{
    if (!m_pendingShow.contains(vault.device)) {
        // Vaults hidden in Preferences, or all of them with the menu, stay quiet unless opened from Plasma Opus.
        const bool shown = Settings::plasmaVaultMenu() && !Settings::plasmaVaultHidden().contains(vault.device);
        if (shown && !vault.message.isEmpty()) {
            Q_EMIT statusMessage(i18nc("@info:status vault name and Plasma Vault's message", "%1: %2", vault.displayName(), vault.message));
        }
        return;
    }
    if (vault.isOpen()) {
        m_pendingShow.remove(vault.device);
        Q_EMIT showRequested(QUrl::fromLocalFile(vault.mountPoint));
    } else if (!vault.isBusy()) {
        // Cancelled or failed: Plasma Vault reports why in its message.
        m_pendingShow.remove(vault.device);
        if (!vault.message.isEmpty()) {
            Q_EMIT statusMessage(i18nc("@info:status vault name and Plasma Vault's message", "%1: %2", vault.displayName(), vault.message));
        }
    }
}
