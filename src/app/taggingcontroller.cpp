// SPDX-License-Identifier: GPL-3.0-or-later

#include "taggingcontroller.h"

#include "fieldeditdialog.h"
#include "librarymovetracker.h"
#include "libraryrefreshdialog.h"
#include "libraryregistry.h"
#include "libraryreviewdialog.h"
#include "tagassignment.h"
#include "tagfieldmanagerwidget.h"
#include "taggroupmanagerwidget.h"
#include "taglibrary.h"
#include "tagmanagerwidget.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QIcon>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

TaggingController::TaggingController(QWidget *window, KActionCollection *actions, Context context)
    : QObject(window)
    , m_window(window)
    , m_actions(actions)
    , m_context(std::move(context))
{
    QAction *create = actions->addAction(QStringLiteral("create_library"), this, &TaggingController::createLibrary);
    create->setText(i18nc("@action", "Create Library from Folder…"));
    create->setIcon(QIcon::fromTheme(QStringLiteral("tag-new"), QIcon::fromTheme(QStringLiteral("tag"))));
    create->setToolTip(i18nc("@info:tooltip", "Index the current folder as a tag library without moving or changing its files"));

    QAction *remove = actions->addAction(QStringLiteral("remove_library"), this, &TaggingController::removeLibrary);
    remove->setText(i18nc("@action", "Remove Library…"));
    remove->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete-remove"), QIcon::fromTheme(QStringLiteral("list-remove"))));

    QAction *refresh = actions->addAction(QStringLiteral("refresh_library"), this, [this] {
        if (const QString root = currentLibrary(); !root.isEmpty()) {
            refreshLibrary(root);
        }
    });
    refresh->setText(i18nc("@action", "Refresh Library"));
    refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    refresh->setToolTip(i18nc("@info:tooltip", "Index new items and find missing ones in the current library"));

    QAction *review = actions->addAction(QStringLiteral("review_library"), this, [this] {
        if (const QString root = currentLibrary(); !root.isEmpty()) {
            reviewLibrary(root);
        }
    });
    review->setText(i18nc("@action", "Review Missing Entries…"));
    review->setIcon(QIcon::fromTheme(QStringLiteral("link")));

    QAction *manage = actions->addAction(QStringLiteral("manage_tags"), this, [this] {
        if (const QString root = currentLibrary(); !root.isEmpty()) {
            manageTags(root);
        }
    });
    manage->setText(i18nc("@action", "Manage Tags…"));
    manage->setIcon(QIcon::fromTheme(QStringLiteral("tag")));
    manage->setToolTip(i18nc("@info:tooltip", "Create, edit, group and delete the tags and fields of the current library"));

    QAction *fields = actions->addAction(QStringLiteral("edit_fields"), this, [this] {
        const KFileItemList items = m_context.selection ? m_context.selection() : KFileItemList();
        if (items.isEmpty()) {
            m_context.status(i18nc("@info", "Select the items whose fields you want to edit."));
            return;
        }
        QStringList paths;
        for (const KFileItem &item : items) {
            if (!item.url().isLocalFile()) {
                m_context.status(i18nc("@info", "Only local files and folders have fields."));
                return;
            }
            if (item.isLink()) {
                m_context.status(i18nc("@info", "Symbolic links can't have fields."));
                return;
            }
            paths.append(QDir::cleanPath(item.url().toLocalFile()));
        }
        editFields(paths);
    });
    fields->setText(i18nc("@action", "Edit Fields…"));
    fields->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    fields->setToolTip(i18nc("@info:tooltip", "Edit the library's text and date fields of the selected items"));

    LibraryMoveTracker *tracker = LibraryMoveTracker::instance();
    connect(tracker, &LibraryMoveTracker::librariesChanged, this, [this](const QStringList &roots) {
        for (const QString &root : roots) {
            Q_EMIT libraryChanged(root);
        }
    });
    connect(tracker, &LibraryMoveTracker::failed, this, [this](const QString &message) {
        if (m_window->isActiveWindow()) {
            m_context.status(i18nc("@info", "Could not update the library after a move: %1", message));
        }
    });
}

