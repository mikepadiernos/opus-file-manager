// SPDX-License-Identifier: GPL-3.0-or-later

#include "taggingcontroller.h"

#include "libraryrefreshdialog.h"
#include "libraryregistry.h"
#include "taglibrary.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>

#include <QAction>
#include <QDir>
#include <QIcon>

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
    dialog->show();
    dialog->start();
    return dialog;
}
