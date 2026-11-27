// SPDX-License-Identifier: GPL-3.0-or-later

#include "adminmode.h"

#include <KProtocolInfo>

namespace
{
const QString AdminScheme = QStringLiteral("admin");
}

bool AdminMode::isAvailable()
{
    return KProtocolInfo::isKnownProtocol(AdminScheme);
}

bool AdminMode::isAdminUrl(const QUrl &url)
{
    return url.scheme() == AdminScheme;
}

bool AdminMode::canToggle(const QUrl &url)
{
    return url.isLocalFile() || isAdminUrl(url);
}

QUrl AdminMode::toAdmin(const QUrl &url)
{
    if (isAdminUrl(url)) {
        return url;
    }
    if (!url.isLocalFile()) {
        return {};
    }
    QUrl admin = url;
    admin.setScheme(AdminScheme);
    return admin;
}

QUrl AdminMode::toUser(const QUrl &url)
{
    if (!isAdminUrl(url)) {
        return url;
    }
    QUrl user = url;
    user.setScheme(QStringLiteral("file"));
    user.setHost(QString());
    return user;
}
