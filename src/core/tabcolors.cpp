// SPDX-License-Identifier: GPL-3.0-or-later

#include "tabcolors.h"

namespace
{
constexpr auto UrlsKey = "Urls";
constexpr auto ColorsKey = "Colors";

QString keyFor(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash).toString(QUrl::FullyEncoded);
}
}

TabColors::TabColors(const KConfigGroup &group, QObject *parent)
    : QObject(parent)
    , m_group(group)
{
    const QStringList urls = m_group.readEntry(UrlsKey, QStringList());
    const QStringList colors = m_group.readEntry(ColorsKey, QStringList());
    for (int i = 0; i < urls.size() && i < colors.size(); ++i) {
        const LabelColor color = LabelColors::fromKey(colors.at(i));
        if (color != LabelColor::None) {
            m_colors.insert(urls.at(i), color);
        }
    }
}

LabelColor TabColors::colorFor(const QUrl &url) const
{
    return m_colors.value(keyFor(url), LabelColor::None);
}

void TabColors::setColor(const QUrl &url, LabelColor color)
{
    if (colorFor(url) == color) {
        return;
    }
    if (color == LabelColor::None) {
        m_colors.remove(keyFor(url));
    } else {
        m_colors.insert(keyFor(url), color);
    }

    if (m_colors.isEmpty()) {
        m_group.deleteEntry(UrlsKey);
        m_group.deleteEntry(ColorsKey);
    } else {
        QStringList urls;
        QStringList colors;
        for (auto it = m_colors.cbegin(); it != m_colors.cend(); ++it) {
            urls.append(it.key());
            colors.append(LabelColors::key(it.value()));
        }
        m_group.writeEntry(UrlsKey, urls);
        m_group.writeEntry(ColorsKey, colors);
    }
    m_group.sync();
    Q_EMIT colorChanged(url);
}
