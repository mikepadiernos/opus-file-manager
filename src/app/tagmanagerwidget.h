// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class TagEditDialog;

/**
 * A searchable list of a library's tags with commands to create, edit and
 * delete them. Edits are saved to the library document straight away.
 */
class TagManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagManagerWidget(QWidget *parent = nullptr);

    /// Shows the tags of the library at @p root, or nothing for an empty root.
    void setLibrary(const QString &root);
    QString library() const;
    /// Reloads the library document, keeping the filter and current tag.
    void reload();

    /// The identifier of the current tag, or an empty string.
    QString currentTag() const;
    void setCurrentTag(const QString &id);
    /// The last error or status message.
    QString message() const;

    /// Opens the dialog for a new tag; returns it so callers and tests can follow it.
    TagEditDialog *createTag();
    /// Opens the dialog for tag @p id, or returns null when it doesn't exist.
    TagEditDialog *editTag(const QString &id);
    /// Asks for confirmation, then deletes the current tag.
    void confirmRemove();
    /// Deletes tag @p id from the library without asking.
    bool removeTag(const QString &id);

Q_SIGNALS:
    /// The library's tags were changed and saved.
    void libraryChanged(const QString &root);

private:
    TagEditDialog *openEditor(const TagLibrary::Tag &tag);
    bool save(const TagLibrary::Tag &tag);
    void applyFilter();
    void updateButtons();
    void setMessage(const QString &message);

    QString m_root;
    TagLibrary::Library m_library;
    QString m_message;
    QLineEdit *m_filter;
    QTreeWidget *m_list;
    QPushButton *m_new;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QLabel *m_status;
};
