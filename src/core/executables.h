// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/// Deciding what counts as a program and how to run one in a terminal.
namespace Executables
{
/**
 * Whether a file is a program the user may want to run: it has the
 * executable bit and is a binary, a shell script, or another text file.
 * @param mimeType the file's MIME type
 * @param executableBit whether the file system marks it executable
 */
bool isProgram(const QString &mimeType, bool executableBit);

/**
 * The shell command a terminal runs for a program.
 * @param program the program's local path
 * @param keepOpen whether the terminal stays open, with a shell, after the
 *        program ends
 * @returns the command, with the path quoted for the shell
 */
QString terminalCommand(const QString &program, bool keepOpen);
}
