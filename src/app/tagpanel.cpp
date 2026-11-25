// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagpanel.h"

#include "libraryregistry.h"
#include "symbolicicon.h"
#include "tagassignment.h"
#include "tageditdialog.h"

#include <KLocalizedString>

#include <QActionGroup>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int IdRole = Qt::UserRole;
}

TagPanel::TagPanel(QWidget *parent)
    : QWidget(parent)
    , m_title(new QLabel(this))
    , m_options(new QToolButton(this))
    , m_filter(new QLineEdit(this))
    , m_list(new QTreeWidget(this))
    , m_create(new QPushButton(QIcon::fromTheme(QStringLiteral("tag-new"), QIcon::fromTheme(QStringLiteral("list-add"))),
                               i18nc("@action:button", "&Create Tag"),
                               this))
    , m_manage(new QPushButton(QIcon::fromTheme(QStringLiteral("tag")), i18nc("@action:button", "&Manage Tags…"), this))
    , m_status(new QLabel(this))
{
    setObjectName(QStringLiteral("tagPanel"));
    m_title->setObjectName(QStringLiteral("tagPanelTitle"));
    m_title->setTextFormat(Qt::PlainText);
    m_title->setWordWrap(true);

    m_options->setObjectName(QStringLiteral("tagPanelOptions"));
    m_options->setIcon(SymbolicIcon::fromTheme(QStringLiteral("application-menu")));
    m_options->setToolTip(i18nc("@info:tooltip", "Tag panel options"));
    m_options->setAccessibleName(i18nc("@action", "Tag panel options"));
    m_options->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_options);
    auto *placements = new QActionGroup(menu);
    m_placeRight = menu->addAction(i18nc("@action", "Right of Panes"));
    m_placeRight->setObjectName(QStringLiteral("tagPanelPlaceRight"));
    m_placeBelow = menu->addAction(i18nc("@action", "Below Panes"));
    m_placeBelow->setObjectName(QStringLiteral("tagPanelPlaceBelow"));
    for (QAction *action : {m_placeRight, m_placeBelow}) {
        action->setCheckable(true);
        placements->addAction(action);
    }
    m_placeRight->setChecked(true);
    menu->addSeparator();
    QAction *hide = menu->addAction(i18nc("@action", "Hide Tag Panel"));
    hide->setObjectName(QStringLiteral("tagPanelHide"));
    connect(m_placeRight, &QAction::triggered, this, [this] {
        Q_EMIT placementRequested(Placement::Right);
    });
    connect(m_placeBelow, &QAction::triggered, this, [this] {
        Q_EMIT placementRequested(Placement::Below);
    });
    connect(hide, &QAction::triggered, this, &TagPanel::closeRequested);
    m_options->setMenu(menu);
    m_options->hide();

    m_filter->setObjectName(QStringLiteral("tagPanelFilter"));
    m_filter->setPlaceholderText(i18nc("@info:placeholder", "Search or create tags…"));
    m_filter->setToolTip(
        i18nc("@info:tooltip", "Press Enter to assign or remove the matching tag, or to create a tag with this name. Press Down to move to the list."));
    m_filter->setClearButtonEnabled(true);
    m_filter->installEventFilter(this);

    m_list->setObjectName(QStringLiteral("tagPanelList"));
    m_list->setAccessibleName(i18nc("@info:accessible", "Tags"));
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSortingEnabled(true);
    m_list->sortByColumn(0, Qt::AscendingOrder);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_create->setObjectName(QStringLiteral("tagPanelCreate"));
    m_manage->setObjectName(QStringLiteral("tagPanelManage"));
    m_status->setObjectName(QStringLiteral("tagPanelStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);

    auto *header = new QHBoxLayout;
    header->addWidget(m_title, 1);
    header->addWidget(m_options);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_create);
    buttons->addStretch();
    buttons->addWidget(m_manage);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addLayout(header);
    layout->addWidget(m_filter);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);
    layout->addWidget(m_status);
    setFocusProxy(m_filter);

    connect(m_filter, &QLineEdit::textChanged, this, [this] {
        applyFilter();
        updateCreateButton();
    });
    connect(m_filter, &QLineEdit::returnPressed, this, &TagPanel::activateSearch);
    connect(m_list, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) {
        if (m_updating || !isAssigning()) {
            return;
        }
        const QString id = item->data(0, IdRole).toString();
        const Qt::CheckState state = item->checkState(0);
        if (state != assignment(id)) {
            setAssigned(id, state == Qt::Checked);
        }
    });
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString id = item->data(0, IdRole).toString();
        if (isAssigning() && (item->flags() & Qt::ItemIsUserCheckable)) {
            setAssigned(id, assignment(id) != Qt::Checked);
        }
    });
    connect(m_create, &QPushButton::clicked, this, [this] {
        if (createAndAssign(m_filter->text())) {
            m_filter->clear();
        }
    });
    connect(m_manage, &QPushButton::clicked, this, [this] {
        Q_EMIT manageRequested(m_root);
    });
    reload();
}

