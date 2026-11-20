// SPDX-License-Identifier: GPL-3.0-or-later

#include "interfacelanguage.h"

#include <QLocale>

Qt::LayoutDirection InterfaceLanguage::layoutDirection(const QStringList &languages)
{
    return languages.isEmpty() ? QLocale::system().textDirection() : QLocale(languages.first()).textDirection();
}