QString TaggingController::currentLibrary() const
{
    const QUrl folder = m_context.folder ? m_context.folder() : QUrl();
    return folder.isLocalFile() ? LibraryRegistry::rootFor(folder.toLocalFile()) : QString();
}

void TaggingController::updateActions()
{
    const QUrl folder = m_context.folder ? m_context.folder() : QUrl();
    const QString library = currentLibrary();
    m_actions->action(QStringLiteral("create_library"))->setEnabled(folder.isLocalFile() && library.isEmpty());
    m_actions->action(QStringLiteral("remove_library"))->setEnabled(!library.isEmpty());
    m_actions->action(QStringLiteral("refresh_library"))->setEnabled(!library.isEmpty());
    m_actions->action(QStringLiteral("review_library"))->setEnabled(!library.isEmpty());
    m_actions->action(QStringLiteral("manage_tags"))->setEnabled(!library.isEmpty());
    m_actions->action(QStringLiteral("edit_fields"))->setEnabled(!library.isEmpty());
}

void TaggingController::createLibrary()
{
    const QUrl folder = m_context.folder ? m_context.folder() : QUrl();
    if (!folder.isLocalFile()) {
        m_context.status(i18nc("@info", "Libraries must be local folders."));
        return;
    }
    const QString root = QDir::cleanPath(folder.toLocalFile());
    const auto answer =
        KMessageBox::questionTwoActions(m_window,
                                        xi18nc("@info",
                                               "<para>Create a tag library rooted at <filename>%1</filename>?</para>"
                                               "<para>Plasma Opus indexes the files and folders below it and stores tags, fields and entries in "
                                               "<filename>%2</filename> in that folder. Files aren't moved, renamed or copied, and existing labels, "
                                               "ratings and notes are kept.</para>",
                                               root,
                                               TagLibrary::fileName()),
                                        i18nc("@title:window", "Create Library"),
                                        KGuiItem(i18nc("@action:button", "Create Library"), QStringLiteral("tag-new")),
                                        KStandardGuiItem::cancel(),
                                        QStringLiteral("ConfirmCreateLibrary"));
    if (answer != KMessageBox::PrimaryAction) {
        return;
    }
    if (const QString error = LibraryRegistry::create(root); !error.isEmpty()) {
        KMessageBox::error(m_window, error, i18nc("@title:window", "Create Library"));
        return;
    }
    Q_EMIT libraryChanged(root);
    updateActions();
    refreshLibrary(root);
}

void TaggingController::removeLibrary()
{
    const QString root = currentLibrary();
    if (root.isEmpty()) {
        return;
    }
    const auto answer =
        KMessageBox::warningTwoActions(m_window,
                                       xi18nc("@info",
                                              "<para>Stop using <filename>%1</filename> as a tag library?</para>"
                                              "<para>The library document <filename>%2</filename> is kept, so you can create the library again later "
                                              "without losing tags. No files are deleted.</para>",
                                              root,
                                              TagLibrary::fileName()),
                                       i18nc("@title:window", "Remove Library"),
                                       KGuiItem(i18nc("@action:button", "Remove Library"), QStringLiteral("list-remove")),
                                       KStandardGuiItem::cancel());
    if (answer != KMessageBox::PrimaryAction) {
        return;
    }
    LibraryRegistry::remove(root);
    m_context.status(i18nc("@info", "Removed the library %1; its document was kept.", root));
    Q_EMIT libraryChanged(root);
    updateActions();
}

QWidget *TaggingController::refreshLibrary(const QString &root)
{
    auto *dialog = new LibraryRefreshDialog(root, m_window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &LibraryRefreshDialog::libraryChanged, this, &TaggingController::libraryChanged);
    connect(dialog, &LibraryRefreshDialog::reviewRequested, this, [this, dialog](const QString &library) {
        dialog->close();
        reviewLibrary(library);
    });
    dialog->show();
    dialog->start();
    return dialog;
}

QWidget *TaggingController::reviewLibrary(const QString &root)
{
    auto *dialog = new LibraryReviewDialog(root, m_window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &LibraryReviewDialog::libraryChanged, this, &TaggingController::libraryChanged);
    dialog->show();
    return dialog;
}