void TagPanel::setContext(const QUrl &folder, const KFileItemList &items)
{
    QString root;
    QStringList paths;
    QStringList relativePaths;
    QString problem;
    if (!items.isEmpty()) {
        for (const KFileItem &item : items) {
            if (!item.url().isLocalFile()) {
                problem = i18nc("@info", "Only local files and folders can be tagged.");
                break;
            }
            if (item.isLink()) {
                problem = i18nc("@info", "Symbolic links can't be tagged.");
                break;
            }
            paths.append(QDir::cleanPath(item.url().toLocalFile()));
        }
        if (problem.isEmpty()) {
            const TagAssignment::Selection selection = TagAssignment::resolve(paths);
            problem = selection.problem;
            root = selection.root;
            relativePaths = selection.relativePaths;
        }
        if (!problem.isEmpty()) {
            paths.clear();
            relativePaths.clear();
            root.clear();
        }
    }
    if (root.isEmpty() && folder.isLocalFile()) {
        root = LibraryRegistry::rootFor(folder.toLocalFile());
    }
    relativePaths.removeDuplicates();
    // Hovering and view updates repeat the same context; keep the search and any error.
    if (root == m_root && paths == m_paths && problem == m_selectionProblem && m_hasContext) {
        return;
    }
    m_hasContext = true;
    if (root != m_root) {
        m_filter->clear();
    }
    m_root = root;
    m_paths = paths;
    m_relativePaths = relativePaths;
    m_selectionProblem = problem;
    m_message.clear();
    m_error = false;
    reload();
}

