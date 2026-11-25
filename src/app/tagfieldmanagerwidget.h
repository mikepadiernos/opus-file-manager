// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <KLocalizedString>

#include <QDialog>
#include <QWidget>

#include <functional>

class KMessageWidget;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

/// The localized name of field type @p type.
QString tagFieldTypeName(TagLibrary::FieldType type);

/**
 * Creates or edits a library field's name and type. The type can't be
 * changed while items have a value for the field, because values aren't
 * converted between types.
 */
class TagFieldDialog : public QDialog
{
    Q_OBJECT

public:
    /// Edits @p field of @p library, or creates a field when its identifier is empty.
    TagFieldDialog(const TagLibrary::Library &library, const TagLibrary::Field &field, QWidget *parent = nullptr);

    /// The edited field, with a new identifier when it is being created.
    TagLibrary::Field field() const;
    /// Why the field can't be saved, or an empty string.
    QString problem() const;

private:
    void validate();

    TagLibrary::Library m_library;
    QString m_id;
    QLineEdit *m_name;
    QComboBox *m_type;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
};

/**
 * The list of a library's text and date fields with commands to create,
 * edit and delete them. Edits are saved to the library document straight
 * away; deleting a field removes its values from every item.
 */
class TagFieldManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagFieldManagerWidget(QWidget *parent = nullptr);

    /// Shows the fields of the library at @p root, or nothing for an empty root.
    void setLibrary(const QString &root);
    QString library() const;
    /// Reloads the library document, keeping the current field.
    void reload();

    /// The identifier of the current field, or an empty string.
    QString currentField() const;
    void setCurrentField(const QString &id);
    /// The last error or status message.
    QString message() const;

    /// Opens the dialog for a new field; returns it so callers and tests can follow it.
    TagFieldDialog *createField();
    /// Opens the dialog for field @p id, or returns null when it doesn't exist.
    TagFieldDialog *editField(const QString &id);
    /// Asks for confirmation, then deletes the current field.
    void confirmRemove();
    /// Deletes field @p id and its values without asking.
    bool removeField(const QString &id);

Q_SIGNALS:
    /// The library's fields were changed and saved.
    void libraryChanged(const QString &root);

private:
    TagFieldDialog *openEditor(const TagLibrary::Field &field);
    /// Saves @p edit; on failure, shows @p failure with the error as its last argument.
    bool change(const std::function<bool(TagLibrary::Library &library, QString *error)> &edit, const KLocalizedString &failure, const QString &current);
    void updateButtons();
    void setMessage(const QString &message);

    QString m_root;
    TagLibrary::Library m_library;
    QString m_message;
    QTreeWidget *m_list;
    QPushButton *m_new;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QLabel *m_status;
};

/**
 * Edits the fields new libraries start with, as "text:Name" or "date:Name"
 * templates, with commands to add, edit, remove and reorder them. Bound to
 * the NewLibraryFields setting, so nothing is saved until it is applied.
 */
class FieldTemplateEditor : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList templates READ templates WRITE setTemplates NOTIFY templatesChanged USER true)

public:
    explicit FieldTemplateEditor(QWidget *parent = nullptr);

    QStringList templates() const;
    /// Shows @p templates, leaving out invalid and repeated entries.
    void setTemplates(const QStringList &templates);
    /// The current row, or -1.
    int currentRow() const;
    void setCurrentRow(int row);

    /// Opens the dialog for a new field; returns it so callers and tests can follow it.
    TagFieldDialog *addField();
    /// Opens the dialog for the field at @p row, or returns null when there is none.
    TagFieldDialog *editField(int row);
    void removeField(int row);
    /// Moves the field at @p row by @p offset rows, keeping it current.
    void moveField(int row, int offset);

Q_SIGNALS:
    void templatesChanged();

private:
    TagFieldDialog *openEditor(const TagLibrary::Field &field);
    void setFields(const QList<TagLibrary::Field> &fields, const QString &current);
    void updateButtons();

    QList<TagLibrary::Field> m_fields;
    QTreeWidget *m_list;
    QPushButton *m_add;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QPushButton *m_up;
    QPushButton *m_down;
};
