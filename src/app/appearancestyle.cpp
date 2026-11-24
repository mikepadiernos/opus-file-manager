// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancestyle.h"

#include "colorschemes.h"
#include "settings.h"

#include <KMainWindow>

#include <QAbstractItemView>
#include <QApplication>
#include <QFrame>
#include <QItemDelegate>
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

/// How much highlight a light selection and a light hover mix into the background.
constexpr qreal LightSelectionAmount = 0.2;
constexpr qreal LightHoverAmount = 0.1;
/// The radius of rounded selections when the corner radius follows the style or is square.
constexpr int DefaultRoundedRadius = 6;
/// Text contrast on a light selection, as for normal text under WCAG 2.
constexpr double TextContrast = 4.5;

QColor lightSelectionFill(const QPalette &palette, QPalette::ColorGroup group)
{
    return mixed(palette.color(group, QPalette::Base), palette.color(group, QPalette::Highlight), LightSelectionAmount);
}

bool readableOnLightSelection(const QColor &text, const QPalette &palette, QPalette::ColorGroup group)
{
    return ColorSchemes::contrastRatio(text, lightSelectionFill(palette, group)) >= TextContrast;
}

void prepareForSelection(QStyleOptionViewItem *option, AppearanceStyle::Selection selection)
{
    if (!AppearanceStyle::isLightSelection(selection)) {
        return;
    }
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const QColor text = option->palette.color(group, QPalette::Text);
        if (readableOnLightSelection(text, option->palette, group)) {
            option->palette.setColor(group, QPalette::HighlightedText, text);
        }
    }
}

/**
 * Strokes a rounded rectangle whose open sides continue into neighbouring
 * cells of the same row, so that only the closed sides show.
 */
