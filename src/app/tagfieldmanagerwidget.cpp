// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagfieldmanagerwidget.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
constexpr int IdRole = Qt::UserRole;

enum Column {
    NameColumn,
    TypeColumn,
    ItemsColumn,
};
}

QString tagFieldTypeName(TagLibrary::FieldType type)
{
    switch (type) {
    case TagLibrary::FieldType::Date:
        return i18nc("@item field type", "Date");
    case TagLibrary::FieldType::Text:
        break;
    }
    return i18nc("@item field type", "Text");
}

TagFieldDialog::TagFieldDialog(const TagLibrary::Library &library, const TagLibrary::Field &field, QWidget *parent)
    : QDialog(parent)
    , m_library(library)
    , m_id(field.id.isEmpty() ? TagLibrary::createId() : field.id)
    , m_name(new QLineEdit(field.name, this))
    , m_type(new QComboBox(this))
    , m_error(new KMessageWidget(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("tagFieldDialog"));
    setWindowTitle(field.id.isEmpty() ? i18nc("@title:window", "New Field") : i18nc("@title:window", "Edit Field"));

    m_name->setObjectName(QStringLiteral("tagFieldName"));
    m_name->setPlaceholderText(i18nc("@info:placeholder", "Required"));
    m_type->setObjectName(QStringLiteral("tagFieldType"));
    m_type->addItem(tagFieldTypeName(TagLibrary::FieldType::Text), int(TagLibrary::FieldType::Text));
    m_type->addItem(tagFieldTypeName(TagLibrary::FieldType::Date), int(TagLibrary::FieldType::Date));
    m_type->setCurrentIndex(m_type->findData(int(field.type)));
    m_error->setObjectName(QStringLiteral("tagFieldError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->setCloseButtonVisible(false);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "&Name:"), m_name);
    form->addRow(i18nc("@label:listbox", "&Type:"), m_type);

    const int usage = field.id.isEmpty() ? 0 : library.fieldUsage(field.id);
    auto *hint = new QLabel(this);
    hint->setObjectName(QStringLiteral("tagFieldTypeHint"));
    hint->setWordWrap(true);
    if (usage > 0) {
        m_type->setEnabled(false);
        hint->setText(i18ncp("@info",
                             "The type can't be changed because one item has a value for this field. Values aren't converted between types.",
                             "The type can't be changed because %1 items have a value for this field. Values aren't converted between types.",
                             usage));
    } else {
        hint->setText(i18nc("@info", "Dates are stored as YYYY-MM-DD and shown in your regional format."));
    }

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addStretch();
    layout->addWidget(m_error);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
        if (problem().isEmpty()) {
            accept();
        }
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_name, &QLineEdit::textChanged, this, &TagFieldDialog::validate);
    connect(m_type, &QComboBox::currentIndexChanged, this, &TagFieldDialog::validate);
    validate();
    resize(400, sizeHint().height());
}

TagLibrary::Field TagFieldDialog::field() const
{
    TagLibrary::Field result;
    result.id = m_id;
    result.name = m_name->text().trimmed();
    result.type = TagLibrary::FieldType(m_type->currentData().toInt());
    return result;
}

QString TagFieldDialog::problem() const
{
    return m_library.checkField(field());
}

void TagFieldDialog::validate()
{
    const TagLibrary::Field edited = field();
    const QString error = m_library.checkField(edited);
    m_error->setText(error);
    m_error->setVisible(!error.isEmpty() && !edited.name.isEmpty());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
}

TagFieldManagerWidget::TagFieldManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_list(new QTreeWidget(this))
    , m_new(new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "&New Field…"), this))
    , m_edit(new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "&Edit…"), this))
    , m_remove(new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:button", "&Delete…"), this))
    , m_status(new QLabel(this))
{
    m_list->setObjectName(QStringLiteral("tagFieldList"));
    m_list->setAccessibleName(i18nc("@info:accessible", "Library fields"));
    m_list->setHeaderLabels({i18nc("@title:column", "Field"), i18nc("@title:column", "Type"), i18nc("@title:column", "Items")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_list->header()->setStretchLastSection(false);
    m_new->setObjectName(QStringLiteral("tagFieldNew"));
    m_edit->setObjectName(QStringLiteral("tagFieldEdit"));
    m_remove->setObjectName(QStringLiteral("tagFieldDelete"));
    m_status->setObjectName(QStringLiteral("tagFieldStatus"));
    m_status->setWordWrap(true);

    auto *hint = new QLabel(i18nc("@info",
                                  "Fields store text or dates on items in this library, alongside their tags. They are kept in the library document; "
                                  "files and their labels, ratings and notes aren't changed."),
                            this);
    hint->setWordWrap(true);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_new);
    buttons->addWidget(m_edit);
    buttons->addStretch();
    buttons->addWidget(m_remove);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(hint);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);
    layout->addWidget(m_status);

    connect(m_list, &QTreeWidget::currentItemChanged, this, &TagFieldManagerWidget::updateButtons);
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        editField(item->data(NameColumn, IdRole).toString());
    });
    connect(m_new, &QPushButton::clicked, this, &TagFieldManagerWidget::createField);
    connect(m_edit, &QPushButton::clicked, this, [this] {
        editField(currentField());
    });
    connect(m_remove, &QPushButton::clicked, this, &TagFieldManagerWidget::confirmRemove);
    updateButtons();
}

