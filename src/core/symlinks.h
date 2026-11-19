// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>

namespace KIO
{
class SimpleJob;
}

namespace Symlinks
{
/**
 * Resolves every symbolic link in a local folder path.
 * @param url the folder to open
 * @returns the canonical location for a local path that exists, otherwise
 *          @p url unchanged
 */
QUrl resolve(const QUrl &url);

/**
 * Creates a link beside an existing local target without overwriting.
 * Returns a KIO job, or sets error when the request is invalid.
 */
KIO::SimpleJob *create(const QUrl &target, const QString &name, QString &error);
}
