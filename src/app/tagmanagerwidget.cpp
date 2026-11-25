// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagmanagerwidget.h"

#include "tageditdialog.h"
#include "taggingsettings.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int IdRole = Qt::UserRole;

enum Column {
    NameColumn,
    ShorthandColumn,
    ParentsColumn,
    GroupsColumn,
    ItemsColumn,
};
}

TagManagerWidget::TagManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_filter(new QLineEdit(this))
    , m_list(new QTreeWidget(this))
    , m_new(new QPushButton(QIcon::fromTheme(QStringLiteral("tag-new"), QIcon::fromTheme(QStringLiteral("list-add"))),
                            i18nc("@action:button", "&New Tag…"),
                            this))
    , m_edit(new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:button", "&Edit…"), this))
    , m_remove(new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:button", "&Delete…"), this))
    , m_status(new QLabel(this))
{
    m_filter->setObjectName(QStringLiteral("tagFilter"));
    m_filter->setPlaceholderText(i18nc("@info:placeholder", "Search tags, aliases and shorthand…"));
    m_filter->setClearButtonEnabled(true);
    m_list->setObjectName(QStringLiteral("tagList"));
    m_list->setAccessibleName(i18nc("@info:accessible", "Tags"));
    m_list->setHeaderLabels({i18nc("@title:column", "Tag"),
                             i18nc("@title:column", "Shorthand"),
                             i18nc("@title:column", "Parents"),
                             i18nc("@title:column", "Groups"),
                             i18nc("@title:column", "Items")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSortingEnabled(true);
    m_list->sortByColumn(NameColumn, Qt::AscendingOrder);
    m_list->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_list->header()->setStretchLastSection(false);
    m_new->setObjectName(QStringLiteral("tagNew"));
    m_edit->setObjectName(QStringLiteral("tagEdit"));
    m_remove->setObjectName(QStringLiteral("tagDelete"));
    m_status->setObjectName(QStringLiteral("tagManagerStatus"));
    m_status->setWordWrap(true);
    m_status->hide();

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_new);
    buttons->addWidget(m_edit);
    buttons->addStretch();
    buttons->addWidget(m_remove);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_filter);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);
    layout->addWidget(m_status);

    connect(m_filter, &QLineEdit::textChanged, this, &TagManagerWidget::applyFilter);
    connect(m_filter, &QLineEdit::returnPressed, this, [this] {
        for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
            if (!m_list->topLevelItem(i)->isHidden()) {
                m_list->setCurrentItem(m_list->topLevelItem(i));
                m_list->setFocus();
                return;
            }
        }
    });
    connect(m_list, &QTreeWidget::currentItemChanged, this, &TagManagerWidget::updateButtons);
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        editTag(item->data(NameColumn, IdRole).toString());
    });
    connect(m_new, &QPushButton::clicked, this, &TagManagerWidget::createTag);
    connect(m_edit, &QPushButton::clicked, this, [this] {
        editTag(currentTag());
    });
    connect(m_remove, &QPushButton::clicked, this, &TagManagerWidget::confirmRemove);
    updateButtons();
}

void TagManagerWidget::setLibrary(const QString &root)
{
    m_root = root;
    setMessage({});
    reload();
}

QString TagManagerWidget::library() const
{
    return m_root;
}

void TagManagerWidget::reload()
{
    const QString current = currentTag();
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
        setMessage(i18nc("@info", "The library is read-only, so its tags can't be changed."));
    }
    QList<QTreeWidgetItem *> items;
    for (const TagLibrary::Tag &tag : m_library.tags()) {
        auto *item = new QTreeWidgetItem;
        item->setData(NameColumn, IdRole, tag.id);
        item->setText(NameColumn, m_library.displayName(tag.id));
        item->setIcon(NameColumn, tagColorIcon(tag.color));
        item->setText(ShorthandColumn, tag.shorthand);
        QStringList parents;
        for (const QString &parent : tag.parents) {
            parents.append(m_library.displayName(parent));
        }
        item->setText(ParentsColumn, parents.join(i18nc("@item list separator", ", ")));
        QStringList groups;
        for (const QString &group : m_library.groupsOf(tag.id)) {
            groups.append(m_library.group(group)->name);
        }
        item->setText(GroupsColumn, groups.join(i18nc("@item list separator", ", ")));
        item->setData(ItemsColumn, Qt::DisplayRole, m_library.usageCount(tag.id));
        item->setTextAlignment(ItemsColumn, Qt::AlignRight | Qt::AlignVCenter);
        if (!tag.aliases.isEmpty()) {
            item->setToolTip(NameColumn, i18nc("@info:tooltip", "Aliases: %1", tag.aliases.join(i18nc("@item list separator", ", "))));
        }
        items.append(item);
    }
    m_list->addTopLevelItems(items);
    applyFilter();
    setCurrentTag(current);
    updateButtons();
}

