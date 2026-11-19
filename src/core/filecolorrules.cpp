// SPDX-License-Identifier: GPL-3.0-or-later

#include "filecolorrules.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace
{
const QRegularExpression ExtensionPattern(QStringLiteral("^[a-z0-9][a-z0-9._+-]{0,31}$"), QRegularExpression::CaseInsensitiveOption);
}

FileColorRules FileColorRules::fromConfig(const KConfigGroup &group)
{
    return FileColorRules(group.readEntry("ExtensionColors", QStringList()));
}

FileColorRules::FileColorRules(const QStringList &entries)
{
    for (const QString &entry : entries) {
        const qsizetype separator = entry.indexOf(u'=');
        if (separator <= 0) {
            continue;
        }
        QString extension = entry.first(separator).trimmed();
        if (extension.startsWith(u'.')) {
            extension.remove(0, 1);
        }
        extension = extension.toCaseFolded();
        const QColor color(entry.sliced(separator + 1).trimmed());
        if (ExtensionPattern.match(extension).hasMatch() && color.isValid()) {
            m_colors.insert(extension, color);
        }
    }
}

QColor FileColorRules::colorForFileName(const QString &fileName) const
{
    const QFileInfo info(fileName);
    const QString completeSuffix = info.completeSuffix().toCaseFolded();
    const auto complete = m_colors.constFind(completeSuffix);
    if (complete != m_colors.cend()) {
        return complete.value();
    }
    const auto suffix = m_colors.constFind(info.suffix().toCaseFolded());
    return suffix == m_colors.cend() ? QColor() : suffix.value();
}

QStringList FileColorRules::entries() const
{
    QStringList result;
    for (auto it = m_colors.cbegin(); it != m_colors.cend(); ++it) {
        result.append(it.key() + u'=' + it.value().name(QColor::HexRgb));
    }
    return result;
}
