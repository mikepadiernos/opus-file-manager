// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QUrl>

namespace LinkedNavigation
{
struct Result {
    QUrl target;
    QString error;
};

Result target(const QUrl &sourceRoot, const QUrl &otherRoot, const QUrl &location);
}