void TagPanel::reload()
{
    m_library = {};
    m_counts.clear();
    QString error;
    if (!m_root.isEmpty()) {
        m_library = TagAssignment::cachedLibrary(m_root, &error);
        if (m_library.isNull()) {
            setMessage(i18nc("@info", "Could not read the tag library: %1", error), true);
        } else if (isAssigning()) {
            m_counts = m_library.tagCounts(m_relativePaths);
        }
    }
    m_title->setText(m_root.isEmpty() ? i18nc("@title", "Tags") : i18nc("@title tags in library %1", "Tags in %1", QFileInfo(m_root).fileName()));
    m_title->setToolTip(QDir::toNativeSeparators(m_root));

    m_updating = true;
    const bool assigning = isAssigning();
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    QHash<QString, QTreeWidgetItem *> existing;
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        existing.insert(item->data(0, IdRole).toString(), item);
    }
    // Items are updated in place so that a check box toggled in the list
    // survives the reload that follows it.
    if (!m_list->property("opusAssigning").isValid() || m_list->property("opusAssigning").toBool() != assigning) {
        qDeleteAll(existing);
        existing.clear();
        m_list->setProperty("opusAssigning", assigning);
    }
    m_list->setSortingEnabled(false);
    const QString separator = i18nc("@item list separator", ", ");
    for (const TagLibrary::Tag &tag : m_library.tags()) {
        QTreeWidgetItem *item = existing.take(tag.id);
        if (!item) {
            item = new QTreeWidgetItem(m_list);
            item->setData(0, IdRole, tag.id);
        }
        item->setText(0, m_library.displayName(tag.id));
        item->setIcon(0, tagColorIcon(tag.color));
        QStringList tooltip;
        if (!tag.aliases.isEmpty()) {
            tooltip.append(i18nc("@info:tooltip", "Aliases: %1", tag.aliases.join(separator)));
        }
        if (!tag.shorthand.isEmpty()) {
            tooltip.append(i18nc("@info:tooltip", "Shorthand: %1", tag.shorthand));
        }
        if (!tag.parents.isEmpty()) {
            QStringList parents;
            for (const QString &parent : tag.parents) {
                parents.append(m_library.displayName(parent));
            }
            tooltip.append(i18nc("@info:tooltip", "Parents: %1", parents.join(separator)));
        }
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (assigning) {
            const int count = m_counts.value(tag.id);
            item->setCheckState(0, assignment(tag.id));
            if (writable) {
                flags |= Qt::ItemIsUserCheckable;
            }
            tooltip.append(i18ncp("@info:tooltip", "Assigned to %2 of %1 selected item", "Assigned to %2 of %1 selected items", m_relativePaths.size(), count));
        }
        item->setFlags(flags);
        item->setToolTip(0, tooltip.join(QLatin1Char('\n')));
    }
    qDeleteAll(existing);
    m_list->setSortingEnabled(true);
    m_updating = false;

    m_filter->setEnabled(!m_library.isNull());
    m_manage->setEnabled(!m_library.isNull());
    applyFilter();
    updateCreateButton();
    updateStatus();
}

void TagPanel::setPlacement(Placement placement)
{
    (placement == Placement::Below ? m_placeBelow : m_placeRight)->setChecked(true);
}

void TagPanel::setTitleVisible(bool visible)
{
    m_options->setVisible(!visible);
}

void TagPanel::focusSearch()
{
    m_filter->setFocus(Qt::ShortcutFocusReason);
    m_filter->selectAll();
}

QString TagPanel::library() const
{
    return m_root;
}

QString TagPanel::message() const
{
    return m_message;
}

Qt::CheckState TagPanel::assignment(const QString &id) const
{
    const int count = isAssigning() ? m_counts.value(id) : 0;
    if (count <= 0) {
        return Qt::Unchecked;
    }
    return count >= m_relativePaths.size() ? Qt::Checked : Qt::PartiallyChecked;
}

bool TagPanel::setAssigned(const QString &id, bool assigned)
{
    if (!isAssigning() || m_library.isNull() || m_library.isReadOnly() || !m_library.tag(id)) {
        return false;
    }
    const QString name = m_library.displayName(id);
    int changed = 0;
    QString error;
    const QStringList tags{id};
    if (!TagAssignment::apply(m_root, m_paths, assigned ? tags : QStringList(), assigned ? QStringList() : tags, &changed, &error)) {
        reload();
        setMessage(assigned ? i18nc("@info", "Could not assign the tag “%1”: %2", name, error)
                            : i18nc("@info", "Could not remove the tag “%1”: %2", name, error),
                   true);
        return false;
    }
    m_message.clear();
    m_error = false;
    reload();
    if (changed) {
        Q_EMIT libraryChanged(m_root);
    }
    return true;
}

bool TagPanel::createAndAssign(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_library.isNull() || m_library.isReadOnly()) {
        return false;
    }
    TagLibrary::Tag tag;
    tag.id = TagLibrary::createId();
    tag.name = trimmed;
    QString error;
    const bool created = TagLibrary::update(
        m_root,
        [&tag](TagLibrary::Library &library, QString *problem) {
            return library.setTag(tag, problem);
        },
        &error);
    TagAssignment::invalidate(m_root);
    if (!created) {
        reload();
        setMessage(i18nc("@info", "Could not create the tag “%1”: %2", trimmed, error), true);
        return false;
    }
    Q_EMIT libraryChanged(m_root);
    if (!isAssigning()) {
        m_message.clear();
        m_error = false;
        reload();
        return true;
    }
    reload();
    return setAssigned(tag.id, true);
}

