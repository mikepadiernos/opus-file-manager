// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>

/**
 * Administrator mode through KIO's admin: worker from kio-admin, as in
 * Dolphin's Act as Administrator. An admin: URL names the same local path as
 * its file: URL; the worker asks Polkit for authorization and performs every
 * operation as root.
 */
namespace AdminMode
{
/// Whether KIO knows the admin: protocol, so kio-admin is installed.
bool isAvailable();

/// Whether @p url is listed through the admin: worker.
bool isAdminUrl(const QUrl &url);

/// Whether @p url can switch into or out of administrator mode.
bool canToggle(const QUrl &url);

/// The admin: URL for a local @p url, @p url itself when already admin:, or an empty URL otherwise.
QUrl toAdmin(const QUrl &url);

/// The file: URL for an admin: @p url, or @p url unchanged otherwise.
QUrl toUser(const QUrl &url);
}
