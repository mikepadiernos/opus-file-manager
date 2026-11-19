// SPDX-License-Identifier: GPL-3.0-or-later

#include "archives.h"

#include <KProtocolManager>

#include <QMimeDatabase>

QUrl Archives::browseUrl(const QUrl &file, const QString &mimeType)
{
    if (!file.isLocalFile()) {
        return {};
    }
    const QString protocol = KProtocolManager::protocolForArchiveMimetype(mimeType);
    if (protocol.isEmpty()) {
        return {};
    }
    QUrl url;
    url.setScheme(protocol);
    url.setPath(file.toLocalFile() + u'/');
    return url;
}

QString Archives::baseName(const QString &fileName)
{
    const QString suffix = QMimeDatabase().suffixForFileName(fileName);
    return suffix.isEmpty() ? fileName : fileName.chopped(suffix.size() + 1);
}
