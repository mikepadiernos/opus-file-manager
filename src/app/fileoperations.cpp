// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileoperations.h"

#include <KConfigGroup>
#include <KIO/CopyJob>
#include <KIO/DeleteOrTrashJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KIO/Paste>
#include <KIO/PasteJob>
#include <KIO/RenameFileDialog>
#include <KJobWidgets>
#include <KPropertiesDialog>
#include <KSharedConfig>
#include <KShell>
#include <KTerminalLauncherJob>

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QMimeData>

namespace
{
void attachWindow(KJob *job, QWidget *window)
{
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    KJobWidgets::setWindow(job, window);
}
}

void FileOperations::copy(const QList<QUrl> &sources, const QUrl &destination, QWidget *window)
{
    if (sources.isEmpty() || !destination.isValid()) {
        return;
    }
    KIO::CopyJob *job = KIO::copy(sources, destination);
    attachWindow(job, window);
    KIO::FileUndoManager::self()->recordCopyJob(job);
}

void FileOperations::move(const QList<QUrl> &sources, const QUrl &destination, QWidget *window)
{
    if (sources.isEmpty() || !destination.isValid()) {
        return;
    }
    KIO::CopyJob *job = KIO::move(sources, destination);
    attachWindow(job, window);
    KIO::FileUndoManager::self()->recordCopyJob(job);
}

void FileOperations::trash(const QList<QUrl> &urls, QWidget *window)
{
    if (urls.isEmpty()) {
        return;
    }
    auto *job = new KIO::DeleteOrTrashJob(urls, KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation, window);
    job->start();
}

void FileOperations::deletePermanently(const QList<QUrl> &urls, QWidget *window)
{
    if (urls.isEmpty()) {
        return;
    }
    auto *job = new KIO::DeleteOrTrashJob(urls, KIO::AskUserActionInterface::Delete, KIO::AskUserActionInterface::ForceConfirmation, window);
    job->start();
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
    const QString action = KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).readEntry("ExecutableAction", QStringLiteral("Ask"));
    const QString path = item.localPath();
    const bool program = !path.isEmpty() && !item.isDir() && QFileInfo(path).isExecutable();
    if (program && (action == QLatin1String("RunInTerminal") || action == QLatin1String("RunInTerminalKeepOpen"))) {
        const QString quoted = KShell::quoteArg(path);
        const bool keepOpen = action == QLatin1String("RunInTerminalKeepOpen");
        const QString command = keepOpen ? QStringLiteral("sh -c ") + KShell::quoteArg(quoted + QStringLiteral("; exec \"${SHELL:-sh}\"")) : quoted;
        auto *terminal = new KTerminalLauncherJob(command);
        terminal->setWorkingDirectory(QFileInfo(path).absolutePath());
        attachWindow(terminal, window);
        terminal->start();
        return;
    }
    auto *job = new KIO::OpenUrlJob(item.targetUrl(), item.mimetype());
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->setRunExecutables(action == QLatin1String("Run"));
    job->setShowOpenOrExecuteDialog(action == QLatin1String("Ask"));
    job->start();
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
