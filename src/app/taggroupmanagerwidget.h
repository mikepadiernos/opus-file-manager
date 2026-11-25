// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <KLocalizedString>

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTreeWidget;
class TagGroupDialog;

/**
 * The ordered list of a library's tag groups with commands to create, edit,
 * reorder and delete them. Groups only organize how tags are listed; deleting
 * a group keeps its tags and their assignments. Edits are saved to the
 * library document straight away.
 */
class TagGroupManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagGroupManagerWidget(QWidget *parent = nullptr);

    /// Shows the groups of the library at @p root, or nothing for an empty root.
    void setLibrary(const QString &root);
    QString library() const;
    /// Reloads the library document, keeping the current group.
    void reload();

    /// The identifier of the current group, or an empty string.
    QString currentGroup() const;
    void setCurrentGroup(const QString &id);
    /// The last error or status message.
    QString message() const;

    /// Opens the dialog for a new group; returns it so callers and tests can follow it.
    TagGroupDialog *createGroup();
    /// Opens the dialog for group @p id, or returns null when it doesn't exist.
    TagGroupDialog *editGroup(const QString &id);
    /// Moves group @p id by @p offset places in the list.
    bool moveGroup(const QString &id, int offset);
    /// Asks for confirmation, then deletes the current group.
    void confirmRemove();
    /// Deletes group @p id from the library without asking.
    bool removeGroup(const QString &id);

Q_SIGNALS:
    /// The library's groups were changed and saved.
    void libraryChanged(const QString &root);

private:
    TagGroupDialog *openEditor(const TagLibrary::Group &group);
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
    QPushButton *m_up;
    QPushButton *m_down;
    QPushButton *m_remove;
    QLabel *m_status;
};
