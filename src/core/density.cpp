// SPDX-License-Identifier: GPL-3.0-or-later

#include "density.h"

#include <QtGlobal>

int Density::itemSpace(Items density, int customSpace)
{
    switch (density) {
    case Items::Normal:
        return NormalItemSpace;
    case Items::Compact:
        return 1;
    case Items::VeryCompact:
        return 0;
    case Items::Custom:
        return qBound(0, customSpace, MaxCustomItemSpace);
    }
    return NormalItemSpace;
}

int Density::barMargin(Bars density)
{
    switch (density) {
    case Bars::Normal:
        return -1;
    case Bars::Compact:
        return 2;
    case Bars::VeryCompact:
        return 0;
    }
    return -1;
}
