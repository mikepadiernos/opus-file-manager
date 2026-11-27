// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudmenu.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QAction>
#include <QMenu>

namespace
{
QString menuText(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}
}

namespace NextcloudMenu
{
void populate(QMenu *menu,
              const QList<NextcloudAccounts::Account> &accounts,
              const QString &error,
              const QString &unavailableReason,
              bool otherPaneAvailable,
              QAction *manage,
              QObject *context,
              const OpenHandler &open)
{
    menu->clear();
    menu->setToolTipsVisible(true);
    for (const NextcloudAccounts::Account &account : accounts) {
        const QIcon icon = SymbolicIcon::fromTheme(QStringLiteral("folder-cloud"));
        QString title = NextcloudAccounts::title(account);
        if (account.source == NextcloudAccounts::Source::OnlineAccounts) {
            title = i18nc("@action:inmenu %1 is an account name", "%1 (KDE Online Accounts)", title);
        }
        if (!unavailableReason.isEmpty()) {
            QAction *unavailable = menu->addAction(icon, menuText(title));
            unavailable->setEnabled(false);
            unavailable->setToolTip(unavailableReason);
            continue;
        }
        QMenu *submenu = menu->addMenu(icon, menuText(title));
        submenu->setObjectName(QStringLiteral("nextcloudAccount-") + account.id);
        submenu->setToolTipsVisible(true);
        submenu->menuAction()->setToolTip(NextcloudAccounts::filesUrl(account).toDisplayString());
        submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:inmenu", "Open"), context, [open, account] {
            open(account, false);
        });
        QAction *other = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-split-left-right")),
                                            i18nc("@action:inmenu", "Open in Other Pane"),
                                            context,
                                            [open, account] {
                                                open(account, true);
                                            });
        other->setEnabled(otherPaneAvailable);
    }
    if (accounts.isEmpty()) {
        QAction *none = menu->addAction(i18nc("@action:inmenu", "No Nextcloud accounts"));
        none->setEnabled(false);
        none->setToolTip(i18nc("@info:tooltip", "Sign in to a Nextcloud server with Manage Nextcloud Accounts, or add one in KDE's Online Accounts settings."));
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
