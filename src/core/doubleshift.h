// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KSharedConfig>

#include <QElapsedTimer>
#include <QObject>

#include <functional>

class QWindow;

/**
 * Detects two taps of Shift on their own in quick succession in one window.
 *
 * Qt and KDE shortcuts can't express a double modifier tap, so this observes
 * key events delivered to the window without consuming them. A tap is a Shift
 * press and release with no other key or mouse button in between; two taps
 * within the double-click interval trigger.
 */
class DoubleShiftDetector : public QObject
{
    Q_OBJECT

public:
    /// Observes key events for the window that @p window returns, which can change.
    explicit DoubleShiftDetector(std::function<QWindow *()> window, QObject *parent = nullptr);
    ~DoubleShiftDetector() override;

    /// Replaces the maximum time between taps, for tests.
    void setInterval(int milliseconds);
    int interval() const;

    /**
     * Explains why double Shift is unavailable, or returns an empty string.
     * Sticky Keys latches Shift on each tap, so double Shift is unavailable
     * while it is on in Plasma's accessibility settings.
     * @param kaccessrc Plasma's accessibility configuration, for tests
     */
    static QString unavailableReason(KSharedConfig::Ptr kaccessrc = {});

Q_SIGNALS:
    void triggered();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool observes(QObject *watched) const;
    void reset();

    std::function<QWindow *()> m_window;
    int m_interval;
    bool m_shiftDown = false;
    bool m_interrupted = false;
    QElapsedTimer m_pressed;
    QElapsedTimer m_lastTap;
};
