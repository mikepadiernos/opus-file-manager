// SPDX-License-Identifier: GPL-3.0-or-later

#include "doubleshift.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QStyleHints>
#include <QWindow>

DoubleShiftDetector::DoubleShiftDetector(std::function<QWindow *()> window, QObject *parent)
    : QObject(parent)
    , m_window(std::move(window))
    , m_interval(QGuiApplication::styleHints()->mouseDoubleClickInterval())
{
    // Window-level delivery sees each key event once, before it reaches a widget.
    qApp->installEventFilter(this);
}

DoubleShiftDetector::~DoubleShiftDetector()
{
    qApp->removeEventFilter(this);
}

void DoubleShiftDetector::setInterval(int milliseconds)
{
    m_interval = milliseconds;
}

int DoubleShiftDetector::interval() const
{
    return m_interval;
}

QString DoubleShiftDetector::unavailableReason(KSharedConfig::Ptr kaccessrc)
{
    if (!kaccessrc) {
        kaccessrc = KSharedConfig::openConfig(QStringLiteral("kaccessrc"), KConfig::NoGlobals);
    }
    if (kaccessrc->group(QStringLiteral("Keyboard")).readEntry("StickyKeys", false)) {
        return i18nc("@info", "Sticky Keys is turned on in Plasma's accessibility settings, so Shift latches instead of being tapped.");
    }
    return {};
}

bool DoubleShiftDetector::observes(QObject *watched) const
{
    return watched->isWindowType() && m_window && watched == m_window();
}

void DoubleShiftDetector::reset()
{
    m_shiftDown = false;
    m_interrupted = false;
    m_lastTap.invalidate();
}

bool DoubleShiftDetector::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseButtonPress:
    case QEvent::FocusOut:
        break;
    default:
        return false;
    }
    if (!observes(watched)) {
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusOut) {
        reset();
        return false;
    }
    const auto *key = static_cast<QKeyEvent *>(event);
    if (key->isAutoRepeat()) {
        if (key->key() == Qt::Key_Shift) {
            // Holding Shift is not a tap.
            m_interrupted = true;
        }
        return false;
    }
    if (key->key() != Qt::Key_Shift) {
        if (event->type() == QEvent::KeyPress) {
            reset();
        }
        return false;
    }
    if (event->type() == QEvent::KeyPress) {
        m_shiftDown = true;
        m_interrupted = false;
        m_pressed.start();
        return false;
    }
    if (!m_shiftDown || m_interrupted || m_pressed.elapsed() > m_interval) {
        reset();
        return false;
    }
    m_shiftDown = false;
    if (m_lastTap.isValid() && m_lastTap.elapsed() <= m_interval) {
        m_lastTap.invalidate();
        Q_EMIT triggered();
    } else {
        m_lastTap.start();
    }
    return false;
}
