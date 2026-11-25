// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldercolors.h"
#include "effectiveappearance.h"

#include "colorschemes.h"
#include "foldercolorrules.h"
#include "foldermetadata.h"
#include "labelcolor.h"
#include "settings.h"
#include "symbolicicon.h"

#include <KFileItem>
#include <KIconUtils>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace
{
struct CachedColor {
    QColor color;
    qint64 read = 0;
};

constexpr qint64 CacheLifetime = 2000;
constexpr qsizetype CacheLimit = 2000;
/// Strength of the tint on full colour icons and icons with emblems.
constexpr qreal PartialTint = 0.5;

QHash<QString, CachedColor> &colorCache()
{
    static QHash<QString, CachedColor> cache;
    return cache;
}

quint64 &generation()
{
    static quint64 value = 0;
    return value;
}

/// The configured folder colour rules, read again after FolderColors::clearCache().
const FolderColorRules &configuredRules(bool reload = false)
{
    static FolderColorRules rules = FolderColorRules::configured();
    if (reload) {
        rules = FolderColorRules::configured();
    }
    return rules;
}

qint64 now()
{
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    return clock.elapsed();
}

bool isPlainFolder(const QString &iconName)
{
    return iconName == QLatin1String("folder") || iconName == QLatin1String("inode-directory");
}

LabelColor nearestLabel(const QColor &color)
{
    LabelColor nearest = LabelColor::None;
    double best = 0;
    for (LabelColor label : LabelColors::all()) {
        const QColor rgb = LabelColors::rgb(label);
        const double distance = std::pow(rgb.redF() - color.redF(), 2) + std::pow(rgb.greenF() - color.greenF(), 2) + std::pow(rgb.blueF() - color.blueF(), 2);
        if (nearest == LabelColor::None || distance < best) {
            nearest = label;
            best = distance;
        }
    }
    return nearest;
}

QStringList variantNames(LabelColor label)
{
    switch (label) {
    case LabelColor::Red:
        return {QStringLiteral("folder-red")};
    case LabelColor::Orange:
        return {QStringLiteral("folder-orange")};
    case LabelColor::Yellow:
        return {QStringLiteral("folder-yellow")};
    case LabelColor::Green:
        return {QStringLiteral("folder-green")};
    case LabelColor::Blue:
        return {QStringLiteral("folder-blue")};
    case LabelColor::Purple:
        return {QStringLiteral("folder-violet"), QStringLiteral("folder-purple")};
    case LabelColor::Pink:
        return {QStringLiteral("folder-magenta"), QStringLiteral("folder-pink")};
    case LabelColor::Gray:
        return {QStringLiteral("folder-grey"), QStringLiteral("folder-gray")};
    case LabelColor::None:
        break;
    }
    return {};
}

/// Whether @p icon is drawn from the icon theme rather than being a preview.
bool isThemeIcon(const QIcon &icon, const QString &iconName, const QStringList &overlays)
{
    if (!icon.name().isEmpty()) {
        return true;
    }
    if (iconName.isEmpty() || overlays.isEmpty()) {
        return false;
    }
    // Above the symbolic size, wrapped icons draw the regular icon with its emblems.
    constexpr int Size = SymbolicIcon::MaxSize + 8;
    return icon.pixmap(Size).toImage() == KIconUtils::addOverlays(QIcon::fromTheme(iconName), overlays).pixmap(Size).toImage();
}

QColor outlineFor(const QColor &color)
{
    return qGray(color.rgb()) > 150 ? QColor(35, 38, 41, 200) : QColor(255, 255, 255, 220);
}

void drawDot(QPainter *painter, const QRectF &rect, const QColor &color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal outline = std::max(1.0, rect.width() / 8);
    painter->setPen(QPen(outlineFor(color), outline));
    painter->setBrush(color);
    painter->drawEllipse(rect.adjusted(outline / 2, outline / 2, -outline / 2, -outline / 2));
    painter->restore();
}

/// Draws a folder icon with its colour, or a colour dot when the icon is null.
class FolderColorEngine : public QIconEngine
{
public:
    FolderColorEngine(const QIcon &icon,
                      const QIcon &variant,
                      const QString &cacheName,
                      const QColor &color,
                      FolderColors::Style style,
                      bool symbolic,
                      bool emblems)
        : m_icon(icon)
        , m_variant(variant)
        , m_cacheName(cacheName)
        , m_color(color)
        , m_style(style)
        , m_symbolic(symbolic)
        , m_emblems(emblems)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
    }

    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return m_icon.isNull() ? size : m_icon.actualSize(size, mode, state);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        // Symbolic icons follow the palette, so cached pixmaps are keyed by it and the theme.
        const QString key = QStringLiteral("opusfm-folder-color:%1:%2:%3:%4:%5:%6:%7x%8@%9:%10:%11")
                                .arg(QString::number(generation()), QString::number(QGuiApplication::palette().cacheKey()), QIcon::themeName())
                                .arg(m_cacheName, m_color.name(QColor::HexArgb))
                                .arg(int(m_style))
                                .arg(size.width())
                                .arg(size.height())
                                .arg(scale)
                                .arg(int(mode))
                                .arg(int(state));
        QPixmap result;
        if (!m_cacheName.isEmpty() && QPixmapCache::find(key, &result)) {
            return result;
        }
        result = render(size, mode, state, scale);
        if (!m_cacheName.isEmpty()) {
            QPixmapCache::insert(key, result);
        }
        return result;
    }

    QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override
    {
        return m_icon.isNull() ? QList<QSize>() : m_icon.availableSizes(mode, state);
    }

    QString iconName() override
    {
        return m_icon.name();
    }

    bool isNull() override
    {
        return m_icon.isNull() && !m_color.isValid();
    }

    QString key() const override
    {
        return QStringLiteral("FolderColorEngine");
    }

    QIconEngine *clone() const override
    {
        return new FolderColorEngine(m_icon, m_variant, m_cacheName, m_color, m_style, m_symbolic, m_emblems);
    }

