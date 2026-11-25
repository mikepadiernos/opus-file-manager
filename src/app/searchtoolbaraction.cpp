// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchtoolbaraction.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QKeyEvent>
#include <QLineEdit>

SearchToolbarAction::SearchToolbarAction(QObject *parent)
    : QWidgetAction(parent)
{
    setText(i18nc("@action", "Search Field"));
    setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    setToolTip(i18nc("@info:tooltip", "Search the active pane's folder"));
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
    field->setMinimumWidth(field->fontMetrics().averageCharWidth() * 16);
    field->setMaximumWidth(field->fontMetrics().averageCharWidth() * 40);
    connect(field, &QLineEdit::returnPressed, this, [this, field] {
        Q_EMIT searchRequested(field->text());
    });
    field->installEventFilter(this);
    return field;
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
