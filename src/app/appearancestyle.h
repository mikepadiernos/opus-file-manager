// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QProxyStyle>

class QStyleOptionTab;
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

    /// The shape of tabs; the order matches the TabShape setting.
    enum class TabShape {
        /// The widget style's tabs, or Rounded when a corner radius is chosen.
        FollowStyle,
        Rounded,
        Square,
        /// No fill for the active tab, only its indicator.
        Flat,
    };

    /// The indicator on the edge of the active tab that faces its content.
    enum class TabIndicator {
        Line,
        ThickLine,
        /// No indicator, except on Flat tabs, which need one.
        None,
    };

    /// How strongly M03 tab colours show.
    enum class TabColorStrength {
        /// Coloured tab text.
        Text,
        /// A light fill of the tab colour.
        Tint,
        /// A strong fill of the tab colour with readable text.
        Strong,
    };

    /// How the window and pane status bars are drawn.
    enum class StatusBarStyle {
        FollowStyle,
        /// A divider on the edge facing the content.
        Separated,
        /// A shaded background and a divider.
        Shaded,
    };

    /// How long transitions take; the order matches the Animations setting.
    enum class Animations {
        /// The widget style's durations, which follow Plasma's animation speed.
        FollowPlasma,
        /// Half the widget style's durations, without panel animations.
        Reduced,
        Off,
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
        TabShape tabShape = TabShape::FollowStyle;
        TabIndicator tabIndicator = TabIndicator::Line;
        TabColorStrength tabColorStrength = TabColorStrength::Text;
        StatusBarStyle statusBarStyle = StatusBarStyle::FollowStyle;
        Animations animations = Animations::FollowPlasma;

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
     * The fill behind a coloured tab.
     * @param window the tab bar's window colour
     * @param color the tab colour
     * @param selected whether the tab is the active tab
     * @returns an invalid colour for TabColorStrength::Text
     */
    static QColor tabColorFill(const QColor &window, const QColor &color, TabColorStrength strength, bool selected);
    /**
     * Whether panels and other large areas may move in transitions, which
     * reduced motion leaves out.
     * @param widget the widget whose style's animation duration applies
     */
    static bool panelAnimationsEnabled(const QWidget *widget);
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
    void unpolish(QWidget *widget) override;
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Draws a divider line along the long axis of a rectangle, or nothing when dividers are hidden.
    void drawDivider(const QStyleOption *option, QPainter *painter, bool hoverable) const;
    /// Draws an item or row panel with the chosen selection style and hover highlight.
    void drawItemPanel(PrimitiveElement element, const QStyleOptionViewItem &item, QPainter *painter, const QWidget *widget) const;
    bool customSelection() const;
    bool isRestyledFrame(const QWidget *widget) const;
    /// Draws a tab shape that Plasma Opus styles itself.
    void drawTabShape(const QStyleOptionTab &tab, TabShape shape, const QColor &tabColor, QPainter *painter) const;
    /// The colour of the tab drawn with @p tab, stored as a QColor in QTabBar::tabData(), when tab colours show as fills.
    QColor tabColor(const QStyleOptionTab &tab, const QWidget *widget) const;
    /// Paints the status bar style behind the contents of @p bar.
    void paintStatusBar(QWidget *bar) const;
    /// Whether @p metric is padding or spacing that the spacing scale applies to.
    static bool isSpacingMetric(PixelMetric metric);
    /// Whether the spacing scale applies to @p widget.
    bool isRespacedWidget(const QWidget *widget) const;
    /// A padding or spacing scaled by the spacing scale.
    int scaledSpacing(int value) const;

    Options m_options;
};
