// SPDX-License-Identifier: GPL-3.0-or-later

#include "taggroupmanagerwidget.h"

#include "tageditdialog.h"
#include "taggroupdialog.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int IdRole = Qt::UserRole;

enum Column {
    NameColumn,
    TagsColumn,
};
}

TagGroupManagerWidget::TagGroupManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_list(new QTreeWidget(this))
    , m_new(new QPushButton(QIcon::fromTheme(QStringLiteral("folder-new"), QIcon::fromTheme(QStringLiteral("list-add"))),
                            i18nc("@action:button", "&New Group…"),
                            this))
    , m_edit(new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "&Edit…"), this))
    , m_up(new QPushButton(QIcon::fromTheme(QStringLiteral("go-up")), i18nc("@action:button", "Move &Up"), this))
    , m_down(new QPushButton(QIcon::fromTheme(QStringLiteral("go-down")), i18nc("@action:button", "Move Do&wn"), this))
    , m_remove(new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:button", "&Delete…"), this))
    , m_status(new QLabel(this))
{
    m_list->setObjectName(QStringLiteral("tagGroupList"));
    m_list->setAccessibleName(i18nc("@info:accessible", "Tag groups"));
    m_list->setHeaderLabels({i18nc("@title:column", "Group"), i18nc("@title:column", "Tags")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->header()->setSectionResizeMode(TagsColumn, QHeaderView::Stretch);
    m_new->setObjectName(QStringLiteral("tagGroupNew"));
    m_edit->setObjectName(QStringLiteral("tagGroupEdit"));
    m_up->setObjectName(QStringLiteral("tagGroupUp"));
    m_down->setObjectName(QStringLiteral("tagGroupDown"));
    m_remove->setObjectName(QStringLiteral("tagGroupDelete"));
    m_status->setObjectName(QStringLiteral("tagGroupStatus"));
    m_status->setWordWrap(true);

    auto *hint =
        new QLabel(i18nc("@info", "Groups organize the tag list in this library. They don't depend on locations or workspaces, and every group is shown."),
                   this);
    hint->setWordWrap(true);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_new);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_up);
    buttons->addWidget(m_down);
    buttons->addStretch();
    buttons->addWidget(m_remove);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(hint);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);
    layout->addWidget(m_status);

    connect(m_list, &QTreeWidget::currentItemChanged, this, &TagGroupManagerWidget::updateButtons);
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        editGroup(item->data(NameColumn, IdRole).toString());
    });
    connect(m_new, &QPushButton::clicked, this, &TagGroupManagerWidget::createGroup);
    connect(m_edit, &QPushButton::clicked, this, [this] {
        editGroup(currentGroup());
    });
    connect(m_up, &QPushButton::clicked, this, [this] {
        moveGroup(currentGroup(), -1);
    });
    connect(m_down, &QPushButton::clicked, this, [this] {
        moveGroup(currentGroup(), 1);
    });
    connect(m_remove, &QPushButton::clicked, this, &TagGroupManagerWidget::confirmRemove);
    updateButtons();
}

void TagGroupManagerWidget::setLibrary(const QString &root)
{
    m_root = root;
    setMessage({});
    reload();
}

QString TagGroupManagerWidget::library() const
{
    return m_root;
}

void TagGroupManagerWidget::reload()
{
    const QString current = currentGroup();
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
        setMessage(i18nc("@info", "The library is read-only, so its groups can't be changed."));
    } else if (m_library.groups().isEmpty()) {
        setMessage(i18nc("@info", "This library has no tag groups yet."));
    } else {
        setMessage({});
    }
    const QString separator = i18nc("@item list separator", ", ");
    for (const TagLibrary::Group &group : m_library.groups()) {
        auto *item = new QTreeWidgetItem(m_list);
        item->setData(NameColumn, IdRole, group.id);
        item->setText(NameColumn, group.name);
        item->setIcon(NameColumn, tagColorIcon(group.color));
        QStringList names;
        for (const QString &tag : group.tags) {
            names.append(m_library.displayName(tag));
        }
        item->setText(TagsColumn, names.isEmpty() ? i18nc("@item no tags in the group", "None") : names.join(separator));
        item->setToolTip(TagsColumn, i18ncp("@info:tooltip", "One tag", "%1 tags", names.size()));
    }
    setCurrentGroup(current);
    updateButtons();
}

QString TagGroupManagerWidget::currentGroup() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item ? item->data(NameColumn, IdRole).toString() : QString();
}