bool TagPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Down) {
        if (QTreeWidgetItem *item = firstVisibleItem()) {
            m_list->setCurrentItem(item);
            m_list->setFocus(Qt::TabFocusReason);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool TagPanel::isAssigning() const
{
    return !m_root.isEmpty() && !m_paths.isEmpty();
}

void TagPanel::applyFilter()
{
    const QString needle = m_filter->text().trimmed();
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        const TagLibrary::Tag *tag = m_library.tag(item->data(0, IdRole).toString());
        bool matches = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive);
        if (!matches && tag) {
            matches = tag->shorthand.contains(needle, Qt::CaseInsensitive)
                || std::any_of(tag->aliases.cbegin(), tag->aliases.cend(), [&needle](const QString &alias) {
                          return alias.contains(needle, Qt::CaseInsensitive);
                      });
        }
        item->setHidden(!matches);
    }
}

void TagPanel::activateSearch()
{
    const QString text = m_filter->text().trimmed();
    if (text.isEmpty() || m_library.isNull()) {
        return;
    }
    const QStringList matches = m_library.findTags(text);
    if (matches.isEmpty()) {
        if (createAndAssign(text)) {
            m_filter->clear();
        }
        return;
    }
    const QString id = matches.first();
    if (!isAssigning()) {
        for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
            if (m_list->topLevelItem(i)->data(0, IdRole).toString() == id) {
                m_list->setCurrentItem(m_list->topLevelItem(i));
            }
        }
        return;
    }
    if (setAssigned(id, assignment(id) != Qt::Checked)) {
        m_filter->clear();
    }
}

void TagPanel::updateCreateButton()
{
    const QString text = m_filter->text().trimmed();
    const bool writable = !m_library.isNull() && !m_library.isReadOnly();
    m_create->setEnabled(writable && !text.isEmpty() && m_library.findTags(text).isEmpty());
    m_create->setToolTip(text.isEmpty() ? i18nc("@info:tooltip", "Type a name in the search field to create a tag.")
                                        : (isAssigning() ? i18nc("@info:tooltip", "Create the tag “%1” and assign it to the selected items.", text)
                                                         : i18nc("@info:tooltip", "Create the tag “%1”.", text)));
}

void TagPanel::updateStatus()
{
    if (m_error) {
        return;
    }
    QString text;
    if (m_root.isEmpty()) {
        text = m_selectionProblem.isEmpty() ? i18nc("@info", "This folder isn't in a tag library. Create one from the Tags menu to tag its files and folders.")
                                            : m_selectionProblem;
    } else if (m_library.isNull()) {
        return;
    } else if (!m_selectionProblem.isEmpty()) {
        text = m_selectionProblem;
    } else if (!isAssigning()) {
        text = i18nc("@info", "Select files or folders to assign tags.");
    } else if (m_library.isReadOnly()) {
        text = i18nc("@info", "The library is read-only, so tags can't be assigned.");
    } else {
        text = i18ncp("@info", "Tags of %1 selected item", "Tags of %1 selected items", m_relativePaths.size());
    }
    setMessage(text);
}

void TagPanel::setMessage(const QString &message, bool error)
{
    m_message = message;
    m_error = error;
    m_status->setText(message);
    m_status->setVisible(!message.isEmpty());
    if (error) {
        m_status->setAccessibleDescription(i18nc("@info:accessible", "Error"));
    } else {
        m_status->setAccessibleDescription({});
    }
}

QTreeWidgetItem *TagPanel::firstVisibleItem() const
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        if (!item->isHidden()) {
            return item;
        }
    }
    return nullptr;
}
