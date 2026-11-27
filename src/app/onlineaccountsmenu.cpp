// SPDX-License-Identifier: GPL-3.0-or-later

#include "onlineaccountsmenu.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QAction>
#include <QIcon>
#include <QMenu>

namespace
{
QString menuText(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}
}

namespace OnlineAccountsMenu
{
void populate(QMenu *menu,
              const QList<OnlineAccounts::Account> &accounts,
              const QString &error,
              const OnlineAccounts::ProtocolCheck &isKnownProtocol,
              bool otherPaneAvailable,
              QAction *manage,
              QObject *context,
              const OpenHandler &open)
{
    menu->clear();
    menu->setToolTipsVisible(true);
    for (const OnlineAccounts::Account &account : accounts) {
        const QString title = account.providerDisplayName.isEmpty()
            ? account.displayName
            : i18nc("@title:menu account name (provider name)", "%1 (%2)", account.displayName, account.providerDisplayName);
        menu->addSection(QIcon::fromTheme(account.iconName), menuText(title));
        const QList<OnlineAccounts::Location> locations = OnlineAccounts::locations(account, isKnownProtocol);
        for (const OnlineAccounts::Location &location : locations) {
            const QIcon icon = QIcon::fromTheme(location.iconName, QIcon::fromTheme(QStringLiteral("folder-cloud")));
            if (!location.isAvailable()) {
                QAction *unavailable = menu->addAction(icon, menuText(location.title));
                unavailable->setEnabled(false);
                unavailable->setToolTip(location.unavailableReason);
                continue;
            }
            QMenu *submenu = menu->addMenu(icon, menuText(location.title));
            submenu->setToolTipsVisible(true);
            submenu->menuAction()->setToolTip(location.url.toDisplayString());
            const QUrl url = location.url;
            submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:inmenu", "Open"), context, [open, url] {
                open(url, false);
            });
            QAction *other = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-split-left-right")),
                                                i18nc("@action:inmenu", "Open in Other Pane"),
                                                context,
                                                [open, url] {
                                                    open(url, true);
                                                });
            other->setEnabled(otherPaneAvailable);
        }
    }
    if (accounts.isEmpty() && error.isEmpty()) {
        QAction *none = menu->addAction(i18nc("@action:inmenu", "No accounts to show"));
        none->setEnabled(false);
        none->setToolTip(i18nc("@info:tooltip",
                               "Add accounts with file storage in KDE's Online Accounts settings, and choose the accounts to show under "
                               "Preferences › KDE Plasma › Online Accounts."));
    }
    if (!error.isEmpty()) {
        QAction *problem = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")), menuText(error));
        problem->setEnabled(false);
    }
    if (manage) {
        menu->addSeparator();
        menu->addAction(manage);
    }
}
}
