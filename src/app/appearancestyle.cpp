// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancestyle.h"

#include "settings.h"

#include <KMainWindow>

#include <QApplication>
#include <QFrame>
#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QSplitterHandle>
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

QColor dividerColor(const QStyleOption *option)
{
    const QPalette::ColorGroup group = colorGroup(option);
    return mixed(option->palette.color(group, QPalette::Window), option->palette.color(group, QPalette::WindowText), 0.2);
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

bool isLineShape(int shape)
{
    return shape == QFrame::HLine || shape == QFrame::VLine;
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

void AppearanceStyle::apply(const Options &options)
{
    AppearanceStyle *style = installed();
    if (!style) {
        if (options == Options()) {
            return;
        }
        // The proxy takes ownership of the current style, so QApplication
        // does not delete it when the proxy replaces it.
        style = new AppearanceStyle(QApplication::style());
        style->m_options = options;
        QApplication::setStyle(style);
        return;
    }
    style->setOptions(options);
}

AppearanceStyle::Options AppearanceStyle::configured()
{
    Options options;
    options.radius = radiusForSetting(Settings::cornerRadius(), Settings::customCornerRadius());
    switch (Settings::dividers()) {
    case Settings::EnumDividers::Thin:
        options.dividers = Dividers::Thin;
        break;
    case Settings::EnumDividers::Hidden:
        options.dividers = Dividers::Hidden;
        break;
    default:
        break;
    }
    options.revealDividers = Settings::revealDividersOnHover();
    return options;
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

AppearanceStyle::Options AppearanceStyle::options() const
{
    return m_options;
}

void AppearanceStyle::setOptions(const Options &options)
{
    Options normalized = options;
    normalized.radius = std::max(normalized.radius, int(FollowStyle));
    if (normalized == m_options) {
        return;
    }
    const bool metrics = normalized.dividers != m_options.dividers;
    m_options = normalized;
    const QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        if (metrics) {
            // Frames and layouts read their sizes from the style again.
            QEvent event(QEvent::StyleChange);
            QCoreApplication::sendEvent(widget, &event);
        }
        widget->update();
    }
}

int AppearanceStyle::radius() const
{
    return m_options.radius;
}

void AppearanceStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    // Hover state lets hidden splitter handles and panel separators reveal themselves.
    if (qobject_cast<QSplitterHandle *>(widget) || qobject_cast<QMainWindow *>(widget)) {
        widget->setAttribute(Qt::WA_Hover);
    }
}

bool AppearanceStyle::isRestyledFrame(const QWidget *widget) const
{
    // Panel frames in dialogs keep the widget style's frames, so lists and
    // previews there stay recognisable.
    const auto *frame = qobject_cast<const QFrame *>(widget);
    return m_options.dividers != Dividers::FollowStyle && frame && frame->frameShape() != QFrame::NoFrame && !isLineShape(frame->frameShape())
        && qobject_cast<const KMainWindow *>(frame->window());
}

void AppearanceStyle::drawDivider(const QStyleOption *option, QPainter *painter, bool hoverable) const
{
    const QRect rect = option->rect;
    const bool vertical = rect.width() < rect.height();
    QColor color;
    if (m_options.dividers == Dividers::Hidden) {
        if (!hoverable || !m_options.revealDividers || !(option->state & State_MouseOver)) {
            return;
        }
        color = option->palette.color(colorGroup(option), QPalette::Highlight);
    } else {
        color = (hoverable && option->state & State_MouseOver) ? option->palette.color(colorGroup(option), QPalette::Highlight) : dividerColor(option);
    }
    const QRect line = vertical ? QRect(rect.center().x(), rect.top(), 1, rect.height()) : QRect(rect.left(), rect.center().y(), rect.width(), 1);
    painter->fillRect(line, color);
}

void AppearanceStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (m_options.dividers != Dividers::FollowStyle) {
        switch (element) {
        case PE_IndicatorDockWidgetResizeHandle:
            drawDivider(option, painter, true);
            return;
        case PE_IndicatorToolBarSeparator:
            drawDivider(option, painter, false);
            return;
        default:
            break;
        }
    }
    if (m_options.radius == FollowStyle) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }
    const int radius = m_options.radius;
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
                fillPath(painter, roundedPath(item->rect, radius, rtl ? end : start, rtl ? start : end), color);
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
            fillPath(painter, roundedPath(QRectF(frame->rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, true, true), fill, outline);
            return;
        }
        break;
    case PE_PanelTipLabel:
        // Rounded corners need a translucent tooltip window, which the widget
        // style decides; an opaque one keeps the style's corners.
        if (widget && widget->testAttribute(Qt::WA_TranslucentBackground)) {
            const QPalette::ColorGroup group = colorGroup(option);
            fillPath(painter,
                     roundedPath(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, true, true),
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
    if (m_options.dividers != Dividers::FollowStyle) {
        switch (element) {
        case CE_Splitter:
            drawDivider(option, painter, true);
            return;
        case CE_ShapedFrame:
            if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
                if (isLineShape(frame->frameShape)) {
                    drawDivider(option, painter, false);
                    return;
                }
                if (isRestyledFrame(widget)) {
                    if (m_options.dividers == Dividers::Thin) {
                        painter->save();
                        painter->setPen(dividerColor(option));
                        painter->setBrush(Qt::NoBrush);
                        painter->drawRect(QRectF(frame->rect).adjusted(0.5, 0.5, -0.5, -0.5));
                        painter->restore();
                    }
                    return;
                }
            }
            break;
        case CE_HeaderSection:
            if (const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
                const QPalette::ColorGroup group = colorGroup(header);
                QColor fill = header->palette.color(group, QPalette::Base);
                if (header->state & (State_Sunken | State_On)) {
                    fill = mixed(fill, header->palette.color(group, QPalette::Highlight), 0.2);
                } else if ((header->state & State_MouseOver) && (header->state & State_Enabled)) {
                    fill = mixed(fill, header->palette.color(group, QPalette::Highlight), 0.1);
                }
                painter->fillRect(header->rect, fill);
                if (m_options.dividers == Dividers::Thin) {
                    const QRect rect = header->rect;
                    const QColor line = dividerColor(header);
                    painter->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), 1), line);
                    const bool last = header->position == QStyleOptionHeader::End || header->position == QStyleOptionHeader::OnlyOneSection;
                    if (header->orientation == Qt::Horizontal && !last) {
                        const int x = header->direction == Qt::RightToLeft ? rect.left() : rect.right();
                        painter->fillRect(QRect(x, rect.top() + rect.height() / 4, 1, rect.height() / 2), line);
                    }
                }
                return;
            }
            break;
        default:
            break;
        }
    }
    const int radius = m_options.radius;
    if (radius != FollowStyle && element == CE_TabBarTabShape) {
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
            fillPath(painter, roundedPath(rect, radius, true, true), fill);
            if (selected) {
                // The selected tab keeps an indicator on the edge facing its
                // content, so it does not rely on the fill alone.
                QRectF indicator;
                constexpr qreal size = 2;
                switch (tab->shape) {
                case QTabBar::RoundedSouth:
                case QTabBar::TriangularSouth:
                    indicator = QRectF(rect.left() + radius, rect.top(), rect.width() - 2 * radius, size);
                    break;
                case QTabBar::RoundedWest:
                case QTabBar::TriangularWest:
                    indicator = QRectF(rect.right() - size, rect.top() + radius, size, rect.height() - 2 * radius);
                    break;
                case QTabBar::RoundedEast:
                case QTabBar::TriangularEast:
                    indicator = QRectF(rect.left(), rect.top() + radius, size, rect.height() - 2 * radius);
                    break;
                default:
                    indicator = QRectF(rect.left() + radius, rect.bottom() - size, rect.width() - 2 * radius, size);
                    break;
                }
                fillPath(painter, roundedPath(indicator, size / 2, true, true), highlight);
            }
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

int AppearanceStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (metric == PM_DefaultFrameWidth && isRestyledFrame(widget)) {
        return m_options.dividers == Dividers::Thin ? 1 : 0;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

int AppearanceStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == SH_Table_GridLineColor && m_options.dividers != Dividers::FollowStyle && option) {
        const QColor color = m_options.dividers == Dividers::Hidden ? QColor(Qt::transparent) : dividerColor(option);
        return static_cast<int>(color.rgba());
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}
