// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudmenu.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QAction>
#include <QFileInfo>
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
    if (handlers.setFavorite && favorite != true) {
        QAction *add =
            submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("starred")), i18nc("@action:inmenu", "Add to Nextcloud Favourites"), context, [handlers] {
                handlers.setFavorite(true);
            });
        add->setObjectName(QStringLiteral("nextcloudAddFavorite"));
        add->setToolTip(i18nc("@info:tooltip", "Mark as a favourite on the Nextcloud server, separate from the Places panel"));
    }
    if (handlers.setFavorite && favorite != false) {
        QAction *remove = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("non-starred")),
                                             i18nc("@action:inmenu", "Remove from Nextcloud Favourites"),
                                             context,
                                             [handlers] {
                                                 handlers.setFavorite(false);
                                             });
        remove->setObjectName(QStringLiteral("nextcloudRemoveFavorite"));
    }
    if (handlers.editTags) {
        QAction *tags =
            submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("tag")), i18nc("@action:inmenu", "Nextcloud Tags…"), context, handlers.editTags);
        tags->setObjectName(QStringLiteral("nextcloudEditTags"));
        tags->setToolTip(i18nc("@info:tooltip", "Assign the server's collaborative tags, which are separate from Plasma Opus tags"));
    }
    if (!submenu->isEmpty()) {
        submenu->addSeparator();
    }
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

