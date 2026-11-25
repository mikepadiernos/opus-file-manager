// SPDX-License-Identifier: GPL-3.0-or-later

#include "fieldeditdialog.h"

#include "tagassignment.h"

#include <KLocalizedString>
#include <KMessageWidget>

#include <QAction>
#include <QCalendarWidget>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidgetAction>

FieldEditDialog::FieldEditDialog(const QString &root, const QStringList &absolutePaths, QWidget *parent)
    : QDialog(parent)
    , m_root(root)
    , m_paths(absolutePaths)
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("fieldEditDialog"));
    setWindowTitle(i18nc("@title:window", "Edit Fields"));
    m_error->setObjectName(QStringLiteral("fieldEditError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);
    m_error->hide();

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(this);
    intro->setObjectName(QStringLiteral("fieldEditIntro"));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QString error;
    m_library = TagLibrary::Library::load(root, &error);
    QStringList relativePaths;
    for (const QString &path : absolutePaths) {
        relativePaths.append(m_library.isNull() ? QString() : m_library.relativePath(path));
    }

    if (m_library.isNull()) {
        intro->setText(i18nc("@info", "Could not read the tag library: %1", error));
    } else if (m_library.fields().isEmpty()) {
        intro->setText(
            i18nc("@info", "This library has no fields yet. Create text or date fields to store values such as a title or the date a photo was taken."));
        auto *manage = new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "&Manage Fields…"), this);
        manage->setObjectName(QStringLiteral("fieldEditManage"));
        connect(manage, &QPushButton::clicked, this, [this] {
            Q_EMIT manageFieldsRequested(m_root);
            reject();
        });
        layout->addWidget(manage, 0, Qt::AlignLeft);
    } else {
        intro->setText(i18ncp("@info",
                              "Fields of the selected item. Clear a value to remove it.",
                              "Fields of %1 selected items. Only the fields you edit are changed; clear a value to remove it from every item.",
                              absolutePaths.size()));
        auto *form = new QFormLayout;
        for (const TagLibrary::Field &field : m_library.fields()) {
            QLineEdit *editor = addEditor(field, relativePaths);
            editor->setReadOnly(m_library.isReadOnly());
            form->addRow(i18nc("@label:textbox field name", "%1:", field.name), editor);
        }
        layout->addLayout(form);
        auto *note =
            new QLabel(m_library.isReadOnly() ? i18nc("@info", "The library is read-only, so its values can't be changed.")
                                              : i18nc("@info", "Values are stored in the library. Files and their labels, ratings and notes aren't changed."),
                       this);
        note->setObjectName(QStringLiteral("fieldEditNote"));
        note->setWordWrap(true);
        layout->addWidget(note);
    }
    layout->addStretch();
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &FieldEditDialog::apply);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    validate();
    resize(440, sizeHint().height());
}

QLineEdit *FieldEditDialog::addEditor(const TagLibrary::Field &field, const QStringList &relativePaths)
{
    auto *editor = new QLineEdit(this);
    editor->setObjectName(QStringLiteral("fieldValue_") + field.id);
    editor->setAccessibleName(field.name);
    editor->setClearButtonEnabled(true);

    QStringList values;
    for (const QString &path : relativePaths) {
        const TagLibrary::Entry *entry = path.isEmpty() ? nullptr : m_library.entry(path);
        const QString value = entry ? entry->fields.value(field.id) : QString();
        if (!values.contains(value)) {
            values.append(value);
        }
    }
    if (values.size() > 1) {
        editor->setPlaceholderText(i18nc("@info:placeholder the selected items have different values", "Several values"));
        editor->setToolTip(i18nc("@info:tooltip", "The selected items have different values. Type a value to set it on every item."));
    } else {
        editor->setText(values.value(0));
        if (field.type == TagLibrary::FieldType::Date) {
            editor->setPlaceholderText(i18nc("@info:placeholder date format, keep the letters in English", "YYYY-MM-DD"));
        }
    }

    if (field.type == TagLibrary::FieldType::Date && !m_library.isReadOnly()) {
        QAction *pick = editor->addAction(QIcon::fromTheme(QStringLiteral("view-calendar")), QLineEdit::TrailingPosition);
        pick->setObjectName(QStringLiteral("fieldDatePicker_") + field.id);
        pick->setToolTip(i18nc("@info:tooltip", "Choose a date"));
        connect(pick, &QAction::triggered, editor, [this, editor, id = field.id] {
            auto *menu = new QMenu(editor);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            auto *calendar = new QCalendarWidget(menu);
            calendar->setObjectName(QStringLiteral("fieldDateCalendar"));
            const QDate current = QDate::fromString(editor->text().trimmed(), Qt::ISODate);
            calendar->setSelectedDate(current.isValid() ? current : QDate::currentDate());
            auto *action = new QWidgetAction(menu);
            action->setDefaultWidget(calendar);
            menu->addAction(action);
            connect(calendar, &QCalendarWidget::clicked, editor, [this, editor, menu, id](QDate date) {
                editor->setText(date.toString(Qt::ISODate));
                m_edited.insert(id);
                validate();
                menu->close();
            });
            menu->popup(editor->mapToGlobal(QPoint(0, editor->height())));
            calendar->setFocus();
        });
    }

    connect(editor, &QLineEdit::textEdited, this, [this, id = field.id] {
        m_edited.insert(id);
        validate();
    });
    m_editors.insert(field.id, editor);
    return editor;
}

QLineEdit *FieldEditDialog::editor(const QString &id) const
{
    return m_editors.value(id);
}

QMap<QString, QString> FieldEditDialog::edits() const
{
    QMap<QString, QString> result;
    for (const QString &id : m_edited) {
        if (const QLineEdit *editor = m_editors.value(id)) {
            result.insert(id, editor->text().trimmed());
        }
    }
    return result;
}

QString FieldEditDialog::problem() const
{
    if (m_library.isNull() || m_library.isReadOnly() || m_library.fields().isEmpty()) {
        return i18nc("@info", "There are no field values to change.");
    }
    const QMap<QString, QString> values = edits();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (const QString error = m_library.checkFieldValue(it.key(), it.value()); !error.isEmpty()) {
            return error;
        }
    }
    return {};
}

void FieldEditDialog::validate()
{
    const QString error = problem();
    const bool usable = !m_library.isNull() && !m_library.isReadOnly() && !m_library.fields().isEmpty();
    m_error->setText(error);
    m_error->setVisible(usable && !error.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
}

bool FieldEditDialog::apply()
{
    if (!problem().isEmpty()) {
        return false;
    }
    const QMap<QString, QString> values = edits();
    if (values.isEmpty()) {
        m_changed = 0;
        accept();
        return true;
    }
    QString error;
    if (!TagAssignment::applyFields(m_root, m_paths, values, &m_changed, &error)) {
        m_error->setText(i18nc("@info", "Could not save the field values: %1", error));
        m_error->show();
        return false;
    }
    Q_EMIT libraryChanged(m_root);
    accept();
    return true;
}

int FieldEditDialog::changedCount() const
{
    return m_changed;
}
