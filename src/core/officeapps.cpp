// SPDX-License-Identifier: GPL-3.0-or-later

#include "officeapps.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace OfficeApps
{
namespace
{
// Base types; templates, macro-enabled variants and aliases inherit from or resolve to them.
const std::pair<const char *, Kind> MimeKinds[] = {
    {"application/vnd.oasis.opendocument.text", Kind::Document},
    {"application/vnd.oasis.opendocument.text-template", Kind::Document},
    {"application/vnd.oasis.opendocument.text-master", Kind::Document},
    {"application/msword", Kind::Document},
    {"application/msword-template", Kind::Document},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", Kind::Document},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.template", Kind::Document},
    {"application/vnd.ms-word.document.macroEnabled.12", Kind::Document},
    {"application/vnd.oasis.opendocument.spreadsheet", Kind::Spreadsheet},
    {"application/vnd.oasis.opendocument.spreadsheet-template", Kind::Spreadsheet},
    {"application/vnd.ms-excel", Kind::Spreadsheet},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", Kind::Spreadsheet},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.template", Kind::Spreadsheet},
    {"application/vnd.ms-excel.sheet.macroEnabled.12", Kind::Spreadsheet},
    {"application/vnd.oasis.opendocument.presentation", Kind::Presentation},
    {"application/vnd.oasis.opendocument.presentation-template", Kind::Presentation},
    {"application/vnd.ms-powerpoint", Kind::Presentation},
    {"application/vnd.openxmlformats-officedocument.presentationml.presentation", Kind::Presentation},
    {"application/vnd.openxmlformats-officedocument.presentationml.slideshow", Kind::Presentation},
    {"application/vnd.openxmlformats-officedocument.presentationml.template", Kind::Presentation},
    {"application/vnd.ms-powerpoint.presentation.macroEnabled.12", Kind::Presentation},
};

QString kindSuffix(Kind kind)
{
    switch (kind) {
    case Kind::Document:
        return QStringLiteral("words");
    case Kind::Spreadsheet:
        return QStringLiteral("sheets");
    case Kind::Presentation:
        return QStringLiteral("stage");
    case Kind::None:
        break;
    }
    return {};
}
}

Kind kind(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return Kind::None;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    for (const auto &[name, kind] : MimeKinds) {
        const QString base = QLatin1String(name);
        if (mimeType == base || (type.isValid() && type.inherits(base))) {
            return kind;
        }
    }
    return Kind::None;
}

bool isOfficeDocument(const QString &mimeType)
{
    return kind(mimeType) != Kind::None;
}

QList<App> apps()
{
    return {App::LibreOffice, App::Calligra, App::OnlyOffice};
}

QString id(App app)
{
    switch (app) {
    case App::LibreOffice:
        return QStringLiteral("libreoffice");
    case App::Calligra:
        return QStringLiteral("calligra");
    case App::OnlyOffice:
        return QStringLiteral("onlyoffice");
    }
    return {};
}

QString name(App app)
{
    switch (app) {
    case App::LibreOffice:
        return i18nc("@item office application", "LibreOffice");
    case App::Calligra:
        return i18nc("@item office application", "Calligra");
    case App::OnlyOffice:
        return i18nc("@item office application", "ONLYOFFICE");
    }
    return {};
}

QString iconName(App app)
{
    switch (app) {
    case App::LibreOffice:
        return QStringLiteral("libreoffice-startcenter");
    case App::Calligra:
        return QStringLiteral("calligrawords");
    case App::OnlyOffice:
        return QStringLiteral("onlyoffice-desktopeditors");
    }
    return {};
}

QString executable(App app, Kind kind)
{
    switch (app) {
    case App::LibreOffice:
        return QStringLiteral("libreoffice");
    case App::Calligra:
        return kind == Kind::None ? QString() : QStringLiteral("calligra") + kindSuffix(kind);
    case App::OnlyOffice:
        return QStringLiteral("onlyoffice-desktopeditors");
    }
    return {};
}

QString desktopName(App app, Kind kind)
{
    switch (app) {
    case App::LibreOffice:
        switch (kind) {
        case Kind::Document:
            return QStringLiteral("libreoffice-writer");
        case Kind::Spreadsheet:
            return QStringLiteral("libreoffice-calc");
        case Kind::Presentation:
            return QStringLiteral("libreoffice-impress");
        case Kind::None:
            break;
        }
        return QStringLiteral("libreoffice-startcenter");
    case App::Calligra:
        return kind == Kind::None ? QString() : QStringLiteral("org.kde.calligra") + kindSuffix(kind);
    case App::OnlyOffice:
        return QStringLiteral("onlyoffice-desktopeditors");
    }
    return {};
}

bool opensRemoteFiles(App app)
{
    return app == App::Calligra;
}

QString findExecutable(App app, Kind kind)
{
    const QString command = executable(app, kind);
    if (command.isEmpty()) {
        return {};
    }
    const QString found = QStandardPaths::findExecutable(command);
    if (found.isEmpty() && app == App::LibreOffice) {
        // Upstream packages and some distributions only install the soffice launcher.
        return QStandardPaths::findExecutable(QStringLiteral("soffice"));
    }
    return found;
}

QString unavailableReason(App app, const QList<Kind> &kinds)
{
    for (const Kind kind : kinds) {
        if (kind != Kind::None && findExecutable(app, kind).isEmpty()) {
            return i18nc("@info %1 is an office application, %2 its command",
                         "%1 is not installed, or its %2 command is not in PATH.",
                         name(app),
                         executable(app, kind));
        }
    }
    return {};
}

QList<Launch> launches(App app, const QList<QUrl> &urls, const QStringList &mimeTypes, QString *error)
{
    if (urls.isEmpty() || urls.size() != mimeTypes.size()) {
        *error = i18nc("@info", "Select one or more office documents to open.");
        return {};
    }
    QList<Launch> result;
    for (qsizetype i = 0; i < urls.size(); ++i) {
        const QUrl &url = urls.at(i);
        const Kind documentKind = kind(mimeTypes.at(i));
        if (documentKind == Kind::None) {
            *error = i18nc("@info %1 is a file name", "%1 is not an office document, spreadsheet or presentation.", url.fileName());
            return {};
        }
        QString argument;
        if (url.isLocalFile()) {
            const QFileInfo info(url.toLocalFile());
            if (!info.isAbsolute() || info.isDir()) {
                *error = i18nc("@info %1 is a file path", "%1 is not an office document, spreadsheet or presentation.", url.toLocalFile());
                return {};
            }
            argument = QDir::cleanPath(info.absoluteFilePath());
        } else if (opensRemoteFiles(app) && url.isValid()) {
            argument = url.toString();
        } else {
            *error = i18nc("@info %1 is an office application", "%1 can only open local files. Copy remote files to a local folder first.", name(app));
            return {};
        }
        // LibreOffice and ONLYOFFICE choose the module from each file; Calligra has a program per kind.
        const bool perKind = app == App::Calligra;
        auto launch = std::find_if(result.begin(), result.end(), [perKind, documentKind](const Launch &existing) {
            return !perKind || existing.kind == documentKind;
        });
        if (launch == result.end()) {
            const QString path = findExecutable(app, documentKind);
            if (path.isEmpty()) {
                *error = unavailableReason(app, {documentKind});
                return {};
            }
            result.append(Launch{path, {}, documentKind});
            launch = result.end() - 1;
        } else if (launch->kind != documentKind) {
            // Startup feedback for a mixed selection uses the application's general desktop file.
            launch->kind = Kind::None;
        }
        launch->arguments.append(argument);
    }
    return result;
}
}
