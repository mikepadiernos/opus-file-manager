// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <KFileItem>

#include <QWidget>

class QAction;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * The tag manager pane: a searchable, keyboard-accessible list of a
 * library's tags whose check boxes show and change how each tag is assigned
 * to the selected files and folders. A partly checked tag is assigned to
 * some of them; checking it assigns it to all. When the library has tag
 * groups, every group is listed in order with its tags below it, followed by
 * the tags that aren't in a group; a tag in several groups appears in each.
 */
class TagPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Placement {
        Right,
        Below,
    };

    explicit TagPanel(QWidget *parent = nullptr);

    /// Shows the library containing @p folder and how its tags are assigned to @p items.
    void setContext(const QUrl &folder, const KFileItemList &items);
    /// Reloads the tags and assignments, keeping the search and current tag.
    void reload();
    void setPlacement(Placement placement);
    /// Shows the placement menu when the dock's title bar is hidden.
    void setTitleVisible(bool visible);
    /// Moves the keyboard focus to the search field.
    void focusSearch();

    /// The library whose tags are shown, or an empty string.
    QString library() const;
    /// The selection summary or the last error.
    QString message() const;
    /// How tag @p id is assigned to the items.
    Qt::CheckState assignment(const QString &id) const;
    /// Assigns tag @p id to every item, or removes it from every item.
    bool setAssigned(const QString &id, bool assigned);
    /// Creates a tag named @p name in the library and assigns it to the items.
    bool createAndAssign(const QString &name);

Q_SIGNALS:
    /// Tags or assignments in the library at @p root were saved.
    void libraryChanged(const QString &root);
    void manageRequested(const QString &root);
    void placementRequested(TagPanel::Placement placement);
    void closeRequested();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// Whether the items resolve to one library, so their tags can be changed.
    bool isAssigning() const;
    void applyFilter();
    void activateSearch();
    void updateCreateButton();
    void updateStatus();
    void setMessage(const QString &message, bool error = false);
    QTreeWidgetItem *firstVisibleItem() const;

    QString m_root;
    QStringList m_paths;
    QStringList m_relativePaths;
    QString m_selectionProblem;
    TagLibrary::Library m_library;
    QHash<QString, int> m_counts;
    QString m_message;
    bool m_error = false;
    bool m_updating = false;
    bool m_hasContext = false;
    QLabel *m_title;
    QToolButton *m_options;
    QAction *m_placeRight;
    QAction *m_placeBelow;
    QLineEdit *m_filter;
    QTreeWidget *m_list;
    QPushButton *m_create;
    QPushButton *m_manage;
    QLabel *m_status;
};
