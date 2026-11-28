// SPDX-License-Identifier: GPL-3.0-or-later

#include "windowsequencehints.h"

#include "commandpalettebar.h"
#include "pane.h"
#include "shortcutguard.h"
#include "shortcuthintpopup.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KXmlGuiWindow>

#include <QAction>
#include <QApplication>
#include <QHash>
#include <QKeyEvent>
#include <QScopedValueRollback>
#include <QTimer>

#include <algorithm>

// Exported by Qt GUI for QTest, which declares it the same way: offers a key
// to Qt's shortcut map as a key press would.
Q_GUI_EXPORT bool qt_sendShortcutOverrideEvent(QObject *o, ulong timestamp, int k, Qt::KeyboardModifiers mods, const QString &text, bool autorep, ushort count);

WindowSequenceHints::WindowSequenceHints(KXmlGuiWindow *window, std::function<Pane *()> pane)
    : QObject(window)
    , m_window(window)
    , m_pane(std::move(pane))
    , m_settle(new QTimer(this))
    , m_delay(new QTimer(this))
{
    // Once the events of the key Qt offered have been delivered.
    m_settle->setSingleShot(true);
    m_settle->setInterval(0);
    connect(m_settle, &QTimer::timeout, this, &WindowSequenceHints::settle);
    m_delay->setSingleShot(true);
    m_delay->setInterval(ShortcutHints::DefaultDelay);
    connect(m_delay, &QTimer::timeout, this, &WindowSequenceHints::showHints);
    connect(qApp, &QApplication::focusChanged, this, &WindowSequenceHints::cancel);
    qApp->installEventFilter(this);
}

WindowSequenceHints::~WindowSequenceHints()
{
    qApp->removeEventFilter(this);
}

bool WindowSequenceHints::isPending() const
{
    return m_tracker.isPending();
}

QList<QKeyCombination> WindowSequenceHints::pending() const
{
    return m_tracker.pending();
}

void WindowSequenceHints::cancel()
{
    if (!m_tracker.isPending()) {
        return;
    }
    const QList<QKeyCombination> pending = m_tracker.pending();
    m_tracker.reset();
    // Qt's shortcut map waits for the rest of a sequence until a key doesn't
    // fit it, and has no other way to end one: offer it a key that doesn't.
    const QList<QKeySequence> shortcuts = sequenceShortcuts();
    const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    for (int key = Qt::Key_F35; key >= Qt::Key_F25; --key) {
        QList<QKeyCombination> typed = pending;
        typed.append(QKeyCombination(modifiers, Qt::Key(key)));
        const bool fits = std::any_of(shortcuts.cbegin(), shortcuts.cend(), [&typed](const QKeySequence &shortcut) {
            const QList<QKeyCombination> keys = ShortcutHints::chords(shortcut);
            return keys == typed || ShortcutHints::continues(keys, typed);
        });
        if (!fits) {
            const QScopedValueRollback cancelling(m_cancelling, true);
            QWidget *focus = QApplication::focusWidget();
            qt_sendShortcutOverrideEvent(focus ? static_cast<QObject *>(focus) : m_window, 0, key, modifiers, QString(), false, 1);
            break;
        }
    }
    update();
}

QList<ShortcutHints::Entry> WindowSequenceHints::entries(KXmlGuiWindow *window, KeyBindings::Context context)
{
    QList<ShortcutHints::Entry> found;
    if (!window) {
        return found;
    }
    QList<CommandPalette::Candidate> candidates;
    const QList<QAction *> menuActions = CommandPaletteBar::collect(window, &candidates);
    QHash<const QAction *, QString> menus;
    for (qsizetype i = 0; i < menuActions.size() && i < candidates.size(); ++i) {
        if (!candidates.at(i).menuPath.isEmpty() && !menus.contains(menuActions.at(i))) {
            menus.insert(menuActions.at(i), KLocalizedString::removeAcceleratorMarker(candidates.at(i).menuPath.first()));
        }
    }
    const QString other = i18nc("@title shortcut hint group for actions in no menu", "Other actions");
    const QList<QAction *> actions = window->actionCollection()->actions();
    for (QAction *action : actions) {
        if (!action->isEnabled() || !KActionCollection::isShortcutsConfigurable(action)) {
            continue;
        }
        const QString text = KLocalizedString::removeAcceleratorMarker(action->text());
        if (text.isEmpty()) {
            continue;
        }
        const QString group = menus.value(action, other);
        for (const QKeySequence &shortcut : KeyBindings::keys(action->shortcuts())) {
            // The focus widget keeps these keys; see ShortcutGuard.
            if (KeyBindings::isReserved(shortcut[0], context)) {
                continue;
            }
            found.append({ShortcutHints::chords(shortcut), text, group});
        }
    }
    return found;
}

