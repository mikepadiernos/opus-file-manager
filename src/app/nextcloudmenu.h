// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudaccount.h"

#include <QList>
#include <QString>

#include <functional>
#include <optional>

class QAction;
class QMenu;
class QObject;

/// Fills the Go › Nextcloud menu.
namespace NextcloudMenu
{
/// Opens the files of @p account in the active pane, or in the other pane
/// when @p otherPane is true.
using OpenHandler = std::function<void(const NextcloudAccounts::Account &account, bool otherPane)>;
/// Opens the Search and Organize dialog of @p account.
using OrganizeHandler = std::function<void(const NextcloudAccounts::Account &account)>;

/// What the Nextcloud submenu of a file's context menu does.
struct ItemHandlers {
    /// Marks the items as favourites on the server, or no longer.
    std::function<void(bool favorite)> setFavorite;
    /// Edits the items' collaborative tags on the server.
    std::function<void()> editTags;
    /// Opens the account's Search and Organize dialog.
    std::function<void()> organize;
    /// Adds the items to a Nextcloud Photos album; offered only when set.
    std::function<void()> addToAlbum;
    /// Lists and manages the Nextcloud shares of the item; offered only when set.
    std::function<void()> shares;
};

/// What else each account's submenu offers; an entry is shown only when its handler is set.
struct AccountHandlers {
    /// Opens the account's Nextcloud Photos albums, shared albums and places.
    OrganizeHandler photos;
    /// Lists and manages the account's Nextcloud shares.
    OrganizeHandler shares;
};

/// Replaces the contents of @p menu with a submenu per account in
/// @p accounts that opens its files. When @p unavailableReason isn't empty,
/// the accounts are disabled and explain it in their tooltip. A non-empty
/// @p error is shown as a disabled entry. The menu ends with @p manage when
/// it is not null. Open actions call @p open with @p context as their
/// receiver. When @p organize is set, each account also offers Search and
/// Organize, followed by the entries of @p more.
void populate(QMenu *menu,
              const QList<NextcloudAccounts::Account> &accounts,
              const QString &error,
              const QString &unavailableReason,
              bool otherPaneAvailable,
              QAction *manage,
              QObject *context,
              const OpenHandler &open,
              const OrganizeHandler &organize = {},
              const AccountHandlers &more = {});

/**
 * Adds a Nextcloud submenu to the context menu of items on a Nextcloud
 * server. @p favorite is whether all the items are favourites, or nullopt
 * when it's unknown or differs, which offers both adding and removing.
 * @return the submenu
 */
QMenu *addItemMenu(QMenu *menu, std::optional<bool> favorite, QObject *context, const ItemHandlers &handlers);
}
