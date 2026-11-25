// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancestyle.h"
#include "effectiveappearance.h"

#include "appearancedefaults.h"
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
#include <QStatusBar>
#include <QStyleOption>
#include <QTabBar>
#include <QToolBar>
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

/// Whether @p widget is a window's status bar or a pane's status bar.
bool isStatusBar(const QWidget *widget)
{
    return qobject_cast<const QStatusBar *>(widget) || widget->objectName() == QLatin1String("paneStatusBar");
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
    options.radius = radiusForSetting(EffectiveAppearance::cornerRadius(), EffectiveAppearance::customCornerRadius());
    switch (EffectiveAppearance::dividers()) {
    case Settings::EnumDividers::Thin:
        options.dividers = Dividers::Thin;
        break;
    case Settings::EnumDividers::Hidden:
        options.dividers = Dividers::Hidden;
        break;
    default:
        break;
    }
    options.revealDividers = EffectiveAppearance::revealDividersOnHover();
    options.selection = static_cast<Selection>(std::clamp(int(EffectiveAppearance::selectionStyle()), 0, int(Selection::AccentBar)));
    options.hoverHighlight = EffectiveAppearance::hoverHighlight();
    options.spacingScale = std::clamp(EffectiveAppearance::spacingScale(), MinimumSpacingScale, MaximumSpacingScale);
    options.tabShape = static_cast<TabShape>(std::clamp(int(EffectiveAppearance::tabShape()), 0, int(TabShape::Flat)));
    options.tabIndicator = static_cast<TabIndicator>(std::clamp(int(EffectiveAppearance::tabIndicator()), 0, int(TabIndicator::None)));
    options.tabColorStrength = static_cast<TabColorStrength>(std::clamp(int(EffectiveAppearance::tabColorStrength()), 0, int(TabColorStrength::Strong)));
    options.statusBarStyle = static_cast<StatusBarStyle>(std::clamp(int(EffectiveAppearance::statusBarStyle()), 0, int(StatusBarStyle::Shaded)));
    options.animations = static_cast<Animations>(std::clamp(int(EffectiveAppearance::animations()), 0, int(Animations::Off)));
    if (AppearanceDefaults::effectsSuspended()) {
        options.radius = FollowStyle;
        options.tabColorStrength = TabColorStrength::Text;
    }
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
    normalized.spacingScale = std::clamp(normalized.spacingScale, MinimumSpacingScale, MaximumSpacingScale);
    if (normalized == m_options) {
        return;
    }
    const bool metrics = normalized.dividers != m_options.dividers || normalized.spacingScale != m_options.spacingScale;
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
    if (isStatusBar(widget)) {
        widget->installEventFilter(this);
    }
}

void AppearanceStyle::unpolish(QWidget *widget)
{
    if (isStatusBar(widget)) {
        widget->removeEventFilter(this);
    }
    QProxyStyle::unpolish(widget);
}

bool AppearanceStyle::eventFilter(QObject *watched, QEvent *event)
{
    // Bars draw no background of their own, so the style paints before their contents.
    if (event->type() == QEvent::Paint && m_options.statusBarStyle != StatusBarStyle::FollowStyle) {
        if (auto *bar = qobject_cast<QWidget *>(watched); bar && isStatusBar(bar) && qobject_cast<const KMainWindow *>(bar->window())) {
            paintStatusBar(bar);
        }
    }
    return QProxyStyle::eventFilter(watched, event);
}

