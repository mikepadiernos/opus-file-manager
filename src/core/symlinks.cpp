// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlinks.h"

#include <QFileInfo>

QUrl Symlinks::resolve(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return url;
    }
    const QString canonical = QFileInfo(url.toLocalFile()).canonicalFilePath();
    return canonical.isEmpty() ? url : QUrl::fromLocalFile(canonical);
}
