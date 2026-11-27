// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudaccount.h"

#include <QList>
#include <QString>

#include <functional>

class QAction;
class QMenu;
class QObject;

/// Fills the Go › Nextcloud menu.
namespace NextcloudMenu
{
/// Opens the files of @p account in the active pane, or in the other pane
/// when @p otherPane is true.
using OpenHandler = std::function<void(const NextcloudAccounts::Account &account, bool otherPane)>;

/// Replaces the contents of @p menu with a submenu per account in
/// @p accounts that opens its files. When @p unavailableReason isn't empty,
/// the accounts are disabled and explain it in their tooltip. A non-empty
/// @p error is shown as a disabled entry. The menu ends with @p manage when
/// it is not null. Open actions call @p open with @p context as their
/// receiver.
void populate(QMenu *menu,
              const QList<NextcloudAccounts::Account> &accounts,
              const QString &error,
              const QString &unavailableReason,
              bool otherPaneAvailable,
              QAction *manage,
              QObject *context,
              const OpenHandler &open);
}
