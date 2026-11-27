// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "onlineaccounts.h"

#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

class QAction;
class QMenu;
class QObject;

/// Fills the Go › Online Accounts menu.
namespace OnlineAccountsMenu
{
/// Opens @p url in the active pane, or in the other pane when @p otherPane is
/// true.
using OpenHandler = std::function<void(const QUrl &url, bool otherPane)>;

/// Replaces the contents of @p menu with a section per account in
/// @p accounts, the accounts chosen to be shown, and a submenu per folder each
/// account gives access to.
/// Unavailable folders are disabled and explain why in their tooltip. A
/// non-empty @p error is shown as a disabled entry. The menu ends with
/// @p manage when it is not null. Open actions call @p open with @p context
/// as their receiver.
void populate(QMenu *menu,
              const QList<OnlineAccounts::Account> &accounts,
              const QString &error,
              const OnlineAccounts::ProtocolCheck &isKnownProtocol,
              bool otherPaneAvailable,
              QAction *manage,
              QObject *context,
              const OpenHandler &open);
}
