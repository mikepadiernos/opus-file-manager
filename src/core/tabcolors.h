// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "labelcolor.h"

#include <KConfigGroup>

#include <QHash>
#include <QObject>
#include <QUrl>

/**
 * The colour of tabs showing each folder, shared by all tabs of all panes.
 * Every change is written to the KConfig group given to the constructor.
 */
class TabColors : public QObject
{
    Q_OBJECT

public:
    /**
     * Loads the colours stored in @p group.
     * @param group where colours are read from and written to
     * @param parent the QObject parent
     */
    explicit TabColors(const KConfigGroup &group, QObject *parent = nullptr);

    /// The colour for tabs showing @p url, or None.
    LabelColor colorFor(const QUrl &url) const;

    /**
     * Sets the colour for tabs showing @p url; None removes it.
     * @param url the folder
     * @param color the new colour
     */
    void setColor(const QUrl &url, LabelColor color);

Q_SIGNALS:
    void colorChanged(const QUrl &url);

private:
    KConfigGroup m_group;
    QHash<QString, LabelColor> m_colors;
};
