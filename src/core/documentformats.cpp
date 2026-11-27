// SPDX-License-Identifier: GPL-3.0-or-later

#include "documentformats.h"
#include "officeapps.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <utility>

namespace DocumentFormats
{
namespace
{
const std::pair<const char *, Kind> MimeKinds[] = {
    {"application/pdf", Kind::Pdf},
    {"application/epub+zip", Kind::Ebook},
    {"application/x-mobipocket-ebook", Kind::Ebook},
    {"application/vnd.amazon.mobi8-ebook", Kind::Ebook},
    {"application/x-fictionbook+xml", Kind::Ebook},
    {"application/x-zip-compressed-fb2", Kind::Ebook},
    {"application/vnd.comicbook+zip", Kind::Comic},
    {"application/vnd.comicbook-rar", Kind::Comic},
    {"application/x-cb7", Kind::Comic},
    {"application/x-cbt", Kind::Comic},
    {"image/vnd.djvu", Kind::DjVu},
    {"image/vnd.djvu+multipage", Kind::DjVu},
    {"application/vnd.ms-htmlhelp", Kind::HelpFile},
};

const char *const InstallableFonts[] = {
    "font/ttf",
    "font/otf",
    "font/collection",
    "application/x-font-type1",
    "application/x-font-pcf",
    "application/x-font-bdf",
    "application/x-font-afm",
};

bool matches(const QMimeType &type, const QString &mimeType, const QString &base)
{
    return mimeType == base || (type.isValid() && (type.name() == base || type.inherits(base)));
}
}

Kind kind(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return Kind::None;
    }
    if (OfficeApps::isOfficeDocument(mimeType)) {
        return Kind::Office;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    for (const auto &[name, kind] : MimeKinds) {
        if (matches(type, mimeType, QLatin1String(name))) {
            return kind;
        }
    }
    const QString name = type.isValid() ? type.name() : mimeType;
    if (name.startsWith(QLatin1String("font/")) || name.startsWith(QLatin1String("application/x-font-"))) {
        return Kind::Font;
    }
    // Other OpenDocument and Office Open XML formats, such as drawings and formulas.
    if (name.startsWith(QLatin1String("application/vnd.oasis.opendocument."))
        || name.startsWith(QLatin1String("application/vnd.openxmlformats-officedocument."))) {
        return Kind::Office;
    }
    return Kind::None;
}

bool usesDocumentViewer(const QString &mimeType)
{
    const Kind formatKind = kind(mimeType);
    return formatKind != Kind::None && formatKind != Kind::Font;
}

QString missingViewerMessage(Kind kind)
{
    switch (kind) {
    case Kind::Pdf:
        return i18nc("@info", "No document viewer is installed for this file type. Install Okular to preview PDF documents.");
    case Kind::Office:
        return i18nc("@info",
                     "No document viewer is installed for this file type. Install Calligra to preview office documents, or use Open in Office "
                     "Application.");
    case Kind::Ebook:
    case Kind::Comic:
    case Kind::DjVu:
    case Kind::HelpFile:
        return i18nc("@info", "No document viewer is installed for this file type. Install Okular to preview ebooks, comics, DjVu documents and help files.");
    case Kind::Font:
    case Kind::None:
        break;
    }
    return i18nc("@info", "No document viewer is installed for this file type.");
}

bool isInstallableFont(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return false;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    for (const char *name : InstallableFonts) {
        if (matches(type, mimeType, QLatin1String(name))) {
            return true;
        }
    }
    return false;
}

QString defaultSampleText()
{
    return i18nc("@info font sample text; use a pangram in your language", "The quick brown fox jumps over the lazy dog 0123456789");
}

QString findFontInstaller()
{
    return QStandardPaths::findExecutable(QStringLiteral("kfontinst"));
}

QString fontInstallerUnavailableReason()
{
    if (!findFontInstaller().isEmpty()) {
        return {};
    }
    return i18nc("@info", "Plasma's font installer, kfontinst, is not installed or is not in PATH.");
}

QStringList fontInstallArguments(const QList<QUrl> &urls)
{
    QStringList arguments;
    arguments.reserve(urls.size());
    for (const QUrl &url : urls) {
        arguments.append(url.isLocalFile() ? QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath()) : url.toString());
    }
    return arguments;
}
}
