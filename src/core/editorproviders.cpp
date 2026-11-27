// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorproviders.h"
#include "settings.h"

#include <KLocalizedString>
#include <KShell>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace EditorProviders
{
QList<Editor> editors()
{
    return {Editor::KWrite, Editor::Kate, Editor::Neovim, Editor::Rune, Editor::Micro, Editor::Nano};
}

QString id(Editor editor)
{
    return executable(editor);
}

QString name(Editor editor)
{
    switch (editor) {
    case Editor::KWrite:
        return i18nc("@item text editor", "KWrite");
    case Editor::Kate:
        return i18nc("@item text editor", "Kate");
    case Editor::Neovim:
        return i18nc("@item text editor", "Neovim");
    case Editor::Rune:
        return i18nc("@item text editor", "Rune");
    case Editor::Micro:
        return i18nc("@item text editor", "micro");
    case Editor::Nano:
        return i18nc("@item text editor", "nano");
    }
    return {};
}

QString iconName(Editor editor)
{
    switch (editor) {
    case Editor::KWrite:
        return QStringLiteral("kwrite");
    case Editor::Kate:
        return QStringLiteral("kate");
    case Editor::Neovim:
    case Editor::Rune:
    case Editor::Micro:
    case Editor::Nano:
        break;
    }
    return QStringLiteral("utilities-terminal");
}

QString executable(Editor editor)
{
    switch (editor) {
    case Editor::KWrite:
        return QStringLiteral("kwrite");
    case Editor::Kate:
        return QStringLiteral("kate");
    case Editor::Neovim:
        return QStringLiteral("nvim");
    case Editor::Rune:
        return QStringLiteral("rune");
    case Editor::Micro:
        return QStringLiteral("micro");
    case Editor::Nano:
        return QStringLiteral("nano");
    }
    return {};
}

QString desktopName(Editor editor)
{
    return runsInTerminal(editor) ? QString() : QStringLiteral("org.kde.") + executable(editor);
}

bool runsInTerminal(Editor editor)
{
    return editor != Editor::KWrite && editor != Editor::Kate;
}

QString findExecutable(Editor editor)
{
    return QStandardPaths::findExecutable(executable(editor));
}

QString unavailableReason(Editor editor)
{
    if (!findExecutable(editor).isEmpty()) {
        return {};
    }
    return i18nc("@info %1 is a text editor, %2 its command", "%1 is not installed, or its %2 command is not in PATH.", name(editor), executable(editor));
}

QStringList arguments(Editor editor, const QList<QUrl> &urls, QString *error)
{
    if (urls.isEmpty()) {
        *error = i18nc("@info", "Select one or more files to edit.");
        return {};
    }
    QStringList result;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            if (runsInTerminal(editor) || !url.isValid()) {
                *error = i18nc("@info %1 is a text editor", "%1 can only open local files. Copy remote files to a local folder first.", name(editor));
                return {};
            }
            result.append(url.toString());
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()) {
            *error = i18nc("@info %1 is a text editor", "Folders can't be opened in %1. Select files instead.", name(editor));
            return {};
        }
        if (!info.isAbsolute()) {
            *error = i18nc("@info %1 is a file path", "%1 is not an absolute path.", url.toLocalFile());
            return {};
        }
        result.append(QDir::cleanPath(info.absoluteFilePath()));
    }
    return result;
}

QString workingDirectory(const QList<QUrl> &urls)
{
    if (urls.isEmpty() || !urls.first().isLocalFile()) {
        return QDir::homePath();
    }
    const QString folder = QFileInfo(urls.first().toLocalFile()).absolutePath();
    return QFileInfo(folder).isDir() ? QDir::cleanPath(folder) : QDir::homePath();
}

std::optional<Editor> preferredEditor()
{
    switch (Settings::textEditorProvider()) {
    case Settings::EnumTextEditorProvider::KWrite:
        return Editor::KWrite;
    case Settings::EnumTextEditorProvider::Kate:
        return Editor::Kate;
    case Settings::EnumTextEditorProvider::Neovim:
        return Editor::Neovim;
    case Settings::EnumTextEditorProvider::Rune:
        return Editor::Rune;
    case Settings::EnumTextEditorProvider::Micro:
        return Editor::Micro;
    case Settings::EnumTextEditorProvider::Nano:
        return Editor::Nano;
    default:
        return std::nullopt;
    }
}

bool usesSeparateTerminal()
{
    return Settings::terminalEditorSession() == Settings::EnumTerminalEditorSession::Separate;
}

QString terminalCommand(const QString &executable, const QStringList &arguments)
{
    return KShell::joinArgs(QStringList{executable} + arguments);
}
}
