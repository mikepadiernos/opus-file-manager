// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QStringList>

/**
 * Throwaway icon theme for tests. Regular icons are solid RegularColor at
 * 16 and 48 px; symbolic variants are solid SymbolicColor at 16 px only.
 */
namespace TestIconTheme
{
inline const QColor RegularColor = Qt::red;
inline const QColor SymbolicColor = Qt::blue;
inline const QColor EmblemColor = Qt::green;
inline const QString Name = QStringLiteral("opusfm-test");

inline bool writeIcon(const QString &root, int size, const QString &name, const QColor &color)
{
    const QString dir = QStringLiteral("%1/%2/%3").arg(root, Name, QString::number(size));
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(color);
    return QDir().mkpath(dir) && image.save(QStringLiteral("%1/%2.png").arg(dir, name));
}

/**
 * Writes the theme under @p root and makes it the current Qt icon theme.
 * @param root a temporary folder used as the only theme search path
 * @param regular names that get only a regular icon
 * @param withSymbolic names that get a regular icon and a "-symbolic" variant
 * @returns false when a file could not be written
 */
inline bool install(const QString &root, const QStringList &regular, const QStringList &withSymbolic, const QStringList &emblems = {})
{
    QFile index(QStringLiteral("%1/%2/index.theme").arg(root, Name));
    if (!QDir().mkpath(QStringLiteral("%1/%2").arg(root, Name)) || !index.open(QIODevice::WriteOnly)) {
        return false;
    }
    index.write(
        "[Icon Theme]\nName=Test\nDirectories=16,48\n\n"
        "[16]\nSize=16\nType=Fixed\n\n"
        "[48]\nSize=48\nType=Fixed\n");
    index.close();

    for (const QString &name : regular + withSymbolic) {
        if (!writeIcon(root, 16, name, RegularColor) || !writeIcon(root, 48, name, RegularColor)) {
            return false;
        }
    }
    for (const QString &name : withSymbolic) {
        if (!writeIcon(root, 16, name + QStringLiteral("-symbolic"), SymbolicColor)) {
            return false;
        }
        for (const QString &name : emblems) {
            if (!writeIcon(root, 16, name, EmblemColor) || !writeIcon(root, 48, name, EmblemColor)) {
                return false;
            }
        }
    }

    QIcon::setThemeSearchPaths({root});
    QIcon::setThemeName(Name);
    return true;
}

/// Colour at the centre of @p icon drawn at @p size logical pixels.
inline QColor colorAt(const QIcon &icon, int size, qreal devicePixelRatio = 1.0)
{
    const QImage image = icon.pixmap(QSize(size, size), devicePixelRatio).toImage();
    return image.pixelColor(image.width() / 2, image.height() / 2);
}

inline bool containsColor(const QIcon &icon, int size, const QColor &color, qreal devicePixelRatio = 1.0)
{
    const QImage image = icon.pixmap(QSize(size, size), devicePixelRatio).toImage();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) == color) {
                return true;
            }
        }
    }
    return false;
}
}
