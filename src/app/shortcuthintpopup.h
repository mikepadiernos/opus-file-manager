// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "shortcuthints.h"

#include <QFrame>
#include <QPointer>

class QGridLayout;
class QLabel;
class QPropertyAnimation;

/**
 * A which-key style list of the keys that can follow a partly typed key
 * sequence, or of the keys that work where the focus is, floating over the
 * bottom of its parent, a pane. It never takes the keyboard focus, so keys
 * keep going where they went: whoever shows it turns its pages, with
 * turnPage(), and hides it. The mouse wheel turns its pages too.
 *
 * It slides in unless animations are off, and announces what it lists to
 * assistive technology.
 */
class ShortcutHintPopup : public QFrame
{
    Q_OBJECT

public:
    /// What the popup lists, so that each source hides only its own hints.
    enum class Source {
        /// A pending sequence of Yazi mode or Vim-style navigation keys.
        Keys,
        /// A pending multi-key window shortcut.
        Window,
        /// The keys that work where the focus is, shown on demand.
        Reference,
    };

    explicit ShortcutHintPopup(QWidget *parent);

    /**
     * Lists @p hints under @p title, with @p note on how to page and leave
     * it, at the bottom of @p area, a descendant of the parent, or of the
     * parent itself without one. Showing new hints from the same source
     * keeps the popup in place.
     */
    void showHints(Source source, const QString &title, const QList<ShortcutHints::Hint> &hints, const QString &note, QWidget *area = nullptr);
    /// Hides the popup if it shows hints from @p source.
    void dismiss(Source source);
    /// Whether the popup is shown with hints from @p source.
    bool isShowing(Source source) const;

    QString title() const;
    QList<ShortcutHints::Hint> hints() const;
    /// Turns @p delta pages forward, or back when negative, as far as there are pages.
    void turnPage(int delta);
    int page() const;
    int pageCount() const;
    /// The hints on the current page, in reading order.
    QList<ShortcutHints::Hint> pageHints() const;

    /// The pages @p key turns: -1 for Page Up, Up or Left, 1 for Page Down, Down or Right, 0 for other keys.
    static int pageDelta(QKeyCombination key);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void layOut();
    void showPage();
    void place(bool slide);
    QString spokenText() const;

    Source m_source = Source::Keys;
    QString m_note;
    QList<ShortcutHints::Hint> m_hints;
    QList<ShortcutHints::Page> m_pages;
    int m_page = 0;
    QPointer<QWidget> m_area;
    QLabel *m_title;
    QWidget *m_grid;
    QGridLayout *m_gridLayout;
    QLabel *m_footer;
    QPropertyAnimation *m_slide;
    int m_wheelDelta = 0;
    int m_descriptionWidth = 0;
    int m_columnGap = 0;
};