QWidget *TaggingController::editFields(const QStringList &absolutePaths)
{
    const TagAssignment::Selection selection = TagAssignment::resolve(absolutePaths);
    if (!selection.problem.isEmpty()) {
        m_context.status(selection.problem);
        return nullptr;
    }
    auto *dialog = new FieldEditDialog(selection.root, absolutePaths, m_window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &FieldEditDialog::libraryChanged, this, &TaggingController::libraryChanged);
    connect(dialog, &FieldEditDialog::manageFieldsRequested, this, [this](const QString &root) {
        manageTags(root, ManagerPage::Fields);
    });
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        if (const int changed = dialog->changedCount(); changed > 0) {
            m_context.status(i18ncp("@info", "Updated the fields of one item.", "Updated the fields of %1 items.", changed));
        }
    });
    dialog->open();
    return dialog;
}

QWidget *TaggingController::manageTags(const QString &root, ManagerPage page)
{
    if (auto *existing = m_window->findChild<QDialog *>(QStringLiteral("tagsDialog"), Qt::FindDirectChildrenOnly)) {
        auto *manager = existing->findChild<TagManagerWidget *>();
        auto *groups = existing->findChild<TagGroupManagerWidget *>();
        auto *fields = existing->findChild<TagFieldManagerWidget *>();
        auto *intro = existing->findChild<QLabel *>(QStringLiteral("tagsDialogIntro"));
        if (manager->library() != root) {
            manager->setLibrary(root);
            groups->setLibrary(root);
            fields->setLibrary(root);
            intro->setText(xi18nc("@info", "Tags of the library <filename>%1</filename>.", root));
        }
        if (page != ManagerPage::Tags) {
            existing->findChild<QTabWidget *>(QStringLiteral("tagsDialogTabs"))->setCurrentIndex(int(page));
        }
        existing->raise();
        existing->activateWindow();
        return existing;
    }
    auto *dialog = new QDialog(m_window);
    dialog->setObjectName(QStringLiteral("tagsDialog"));
    dialog->setWindowTitle(i18nc("@title:window", "Manage Tags"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    auto *intro = new QLabel(xi18nc("@info", "Tags of the library <filename>%1</filename>.", root), dialog);
    intro->setObjectName(QStringLiteral("tagsDialogIntro"));
    intro->setWordWrap(true);
    auto *tabs = new QTabWidget(dialog);
    tabs->setObjectName(QStringLiteral("tagsDialogTabs"));
    auto *manager = new TagManagerWidget(tabs);
    manager->setLibrary(root);
    auto *groups = new TagGroupManagerWidget(tabs);
    groups->setLibrary(root);
    tabs->addTab(manager, QIcon::fromTheme(QStringLiteral("tag")), i18nc("@title:tab", "&Tags"));
    tabs->addTab(groups, QIcon::fromTheme(QStringLiteral("folder-tag"), QIcon::fromTheme(QStringLiteral("folder"))), i18nc("@title:tab", "&Groups"));
    auto *fields = new TagFieldManagerWidget(tabs);
    fields->setLibrary(root);
    tabs->addTab(fields, QIcon::fromTheme(QStringLiteral("view-list-details")), i18nc("@title:tab", "&Fields"));
    tabs->setCurrentIndex(int(page));
    // Tag names appear in the groups list and group names in the tags list.
    connect(manager, &TagManagerWidget::libraryChanged, groups, &TagGroupManagerWidget::reload);
    connect(groups, &TagGroupManagerWidget::libraryChanged, manager, &TagManagerWidget::reload);
    connect(manager, &TagManagerWidget::libraryChanged, this, &TaggingController::libraryChanged);
    connect(groups, &TagGroupManagerWidget::libraryChanged, this, &TaggingController::libraryChanged);
    connect(fields, &TagFieldManagerWidget::libraryChanged, this, &TaggingController::libraryChanged);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(intro);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);
    dialog->resize(640, 480);
    dialog->show();
    return dialog;
}