void TagFieldManagerWidget::setLibrary(const QString &root)
{
    m_root = root;
    setMessage({});
    reload();
}

QString TagFieldManagerWidget::library() const
{
    return m_root;
}

void TagFieldManagerWidget::reload()
{
    const QString current = currentField();
    m_list->clear();
    m_library = {};
    if (m_root.isEmpty()) {
        updateButtons();
        return;
    }
    QString error;
    m_library = TagLibrary::Library::load(m_root, &error);
    if (m_library.isNull()) {
        setMessage(error);
        updateButtons();
        return;
    }
    if (m_library.isReadOnly()) {
        setMessage(i18nc("@info", "The library is read-only, so its fields can't be changed."));
    } else if (m_library.fields().isEmpty()) {
        setMessage(i18nc("@info", "This library has no fields yet."));
    } else {
        setMessage({});
    }
    for (const TagLibrary::Field &field : m_library.fields()) {
        auto *item = new QTreeWidgetItem(m_list);
        item->setData(NameColumn, IdRole, field.id);
        item->setText(NameColumn, field.name);
        item->setText(TypeColumn, tagFieldTypeName(field.type));
        item->setText(ItemsColumn, QLocale().toString(m_library.fieldUsage(field.id)));
        item->setTextAlignment(ItemsColumn, Qt::AlignRight | Qt::AlignVCenter);
    }
    m_list->resizeColumnToContents(TypeColumn);
    m_list->resizeColumnToContents(ItemsColumn);
    setCurrentField(current);
    updateButtons();
}

QString TagFieldManagerWidget::currentField() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item ? item->data(NameColumn, IdRole).toString() : QString();
}

void TagFieldManagerWidget::setCurrentField(const QString &id)
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        if (m_list->topLevelItem(i)->data(NameColumn, IdRole).toString() == id) {
            m_list->setCurrentItem(m_list->topLevelItem(i));
            return;
        }
    }
}

QString TagFieldManagerWidget::message() const
{
    return m_message;
}

TagFieldDialog *TagFieldManagerWidget::createField()
{
    if (m_library.isNull() || m_library.isReadOnly()) {
        return nullptr;
    }
    return openEditor({});
}

TagFieldDialog *TagFieldManagerWidget::editField(const QString &id)
{
    const TagLibrary::Field *field = m_library.field(id);
    if (!field || m_library.isReadOnly()) {
        return nullptr;
    }
    return openEditor(*field);
}

TagFieldDialog *TagFieldManagerWidget::openEditor(const TagLibrary::Field &field)
{
    auto *dialog = new TagFieldDialog(m_library, field, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const TagLibrary::Field edited = dialog->field();
        change(
            [edited](TagLibrary::Library &library, QString *error) {
                return library.setField(edited, error);
            },
            ki18nc("@info", "Could not save the field “%1”: %2").subs(edited.name),
            edited.id);
    });
    dialog->open();
    return dialog;
}

void TagFieldManagerWidget::confirmRemove()
{
    const TagLibrary::Field *field = m_library.field(currentField());
    if (!field) {
        return;
    }
    const QString id = field->id;
    const int usage = m_library.fieldUsage(id);
    const QString values = usage > 0 ? i18ncp("@info", "Its value on one item will be removed.", "Its values on %1 items will be removed.", usage)
                                     : i18nc("@info", "No items have a value for it.");
    const auto answer =
        KMessageBox::warningContinueCancel(this,
                                           xi18nc("@info", "<para>Delete the field “%1”?</para><para>%2 No files are changed.</para>", field->name, values),
                                           i18nc("@title:window", "Delete Field"),
                                           KStandardGuiItem::del());
    if (answer == KMessageBox::Continue) {
        removeField(id);
    }
}

bool TagFieldManagerWidget::removeField(const QString &id)
{
    int removed = 0;
    const bool saved = change(
        [id, &removed](TagLibrary::Library &library, QString *error) {
            if (!library.field(id)) {
                *error = i18nc("@info", "The field no longer exists.");
                return false;
            }
            removed = library.removeField(id);
            return true;
        },
        ki18nc("@info", "Could not delete the field: %1"),
        {});
    if (saved) {
        setMessage(i18ncp("@info", "Deleted the field and its value on one item.", "Deleted the field and its values on %1 items.", removed));
    }
    return saved;
}

bool TagFieldManagerWidget::change(const std::function<bool(TagLibrary::Library &library, QString *error)> &edit,
                                   const KLocalizedString &failure,
                                   const QString &current)
{
    QString error;
    if (!TagLibrary::update(m_root, edit, &error)) {
        reload();
        setMessage(failure.subs(error).toString());
        return false;
    }
    reload();
    if (!current.isEmpty()) {
        setCurrentField(current);
    }
    Q_EMIT libraryChanged(m_root);
    return true;
}

void TagFieldManagerWidget::updateButtons()
{
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    const bool selected = m_list->currentItem() != nullptr;
    m_new->setEnabled(writable);
    m_edit->setEnabled(writable && selected);
    m_remove->setEnabled(writable && selected);
}

void TagFieldManagerWidget::setMessage(const QString &message)
{
    m_message = message;
    m_status->setText(message);
    m_status->setVisible(!message.isEmpty());
}
