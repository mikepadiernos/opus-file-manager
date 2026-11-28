// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazikeys.h"

#include "vimnavigation.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QScopedValueRollback>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

#include <algorithm>

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

void YaziKeys::setKeymap(const YaziMode::Keymap &keymap)
{
    m_matcher.setKeymap(keymap);
    m_timeout->stop();
    Q_EMIT stateChanged();
}

void YaziKeys::setModifiedKeysYield(bool yield)
{
    m_modifiedKeysYield = yield;
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

void YaziKeys::setEscapeOverride(std::function<bool()> active)
{
    m_escapeActive = std::move(active);
}

bool YaziKeys::overridesEscape(QKeyCombination key) const
{
    return key.key() == Qt::Key_Escape && (key.keyboardModifiers() & ~Qt::KeypadModifier) == Qt::NoModifier && !m_matcher.isPending() && m_escapeActive
        && m_escapeActive();
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

void YaziKeys::sendKey(QWidget *target, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    const QScopedValueRollback sending(m_sending, true);
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(target, &press);
}

bool YaziKeys::move(QAbstractItemView *view, YaziMode::Command command, int count, Qt::KeyboardModifiers modifiers, const std::function<void()> &afterStep)
{
    using YaziMode::Command;
    Qt::Key key = Qt::Key_unknown;
    int steps = qBound(1, count, YaziMode::MaximumCount);
    switch (command) {
    case Command::MoveUp:
        key = Qt::Key_Up;
        break;
    case Command::MoveDown:
        key = Qt::Key_Down;
        break;
    case Command::MoveFirst:
        key = Qt::Key_Home;
        steps = 1;
        break;
    case Command::MoveLast:
        key = Qt::Key_End;
        steps = 1;
        break;
    case Command::PageUp:
        key = Qt::Key_PageUp;
        break;
    case Command::PageDown:
        key = Qt::Key_PageDown;
        break;
    case Command::HalfPageUp:
        key = Qt::Key_Up;
        steps *= halfPageRows(view);
        break;
    case Command::HalfPageDown:
        key = Qt::Key_Down;
        steps *= halfPageRows(view);
        break;
    default:
        return false;
    }
    for (int step = 0; step < steps; ++step) {
        const QPersistentModelIndex before = view->currentIndex();
        sendKey(view, key, modifiers);
        if (afterStep) {
            afterStep();
        }
        if (before == view->currentIndex()) {
            break;
        }
    }
    return true;
}

bool YaziKeys::isTypingWidget(const QWidget *widget)
{
    return qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTextEdit *>(widget) || qobject_cast<const QPlainTextEdit *>(widget)
        || qobject_cast<const QAbstractSpinBox *>(widget) || qobject_cast<const QComboBox *>(widget) || qobject_cast<const QKeySequenceEdit *>(widget);
}

bool YaziKeys::isEditing(const QAbstractItemView *view)
{
    // QAbstractItemView::state() is protected; an inline editor is a typing widget on the viewport.
    const QList<QWidget *> children = view->viewport()->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
    return std::any_of(children.cbegin(), children.cend(), [](const QWidget *child) {
        return child->isVisible() && isTypingWidget(child);
    });
}

int YaziKeys::halfPageRows(const QAbstractItemView *view)
{
    QModelIndex probe = view->currentIndex();
    if (!probe.isValid() && view->model()) {
        probe = view->model()->index(0, 0, view->rootIndex());
    }
    int rowHeight = probe.isValid() ? view->visualRect(probe).height() : 0;
    if (rowHeight <= 0) {
        rowHeight = view->fontMetrics().height();
    }
    return qMax(1, view->viewport()->height() / qMax(1, rowHeight) / 2);
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
        if (overridesEscape(key)) {
            event->accept();
            return true;
        }
        if (m_matcher.test(key) == Result::NoMatch) {
            return QObject::eventFilter(watched, event);
        }
        // Leave a key with Ctrl, Alt or Meta to a window shortcut that uses it; the key press arrives when none does.
        if (m_modifiedKeysYield && VimNavigation::yieldsToShortcuts(YaziMode::normalizedKey(key))) {
            return QObject::eventFilter(watched, event);
        }
        event->accept();
        return true;
    }
    if (overridesEscape(key)) {
        Q_EMIT escapePressed();
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