void AppearanceStyle::paintStatusBar(QWidget *bar) const
{
    QPainter painter(bar);
    const QPalette::ColorGroup group = bar->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    const QPalette palette = bar->palette();
    const QColor window = palette.color(group, QPalette::Window);
    const QColor text = palette.color(group, QPalette::WindowText);
    if (m_options.statusBarStyle == StatusBarStyle::Shaded) {
        painter.fillRect(bar->rect(), mixed(window, text, 0.06));
    }
    painter.fillRect(QRect(0, 0, bar->width(), 1), mixed(window, text, 0.2));
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
    if (element == CE_TabBarTabShape) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            TabShape shape = m_options.tabShape;
            if (shape == TabShape::FollowStyle && m_options.radius != FollowStyle) {
                shape = TabShape::Rounded;
            }
            const QColor color = tabColor(*tab, widget);
            if (shape != TabShape::FollowStyle) {
                drawTabShape(*tab, shape, color, painter);
                return;
            }
            QProxyStyle::drawControl(element, option, painter, widget);
            if (color.isValid()) {
                QColor fill = color;
                fill.setAlphaF(m_options.tabColorStrength == TabColorStrength::Strong ? 0.45 : 0.2);
                painter->fillRect(tab->rect, fill);
            }
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

QColor AppearanceStyle::tabColorFill(const QColor &window, const QColor &color, TabColorStrength strength, bool selected)
{
    switch (strength) {
    case TabColorStrength::Tint:
        return mixed(window, color, selected ? 0.3 : 0.2);
    case TabColorStrength::Strong:
        return mixed(window, color, selected ? 0.6 : 0.45);
    case TabColorStrength::Text:
        break;
    }
    return {};
}

QColor AppearanceStyle::tabColor(const QStyleOptionTab &tab, const QWidget *widget) const
{
    if (m_options.tabColorStrength == TabColorStrength::Text) {
        return {};
    }
    const auto *bar = qobject_cast<const QTabBar *>(widget);
    if (!bar || tab.tabIndex < 0 || tab.tabIndex >= bar->count()) {
        return {};
    }
    const QVariant data = bar->tabData(tab.tabIndex);
    return data.typeId() == QMetaType::QColor ? data.value<QColor>() : QColor();
}

void AppearanceStyle::drawTabShape(const QStyleOptionTab &tab, TabShape shape, const QColor &tabColor, QPainter *painter) const
{
    const bool selected = tab.state & State_Selected;
    const bool hovered = (tab.state & State_MouseOver) && (tab.state & State_Enabled);
    const QPalette::ColorGroup group = colorGroup(&tab);
    const QColor highlight = tab.palette.color(group, QPalette::Highlight);
    int radius = 0;
    if (shape == TabShape::Rounded) {
        radius = m_options.radius == FollowStyle ? DefaultRoundedRadius : m_options.radius;
    }
    const QRectF rect = QRectF(tab.rect).adjusted(1, 1, -1, -1);
    QColor fill;
    if (tabColor.isValid()) {
        fill = tabColorFill(tab.palette.color(group, QPalette::Window), tabColor, m_options.tabColorStrength, selected);
        if (hovered && !selected) {
            fill = mixed(fill, highlight, LightHoverAmount);
        }
    } else if ((selected && shape != TabShape::Flat) || hovered) {
        fill = highlight;
        fill.setAlphaF(selected ? 0.2 : 0.1);
    }
    if (fill.isValid()) {
        fillPath(painter, roundedPath(rect, radius, true, true), fill);
    }
    if (!selected) {
        return;
    }
    // The active tab keeps an indicator on the edge facing its content, so it
    // does not rely on the fill alone. Flat tabs have no fill, so they always
    // keep one.
    qreal size = 0;
    switch (m_options.tabIndicator) {
    case TabIndicator::ThickLine:
        size = 4;
        break;
    case TabIndicator::None:
        size = shape == TabShape::Flat ? 2 : 0;
        break;
    case TabIndicator::Line:
        size = 2;
        break;
    }
    if (size <= 0) {
        return;
    }
    QRectF indicator;
    switch (tab.shape) {
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

int AppearanceStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (metric == PM_DefaultFrameWidth && isRestyledFrame(widget)) {
        return m_options.dividers == Dividers::Thin ? 1 : 0;
    }
    const int value = QProxyStyle::pixelMetric(metric, option, widget);
    if (isSpacingMetric(metric) && isRespacedWidget(widget)) {
        return scaledSpacing(value);
    }
    return value;
}

QSize AppearanceStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    const QSize size = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
    const bool padded = type == CT_TabBarTab || (type == CT_ToolButton && widget && qobject_cast<const QToolBar *>(widget->parentWidget()));
    if (!padded || !isRespacedWidget(widget) || !size.isValid()) {
        return size;
    }
    // Only the padding around the contents is scaled, so text and icons keep their size.
    const QSize padding = (size - contentsSize).expandedTo(QSize(0, 0));
    return contentsSize + QSize(scaledSpacing(padding.width()), scaledSpacing(padding.height()));
}

bool AppearanceStyle::isSpacingMetric(PixelMetric metric)
{
    switch (metric) {
    case PM_LayoutLeftMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin:
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
    case PM_ToolBarItemSpacing:
    case PM_ToolBarItemMargin:
    case PM_TabBarTabHSpace:
    case PM_TabBarTabVSpace:
    case PM_DockWidgetTitleMargin:
    case PM_DockWidgetTitleBarButtonMargin:
        return true;
    default:
        return false;
    }
}

bool AppearanceStyle::isRespacedWidget(const QWidget *widget) const
{
    // Dialogs keep the widget style's spacing, as they keep Plasma's fonts.
    return m_options.spacingScale != 100 && widget && qobject_cast<const KMainWindow *>(widget->window());
}

int AppearanceStyle::scaledSpacing(int value) const
{
    return value > 0 ? qRound(value * m_options.spacingScale / 100.0) : value;
}

int AppearanceStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == SH_Table_GridLineColor && m_options.dividers != Dividers::FollowStyle && option) {
        const QColor color = m_options.dividers == Dividers::Hidden ? QColor(Qt::transparent) : dividerColor(option);
        return static_cast<int>(color.rgba());
    }
    switch (m_options.animations) {
    case Animations::Off:
        if (hint == SH_Widget_Animation_Duration || hint == SH_Widget_Animate) {
            return 0;
        }
        break;
    case Animations::Reduced:
        if (hint == SH_Widget_Animation_Duration) {
            return QProxyStyle::styleHint(hint, option, widget, returnData) / 2;
        }
        break;
    case Animations::FollowPlasma:
        break;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

bool AppearanceStyle::panelAnimationsEnabled(const QWidget *widget)
{
    const AppearanceStyle *style = installed();
    if (style && style->m_options.animations != Animations::FollowPlasma) {
        return false;
    }
    return QApplication::style()->styleHint(SH_Widget_Animation_Duration, nullptr, widget) > 0;
}
