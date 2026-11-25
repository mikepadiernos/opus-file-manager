// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryconfirmations.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

LibraryConfirmations::Confirmation LibraryConfirmations::create(const QString &root, TagLibrary::Storage storage, TagLibrary::Storage preferred)
{
    Confirmation confirmation;
    confirmation.title = i18nc("@title:window", "Create Library");
    confirmation.action = KGuiItem(i18nc("@action:button", "Create Library"), QStringLiteral("tag-new"));
    confirmation.dontAskAgainName = QStringLiteral("ConfirmCreateLibrary");
    if (storage != preferred) {
        confirmation.question = xi18nc("@info",
                                       "<para>Create a tag library rooted at <filename>%1</filename>?</para>"
                                       "<para>A library document for this folder already exists at <filename>%2</filename>, so it is used where it is. "
                                       "Files aren't moved, renamed or copied, and existing labels, ratings and notes are kept.</para>",
                                       root,
                                       TagLibrary::documentPath(root));
    } else if (storage == TagLibrary::Storage::PrivateFolder) {
        confirmation.question = xi18nc("@info",
                                       "<para>Create a tag library rooted at <filename>%1</filename>?</para>"
                                       "<para>Plasma Opus indexes the files and folders below it and stores tags, fields and entries privately in "
                                       "<filename>%2</filename>, so nothing is written to the folder. Files aren't moved, renamed or copied, and "
                                       "existing labels, ratings and notes are kept.</para>",
                                       root,
                                       TagLibrary::privateDocumentPath(root));
    } else {
        confirmation.question = xi18nc("@info",
                                       "<para>Create a tag library rooted at <filename>%1</filename>?</para>"
                                       "<para>Plasma Opus indexes the files and folders below it and stores tags, fields and entries in "
                                       "<filename>%2</filename> in that folder. Files aren't moved, renamed or copied, and existing labels, "
                                       "ratings and notes are kept.</para>",
                                       root,
                                       TagLibrary::fileName());
    }
    return confirmation;
}

LibraryConfirmations::Confirmation LibraryConfirmations::remove(const QString &root)
{
    Confirmation confirmation;
    confirmation.title = i18nc("@title:window", "Remove Library");
    confirmation.question = xi18nc("@info",
                                   "<para>Stop using <filename>%1</filename> as a tag library?</para>"
                                   "<para>The library document <filename>%2</filename> is kept, so you can create the library again later "
                                   "without losing tags. No files are deleted.</para>",
                                   root,
                                   TagLibrary::documentPath(root));
    confirmation.action = KGuiItem(i18nc("@action:button", "Remove Library"), QStringLiteral("list-remove"));
    confirmation.warning = true;
    return confirmation;
}

LibraryConfirmations::Confirmation LibraryConfirmations::moveDocument(const QString &root, TagLibrary::Storage target)
{
    Confirmation confirmation;
    confirmation.title = i18nc("@title:window", "Move Library Metadata");
    if (target == TagLibrary::Storage::PrivateFolder) {
        confirmation.question = xi18nc("@info",
                                       "<para>Keep the metadata of the library <filename>%1</filename> privately?</para>"
                                       "<para>The library document is moved from <filename>%2</filename> to <filename>%3</filename> in Plasma "
                                       "Opus's data folder, so nothing is kept in the library folder. Tags, fields and entries are kept, and no "
                                       "other file is changed.</para>",
                                       root,
                                       TagLibrary::folderDocumentPath(root),
                                       TagLibrary::privateDocumentPath(root));
        confirmation.action = KGuiItem(i18nc("@action:button", "Keep Privately"), QStringLiteral("security-high"));
    } else {
        confirmation.question = xi18nc("@info",
                                       "<para>Keep the metadata of the library <filename>%1</filename> in the library folder?</para>"
                                       "<para>The library document is moved from <filename>%2</filename> in Plasma Opus's data folder to "
                                       "<filename>%3</filename>. Tags, fields and entries are kept, and no other file is changed.</para>",
                                       root,
                                       TagLibrary::privateDocumentPath(root),
                                       TagLibrary::folderDocumentPath(root));
        confirmation.action = KGuiItem(i18nc("@action:button", "Keep in Library Folder"), QStringLiteral("folder"));
    }
    return confirmation;
}

bool LibraryConfirmations::ask(QWidget *parent, const Confirmation &confirmation)
{
    const auto answer = confirmation.warning ? KMessageBox::warningTwoActions(parent,
                                                                              confirmation.question,
                                                                              confirmation.title,
                                                                              confirmation.action,
                                                                              KStandardGuiItem::cancel(),
                                                                              confirmation.dontAskAgainName)
                                             : KMessageBox::questionTwoActions(parent,
                                                                               confirmation.question,
                                                                               confirmation.title,
                                                                               confirmation.action,
                                                                               KStandardGuiItem::cancel(),
                                                                               confirmation.dontAskAgainName);
    return answer == KMessageBox::PrimaryAction;
}
