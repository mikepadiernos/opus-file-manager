// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * Office documents and the separate office applications that open them.
 * Plasma Opus only previews these documents; editing happens in the chosen
 * application, which handles saving and format conversion itself.
 */
namespace OfficeApps
{
enum class Kind {
    None,
    Document,
    Spreadsheet,
    Presentation,
};

enum class App {
    LibreOffice,
    Calligra,
    OnlyOffice,
};

/// The office kind of @p mimeType, following MIME aliases and inheritance.
Kind kind(const QString &mimeType);
/// Whether @p mimeType is an OpenDocument or Microsoft Office document, spreadsheet or presentation.
bool isOfficeDocument(const QString &mimeType);

/// All applications, in menu order.
QList<App> apps();
/// A stable identifier, such as "libreoffice".
QString id(App app);
/// The translated name shown in menus and messages.
QString name(App app);
QString iconName(App app);
/// The command that opens documents of @p kind; Calligra has one per kind.
QString executable(App app, Kind kind);
/// The desktop file name used for startup feedback.
QString desktopName(App app, Kind kind);
/// Whether the application opens remote files itself, through KIO.
bool opensRemoteFiles(App app);

/// The application's executable path for @p kind, or an empty string when it is not in PATH.
QString findExecutable(App app, Kind kind);
/// Why the application can't open @p kinds, or an empty string when it can.
QString unavailableReason(App app, const QList<Kind> &kinds);

struct Launch {
    QString executable;
    QStringList arguments;
    /// The documents' kind, or None when a single launch opens several kinds.
    Kind kind = Kind::None;
};

/**
 * The commands that open @p urls in @p app, without a shell: one for
 * LibreOffice and ONLYOFFICE, and one per kind for Calligra. Local files are
 * passed as absolute paths, so none can be read as an option.
 * @param mimeTypes the MIME type of each URL
 * @param error receives why the selection can't be opened; the result is
 * then empty
 */
QList<Launch> launches(App app, const QList<QUrl> &urls, const QStringList &mimeTypes, QString *error);
}
