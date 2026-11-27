// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "plasmavaults.h"

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QUrl>

#include <functional>

class QMenu;

/**
 * Fills a menu with Plasma Vault's vaults and their actions. Opening, closing,
 * configuring and creating vaults are left to Plasma Vault and its dialogs;
 * an opened vault is shown in a pane, and closing a vault in use asks first.
 */
class PlasmaVaultMenu : public QObject
{
    Q_OBJECT

public:
    /// Returns whether to close a vault that the listed processes are using.
    using ConfirmClose = std::function<bool(const PlasmaVaults::Vault &vault, const QStringList &users)>;

    PlasmaVaultMenu(QMenu *menu, VaultClient *client, QObject *parent = nullptr);

    VaultClient *client() const;
    void setConfirmClose(ConfirmClose confirm);
    /// Where processes are looked up when closing a vault; /proc by default.
    void setProcRoot(const QString &procRoot);

    /// Rebuilds the menu from Plasma Vault's current vaults.
    void populate();
    /// Asks Plasma Vault to open a closed vault, then shows it once it is open.
    void openVault(const PlasmaVaults::Vault &vault);
    /// Asks Plasma Vault to close an open vault, after confirming when processes use it.
    void closeVault(const PlasmaVaults::Vault &vault);

Q_SIGNALS:
    /// Requests showing an open vault's folder in a pane.
    void showRequested(const QUrl &url);
    /// A translated message for the status bar.
    void statusMessage(const QString &message);

private:
    void onVaultChanged(const PlasmaVaults::Vault &vault);

    QPointer<QMenu> m_menu;
    VaultClient *m_client;
    ConfirmClose m_confirmClose;
    QString m_procRoot = QStringLiteral("/proc");
    /// Vaults opened from Plasma Opus, shown in a pane once they are open.
    QSet<QString> m_pendingShow;
};
