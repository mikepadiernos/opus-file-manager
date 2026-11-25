// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchpanel.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

SearchPanel::SearchPanel(SearchWidget::WorkspaceScopeProvider workspaceScope, QWidget *parent)
    : QWidget(parent)
    , m_title(new QLabel(i18nc("@title", "Search"), this))
    , m_search(new SearchWidget(QUrl(), this, {}, std::move(workspaceScope)))
{
    setObjectName(QStringLiteral("searchPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("searchPanelHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 8, 0);
    m_title->setObjectName(QStringLiteral("searchPanelTitle"));
    headerLayout->addWidget(m_title, 1);
    auto *close = new QToolButton(header);
    close->setObjectName(QStringLiteral("searchPanelClose"));
    close->setIcon(SymbolicIcon::fromTheme(QStringLiteral("window-close")));
    close->setToolTip(i18nc("@info:tooltip", "Hide search panel"));
    close->setAccessibleName(i18nc("@action", "Hide search panel"));
    connect(close, &QToolButton::clicked, this, &SearchPanel::closeRequested);
    headerLayout->addWidget(close);
    layout->addWidget(header);
    layout->addWidget(m_search, 1);
}

SearchWidget *SearchPanel::searchWidget() const
{
    return m_search;
}

void SearchPanel::setTitleVisible(bool visible)
{
    m_title->setVisible(visible);
}
