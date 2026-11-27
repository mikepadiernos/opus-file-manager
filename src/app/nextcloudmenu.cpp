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
              const OpenHandler &open,
              const OrganizeHandler &organize,
              const AccountHandlers &more)
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
        if (organize) {
            submenu->addSeparator();
            QAction *search = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("system-search")),
                                                 i18nc("@action:inmenu", "Search and Organize…"),
                                                 context,
                                                 [organize, account] {
                                                     organize(account);
                                                 });
            search->setObjectName(QStringLiteral("nextcloudOrganize"));
            search->setToolTip(i18nc("@info:tooltip", "Search the server and list your favourites and files with a Nextcloud tag"));
        }
        if (more.photos) {
            if (!organize) {
                submenu->addSeparator();
            }
            QAction *photos = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-pictures")),
                                                 i18nc("@action:inmenu", "Photos…"),
                                                 context,
                                                 [handler = more.photos, account] {
                                                     handler(account);
                                                 });
            photos->setObjectName(QStringLiteral("nextcloudPhotos"));
            photos->setToolTip(i18nc("@info:tooltip", "Browse the albums, shared albums and places of Nextcloud Photos"));
        }
        if (more.shares) {
            if (!organize && !more.photos) {
                submenu->addSeparator();
            }
            QAction *shares = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-publicshare")),
                                                 i18nc("@action:inmenu", "Shares…"),
                                                 context,
                                                 [handler = more.shares, account] {
                                                     handler(account);
                                                 });
            shares->setObjectName(QStringLiteral("nextcloudShares"));
            shares->setToolTip(i18nc("@info:tooltip", "List what you shared and what others shared with you, and change or revoke your shares"));
        }
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

QMenu *addItemMenu(QMenu *menu, std::optional<bool> favorite, QObject *context, const ItemHandlers &handlers)
{
    QMenu *submenu = menu->addMenu(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")), i18nc("@menu", "Nextcloud"));
    submenu->setObjectName(QStringLiteral("nextcloudItemMenu"));
    submenu->setToolTipsVisible(true);
    if (favorite != true) {
        QAction *add =
            submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("starred")), i18nc("@action:inmenu", "Add to Nextcloud Favourites"), context, [handlers] {
                handlers.setFavorite(true);
            });
        add->setObjectName(QStringLiteral("nextcloudAddFavorite"));
        add->setToolTip(i18nc("@info:tooltip", "Mark as a favourite on the Nextcloud server, separate from the Places panel"));
    }
    if (favorite != false) {
        QAction *remove = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("non-starred")),
                                             i18nc("@action:inmenu", "Remove from Nextcloud Favourites"),
                                             context,
                                             [handlers] {
                                                 handlers.setFavorite(false);
                                             });
        remove->setObjectName(QStringLiteral("nextcloudRemoveFavorite"));
    }
    QAction *tags = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("tag")), i18nc("@action:inmenu", "Nextcloud Tags…"), context, handlers.editTags);
    tags->setObjectName(QStringLiteral("nextcloudEditTags"));
    tags->setToolTip(i18nc("@info:tooltip", "Assign the server's collaborative tags, which are separate from Plasma Opus tags"));
    submenu->addSeparator();
    QAction *organize = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("system-search")),
                                           i18nc("@action:inmenu", "Search and Organize…"),
                                           context,
                                           handlers.organize);
    organize->setObjectName(QStringLiteral("nextcloudItemOrganize"));
    if (handlers.addToAlbum) {
        QAction *album = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-pictures")),
                                            i18nc("@action:inmenu", "Add to Nextcloud Album…"),
                                            context,
                                            handlers.addToAlbum);
        album->setObjectName(QStringLiteral("nextcloudAddToAlbum"));
        album->setToolTip(i18nc("@info:tooltip", "List the files in a Nextcloud Photos album; the files stay where they are"));
    }
    if (handlers.shares) {
        QAction *shares = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-publicshare")),
                                             i18nc("@action:inmenu", "Nextcloud Shares…"),
                                             context,
                                             handlers.shares);
        shares->setObjectName(QStringLiteral("nextcloudItemShares"));
        shares->setToolTip(i18nc("@info:tooltip", "Share with a public link, a user or a group, and change or revoke the item's shares"));
    }
    return submenu;
}
}
