// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keyboardhints.h"

#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

#include <functional>

class KFilePlacesView;
class Pane;
class QAbstractItemView;
class QLabel;
class QModelIndex;

/**
 * Keyboard hints: short key labels over the targets the keyboard can reach
 * directly, such as the visible files and folders of a pane. Typing a label
 * activates its target; typing narrows the labels, and Escape, any key that
 * continues no label, scrolling, resizing, a listing change or moving the
 * focus cancels them.
 *
 * The overlay covers its window without taking mouse events. While it shows,
 * it has the keyboard focus, so keys reach neither the window's shortcuts nor
 * the widget that had the focus, which gets it back afterwards. It doesn't
 * accept input method text and shows and hides without animation.
 */
class KeyboardHintOverlay : public QWidget
{
    Q_OBJECT

public:
    /// What typing a file or folder label does without Shift; Shift does the other.
    enum class ItemAction {
        Select,
        Open,
    };

    struct Target {
        KeyboardHints::Kind kind = KeyboardHints::Kind::Item;
        /// The widget the target is in, and its area there.
        QPointer<QWidget> widget;
        QRect rect;
        /// The target's name, for assistive technology.
        QString name;
        /// Activates the target; @p shift is true when Shift was held on the label's last key.
        std::function<void(bool shift)> activate;
    };

    explicit KeyboardHintOverlay(QWidget *window);
    ~KeyboardHintOverlay() override;

    /**
     * Labels the visible items of @p pane's file view, its location segments
     * and its other tabs, and the visible places of @p places, and takes the
     * keyboard focus.
     * @param alphabet the keys labels are made of
     * @return false, showing nothing, when there is nothing to label
     */
    bool start(Pane *pane, const QList<KFilePlacesView *> &places, const QString &alphabet, ItemAction action);
    /// Labels @p targets and takes the keyboard focus; false when there are none.
    bool start(const QList<Target> &targets, const QString &alphabet);
    /// Hides the labels, giving the keyboard focus back when the overlay has it.
    void cancel();
    bool isActive() const;

    /// The shown targets and their labels, in the order of targets().
    QList<Target> targets() const;
    QStringList labels() const;
    /// The label widgets, in the order of targets(); hidden ones no longer match the typed keys.
    QList<QLabel *> labelWidgets() const;
    QString typed() const;

    /// The visible items of @p view, each activated with @p activate.
    static QList<Target> itemTargets(QAbstractItemView *view, const std::function<void(const QModelIndex &, bool shift)> &activate);
    /// The visible location segments, tabs other than the current one, and places.
    static QList<Target> locationTargets(Pane *pane);
    static QList<Target> tabTargets(Pane *pane);
    static QList<Target> placeTargets(KFilePlacesView *view);

Q_SIGNALS:
    /// The labels were hidden, whether a target was activated or not.
    void finished();

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void watchResize(QWidget *widget);
    void watchView(QAbstractItemView *view);
    void updateLabels();
    void finish(bool restoreFocus);

    QList<Target> m_targets;
    QStringList m_labels;
    QList<QLabel *> m_labelWidgets;
    KeyboardHints::Matcher m_matcher;
    QPointer<QWidget> m_previousFocus;
    QList<QPointer<QWidget>> m_watched;
    /// The parent of connections that cancel the labels; deleted when they hide.
    QObject *m_watchers = nullptr;
    bool m_active = false;
};
