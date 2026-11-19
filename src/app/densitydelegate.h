// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "density.h"

#include <KFileItemDelegate>

/**
 * KFileItemDelegate whose rows have a chosen space above and below the
 * item instead of the default Density::NormalItemSpace.
 */
class DensityDelegate : public KFileItemDelegate
{
    Q_OBJECT

public:
    using KFileItemDelegate::KFileItemDelegate;

    int itemSpace() const;
    /// @param space pixels above and below each item
    void setItemSpace(int space);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    int m_itemSpace = Density::NormalItemSpace;
};
