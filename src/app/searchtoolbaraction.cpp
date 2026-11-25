// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchtoolbaraction.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

SearchToolbarAction::SearchToolbarAction(QObject *parent)
    : QWidgetAction(parent)
{
    setText(i18nc("@action", "Search Field"));
    setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    setToolTip(i18nc("@info:tooltip", "Search with the search panel's scope and filters, set by default on the Search page of Preferences"));
}

void SearchToolbarAction::setSavedSearchProvider(SavedSearchProvider provider)
{
    m_savedSearches = std::move(provider);
}

QWidget *SearchToolbarAction::createWidget(QWidget *parent)
{
    auto *field = new QLineEdit(parent);
    field->setObjectName(QStringLiteral("toolbarSearch"));
    field->setPlaceholderText(i18nc("@info:placeholder", "Search…"));
    field->setToolTip(toolTip());
    field->setAccessibleName(i18nc("@label", "Search"));
    field->setClearButtonEnabled(true);
    field->addAction(SymbolicIcon::fromTheme(QStringLiteral("edit-find")), QLineEdit::LeadingPosition);
    auto *saved = field->addAction(SymbolicIcon::fromTheme(QStringLiteral("view-list-details")), QLineEdit::TrailingPosition);
    saved->setObjectName(QStringLiteral("toolbarSavedSearches"));
    saved->setText(i18nc("@action", "Saved Searches"));
    saved->setToolTip(i18nc("@info:tooltip", "Run a saved search from the search panel"));
    connect(saved, &QAction::triggered, this, [this, field] {
        showSavedSearches(field);
    });
    field->setMinimumWidth(field->fontMetrics().averageCharWidth() * 16);
    field->setMaximumWidth(field->fontMetrics().averageCharWidth() * 40);
    connect(field, &QLineEdit::returnPressed, this, [this, field] {
        Q_EMIT searchRequested(field->text());
    });
    field->installEventFilter(this);
    return field;
}

void SearchToolbarAction::showSavedSearches(QWidget *field)
{
    auto *menu = new QMenu(field);
    menu->setObjectName(QStringLiteral("toolbarSavedSearchesMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QStringList names = m_savedSearches ? m_savedSearches() : QStringList();
    for (const QString &name : names) {
        // Plain text: an ampersand in a saved search name is not a mnemonic.
        QString label = name;
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        menu->addAction(label, this, [this, name] {
            Q_EMIT savedSearchRequested(name);
        });
    }
    if (names.isEmpty()) {
        menu->addAction(i18nc("@item:inmenu", "No Saved Searches"))->setEnabled(false);
    }
    menu->popup(field->mapToGlobal(QPoint(field->width() - menu->sizeHint().width(), field->height())));
}

bool SearchToolbarAction::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && qobject_cast<QLineEdit *>(watched)) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if ((key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
            if (key->key() == Qt::Key_Down) {
                Q_EMIT resultsRequested();
                return true;
            }
            if (key->key() == Qt::Key_Escape) {
                Q_EMIT escapePressed();
                return true;
            }
        }
    }
    return QWidgetAction::eventFilter(watched, event);
}
