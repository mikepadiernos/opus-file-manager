// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QDialog>

class KColorButton;
class KMessageWidget;
class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QListWidget;

/**
 * Edits a tag group's name, colour and member tags. A tag can belong to
 * several groups. The dialog only returns the edited group; the caller saves
 * it.
 */
class TagGroupDialog : public QDialog
{
    Q_OBJECT

public:
    /// Edits @p group, or a new group when its identifier is empty, against a snapshot of @p library.
    TagGroupDialog(const TagLibrary::Library &library, const TagLibrary::Group &group, QWidget *parent = nullptr);

    TagLibrary::Group group() const;
    /// Why the group can't be saved, or an empty string.
    QString problem() const;

private:
    void populateTags();
    void filterTags(const QString &text);
    void validate();

    TagLibrary::Library m_library;
    QString m_id;
    /// Member tags in display order; newly checked tags are added at the end.
    QStringList m_tags;
    QLineEdit *m_name;
    QCheckBox *m_useColor;
    KColorButton *m_color;
    QLineEdit *m_tagFilter;
    QListWidget *m_tagList;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
};
