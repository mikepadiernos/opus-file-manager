// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>
#include <Qt>

namespace InterfaceLanguage
{
bool apply(const QString &language);
Qt::LayoutDirection layoutDirection(const QStringList &languages);
Qt::LayoutDirection layoutDirection(const QStringList &languages, int overrideDirection);
}
