// SPDX-License-Identifier: GPL-3.0-or-later

#include "edittools.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

namespace EditTools
{
namespace
{
bool inherits(const QMimeType &type, const QString &mimeType, const char *base)
{
    const QString name = QLatin1String(base);
    return mimeType == name || (type.isValid() && type.inherits(name));
}

QString toolList(const QList<Tool> &tools)
{
    QStringList names;
    for (const Tool tool : tools) {
        if (tool != Tool::BuiltInEditor) {
            names.append(name(tool));
        }
    }
    if (names.size() < 2) {
        return names.join(QString());
    }
    const QString last = names.takeLast();
    return i18nc("@item alternatives; %1 is a comma-separated list, %2 the last item", "%1 or %2", names.join(QStringLiteral(", ")), last);
}
}

Category category(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return Category::None;
    }
    if (OfficeApps::isOfficeDocument(mimeType)) {
        return Category::Office;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    if (inherits(type, mimeType, "image/svg+xml")) {
        return Category::Vector;
    }
    if (inherits(type, mimeType, "text/plain") || mimeType == QLatin1String("application/x-zerosize")) {
        return Category::Text;
    }
    if (mimeType.startsWith(QLatin1String("image/"))) {
        return Category::Image;
    }
    if (mimeType.startsWith(QLatin1String("audio/"))) {
        return Category::Audio;
    }
    return Category::None;
}

QList<Tool> candidates(Category category)
{
    switch (category) {
    case Category::Text:
        return {Tool::BuiltInEditor};
    case Category::Office:
        return {Tool::LibreOffice, Tool::Calligra, Tool::OnlyOffice};
    case Category::Vector:
        // SVG is XML, so the built-in editor can edit its source.
        return {Tool::Inkscape, Tool::BuiltInEditor};
    case Category::Image:
        return {Tool::KolourPaint, Tool::Krita, Tool::Gimp};
    case Category::Audio:
        return {Tool::Audacity, Tool::Tenacity};
    case Category::None:
        break;
    }
    return {};
}

std::optional<OfficeApps::App> officeApp(Tool tool)
{
    switch (tool) {
    case Tool::LibreOffice:
        return OfficeApps::App::LibreOffice;
    case Tool::Calligra:
        return OfficeApps::App::Calligra;
    case Tool::OnlyOffice:
        return OfficeApps::App::OnlyOffice;
    default:
        return std::nullopt;
    }
}

QString name(Tool tool)
{
    if (const auto app = officeApp(tool)) {
        return OfficeApps::name(*app);
    }
    switch (tool) {
    case Tool::BuiltInEditor:
        return i18nc("@item edit tool", "the built-in editor");
    case Tool::Inkscape:
        return i18nc("@item edit tool", "Inkscape");
    case Tool::KolourPaint:
        return i18nc("@item edit tool", "KolourPaint");
    case Tool::Krita:
        return i18nc("@item edit tool", "Krita");
    case Tool::Gimp:
        return i18nc("@item edit tool", "GIMP");
    case Tool::Audacity:
        return i18nc("@item edit tool", "Audacity");
    case Tool::Tenacity:
        return i18nc("@item edit tool", "Tenacity");
    default:
        return {};
    }
}

QString executable(Tool tool, const QString &mimeType)
{
    if (const auto app = officeApp(tool)) {
        return OfficeApps::executable(*app, OfficeApps::kind(mimeType));
    }
    switch (tool) {
    case Tool::Inkscape:
        return QStringLiteral("inkscape");
    case Tool::KolourPaint:
        return QStringLiteral("kolourpaint");
    case Tool::Krita:
        return QStringLiteral("krita");
    case Tool::Gimp:
        return QStringLiteral("gimp");
    case Tool::Audacity:
        return QStringLiteral("audacity");
    case Tool::Tenacity:
        return QStringLiteral("tenacity");
    default:
        return {};
    }
}

QString desktopName(Tool tool, const QString &mimeType)
{
    if (const auto app = officeApp(tool)) {
        return OfficeApps::desktopName(*app, OfficeApps::kind(mimeType));
    }
    switch (tool) {
    case Tool::Inkscape:
        return QStringLiteral("org.inkscape.Inkscape");
    case Tool::KolourPaint:
        return QStringLiteral("org.kde.kolourpaint");
    case Tool::Krita:
        return QStringLiteral("org.kde.krita");
    case Tool::Gimp:
        return QStringLiteral("org.gimp.GIMP");
    case Tool::Audacity:
        return QStringLiteral("audacity");
    case Tool::Tenacity:
        return QStringLiteral("org.tenacityaudio.Tenacity");
    default:
        return {};
    }
}

bool opensRemoteFiles(Tool tool)
{
    if (const auto app = officeApp(tool)) {
        return OfficeApps::opensRemoteFiles(*app);
    }
    return tool == Tool::BuiltInEditor || tool == Tool::KolourPaint;
}

QString findInPath(Tool tool, const QString &mimeType)
{
    if (const auto app = officeApp(tool)) {
        return OfficeApps::findExecutable(*app, OfficeApps::kind(mimeType));
    }
    const QString command = executable(tool, mimeType);
    return command.isEmpty() ? QString() : QStandardPaths::findExecutable(command);
}

std::optional<Choice> choose(const QUrl &url, const QString &mimeType, QString *error, const Finder &finder)
{
    const Category fileCategory = category(mimeType);
    QList<Tool> tools = candidates(fileCategory);
    std::optional<Tool> preferred;
    if (fileCategory == Category::Office) {
        if (const auto app = OfficeApps::preferredApp()) {
            for (const Tool tool : std::as_const(tools)) {
                if (officeApp(tool) == app) {
                    preferred = tool;
                }
            }
            if (preferred) {
                tools = {*preferred};
            }
        }
    }
    if (tools.isEmpty()) {
        const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
        *error = i18nc("@info %1 is a file name, %2 a file type",
                       "Plasma Opus has no edit tool for %1 (%2).",
                       url.fileName(),
                       type.isValid() ? type.comment() : mimeType);
        return std::nullopt;
    }
    if (url.isLocalFile() && (!QFileInfo(url.toLocalFile()).isAbsolute() || QFileInfo(url.toLocalFile()).isDir())) {
        *error = i18nc("@info %1 is a file path", "%1 is not a file that can be edited.", url.toLocalFile());
        return std::nullopt;
    }
    std::optional<Tool> localOnly;
    for (const Tool tool : tools) {
        if (tool == Tool::BuiltInEditor) {
            return Choice{tool, {}};
        }
        const QString path = finder(tool, mimeType);
        if (path.isEmpty()) {
            continue;
        }
        if (!url.isLocalFile() && !opensRemoteFiles(tool)) {
            localOnly = localOnly.value_or(tool);
            continue;
        }
        return Choice{tool, path};
    }
    if (localOnly) {
        *error = i18nc("@info %1 is an application", "%1 can only open local files. Copy the file to a local folder first.", name(*localOnly));
        return std::nullopt;
    }
    if (preferred) {
        *error = i18nc("@info %1 is an office application, %2 its command",
                       "%1, chosen under Preferences › Applications › Office, is not installed, or its %2 command is not in PATH.",
                       name(*preferred),
                       executable(*preferred, mimeType));
        return std::nullopt;
    }
    switch (fileCategory) {
    case Category::Office:
        *error = i18nc("@info %1 lists applications", "No office application is installed. Install %1.", toolList(tools));
        break;
    case Category::Image:
        *error = i18nc("@info %1 lists applications", "No image editor is installed. Install %1.", toolList(tools));
        break;
    case Category::Audio:
        *error = i18nc("@info %1 lists applications", "No audio editor is installed. Install %1.", toolList(tools));
        break;
    default:
        *error = i18nc("@info %1 lists applications", "No edit tool is installed. Install %1.", toolList(tools));
        break;
    }
    return std::nullopt;
}

QStringList arguments(const QUrl &url)
{
    if (url.isLocalFile()) {
        return {QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath())};
    }
    return {url.toString()};
}
}