QList<QKeySequence> WindowSequenceHints::sequenceShortcuts() const
{
    QList<QKeySequence> found;
    const QList<QAction *> actions = m_window->actionCollection()->actions();
    for (const QAction *action : actions) {
        if (!action->isEnabled()) {
            continue;
        }
        for (const QKeySequence &shortcut : action->shortcuts()) {
            if (shortcut.count() > 1) {
                found.append(shortcut);
            }
        }
    }
    return found;
}

bool WindowSequenceHints::inWindow(const QObject *object) const
{
    return object->isWidgetType() && static_cast<const QWidget *>(object)->window() == m_window;
}

bool WindowSequenceHints::eventFilter(QObject *watched, QEvent *event)
{
    if (m_cancelling) {
        return QObject::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        // The window, then the focus widget, receive it; follow the widget's.
        if (inWindow(watched)) {
            const bool ended = m_tracker.shortcutOverride(static_cast<QKeyEvent *>(event)->keyCombination());
            if (m_tracker.hasCandidate()) {
                m_settle->start();
            }
            if (ended) {
                update();
            }
        }
        break;
    case QEvent::KeyPress:
        // Not the key presses that widgets send each other, as Yazi mode does.
        if (event->spontaneous() && watched->isWidgetType() && m_tracker.keyPressed(static_cast<QKeyEvent *>(event)->keyCombination())) {
            update();
        }
        break;
    case QEvent::Shortcut:
        if (m_tracker.shortcutActivated()) {
            update();
        }
        break;
    case QEvent::KeyRelease:
        if ((m_tracker.isPending() || m_tracker.hasCandidate()) && event->spontaneous() && inWindow(watched)) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (m_tracker.keyReleased(key->keyCombination(), key->isAutoRepeat(), m_tracker.isPending() ? sequenceShortcuts() : QList<QKeySequence>())) {
                update();
            }
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void WindowSequenceHints::settle()
{
    if (m_tracker.hasCandidate() && m_tracker.settle(sequenceShortcuts())) {
        update();
    }
}

void WindowSequenceHints::update()
{
    if (!m_tracker.isPending()) {
        m_delay->stop();
        if (m_popup) {
            m_popup->dismiss(ShortcutHintPopup::Source::Window);
        }
        return;
    }
    if (m_popup && m_popup->isShowing(ShortcutHintPopup::Source::Window)) {
        showHints();
    } else if (!m_delay->isActive()) {
        m_delay->start();
    }
}

void WindowSequenceHints::showHints()
{
    Pane *pane = m_pane ? m_pane() : nullptr;
    if (!pane || !m_tracker.isPending()) {
        return;
    }
    const KeyBindings::Context context = ShortcutGuard::focusContext(QApplication::focusWidget()).value_or(KeyBindings::Context::Application);
    const QList<QKeyCombination> pending = m_tracker.pending();
    const QList<ShortcutHints::Hint> hints = ShortcutHints::hints(entries(m_window, context), pending, ShortcutHints::KeyStyle::Native);
    if (hints.isEmpty()) {
        return;
    }
    const QKeyCombination none = QKeyCombination::fromCombined(0);
    const QString typed =
        QKeySequence(pending.value(0, none), pending.value(1, none), pending.value(2, none), pending.value(3, none)).toString(QKeySequence::NativeText);
    if (m_popup && m_popup != pane->shortcutHintPopup()) {
        m_popup->dismiss(ShortcutHintPopup::Source::Window);
    }
    m_popup = pane->shortcutHintPopup();
    m_popup->showHints(ShortcutHintPopup::Source::Window,
                       i18nc("@title shortcut hints; %1 is the keys typed so far", "%1…", typed),
                       hints,
                       i18nc("@info shortcut hints footer for window shortcuts", "Scroll to turn pages · Escape cancels"),
                       pane->shortcutHintArea());
}
