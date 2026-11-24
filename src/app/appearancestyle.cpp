// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancestyle.h"

#include "settings.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QTabBar>
#include <QWidget>

#include <algorithm>

namespace
{
QPalette::ColorGroup colorGroup(const QStyleOption *option)
{
    if (!(option->state & QStyle::State_Enabled)) {
        return QPalette::Disabled;
    }
    return option->state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
}

QColor mixed(const QColor &first, const QColor &second, qreal secondAmount)
{
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * secondAmount,
                            first.greenF() + (second.greenF() - first.greenF()) * secondAmount,
                            first.blueF() + (second.blueF() - first.blueF()) * secondAmount);
}

/// A rectangle with rounded corners on the left side, the right side or both.
QPainterPath roundedPath(const QRectF &rect, qreal radius, bool left, bool right)
{
    radius = std::min({radius, rect.width() / 2, rect.height() / 2});
    QPainterPath path;
    if (radius <= 0 || (!left && !right)) {
        path.addRect(rect);
        return path;
    }
    const qreal l = left ? radius : 0;
    const qreal r = right ? radius : 0;
    path.moveTo(rect.left() + l, rect.top());
    path.lineTo(rect.right() - r, rect.top());
    if (r > 0) {
        path.arcTo(QRectF(rect.right() - 2 * r, rect.top(), 2 * r, 2 * r), 90, -90);
    }
    path.lineTo(rect.right(), rect.bottom() - r);
    if (r > 0) {
        path.arcTo(QRectF(rect.right() - 2 * r, rect.bottom() - 2 * r, 2 * r, 2 * r), 0, -90);
    }
    path.lineTo(rect.left() + l, rect.bottom());
    if (l > 0) {
        path.arcTo(QRectF(rect.left(), rect.bottom() - 2 * l, 2 * l, 2 * l), 270, -90);
    }
    path.lineTo(rect.left(), rect.top() + l);
    if (l > 0) {
        path.arcTo(QRectF(rect.left(), rect.top(), 2 * l, 2 * l), 180, -90);
    }
    path.closeSubpath();
    return path;
}

void fillPath(QPainter *painter, const QPainterPath &path, const QColor &color, const QColor &outline = QColor())
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(outline.isValid() ? QPen(outline, 1) : Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(path);
    painter->restore();
}
}

AppearanceStyle::AppearanceStyle(QStyle *base)
    : QProxyStyle(base)
{
    setObjectName(QStringLiteral("appearanceStyle"));
}

AppearanceStyle *AppearanceStyle::installed()
{
    return qobject_cast<AppearanceStyle *>(QApplication::style());
}

void AppearanceStyle::applyCornerRadius(int radius)
{
    AppearanceStyle *style = installed();
    if (!style) {
        if (radius == FollowStyle) {
            return;
        }
        // The proxy takes ownership of the current style, so QApplication
        // does not delete it when the proxy replaces it.
        style = new AppearanceStyle(QApplication::style());
        QApplication::setStyle(style);
    }
    style->setRadius(radius);
}

int AppearanceStyle::radiusForSetting(int choice, int custom)
{
    switch (choice) {
    case Settings::EnumCornerRadius::None:
        return 0;
    case Settings::EnumCornerRadius::Small:
        return 2;
    case Settings::EnumCornerRadius::Medium:
        return 4;
    case Settings::EnumCornerRadius::Large:
        return 8;
    case Settings::EnumCornerRadius::Custom:
        return std::clamp(custom, 0, 24);
    default:
        return FollowStyle;
    }
}

int AppearanceStyle::cornerRadius(int fallback)
{
    const AppearanceStyle *style = installed();
    return style && style->radius() != FollowStyle ? style->radius() : fallback;
}

int AppearanceStyle::radius() const
{
    return m_radius;
}

void AppearanceStyle::setRadius(int radius)
{
    radius = std::max(radius, int(FollowStyle));
    if (radius == m_radius) {
        return;
    }
    m_radius = radius;
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        widget->update();
    }
}

void AppearanceStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (m_radius == FollowStyle) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }
    switch (element) {
    case PE_PanelItemViewRow:
        // Item panels draw the rounded selection; a row-wide fill would hide it.
        if (const auto *row = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            QStyleOptionViewItem plain(*row);
            plain.state &= ~(State_Selected | State_MouseOver);
            QProxyStyle::drawPrimitive(element, &plain, painter, widget);
            return;
        }
        break;
    case PE_PanelItemViewItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            const bool selected = item->state & State_Selected;
            const bool hovered = (item->state & State_MouseOver) && (item->state & State_Enabled);
            QStyleOptionViewItem plain(*item);
            plain.state &= ~(State_Selected | State_MouseOver);
            QProxyStyle::drawPrimitive(element, &plain, painter, widget);
            if (selected || hovered) {
                QColor color = item->palette.color(colorGroup(item), QPalette::Highlight);
                if (!selected) {
                    color.setAlphaF(0.3);
                }
                // Round only the outer ends of a selection that spans columns.
                bool start = true;
                bool end = true;
                if (item->viewItemPosition == QStyleOptionViewItem::Beginning) {
                    end = false;
                } else if (item->viewItemPosition == QStyleOptionViewItem::Middle) {
                    start = end = false;
                } else if (item->viewItemPosition == QStyleOptionViewItem::End) {
                    start = false;
                }
                const bool rtl = item->direction == Qt::RightToLeft;
                fillPath(painter, roundedPath(item->rect, m_radius, rtl ? end : start, rtl ? start : end), color);
            }
            return;
        }
        break;
    case PE_PanelLineEdit:
    case PE_FrameLineEdit:
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option); frame && frame->lineWidth > 0) {
            const QPalette::ColorGroup group = colorGroup(frame);
            const QColor outline = frame->state & State_HasFocus
                ? frame->palette.color(group, QPalette::Highlight)
                : mixed(frame->palette.color(group, QPalette::Window), frame->palette.color(group, QPalette::WindowText), 0.3);
            const QColor fill = element == PE_PanelLineEdit ? frame->palette.color(group, QPalette::Base) : QColor(Qt::transparent);
            fillPath(painter, roundedPath(QRectF(frame->rect).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, true, true), fill, outline);
            return;
        }
        break;
    case PE_PanelTipLabel:
        // Rounded corners need a translucent tooltip window, which the widget
        // style decides; an opaque one keeps the style's corners.
        if (widget && widget->testAttribute(Qt::WA_TranslucentBackground)) {
            const QPalette::ColorGroup group = colorGroup(option);
            fillPath(painter,
                     roundedPath(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, true, true),
                     option->palette.color(group, QPalette::ToolTipBase),
                     mixed(option->palette.color(group, QPalette::ToolTipBase), option->palette.color(group, QPalette::ToolTipText), 0.25));
            return;
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void AppearanceStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (m_radius != FollowStyle && element == CE_TabBarTabShape) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            const bool selected = tab->state & State_Selected;
            const bool hovered = (tab->state & State_MouseOver) && (tab->state & State_Enabled);
            if (!selected && !hovered) {
                return;
            }
            const QPalette::ColorGroup group = colorGroup(tab);
            QColor highlight = tab->palette.color(group, QPalette::Highlight);
            QColor fill = highlight;
            fill.setAlphaF(selected ? 0.2 : 0.1);
            const QRectF rect = QRectF(tab->rect).adjusted(1, 1, -1, -1);
            fillPath(painter, roundedPath(rect, m_radius, true, true), fill);
            if (selected) {
                // The selected tab keeps an indicator on the edge facing its
                // content, so it does not rely on the fill alone.
                QRectF indicator;
                constexpr qreal size = 2;
                switch (tab->shape) {
                case QTabBar::RoundedSouth:
                case QTabBar::TriangularSouth:
                    indicator = QRectF(rect.left() + m_radius, rect.top(), rect.width() - 2 * m_radius, size);
                    break;
                case QTabBar::RoundedWest:
                case QTabBar::TriangularWest:
                    indicator = QRectF(rect.right() - size, rect.top() + m_radius, size, rect.height() - 2 * m_radius);
                    break;
                case QTabBar::RoundedEast:
                case QTabBar::TriangularEast:
                    indicator = QRectF(rect.left(), rect.top() + m_radius, size, rect.height() - 2 * m_radius);
                    break;
                default:
                    indicator = QRectF(rect.left() + m_radius, rect.bottom() - size, rect.width() - 2 * m_radius, size);
                    break;
                }
                fillPath(painter, roundedPath(indicator, size / 2, true, true), highlight);
            }
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}
