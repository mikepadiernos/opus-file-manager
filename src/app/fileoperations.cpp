// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileoperations.h"

#include "executables.h"
#include "settings.h"
#include "symlinks.h"
#include "transferqueue.h"

#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/DropJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KIO/Paste>
#include <KIO/PasteJob>
#include <KIO/RenameFileDialog>
#include <KIO/SimpleJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KPropertiesDialog>
#include <KTerminalLauncherJob>

#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QPointer>

namespace
{
void attachWindow(KJob *job, QWidget *window)
{
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    KJobWidgets::setWindow(job, window);
}
}

TransferQueue *FileOperations::transferQueue()
{
    static auto *queue = new TransferQueue(qApp);
    queue->setConcurrency(Settings::transferQueueEnabled() ? Settings::transferQueueConcurrency() : 0);
    return queue;
}

KJob *FileOperations::copy(const QList<QUrl> &sources, const QUrl &destination, QWidget *window)
{
    if (sources.isEmpty() || !destination.isValid()) {
        return nullptr;
    }
    const QPointer<QWidget> safeWindow(window);
    const int conflict = Settings::transferConflictPolicy();
    return transferQueue()->enqueue(
        i18np("Copy one item to %2", "Copy %1 items to %2", sources.size(), destination.toDisplayString()),
        [sources, destination, safeWindow, conflict] {
            KIO::CopyJob *job =
                KIO::copy(sources, destination, conflict == Settings::EnumTransferConflictPolicy::Overwrite ? KIO::Overwrite : KIO::DefaultFlags);
            job->setAutoSkip(conflict == Settings::EnumTransferConflictPolicy::Skip);
            job->setAutoRename(conflict == Settings::EnumTransferConflictPolicy::Rename);
            attachWindow(job, safeWindow);
            KIO::FileUndoManager::self()->recordCopyJob(job);
            return job;
        });
}

KJob *FileOperations::move(const QList<QUrl> &sources, const QUrl &destination, QWidget *window)
{
    if (sources.isEmpty() || !destination.isValid()) {
        return nullptr;
    }
    const QPointer<QWidget> safeWindow(window);
    const int conflict = Settings::transferConflictPolicy();
    return transferQueue()->enqueue(
        i18np("Move one item to %2", "Move %1 items to %2", sources.size(), destination.toDisplayString()),
        [sources, destination, safeWindow, conflict] {
            KIO::CopyJob *job =
                KIO::move(sources, destination, conflict == Settings::EnumTransferConflictPolicy::Overwrite ? KIO::Overwrite : KIO::DefaultFlags);
            job->setAutoSkip(conflict == Settings::EnumTransferConflictPolicy::Skip);
            job->setAutoRename(conflict == Settings::EnumTransferConflictPolicy::Rename);
            attachWindow(job, safeWindow);
            KIO::FileUndoManager::self()->recordCopyJob(job);
            return job;
        });
}

KJob *FileOperations::trash(const QList<QUrl> &urls, QWidget *window)
{
    if (urls.isEmpty()) {
        return nullptr;
    }
    auto *job = new KIO::DeleteOrTrashJob(urls, KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation, window);
    job->start();
    return job;
}

KJob *FileOperations::deletePermanently(const QList<QUrl> &urls, QWidget *window)
{
    if (urls.isEmpty()) {
        return nullptr;
    }
    auto *job = new KIO::DeleteOrTrashJob(urls, KIO::AskUserActionInterface::Delete, KIO::AskUserActionInterface::DefaultConfirmation, window);
    job->start();
    return job;
}

void FileOperations::rename(const KFileItemList &items, QWidget *window)
{
    if (items.isEmpty()) {
        return;
    }
    auto *dialog = new KIO::RenameFileDialog(items, window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

KJob *FileOperations::createSymlink(const QUrl &target, const QString &name, QString &error, QWidget *window)
{
    KIO::SimpleJob *job = Symlinks::create(target, name, error);
    if (job) {
        attachWindow(job, window);
    }
    return job;
}

void FileOperations::copyToClipboard(const QList<QUrl> &urls, bool cut)
{
    if (urls.isEmpty()) {
        return;
    }
    auto *mimeData = new QMimeData;
    mimeData->setUrls(urls);
    KIO::setClipboardDataCut(mimeData, cut);
    QApplication::clipboard()->setMimeData(mimeData);
}

bool FileOperations::paste(const QUrl &destination, QWidget *window)
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData || !destination.isValid()) {
        return false;
    }
    bool enabled = false;
    KIO::pasteActionText(mimeData, &enabled, KFileItem(destination));
    if (!enabled) {
        return false;
    }
    KIO::PasteJob *job = KIO::paste(mimeData, destination);
    attachWindow(job, window);
    return true;
}

QString FileOperations::pasteActionText(const KFileItem &destination, bool *enabled)
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData) {
        if (enabled) {
            *enabled = false;
        }
        return QString();
    }
    return KIO::pasteActionText(mimeData, enabled, destination);
}

void FileOperations::open(const KFileItem &item, QWidget *window)
{
    if (item.isNull()) {
        return;
    }
    const auto action = Settings::executableAction();
    const QString path = item.localPath();
    const bool program = !path.isEmpty() && !item.isDir() && Executables::isProgram(item.mimetype(), QFileInfo(path).isExecutable());
    if (program && (action == Settings::EnumExecutableAction::RunInTerminal || action == Settings::EnumExecutableAction::RunInTerminalKeepOpen)) {
        auto *terminal = new KTerminalLauncherJob(Executables::terminalCommand(path, action == Settings::EnumExecutableAction::RunInTerminalKeepOpen));
        terminal->setWorkingDirectory(QFileInfo(path).absolutePath());
        terminal->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
        terminal->start();
        return;
    }
    auto *job = new KIO::OpenUrlJob(item.targetUrl(), item.mimetype());
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->setRunExecutables(action == Settings::EnumExecutableAction::Run);
    job->setShowOpenOrExecuteDialog(action == Settings::EnumExecutableAction::Ask);
    job->start();
}

KIO::DropJob *FileOperations::drop(const QDropEvent *event, const QUrl &destination, QWidget *window)
{
    const auto setting = Settings::dropAction();
    const bool modifierHeld = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
    KIO::DropJob *job = nullptr;
    if (modifierHeld || setting == Settings::EnumDropAction::Ask) {
        job = KIO::drop(event, destination);
    } else {
        const bool copy = setting == Settings::EnumDropAction::Copy;
        QDropEvent decided(event->position(), event->possibleActions(), event->mimeData(), event->buttons(), copy ? Qt::ControlModifier : Qt::ShiftModifier);
        decided.setDropAction(copy ? Qt::CopyAction : Qt::MoveAction);
        job = KIO::drop(&decided, destination);
    }
    KJobWidgets::setWindow(job, window);
    return job;
}

void FileOperations::openTerminal(const QUrl &folder, QWidget *window)
{
    auto *job = new KTerminalLauncherJob(QString());
    job->setWorkingDirectory(folder.isLocalFile() ? folder.toLocalFile() : QString());
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->start();
}

void FileOperations::showProperties(const KFileItemList &items, QWidget *window)
{
    if (items.isEmpty()) {
        return;
    }
    KPropertiesDialog::showDialog(items, window, false);
}
