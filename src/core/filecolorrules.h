// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QColor>
#include <QMap>
#include <QStringList>

class FileColorRules
{
public:
    static FileColorRules fromConfig(const KConfigGroup &group);

    explicit FileColorRules(const QStringList &entries = {});

    QColor colorForFileName(const QString &fileName) const;
    QStringList entries() const;

private:
    QMap<QString, QColor> m_colors;
};
