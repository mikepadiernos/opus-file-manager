// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "yazimode.h"

#include <QObject>

#include <functional>

class QAbstractItemView;
class QTimer;
class QWidget;

/**
 * Applies a Yazi mode key preset to the widgets it watches. Keys are matched
 * only while the filter is enabled and the focus widget passes the focus
 * filter; keys the preset doesn't bind are let through untouched, so the
 * widgets' own keys, type-ahead and window shortcuts keep working. Matched
 * keys also override window shortcuts while the focus is in a watched
 * widget, so a preset key such as Ctrl+D reaches the columns.
 *
 * A partly typed sequence expires after PendingTimeout milliseconds and is
 * cancelled by Escape or when the focus moves.
 */
class YaziKeys : public QObject
{
    Q_OBJECT

public:
    /// How long a partly typed key sequence waits for its next key, in milliseconds.
    static constexpr int PendingTimeout = 1000;

    explicit YaziKeys(QObject *parent = nullptr);

    void setPreset(YaziMode::Preset preset);
    YaziMode::Preset preset() const;
    /// Matches @p keymap instead of the preset's keys, as Vim-style navigation outside Yazi mode does.
    void setKeymap(const YaziMode::Keymap &keymap);
    /**
     * Lets window shortcuts keep keys with Ctrl, Alt or Meta: such a key is
     * matched only when no shortcut takes it. Plain keys still override
     * window shortcuts.
     */
    void setModifiedKeysYield(bool yield);
    /// Disabling also cancels a pending sequence.
    void setEnabled(bool enabled);
    bool isEnabled() const;
    /**
     * Sets which focus widgets the keys apply to. Widgets that take typed
     * text, as isTypingWidget() tells, are always left alone.
     */
    void setFocusFilter(std::function<bool(QWidget *)> accepts);
    /**
     * Lets Escape, when no sequence is pending, emit escapePressed() instead
     * of reaching the focus widget while @p active returns true, as when a
     * search started from the columns can be cancelled.
     */
    void setEscapeOverride(std::function<bool()> active);

    /// Matches keys sent to @p widget and, for a container, keys its children let through.
    void watch(QWidget *widget);

    bool isPending() const;
    /// The count and keys typed so far of an incomplete sequence, such as "g", or empty.
    QString pendingText() const;
    void cancel();

    /// Sends @p key to @p target as the widget's own key, bypassing the preset.
    void sendKey(QWidget *target, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    /**
     * Moves the current item of @p view for a movement command, such as
     * MoveDown, MoveLast or HalfPageUp, repeated @p count times where the
     * command takes a count, by sending the view its own keys with
     * @p modifiers. The view moves the focus and selection as those keys
     * always do, in every view mode. @p afterStep runs after each key.
     * @returns false when @p command isn't a movement command
     */
    bool move(QAbstractItemView *view,
              YaziMode::Command command,
              int count,
              Qt::KeyboardModifiers modifiers = Qt::NoModifier,
              const std::function<void()> &afterStep = {});

    /// Whether @p widget takes typed text, such as a line edit, a text editor, a spin box or a combo box.
    static bool isTypingWidget(const QWidget *widget);
    /// Whether @p view has an inline editor open, as when renaming in place.
    static bool isEditing(const QAbstractItemView *view);
    /// About half the rows @p view shows at once.
    static int halfPageRows(const QAbstractItemView *view);

Q_SIGNALS:
    /// A key sequence completed; run @p command @p count times where that makes sense.
    void commandTriggered(YaziMode::Command command, int count);
    /// The preset or the pending keys changed.
    void stateChanged();
    /// Escape was pressed while the escape override was active; see setEscapeOverride().
    void escapePressed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool appliesTo(QWidget *focus) const;
    bool overridesEscape(QKeyCombination key) const;

    YaziMode::Preset m_preset = YaziMode::Preset::Standard;
    YaziMode::KeyMatcher m_matcher;
    QTimer *m_timeout = nullptr;
    std::function<bool(QWidget *)> m_accepts;
    std::function<bool()> m_escapeActive;
    bool m_enabled = false;
    bool m_sending = false;
    bool m_modifiedKeysYield = false;
};
