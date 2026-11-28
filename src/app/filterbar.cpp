// SPDX-License-Identifier: GPL-3.0-or-later

#include "filterbar.h"

#include "settings.h"
#include "symbolicicon.h"

#include <KLocalizedString>

#include <QActionGroup>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>

FilterBar::FilterBar(QWidget *parent)
    : QWidget(parent)
    , m_text(new QLineEdit(this))
    , m_scopeButton(new QToolButton(this))
{
    m_scope = static_cast<NameFilter::Scope>(qBound(0, static_cast<int>(Settings::defaultFilterScope()), 2));
    m_text->setPlaceholderText(i18nc("@info:placeholder", "Filter by name…"));
    m_text->setClearButtonEnabled(true);
    m_text->installEventFilter(this);
    connect(m_text, &QLineEdit::textChanged, this, [this] {
        Q_EMIT filterChanged(filter());
    });

    auto *menu = new QMenu(m_scopeButton);
    auto *group = new QActionGroup(menu);
    const QList<std::pair<NameFilter::Scope, QString>> scopes{
        {NameFilter::Scope::Files, i18nc("@option:radio filter applies to", "Files")},
        {NameFilter::Scope::Folders, i18nc("@option:radio filter applies to", "Folders")},
        {NameFilter::Scope::FilesAndFolders, i18nc("@option:radio filter applies to", "Files and Folders")},
    };
    for (const auto &[scope, text] : scopes) {
        QAction *action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(scope == m_scope);
        action->setData(static_cast<int>(scope));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, scope] {
            setScope(scope);
        });
    }
    m_scopeButton->setObjectName(QStringLiteral("filterScope"));
    m_scopeButton->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-filter")));
    m_scopeButton->setToolTip(i18nc("@info:tooltip", "What the filter applies to"));
    m_scopeButton->setAutoRaise(true);
    m_scopeButton->setPopupMode(QToolButton::InstantPopup);
    m_scopeButton->setMenu(menu);

    auto *closeButton = new QToolButton(this);
    closeButton->setObjectName(QStringLiteral("filterClose"));
    closeButton->setIcon(SymbolicIcon::fromTheme(QStringLiteral("dialog-close")));
    closeButton->setToolTip(i18nc("@info:tooltip", "Clear and close the filter"));
    closeButton->setAutoRaise(true);
    connect(closeButton, &QToolButton::clicked, this, &FilterBar::close);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_scopeButton);
    layout->addWidget(closeButton);
    hide();
}

NameFilter FilterBar::filter() const
{
    return NameFilter(m_text->text(), m_scope);
}

void FilterBar::setFilter(const NameFilter &filter)
{
    m_text->setText(filter.text());
    setScope(filter.scope());
}

void FilterBar::open(const QString &typedText)
{
    show();
    if (!typedText.isEmpty()) {
        m_text->setText(m_text->text() + typedText);
    }
    m_text->setFocus();
}

void FilterBar::close()
{
    m_text->clear();
    hide();
    Q_EMIT closed();
}

void FilterBar::setEscapeKeepsFilter(bool keep)
{
    m_escapeKeepsFilter = keep;
}

bool FilterBar::escapeKeepsFilter() const
{
    return m_escapeKeepsFilter;
}

bool FilterBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_text && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) {
            if (m_escapeKeepsFilter && !m_text->text().isEmpty()) {
                Q_EMIT leaveRequested();
            } else {
                close();
            }
            return true;
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            Q_EMIT activateFirstRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FilterBar::setScope(NameFilter::Scope scope)
{
    m_scope = scope;
    for (QAction *action : m_scopeButton->menu()->actions()) {
        action->setChecked(action->data().toInt() == static_cast<int>(scope));
    }
    Q_EMIT filterChanged(filter());
}
