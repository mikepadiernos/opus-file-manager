// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidgetAction>

/// Optional search field that can be placed on a toolbar. Each toolbar gets its own line edit.
class SearchToolbarAction : public QWidgetAction
{
    Q_OBJECT

public:
    explicit SearchToolbarAction(QObject *parent = nullptr);

Q_SIGNALS:
    /// Emitted when Return is pressed in a field; @p text may be empty.
    void searchRequested(const QString &text);
    /// Emitted when Down is pressed in a field, to move focus to the results.
    void resultsRequested();
    /// Emitted when Escape is pressed in a field.
    void escapePressed();

protected:
    QWidget *createWidget(QWidget *parent) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};
