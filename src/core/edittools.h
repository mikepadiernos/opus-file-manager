// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "officeapps.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <optional>

/**
 * Chooses a tool that edits a viewable file: the built-in editor for text,
 * or a separate application for office documents, images and audio. Viewing
 * and editing are separate: a format the preview panel shows only has an
 * edit tool when one is listed here.
 */
namespace EditTools
{
enum class Category {
    None,
    Text,
    Office,
    Vector,
    Image,
    Audio,
};

enum class Tool {
    BuiltInEditor,
    LibreOffice,
    Calligra,
    OnlyOffice,
    Inkscape,
    KolourPaint,
    Krita,
    Gimp,
    Audacity,
    Tenacity,
};

/// The edit category of @p mimeType, following MIME inheritance.
Category category(const QString &mimeType);
/// The tools that edit @p category, in order of preference.
QList<Tool> candidates(Category category);

/// The translated name shown in menus and messages.
QString name(Tool tool);
/// The command that starts a separate tool for @p mimeType, or an empty string for the built-in editor.
QString executable(Tool tool, const QString &mimeType = {});
/// The desktop file name used for startup feedback.
QString desktopName(Tool tool, const QString &mimeType = {});
/// Whether the tool opens remote files itself.
bool opensRemoteFiles(Tool tool);
/// The office application behind an office tool.
std::optional<OfficeApps::App> officeApp(Tool tool);

/// A lookup that returns a command's executable path, or an empty string when it is missing.
using Finder = std::function<QString(Tool tool, const QString &mimeType)>;
/// Looks commands up in PATH; office tools follow OfficeApps::findExecutable.
QString findInPath(Tool tool, const QString &mimeType);

struct Choice {
    Tool tool = Tool::BuiltInEditor;
    /// The tool's executable path; empty for the built-in editor.
    QString executable;
};

/**
 * The first available tool that edits @p url.
 * @param error receives why no tool can edit it, naming tools to install
 * when none is available; the result is then empty
 */
std::optional<Choice> choose(const QUrl &url, const QString &mimeType, QString *error, const Finder &finder = findInPath);

/// The arguments that open @p url in a separate tool: an absolute local path or, when supported, the URL.
QStringList arguments(const QUrl &url);
}
