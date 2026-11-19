// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QRectF>

namespace Treemap
{
QList<QRectF> layout(const QList<quint64> &weights, const QRectF &bounds);
}
