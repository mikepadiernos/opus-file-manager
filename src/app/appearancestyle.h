// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QProxyStyle>

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

    /// How splitter handles, separators, header and grid lines and frames are drawn.
    enum class Dividers {
        FollowStyle,
        Thin,
        Hidden,
    };

    struct Options {
        int radius = FollowStyle;
        Dividers dividers = Dividers::FollowStyle;
        /// Whether hidden splitter handles and panel separators appear on hover.
        bool revealDividers = true;

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

    Options options() const;
    void setOptions(const Options &options);
    int radius() const;

    void polish(QWidget *widget) override;
    using QProxyStyle::polish;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;

private:
    /// Draws a divider line along the long axis of a rectangle, or nothing when dividers are hidden.
    void drawDivider(const QStyleOption *option, QPainter *painter, bool hoverable) const;
    bool isRestyledFrame(const QWidget *widget) const;

    Options m_options;
};
