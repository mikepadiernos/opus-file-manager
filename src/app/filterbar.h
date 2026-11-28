// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "namefilter.h"

#include <QWidget>

class QLineEdit;
class QToolButton;

/**
 * A pane's filter bar: the filter text, a button choosing whether it
 * applies to files, folders or both, and a close button. Escape or closing
 * clears the text and hides the bar, unless Escape is set to keep the filter.
 */
class FilterBar : public QWidget
{
    Q_OBJECT

public:
    explicit FilterBar(QWidget *parent = nullptr);

    NameFilter filter() const;
    void setFilter(const NameFilter &filter);

    /**
     * Shows the bar and focuses its text field.
     * @param typedText characters typed in the view, appended to the text
     */
    void open(const QString &typedText = QString());

    /// Clears the text and hides the bar.
    void close();

    /**
     * Makes Escape in the text field keep a filter that has text, and only
     * ask to give the focus back to the view with leaveRequested(), as Vim
     * leaves a mode. Escape still closes the bar when the text is empty.
     */
    void setEscapeKeepsFilter(bool keep);
    bool escapeKeepsFilter() const;

Q_SIGNALS:
    void filterChanged(const NameFilter &filter);
    /// Enter was pressed in the text field.
    void activateFirstRequested();
    /// The bar closed; focus should go back to the view.
    void closed();
    /// Escape kept the filter; focus should go back to the view. See setEscapeKeepsFilter().
    void leaveRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setScope(NameFilter::Scope scope);

    QLineEdit *m_text = nullptr;
    QToolButton *m_scopeButton = nullptr;
    NameFilter::Scope m_scope = NameFilter::Scope::Files;
    bool m_escapeKeepsFilter = false;
};
