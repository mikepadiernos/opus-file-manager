// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "yazimode.h"

#include <QObject>

#include <functional>

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
    void sendKey(QWidget *target, Qt::Key key);

    /// Whether @p widget takes typed text, such as a line edit, a text editor, a spin box or a combo box.
    static bool isTypingWidget(const QWidget *widget);

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
};
