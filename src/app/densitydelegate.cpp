// SPDX-License-Identifier: GPL-3.0-or-later

#include "densitydelegate.h"

#include "density.h"

#include <QtGlobal>

int DensityDelegate::itemSpace() const
{
    return m_itemSpace;
}

void DensityDelegate::setItemSpace(int space)
{
    m_itemSpace = space;
}

QSize DensityDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = KFileItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(1, size.height() + 2 * (m_itemSpace - Density::NormalItemSpace)));
    return size;
}
