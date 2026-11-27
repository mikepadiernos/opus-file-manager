// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * Editors that open files as alternatives to the built-in editor: KWrite and
 * Kate as separate applications, and terminal editors in a terminal session.
 */
namespace EditorProviders
{
enum class Editor {
    KWrite,
    Kate,
    Neovim,
    Rune,
    Micro,
    Nano,
};

/// All editors, in menu order.
QList<Editor> editors();
/// A stable identifier, such as "nvim".
QString id(Editor editor);
/// The translated name shown in menus and messages.
QString name(Editor editor);
QString iconName(Editor editor);
QString executable(Editor editor);
/// The desktop file name used for startup feedback, or an empty string for terminal editors.
QString desktopName(Editor editor);
/// Whether the editor runs in a terminal session rather than its own window.
bool runsInTerminal(Editor editor);

/// The editor's executable path, or an empty string when it is not in PATH.
QString findExecutable(Editor editor);
/// Why the editor can't be used, or an empty string when it can.
QString unavailableReason(Editor editor);

/**
 * The arguments that open @p urls, without a shell. Local files are passed as
 * absolute paths, so none can be read as an option. KWrite and Kate also
 * receive remote URLs, which they open through KIO; terminal editors accept
 * only local files.
 * @param error receives why the selection can't be opened; the result is
 * then empty
 */
QStringList arguments(Editor editor, const QList<QUrl> &urls, QString *error);
/// The local folder a terminal editor starts in: the first file's folder.
QString workingDirectory(const QList<QUrl> &urls);
}
