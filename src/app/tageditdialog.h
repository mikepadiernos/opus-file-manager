// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QDialog>
#include <QIcon>

class KColorButton;
class KMessageWidget;
class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;

/// A colour swatch for a tag or group, or a plain tag icon without a colour.
QIcon tagColorIcon(const QString &color);

/**
 * Edits a tag's name, aliases, shorthand, colour and parents. Parents that
 * would form a cycle can't be chosen, and a name shared with another tag
 * shows how the tags will be told apart. The dialog only returns the edited
 * tag; the caller saves it.
 */
class TagEditDialog : public QDialog
{
    Q_OBJECT

public:
    /// Edits @p tag, or a new tag when its identifier is empty, against a snapshot of @p library.
    TagEditDialog(const TagLibrary::Library &library, const TagLibrary::Tag &tag, QWidget *parent = nullptr);

    TagLibrary::Tag tag() const;
    /// Why the tag can't be saved, or an empty string.
    QString problem() const;
    /// How the tag will be shown when its name is shared, or an empty string.
    QString duplicateNote() const;

private:
    void populateParents();
    void filterParents(const QString &text);
    void validate();

    TagLibrary::Library m_library;
    QString m_id;
    QStringList m_parents;
    QLineEdit *m_name;
    QPlainTextEdit *m_aliases;
    QLineEdit *m_shorthand;
    QCheckBox *m_useColor;
    KColorButton *m_color;
    QLineEdit *m_parentFilter;
    QListWidget *m_parentList;
    KMessageWidget *m_note;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
};