void strokeShape(QPainter *painter, QRectF shape, qreal radius, bool closedLeft, bool closedRight, const QPen &pen, const QRect &clip)
{
    const qreal overlap = radius + pen.widthF() + 2;
    if (!closedLeft) {
        shape.setLeft(shape.left() - overlap);
    }
    if (!closedRight) {
        shape.setRight(shape.right() + overlap);
    }
    painter->save();
    painter->setClipRect(clip, Qt::IntersectClip);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(roundedPath(shape, radius, true, true));
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
    options.selection = static_cast<Selection>(std::clamp(int(Settings::selectionStyle()), 0, int(Selection::AccentBar)));
    options.hoverHighlight = Settings::hoverHighlight();
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

AppearanceStyle::Selection AppearanceStyle::selectionStyle()
{
    const AppearanceStyle *style = installed();
    return style ? style->m_options.selection : Selection::FollowStyle;
}

bool AppearanceStyle::hoverHighlight()
{
    const AppearanceStyle *style = installed();
    return !style || style->m_options.hoverHighlight;
}

bool AppearanceStyle::isLightSelection(Selection selection)
{
    return selection == Selection::Outlined || selection == Selection::AccentBar;
}

void AppearanceStyle::prepareItem(QStyleOptionViewItem *option)
{
    prepareForSelection(option, selectionStyle());
}

bool AppearanceStyle::customSelection() const
{
    return m_options.radius != FollowStyle || m_options.selection != Selection::FollowStyle;
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
    if (element == PE_PanelItemViewItem || element == PE_PanelItemViewRow) {
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            drawItemPanel(element, *item, painter, widget);
            return;
        }
    }
    if (element == PE_FrameFocusRect && m_options.selection != Selection::FollowStyle) {
        // Item panels draw their own focus indicator. QItemDelegate draws its
        // selection without them, so its views keep the style's indicator.
        const auto *view = qobject_cast<const QAbstractItemView *>(widget);
        if (view && !qobject_cast<const QItemDelegate *>(view->itemDelegate())) {
            return;
        }
    }
    if (m_options.radius == FollowStyle) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }
    const int radius = m_options.radius;
    switch (element) {
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

void AppearanceStyle::drawItemPanel(PrimitiveElement element, const QStyleOptionViewItem &item, QPainter *painter, const QWidget *widget) const
{
    QStyleOptionViewItem plain(item);
    if (!m_options.hoverHighlight) {
        plain.state &= ~State_MouseOver;
    }
    if (!customSelection()) {
        QProxyStyle::drawPrimitive(element, &plain, painter, widget);
        return;
    }
    // Item panels draw the selection; a row-wide fill would hide it.
    plain.state &= ~(State_Selected | State_MouseOver);
    QProxyStyle::drawPrimitive(element, &plain, painter, widget);
    if (element != PE_PanelItemViewItem) {
        return;
    }
    const bool selected = item.state & State_Selected;
    const bool hovered = m_options.hoverHighlight && (item.state & State_MouseOver) && (item.state & State_Enabled);
    const bool focused = m_options.selection != Selection::FollowStyle && (item.state & State_HasFocus);
    if (!selected && !hovered && !focused) {
        return;
    }

    const QPalette::ColorGroup group = colorGroup(&item);
    const QColor highlight = item.palette.color(group, QPalette::Highlight);
    const QColor base = item.palette.color(group, QPalette::Base);
    Selection selection = m_options.selection;
    qreal radius = 0;
    switch (selection) {
    case Selection::FollowStyle:
        // Only the corner radius differs from the widget style.
        selection = Selection::Filled;
        radius = m_options.radius;
        break;
    case Selection::Filled:
        break;
    case Selection::Rounded:
        radius = m_options.radius > 0 ? m_options.radius : DefaultRoundedRadius;
        break;
    case Selection::Outlined:
    case Selection::AccentBar:
        radius = std::max(m_options.radius, 0);
        // Delegates that were not prepared draw selected text that may be
        // unreadable on a light highlight, so they get a solid one.
        if (!readableOnLightSelection(item.palette.color(group, QPalette::HighlightedText), item.palette, group)) {
            selection = Selection::Filled;
        }
        break;
    }
    const bool light = isLightSelection(selection);

    // Round only the outer ends of a selection that spans columns.
    bool start = true;
    bool end = true;
    if (item.viewItemPosition == QStyleOptionViewItem::Beginning) {
        end = false;
    } else if (item.viewItemPosition == QStyleOptionViewItem::Middle) {
        start = end = false;
    } else if (item.viewItemPosition == QStyleOptionViewItem::End) {
        start = false;
    }
    const bool rtl = item.direction == Qt::RightToLeft;
    const bool left = rtl ? end : start;
    const bool right = rtl ? start : end;
    const QRectF rect(item.rect);

    if (selected || hovered) {
        QColor fill = highlight;
        if (light) {
            fill.setAlphaF(selected ? LightSelectionAmount : LightHoverAmount);
        } else if (!selected) {
            fill.setAlphaF(0.3);
        }
        fillPath(painter, roundedPath(rect, radius, left, right), fill);
    }
    const QColor accent = ColorSchemes::readable(highlight, base);
    if (selected && selection == Selection::Outlined) {
        strokeShape(painter, rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, left, right, QPen(accent, 1), item.rect);
    } else if (selected && selection == Selection::AccentBar && start) {
        constexpr qreal width = 3;
        const QRectF bar = rtl ? QRectF(rect.right() - width, rect.top(), width, rect.height()) : QRectF(rect.left(), rect.top(), width, rect.height());
        fillPath(painter, roundedPath(bar, width / 2, true, true), accent);
    }
    if (focused) {
        // A dashed ring marks the current item, so keyboard focus stays
        // visible with and without a selection.
        const QColor color = selected && !light ? item.palette.color(group, QPalette::HighlightedText) : accent;
        strokeShape(painter, rect.adjusted(2.5, 2.5, -2.5, -2.5), std::max<qreal>(0, radius - 2), left, right, QPen(color, 1, Qt::DashLine), item.rect);
    }
}

void AppearanceStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == CE_ItemViewItem && isLightSelection(m_options.selection)) {
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            QStyleOptionViewItem prepared(*item);
            prepareForSelection(&prepared, m_options.selection);
            QProxyStyle::drawControl(element, &prepared, painter, widget);
            return;
        }
    }
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
