// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QUrl>

class QWidget;

namespace BatchConversionDialog
{
void run(QWidget *parent, const QList<QUrl> &sources);
}
