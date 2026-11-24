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

    explicit AppearanceStyle(QStyle *base);

    /// The application's AppearanceStyle, or nullptr when it is not installed.
    static AppearanceStyle *installed();
    /**
     * Applies a corner radius to item highlights, input fields, tabs and
     * tooltips, wrapping the application style when needed.
     * @param radius the radius in logical pixels, or FollowStyle
     */
    static void applyCornerRadius(int radius);
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

    int radius() const;
    void setRadius(int radius);

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;

private:
    int m_radius = FollowStyle;
};
