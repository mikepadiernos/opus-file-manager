// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcutreference.h"

#include "pane.h"
#include "shortcutguard.h"
#include "shortcuthintpopup.h"
#include "windowsequencehints.h"
#include "yazicolumns.h"
#include "yazikeys.h"

#include <KLocalizedString>
#include <KXmlGuiWindow>

#include <QApplication>
#include <QKeyEvent>
#include <QSet>

namespace
{
bool isModifierKey(int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_unknown:
        return true;
    default:
        return false;
    }
}

bool hasCommandModifier(QKeyCombination key)
{
    return key.keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

int firstKey(const ShortcutHints::Entry &entry)
{
    return entry.keys.isEmpty() ? 0 : YaziMode::normalizedKey(entry.keys.first()).toCombined();
}
}

ShortcutReference::ShortcutReference(KXmlGuiWindow *window, std::function<Pane *()> pane)
    : QObject(window)
    , m_window(window)
    , m_pane(std::move(pane))
{
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (now) {
            hide();
        }
    });
}

ShortcutReference::~ShortcutReference()
{
    qApp->removeEventFilter(this);
}

void ShortcutReference::toggle()
{
    if (isShowing()) {
        hide();
    } else {
        show();
    }
}

bool ShortcutReference::isShowing() const
{
    return m_popup && m_popup->isShowing(ShortcutHintPopup::Source::Reference);
}

QList<ShortcutHints::Hint> ShortcutReference::hints(KXmlGuiWindow *window, Pane *pane, QWidget *focus, QString *title)
{
    const KeyBindings::Context context = ShortcutGuard::focusContext(focus).value_or(KeyBindings::Context::Application);
    QList<ShortcutHints::Entry> windowEntries = WindowSequenceHints::entries(window, context);
    QList<ShortcutHints::Entry> keyEntries;
    QString where = KeyBindings::contextText(context);
    if (pane && focus && pane->isAncestorOf(focus)) {
        YaziKeys *yazi = pane->yaziColumns()->keys();
        YaziKeys *vim = pane->vimKeys();
        YaziKeys *keys = nullptr;
        if (yazi->isEnabled() && yazi->appliesTo(focus)) {
            keys = yazi;
            where = i18nc("@title shortcut reference context", "Yazi mode");
        } else if (vim->isEnabled() && vim->appliesTo(focus)) {
            keys = vim;
            where = i18nc("@title shortcut reference context", "Vim-style navigation");
        }
        if (keys) {
            keyEntries = ShortcutHints::keymapEntries(keys->keymap());
            // Vim-style navigation lets window shortcuts keep keys with Ctrl,
            // Alt or Meta; Yazi mode's keys and plain keys take precedence.
            const bool yield = keys == vim;
            QSet<int> windowFirst;
            for (const ShortcutHints::Entry &entry : std::as_const(windowEntries)) {
                windowFirst.insert(firstKey(entry));
            }
            QSet<int> keysFirst;
            keyEntries.removeIf([&](const ShortcutHints::Entry &entry) {
                const bool yielded = yield && hasCommandModifier(entry.keys.first()) && windowFirst.contains(firstKey(entry));
                if (!yielded) {
                    keysFirst.insert(firstKey(entry));
                }
                return yielded;
            });
            windowEntries.removeIf([&](const ShortcutHints::Entry &entry) {
                return keysFirst.contains(firstKey(entry));
            });
        }
    }
    if (title) {
        *title = i18nc("@title shortcut reference; %1 is where the keyboard focus is, such as a file view", "Shortcuts: %1", where);
    }
    return ShortcutHints::hints(keyEntries, {}, ShortcutHints::KeyStyle::Typed, true)
        + ShortcutHints::hints(windowEntries, {}, ShortcutHints::KeyStyle::Native, true);
}

void ShortcutReference::show()
{
    Pane *pane = m_pane ? m_pane() : nullptr;
    if (!pane) {
        return;
    }
    QWidget *focus = QApplication::focusWidget();
    if (focus && focus->window() != m_window) {
        focus = nullptr;
    }
    QString title;
    QList<ShortcutHints::Hint> found = hints(m_window, pane, focus, &title);
    if (found.isEmpty()) {
        found.append({QString(), i18nc("@item shortcut reference", "No shortcuts work here."), QString(), false});
    }
    const std::optional<KeyBindings::Context> context = ShortcutGuard::focusContext(focus);
    m_takesKeys = context == KeyBindings::Context::FileView || context == KeyBindings::Context::FolderTree;
    if (m_popup && m_popup != pane->shortcutHintPopup()) {
        m_popup->dismiss(ShortcutHintPopup::Source::Reference);
    }
    m_popup = pane->shortcutHintPopup();
    m_popup->showHints(
        ShortcutHintPopup::Source::Reference,
        title,
        found,
        m_takesKeys ? i18nc("@info shortcut reference footer", "Page Up and Page Down turn pages · Escape closes · Help › Keyboard Map… lists every shortcut")
                    : i18nc("@info shortcut reference footer", "Scroll to turn pages · Any key closes · Help › Keyboard Map… lists every shortcut"),
        pane->shortcutHintArea());
    qApp->removeEventFilter(this);
    qApp->installEventFilter(this);
}

void ShortcutReference::hide()
{
    qApp->removeEventFilter(this);
    if (m_popup) {
        m_popup->dismiss(ShortcutHintPopup::Source::Reference);
    }
}

bool ShortcutReference::takesKeys() const
{
    return m_takesKeys;
}

bool ShortcutReference::handleKey(QKeyEvent *key, bool press)
{
    if (isModifierKey(key->key())) {
        return false;
    }
    const QKeyCombination combination = key->keyCombination();
    const int delta = ShortcutHintPopup::pageDelta(combination);
    if (takesKeys() && (combination == QKeyCombination(Qt::Key_Escape) || delta != 0)) {
        // An accepted override sends the key to the focus widget, where it is eaten.
        key->accept();
        if (press) {
            if (delta != 0) {
                m_popup->turnPage(delta);
            } else {
                hide();
            }
        }
        return true;
    }
    hide();
    return false;
}

bool ShortcutReference::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type != QEvent::ShortcutOverride && type != QEvent::KeyPress) {
        return QObject::eventFilter(watched, event);
    }
    if (!isShowing()) {
        // Other hints took the popup.
        qApp->removeEventFilter(this);
        return QObject::eventFilter(watched, event);
    }
    QWidget *focus = QApplication::focusWidget();
    if (!focus || watched != focus || focus->window() != m_window) {
        return QObject::eventFilter(watched, event);
    }
    if (handleKey(static_cast<QKeyEvent *>(event), type == QEvent::KeyPress)) {
        return true;
    }
    return QObject::eventFilter(watched, event);
}
