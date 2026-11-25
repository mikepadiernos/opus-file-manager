// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazikeys.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScopedValueRollback>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

YaziKeys::YaziKeys(QObject *parent)
    : QObject(parent)
    , m_matcher(YaziMode::presetKeymap(YaziMode::Preset::Standard))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(PendingTimeout);
    connect(m_timeout, &QTimer::timeout, this, &YaziKeys::cancel);
    connect(qApp, &QApplication::focusChanged, this, &YaziKeys::cancel);
}

void YaziKeys::setPreset(YaziMode::Preset preset)
{
    if (preset == m_preset) {
        return;
    }
    m_preset = preset;
    m_matcher.setKeymap(YaziMode::presetKeymap(preset));
    m_timeout->stop();
    Q_EMIT stateChanged();
}

YaziMode::Preset YaziKeys::preset() const
{
    return m_preset;
}

void YaziKeys::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        cancel();
    }
}

bool YaziKeys::isEnabled() const
{
    return m_enabled;
}

void YaziKeys::setFocusFilter(std::function<bool(QWidget *)> accepts)
{
    m_accepts = std::move(accepts);
}

void YaziKeys::watch(QWidget *widget)
{
    widget->installEventFilter(this);
}

bool YaziKeys::isPending() const
{
    return m_matcher.isPending();
}

QString YaziKeys::pendingText() const
{
    return m_matcher.pendingText();
}

void YaziKeys::cancel()
{
    m_timeout->stop();
    if (!m_matcher.isPending()) {
        return;
    }
    m_matcher.reset();
    Q_EMIT stateChanged();
}

void YaziKeys::sendKey(QWidget *target, Qt::Key key)
{
    const QScopedValueRollback sending(m_sending, true);
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &press);
}

bool YaziKeys::isTypingWidget(const QWidget *widget)
{
    return qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTextEdit *>(widget) || qobject_cast<const QPlainTextEdit *>(widget)
        || qobject_cast<const QAbstractSpinBox *>(widget) || qobject_cast<const QComboBox *>(widget) || qobject_cast<const QKeySequenceEdit *>(widget);
}

bool YaziKeys::appliesTo(QWidget *focus) const
{
    return focus && !isTypingWidget(focus) && m_accepts && m_accepts(focus);
}

bool YaziKeys::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if ((type != QEvent::KeyPress && type != QEvent::ShortcutOverride) || m_sending || !m_enabled) {
        return QObject::eventFilter(watched, event);
    }
    QWidget *focus = QApplication::focusWidget();
    if (!appliesTo(focus)) {
        return QObject::eventFilter(watched, event);
    }
    QKeyCombination key = static_cast<QKeyEvent *>(event)->keyCombination();
    // Like the folder view, follow the layout: in right-to-left layouts the parent is to the right.
    if (focus->layoutDirection() == Qt::RightToLeft && (key.key() == Qt::Key_Left || key.key() == Qt::Key_Right)) {
        key = QKeyCombination(key.keyboardModifiers(), key.key() == Qt::Key_Left ? Qt::Key_Right : Qt::Key_Left);
    }
    using Result = YaziMode::KeyMatcher::Result;
    if (type == QEvent::ShortcutOverride) {
        if (m_matcher.test(key) == Result::NoMatch) {
            return QObject::eventFilter(watched, event);
        }
        event->accept();
        return true;
    }

    const QString pendingBefore = m_matcher.pendingText();
    const Result result = m_matcher.feed(key);
    if (result == Result::Pending) {
        m_timeout->start();
    } else {
        m_timeout->stop();
    }
    if (m_matcher.pendingText() != pendingBefore) {
        Q_EMIT stateChanged();
    }
    switch (result) {
    case Result::NoMatch:
        return QObject::eventFilter(watched, event);
    case Result::Pending:
    case Result::Cancelled:
        return true;
    case Result::Matched:
        Q_EMIT commandTriggered(m_matcher.command(), YaziMode::commandTakesCount(m_matcher.command()) ? m_matcher.count() : 1);
        return true;
    }
    return true;
}
