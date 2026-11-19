// SPDX-License-Identifier: GPL-3.0-or-later

#include "symbolicicon.h"

#include <KIconUtils>

#include <QIconEngine>
#include <QPainter>

namespace
{
const QString SymbolicSuffix = QStringLiteral("-symbolic");

/// Draws @c m_symbolic up to SymbolicIcon::MaxSize and @c m_regular above it.
class SymbolicIconEngine : public QIconEngine
{
public:
    SymbolicIconEngine(const QIcon &regular, const QIcon &symbolic)
        : m_regular(regular)
        , m_symbolic(symbolic)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        iconFor(rect.size()).paint(painter, rect, Qt::AlignCenter, mode, state);
    }

    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return iconFor(size).actualSize(size, mode, state);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return iconFor(size).pixmap(size, mode, state);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        return iconFor(size).pixmap(size, scale, mode, state);
    }

    QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override
    {
        return m_regular.availableSizes(mode, state);
    }

    QString iconName() override
    {
        return m_regular.name();
    }

    bool isNull() override
    {
        return m_regular.isNull();
    }

    QString key() const override
    {
        return QStringLiteral("SymbolicIconEngine");
    }

    QIconEngine *clone() const override
    {
        return new SymbolicIconEngine(m_regular, m_symbolic);
    }

private:
    const QIcon &iconFor(const QSize &size) const
    {
        return qMax(size.width(), size.height()) <= SymbolicIcon::MaxSize ? m_symbolic : m_regular;
    }

    QIcon m_regular;
    QIcon m_symbolic;
};
}

QIcon SymbolicIcon::wrap(const QIcon &icon)
{
    return wrap(icon, icon.name(), {});
}

QIcon SymbolicIcon::wrap(const QIcon &icon, const QString &iconName, const QStringList &overlays)
{
    const QString name = icon.name().isEmpty() ? iconName : icon.name();
    if (name.isEmpty() || name.endsWith(SymbolicSuffix) || !QIcon::hasThemeIcon(name + SymbolicSuffix)) {
        return icon;
    }
    if (icon.name().isEmpty()) {
        if (overlays.isEmpty()) {
            return icon;
        }
        const QIcon themed = KIconUtils::addOverlays(QIcon::fromTheme(name), overlays);
        if (icon.pixmap(16).toImage() != themed.pixmap(16).toImage()) {
            return icon;
        }
    }
    QIcon symbolic = QIcon::fromTheme(name + SymbolicSuffix);
    if (!overlays.isEmpty()) {
        symbolic = KIconUtils::addOverlays(symbolic, overlays);
    }
    return QIcon(new SymbolicIconEngine(icon, symbolic));
}

QIcon SymbolicIcon::fromTheme(const QString &name)
{
    return wrap(QIcon::fromTheme(name));
}