void populateDesktopClient(QMenu *menu,
                           const DesktopClient &client,
                           const QList<NextcloudAccounts::Account> &accounts,
                           bool otherPaneAvailable,
                           QObject *context,
                           const DesktopHandlers &handlers)
{
    using namespace NextcloudDesktop;
    // clear() leaves submenus behind, and the menu is filled again whenever the status changes.
    const QList<QMenu *> folderMenus = menu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly);
    menu->clear();
    qDeleteAll(folderMenus);
    menu->setToolTipsVisible(true);
    QAction *open =
        menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-sync")), i18nc("@action:inmenu", "Open Desktop Client"), context, handlers.openClient);
    open->setObjectName(QStringLiteral("nextcloudOpenDesktopClient"));
    open->setEnabled(client.installed && handlers.openClient);
    open->setToolTip(client.installed ? i18nc("@info:tooltip", "Show the Nextcloud desktop client, which synchronizes these folders")
                                      : i18nc("@info:tooltip", "The Nextcloud desktop client isn't installed."));

    const bool checked = client.statusEnabled && client.statusChecked;
    QAction *status = nullptr;
    if (client.statusEnabled && !client.statusChecked) {
        status = menu->addAction(i18nc("@info:status", "Checking synchronization status…"));
    } else if (checked && !client.running) {
        status = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("state-offline")), i18nc("@info:status", "Desktop client not running"));
        status->setToolTip(i18nc("@info:tooltip", "Synchronization status is unavailable, and changes aren't synchronized until the desktop client starts."));
    }
    if (status) {
        status->setObjectName(QStringLiteral("nextcloudDesktopStatus"));
        status->setEnabled(false);
    }
    if (!client.configuration.error.isEmpty()) {
        QAction *problem = menu->addAction(SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")), menuText(client.configuration.error));
        problem->setEnabled(false);
    }
    menu->addSeparator();
    if (client.configuration.folders.isEmpty()) {
        QAction *none = menu->addAction(i18nc("@action:inmenu", "No synchronized folders"));
        none->setEnabled(false);
        none->setToolTip(i18nc("@info:tooltip", "Add folders to synchronize in the Nextcloud desktop client."));
    }
    for (const Folder &folder : client.configuration.folders) {
        const bool exists = QFileInfo(folder.localPath).isDir();
        const auto reported = client.statuses.constFind(folder.localPath);
        const FolderState state =
            folderState(folder, exists, client.running, reported == client.statuses.cend() ? std::nullopt : std::optional<QString>(*reported));
        QString name = QFileInfo(folder.localPath).fileName();
        if (name.isEmpty()) {
            name = folder.localPath;
        }
        // While the status is being checked, or when it isn't, nothing is claimed about the folder.
        QString title = name;
        if (checked) {
            title = state.sync == Sync::Unknown || state.sync == Sync::NotSynchronized
                ? i18nc("@action:inmenu %1 is a folder name, %2 where its files are", "%1 (%2)", name, availabilityText(state.availability))
                : i18nc("@action:inmenu %1 is a folder name, %2 where its files are, %3 its sync status",
                        "%1 (%2, %3)",
                        name,
                        availabilityText(state.availability),
                        syncText(state.sync));
        }
        QString icon = QStringLiteral("folder-sync");
        if (checked) {
            switch (state.sync) {
            case Sync::UpToDate:
                icon = QStringLiteral("state-ok");
                break;
            case Sync::Syncing:
                icon = QStringLiteral("state-sync");
                break;
            case Sync::Warning:
                icon = QStringLiteral("state-warning");
                break;
            case Sync::Error:
                icon = QStringLiteral("state-error");
                break;
            case Sync::Unknown:
            case Sync::NotSynchronized:
                icon = state.availability == Availability::Offline ? QStringLiteral("state-offline") : QStringLiteral("state-information");
                break;
            }
        }
        QMenu *submenu = menu->addMenu(SymbolicIcon::fromTheme(icon), menuText(title));
        submenu->setObjectName(QStringLiteral("nextcloudDesktopFolder-%1-%2").arg(folder.accountId, folder.id));
        submenu->setToolTipsVisible(true);
        const std::optional<NextcloudAccounts::Account> account = accountFor(folder, accounts);
        QStringList tooltip{folder.localPath};
        if (!folder.server.isEmpty()) {
            QString server = folder.server.toDisplayString();
            if (!folder.user.isEmpty()) {
                server = i18nc("@info:tooltip %1 is a user, %2 a server", "%1 on %2", folder.user, server);
            }
            tooltip.append(i18nc("@info:tooltip %1 is a folder on the server, %2 the account",
                                 "Synchronized with %1 of %2",
                                 folder.targetPath.isEmpty() ? QStringLiteral("/") : folder.targetPath,
                                 server));
        }
        if (checked) {
            tooltip.append(state.description);
        } else if (client.statusEnabled) {
            tooltip.append(i18nc("@info:tooltip", "Checking the desktop client's status…"));
        } else {
            tooltip.append(i18nc("@info:tooltip", "The desktop client isn't asked for the synchronization status, as set in Preferences."));
        }
        submenu->menuAction()->setToolTip(tooltip.join(QLatin1Char('\n')));

        QAction *openFolder = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")),
                                                 i18nc("@action:inmenu", "Open"),
                                                 context,
                                                 [handler = handlers.openFolder, path = folder.localPath] {
                                                     handler(path, false);
                                                 });
        openFolder->setObjectName(QStringLiteral("nextcloudDesktopOpen"));
        openFolder->setEnabled(exists && handlers.openFolder);
        openFolder->setToolTip(exists ? i18nc("@info:tooltip", "Open the folder on this computer")
                                      : i18nc("@info:tooltip", "The folder isn't on this computer."));
        QAction *other = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-split-left-right")),
                                            i18nc("@action:inmenu", "Open in Other Pane"),
                                            context,
                                            [handler = handlers.openFolder, path = folder.localPath] {
                                                handler(path, true);
                                            });
        other->setObjectName(QStringLiteral("nextcloudDesktopOpenOther"));
        other->setEnabled(exists && otherPaneAvailable && handlers.openFolder);
        if (folder.server.isEmpty()) {
            continue;
        }
        QAction *server = submenu->addAction(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")), i18nc("@action:inmenu", "Open on Server"));
        server->setObjectName(QStringLiteral("nextcloudDesktopOpenServer"));
        if (account && handlers.openServerFolder) {
            const QUrl url = serverFolder(*account, folder);
            QObject::connect(server, &QAction::triggered, context, [handler = handlers.openServerFolder, url] {
                handler(url);
            });
            server->setToolTip(
                i18nc("@info:tooltip %1 is an account", "Browse the folder on the server with the Nextcloud account %1", NextcloudAccounts::title(*account)));
        } else {
            server->setEnabled(false);
            server->setToolTip(i18nc("@info:tooltip %1 is a user, %2 a server",
                                     "Sign in as %1 to %2 with Manage Nextcloud Accounts to browse the folder on the server.",
                                     folder.user,
                                     folder.server.toDisplayString()));
        }
    }
}

KService::Ptr desktopClientService()
{
    for (const QString &name : {QStringLiteral("com.nextcloud.desktopclient.nextcloud"), QStringLiteral("nextcloud"), QStringLiteral("Nextcloud")}) {
        if (KService::Ptr service = KService::serviceByDesktopName(name)) {
            return service;
        }
    }
    return {};
}
}
