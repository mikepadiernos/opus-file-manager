// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QProxyStyle>

class QStyleOptionViewItem;

/**
 * App-local style rules layered over Plasma's widget style. The style is
 * installed only when an option differs from the widget style's own
 * appearance, and it delegates everything else to the widget style.
 */
class AppearanceStyle : public QProxyStyle
{
    Q_OBJECT

public:
    /// A corner radius that leaves corners to the widget style.
    static constexpr int FollowStyle = -1;
    static constexpr int MinimumSpacingScale = 50;
    static constexpr int MaximumSpacingScale = 200;

    /// How splitter handles, separators, header and grid lines and frames are drawn.
    enum class Dividers {
        FollowStyle,
        Thin,
        Hidden,
    };

    /// How selected items are highlighted in item views.
    enum class Selection {
        FollowStyle,
        /// A solid highlight with square corners.
        Filled,
        /// A light highlight with an outline.
        Outlined,
        /// A solid highlight with rounded corners.
        Rounded,
        /// A light highlight with a bar at the start of the item or row.
        AccentBar,
    };

    struct Options {
        int radius = FollowStyle;
        Dividers dividers = Dividers::FollowStyle;
        /// Whether hidden splitter handles and panel separators appear on hover.
        bool revealDividers = true;
        Selection selection = Selection::FollowStyle;
        /// Whether items under the pointer are highlighted.
        bool hoverHighlight = true;
        /// Padding and spacing of toolbars, tabs, panels and status bars, in percent of the widget style's.
        int spacingScale = 100;

        bool operator==(const Options &) const = default;
    };

    explicit AppearanceStyle(QStyle *base);

    /// The application's AppearanceStyle, or nullptr when it is not installed.
    static AppearanceStyle *installed();
    /**
     * Applies style options, wrapping the application style when an option
     * differs from the widget style's own appearance.
     */
    static void apply(const Options &options);
    /// The options configured in Settings.
    static Options configured();
    /**
     * The radius for a CornerRadius setting.
     * @param choice a Settings::EnumCornerRadius value
     * @param custom the radius used for the Custom choice
     */
    static int radiusForSetting(int choice, int custom);
    /**
     * The corner radius for shapes that Plasma Opus draws itself.
     * @param fallback the radius to use when following the widget style
     */
    static int cornerRadius(int fallback);
    /// The selection style of the installed style, or FollowStyle.
    static Selection selectionStyle();
    /// Whether items under the pointer are highlighted.
    static bool hoverHighlight();
    /// Whether @p selection draws a light highlight behind normal text.
    static bool isLightSelection(Selection selection);
    /**
     * Prepares an item for a delegate that draws selected text in the
     * HighlightedText colour. With a light selection style, selected text
     * keeps the normal text colour when it is readable on the highlight.
     */
    static void prepareItem(QStyleOptionViewItem *option);

    Options options() const;
    void setOptions(const Options &options);
    int radius() const;

    void polish(QWidget *widget) override;
    using QProxyStyle::polish;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;

private:
    /// Draws a divider line along the long axis of a rectangle, or nothing when dividers are hidden.
    void drawDivider(const QStyleOption *option, QPainter *painter, bool hoverable) const;
    /// Draws an item or row panel with the chosen selection style and hover highlight.
    void drawItemPanel(PrimitiveElement element, const QStyleOptionViewItem &item, QPainter *painter, const QWidget *widget) const;
    bool customSelection() const;
    bool isRestyledFrame(const QWidget *widget) const;
    /// Whether @p metric is padding or spacing that the spacing scale applies to.
    static bool isSpacingMetric(PixelMetric metric);
    /// Whether the spacing scale applies to @p widget.
    bool isRespacedWidget(const QWidget *widget) const;
    /// A padding or spacing scaled by the spacing scale.
    int scaledSpacing(int value) const;

    Options m_options;
};
