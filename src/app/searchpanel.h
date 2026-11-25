// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "searchwidget.h"

#include <QWidget>

class QLabel;

/// Embedded search panel shown below or beside both panes, like the terminal.
class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(SearchWidget::WorkspaceScopeProvider workspaceScope, QWidget *parent = nullptr);
    SearchWidget *searchWidget() const;
    void setTitleVisible(bool visible);

Q_SIGNALS:
    void closeRequested();

private:
    QLabel *m_title;
    SearchWidget *m_search;
};