QString TagManagerWidget::currentTag() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item ? item->data(NameColumn, IdRole).toString() : QString();
}

void TagManagerWidget::setCurrentTag(const QString &id)
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        if (m_list->topLevelItem(i)->data(NameColumn, IdRole).toString() == id) {
            m_list->setCurrentItem(m_list->topLevelItem(i));
            return;
        }
    }
}

QString TagManagerWidget::message() const
{
    return m_message;
}

TagEditDialog *TagManagerWidget::createTag()
{
    if (m_library.isNull() || m_library.isReadOnly()) {
        return nullptr;
    }
    TagLibrary::Tag tag;
    tag.name = m_filter->text().trimmed();
    tag.color = TaggingSettings::defaultTagColor();
    return openEditor(tag);
}

TagEditDialog *TagManagerWidget::editTag(const QString &id)
{
    const TagLibrary::Tag *tag = m_library.tag(id);
    if (!tag || m_library.isReadOnly()) {
        return nullptr;
    }
    return openEditor(*tag);
}

TagEditDialog *TagManagerWidget::openEditor(const TagLibrary::Tag &tag)
{
    auto *dialog = new TagEditDialog(m_library, tag, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const TagLibrary::Tag edited = dialog->tag();
        if (save(edited)) {
            setCurrentTag(edited.id);
        }
    });
    dialog->open();
    return dialog;
}

bool TagManagerWidget::save(const TagLibrary::Tag &tag)
{
    QString error;
    const bool saved = TagLibrary::update(
        m_root,
        [&tag](TagLibrary::Library &library, QString *problem) {
            return library.setTag(tag, problem);
        },
        &error);
    if (!saved) {
        setMessage(i18nc("@info", "Could not save the tag “%1”: %2", tag.name, error));
        reload();
        return false;
    }
    setMessage({});
    reload();
    Q_EMIT libraryChanged(m_root);
    return true;
}

void TagManagerWidget::confirmRemove()
{
    const QString id = currentTag();
    const TagLibrary::Tag *tag = m_library.tag(id);
    if (!tag) {
        return;
    }
    const int uses = m_library.usageCount(id);
    const int children = int(std::count_if(m_library.tags().cbegin(), m_library.tags().cend(), [&id](const TagLibrary::Tag &other) {
        return other.parents.contains(id);
    }));
    QString details = uses ? i18ncp("@info", "It is removed from one item.", "It is removed from %1 items.", uses) : i18nc("@info", "No items have this tag.");
    if (children) {
        details += QLatin1Char(' ') + i18ncp("@info", "One tag loses it as a parent.", "%1 tags lose it as a parent.", children);
    }
    const auto answer = KMessageBox::warningContinueCancel(
        this,
        xi18nc("@info", "<para>Delete the tag “%1”?</para><para>%2 No files are changed.</para>", m_library.displayName(id), details),
        i18nc("@title:window", "Delete Tag"),
        KStandardGuiItem::del());
    if (answer == KMessageBox::Continue) {
        removeTag(id);
    }
}

bool TagManagerWidget::removeTag(const QString &id)
{
    QString error;
    const bool removed = TagLibrary::update(
        m_root,
        [&id](TagLibrary::Library &library, QString *problem) {
            if (!library.tag(id)) {
                *problem = i18nc("@info", "The tag no longer exists.");
                return false;
            }
            library.removeTag(id);
            return true;
        },
        &error);
    if (!removed) {
        setMessage(i18nc("@info", "Could not delete the tag: %1", error));
        reload();
        return false;
    }
    setMessage({});
    reload();
    Q_EMIT libraryChanged(m_root);
    return true;
}

void TagManagerWidget::applyFilter()
{
    const QString needle = m_filter->text().trimmed();
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        item->setHidden(!tagMatchesFilter(m_library, item->data(NameColumn, IdRole).toString(), item->text(NameColumn), needle));
    }
}

void TagManagerWidget::updateButtons()
{
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    const bool selected = !currentTag().isEmpty();
    m_filter->setEnabled(!m_library.isNull());
    m_new->setEnabled(writable);
    m_edit->setEnabled(writable && selected);
    m_remove->setEnabled(writable && selected);
}

void TagManagerWidget::setMessage(const QString &message)
{
    m_message = message;
    m_status->setText(message);
    m_status->setVisible(!message.isEmpty());
}
