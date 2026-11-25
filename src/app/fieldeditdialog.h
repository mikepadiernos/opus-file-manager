// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QDialog>
#include <QHash>
#include <QMap>
#include <QSet>

class KMessageWidget;
class QDialogButtonBox;
class QLineEdit;

/**
 * Edits the text and date field values of items in one library. Only the
 * fields the user edits are applied, so items keep their other values; an
 * emptied field is cleared. Values are stored in the library document, and
 * files and their labels, ratings and notes aren't changed.
 */
class FieldEditDialog : public QDialog
{
    Q_OBJECT

public:
    /// Edits the items at @p absolutePaths, which must all be in the library at @p root.
    FieldEditDialog(const QString &root, const QStringList &absolutePaths, QWidget *parent = nullptr);

    /// The editor for field @p id, or null.
    QLineEdit *editor(const QString &id) const;
    /// The edited fields and their new values; an empty value clears the field.
    QMap<QString, QString> edits() const;
    /// Why the edits can't be applied, or an empty string.
    QString problem() const;
    /// Applies the edits; on success emits libraryChanged() and accepts the dialog.
    bool apply();
    /// The number of items whose values the last apply() changed.
    int changedCount() const;

Q_SIGNALS:
    void libraryChanged(const QString &root);
    /// The user asked to create or edit the library's fields.
    void manageFieldsRequested(const QString &root);

private:
    QLineEdit *addEditor(const TagLibrary::Field &field, const QStringList &relativePaths);
    void validate();

    QString m_root;
    QStringList m_paths;
    TagLibrary::Library m_library;
    QHash<QString, QLineEdit *> m_editors;
    QSet<QString> m_edited;
    int m_changed = 0;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
};