private:
    QPixmap render(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) const
    {
        const bool small = std::max(size.width(), size.height()) <= SymbolicIcon::MaxSize;
        if (m_icon.isNull()) {
            QPixmap dot(size * scale);
            dot.setDevicePixelRatio(scale);
            dot.fill(Qt::transparent);
            QPainter painter(&dot);
            const qreal edge = std::min(size.width(), size.height()) * 0.6;
            drawDot(&painter, QRectF((size.width() - edge) / 2, (size.height() - edge) / 2, edge, edge), m_color);
            return dot;
        }
        if (m_style == FolderColors::Style::Icon && !m_variant.isNull() && !(small && m_symbolic)) {
            return m_variant.pixmap(size, scale, mode, state);
        }
        QPixmap pixmap = m_icon.pixmap(size, scale, mode, state);
        if (pixmap.isNull()) {
            return pixmap;
        }
        const QSizeF logical = pixmap.deviceIndependentSize();
        QPainter painter(&pixmap);
        if (m_style == FolderColors::Style::Badge) {
            const qreal edge = std::max(4.0, std::round(std::min(logical.width(), logical.height()) * 0.4));
            drawDot(&painter, QRectF(logical.width() - edge, logical.height() - edge, edge, edge), m_color);
            return pixmap;
        }
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        painter.setOpacity(small && m_symbolic && !m_emblems ? 1.0 : PartialTint);
        painter.fillRect(QRectF(QPointF(), logical), m_color);
        return pixmap;
    }

    QIcon m_icon;
    QIcon m_variant;
    QString m_cacheName;
    QColor m_color;
    FolderColors::Style m_style;
    bool m_symbolic;
    bool m_emblems;
};
}

FolderColors::Style FolderColors::configured()
{
    switch (EffectiveAppearance::folderColorStyle()) {
    case Settings::EnumFolderColorStyle::Badge:
        return Style::Badge;
    case Settings::EnumFolderColorStyle::Name:
        return Style::Name;
    case Settings::EnumFolderColorStyle::Hidden:
        return Style::Hidden;
    default:
        return Style::Icon;
    }
}

QColor FolderColors::color(const QString &localPath)
{
    if (localPath.isEmpty()) {
        return {};
    }
    QHash<QString, CachedColor> &cache = colorCache();
    const qint64 time = now();
    if (const auto found = cache.constFind(localPath); found != cache.constEnd() && time - found->read < CacheLifetime) {
        return found->color;
    }
    if (cache.size() > CacheLimit) {
        cache.clear();
    }
    const QColor explicitColor = QColor::fromString(FolderMetadata::read(localPath).value(QStringLiteral("color")).toString());
    const QColor color = explicitColor.isValid() ? explicitColor : configuredRules().colorFor(localPath);
    cache.insert(localPath, {color, time});
    return color;
}

QColor FolderColors::color(const QUrl &url)
{
    return url.isLocalFile() ? color(url.toLocalFile()) : QColor();
}

void FolderColors::clearCache()
{
    colorCache().clear();
    configuredRules(true);
    ++generation();
}

QString FolderColors::themedIconName(const QString &iconName, const QColor &color)
{
    if (!color.isValid() || !isPlainFolder(iconName)) {
        return {};
    }
    const QStringList names = variantNames(nearestLabel(color));
    const auto found = std::find_if(names.cbegin(), names.cend(), [](const QString &name) {
        return QIcon::hasThemeIcon(name);
    });
    return found == names.cend() ? QString() : *found;
}

QIcon FolderColors::decorate(const QIcon &icon, const QString &iconName, const QStringList &overlays, const QColor &color, Style style)
{
    if (!color.isValid() || icon.isNull() || (style != Style::Icon && style != Style::Badge) || !isThemeIcon(icon, iconName, overlays)) {
        return icon;
    }
    const QString variantName = overlays.isEmpty() ? themedIconName(iconName, color) : QString();
    const QIcon variant = variantName.isEmpty() ? QIcon() : QIcon::fromTheme(variantName);
    const bool symbolic = !iconName.isEmpty() && QIcon::hasThemeIcon(iconName + QStringLiteral("-symbolic"));
    const QString cacheName = iconName.isEmpty() ? QString() : iconName + u'+' + overlays.join(u',');
    return QIcon(new FolderColorEngine(icon, variant, cacheName, color, style, symbolic, !overlays.isEmpty()));
}

QIcon FolderColors::dot(const QColor &color)
{
    if (!color.isValid()) {
        return {};
    }
    return QIcon(new FolderColorEngine(QIcon(), QIcon(), QStringLiteral("dot"), color, Style::Badge, false, false));
}

QIcon FolderColors::decorate(const QIcon &icon, const KFileItem &item)
{
    const Style style = configured();
    if ((style != Style::Icon && style != Style::Badge) || !item.isDir() || item.localPath().isEmpty()) {
        return icon;
    }
    return decorate(icon, item.iconName(), item.overlays(), color(item.localPath()), style);
}

QColor FolderColors::nameColor(const KFileItem &item, const QColor &background)
{
    if (configured() != Style::Name || !item.isDir()) {
        return {};
    }
    const QColor folderColor = color(item.localPath());
    return folderColor.isValid() ? ColorSchemes::readable(folderColor, background, 4.5) : QColor();
}
