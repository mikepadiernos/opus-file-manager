// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "branding.h"

#include <QFrame>
#include <QPointer>

class QLabel;
class QTimer;
class QVariantAnimation;

/**
 * A brief presentation of the Plasma Opus logo, version and description over
 * the main window at startup.
 *
 * It never delays or blocks use: the window is usable while it shows, pointer
 * input passes through it, it never takes focus, and any key, click, scroll or
 * touch dismisses it without being consumed. It hides itself after a moment,
 * fading only when animations are enabled, and follows the colour scheme and
 * high contrast.
 */
class StartupPresentation : public QFrame
{
    Q_OBJECT

public:
    /**
     * Presents over @p window after it is shown, when the StartupPresentation
     * setting is on.
     * @returns the presentation, or nullptr when the setting is off
     */
    static StartupPresentation *presentIfEnabled(QWidget *window);

    StartupPresentation(QWidget *window, Branding::LogoVariant variant, bool highContrast);

    /// The card's colours.
    Branding::PresentationColors colors() const;
    /// The logo shown on the card.
    QPixmap logo() const;

    /// Replaces the time the presentation stays fully visible, for tests.
    void setHoldDuration(int milliseconds);

    /// Shows the presentation and starts the timer that hides it.
    void start();
    /// Hides the presentation, fading when animations are enabled, then deletes it.
    void dismiss();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateLogo();
    void reposition();

    QPointer<QWidget> m_window;
    Branding::PresentationColors m_colors;
    bool m_highContrast;
    QLabel *m_logo;
    QTimer *m_holdTimer;
    QVariantAnimation *m_fade = nullptr;
    bool m_dismissing = false;
};
