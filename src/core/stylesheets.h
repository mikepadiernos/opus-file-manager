// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <optional>

/**
 * Checks for app-local Qt Style Sheets. Qt Style Sheets are a CSS-like
 * subset: these checks bound their size and refuse input that could load
 * files or change widget behaviour rather than appearance.
 */
namespace StyleSheets
{
/// Larger stylesheets are refused, in characters and in file bytes.
inline constexpr qsizetype MaximumSize = 64 * 1024;
inline constexpr int MaximumRules = 2000;

/**
 * Checks @p sheet without asking Qt to parse it: its size and rule count,
 * balanced braces, parentheses, comments and strings, and that it uses no
 * at-rules such as @import, no qproperty- declarations and no url() other
 * than Qt resources (":/" or "qrc:/").
 * @returns an error message that names the line, or an empty string
 */
QString check(const QString &sheet);

/// Reads a stylesheet file of at most MaximumSize bytes as UTF-8.
std::optional<QString> readFile(const QString &path, QString *error);

/// Writes @p sheet as UTF-8. @returns an error message, or an empty string
QString writeFile(const QString &path, const QString &sheet);
}