void TagGroupManagerWidget::setCurrentGroup(const QString &id)
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        if (m_list->topLevelItem(i)->data(NameColumn, IdRole).toString() == id) {
            m_list->setCurrentItem(m_list->topLevelItem(i));
            return;
        }
    }
}

QString TagGroupManagerWidget::message() const
{
    return m_message;
}

TagGroupDialog *TagGroupManagerWidget::createGroup()
{
    if (m_library.isNull() || m_library.isReadOnly()) {
        return nullptr;
    }
    return openEditor({});
}

TagGroupDialog *TagGroupManagerWidget::editGroup(const QString &id)
{
    const TagLibrary::Group *group = m_library.group(id);
    if (!group || m_library.isReadOnly()) {
        return nullptr;
    }
    return openEditor(*group);
}

TagGroupDialog *TagGroupManagerWidget::openEditor(const TagLibrary::Group &group)
{
    auto *dialog = new TagGroupDialog(m_library, group, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const TagLibrary::Group edited = dialog->group();
        change(
            [edited](TagLibrary::Library &library, QString *error) {
                return library.setGroup(edited, error);
            },
            ki18nc("@info", "Could not save the group “%1”: %2").subs(edited.name),
            edited.id);
    });
    dialog->open();
    return dialog;
}

bool TagGroupManagerWidget::moveGroup(const QString &id, int offset)
{
    const auto &groups = m_library.groups();
    const auto found = std::find_if(groups.cbegin(), groups.cend(), [&id](const TagLibrary::Group &group) {
        return group.id == id;
    });
    if (found == groups.cend() || m_library.isReadOnly()) {
        return false;
    }
    const int index = int(found - groups.cbegin()) + offset;
    if (offset == 0 || index < 0 || index >= groups.size()) {
        return false;
    }
    // The neighbour is matched by identity, so a concurrent edit in another window doesn't move the group too far.
    const QString neighbour = groups.at(index).id;
    return change(
        [id, neighbour](TagLibrary::Library &library, QString *error) {
            const auto &current = library.groups();
            const auto at = [&current](const QString &groupId) {
                return int(std::find_if(current.cbegin(),
                                        current.cend(),
                                        [&groupId](const TagLibrary::Group &group) {
                                            return group.id == groupId;
                                        })
                           - current.cbegin());
            };
            const int target = at(neighbour);
            if (!library.group(id) || target >= current.size()) {
                *error = i18nc("@info", "The group no longer exists.");
                return false;
            }
            return library.moveGroup(id, target);
        },
        ki18nc("@info", "Could not move the group: %1"),
        id);
}

void TagGroupManagerWidget::confirmRemove()
{
    const TagLibrary::Group *group = m_library.group(currentGroup());
    if (!group) {
        return;
    }
    const QString id = group->id;
    const auto answer = KMessageBox::warningContinueCancel(
        this,
        xi18nc("@info", "<para>Delete the group “%1”?</para><para>Its tags and their assignments are kept. No files are changed.</para>", group->name),
        i18nc("@title:window", "Delete Tag Group"),
        KStandardGuiItem::del());
    if (answer == KMessageBox::Continue) {
        removeGroup(id);
    }
}

bool TagGroupManagerWidget::removeGroup(const QString &id)
{
    return change(
        [id](TagLibrary::Library &library, QString *error) {
            if (!library.removeGroup(id)) {
                *error = i18nc("@info", "The group no longer exists.");
                return false;
            }
            return true;
        },
        ki18nc("@info", "Could not delete the group: %1"),
        {});
}

bool TagGroupManagerWidget::change(const std::function<bool(TagLibrary::Library &library, QString *error)> &edit,
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
        setCurrentGroup(current);
    }
    Q_EMIT libraryChanged(m_root);
    return true;
}

void TagGroupManagerWidget::updateButtons()
{
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    QTreeWidgetItem *item = m_list->currentItem();
    const int index = item ? m_list->indexOfTopLevelItem(item) : -1;
    m_new->setEnabled(writable);
    m_edit->setEnabled(writable && item);
    m_up->setEnabled(writable && index > 0);
    m_down->setEnabled(writable && index >= 0 && index < m_list->topLevelItemCount() - 1);
    m_remove->setEnabled(writable && item);
}

void TagGroupManagerWidget::setMessage(const QString &message)
{
    m_message = message;
    m_status->setText(message);
    m_status->setVisible(!message.isEmpty());
}
