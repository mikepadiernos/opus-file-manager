// SPDX-License-Identifier: GPL-3.0-or-later

#include "filetypeplaces.h"

#include <KLocalizedString>

#ifdef OPUSFM_HAVE_BALOO
#include <Baloo/Query>
#endif

namespace
{
const QLatin1String Scheme("opus-search");

QStringList extensions(FileTypePlaces::Category category)
{
    using Category = FileTypePlaces::Category;
    switch (category) {
    case Category::Documents:
        return {QStringLiteral("pdf"),  QStringLiteral("odt"),  QStringLiteral("ott"), QStringLiteral("fodt"), QStringLiteral("ods"),  QStringLiteral("ots"),
                QStringLiteral("fods"), QStringLiteral("odp"),  QStringLiteral("otp"), QStringLiteral("fodp"), QStringLiteral("odg"),  QStringLiteral("doc"),
                QStringLiteral("docx"), QStringLiteral("docm"), QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("xlsm"), QStringLiteral("ppt"),
                QStringLiteral("pptx"), QStringLiteral("pptm"), QStringLiteral("rtf"), QStringLiteral("ps")};
    case Category::Archives:
        return {QStringLiteral("zip"),
                QStringLiteral("7z"),
                QStringLiteral("rar"),
                QStringLiteral("tar"),
                QStringLiteral("gz"),
                QStringLiteral("tgz"),
                QStringLiteral("bz2"),
                QStringLiteral("tbz2"),
                QStringLiteral("xz"),
                QStringLiteral("txz"),
                QStringLiteral("zst"),
                QStringLiteral("tzst"),
                QStringLiteral("lz"),
                QStringLiteral("lzma"),
                QStringLiteral("cab"),
                QStringLiteral("cpio"),
                QStringLiteral("ar")};
    case Category::Ebooks:
        return {QStringLiteral("epub"),
                QStringLiteral("mobi"),
                QStringLiteral("azw"),
                QStringLiteral("azw3"),
                QStringLiteral("fb2"),
                QStringLiteral("cbz"),
                QStringLiteral("cbr"),
                QStringLiteral("cb7"),
                QStringLiteral("djvu"),
                QStringLiteral("djv")};
    case Category::Packages:
        return {QStringLiteral("flatpak"),
                QStringLiteral("flatpakref"),
                QStringLiteral("flatpakrepo"),
                QStringLiteral("appimage"),
                QStringLiteral("rpm"),
                QStringLiteral("deb"),
                QStringLiteral("snap")};
    default:
        return {};
    }
}
}

QList<FileTypePlaces::Category> FileTypePlaces::categories()
{
    return {Category::Documents,
            Category::Images,
            Category::Audio,
            Category::Video,
            Category::Archives,
            Category::Ebooks,
            Category::Packages,
            Category::LargeFiles};
}

QString FileTypePlaces::id(Category category)
{
    switch (category) {
    case Category::Documents:
        return QStringLiteral("documents");
    case Category::Images:
        return QStringLiteral("images");
    case Category::Audio:
        return QStringLiteral("audio");
    case Category::Video:
        return QStringLiteral("video");
    case Category::Archives:
        return QStringLiteral("archives");
    case Category::Ebooks:
        return QStringLiteral("ebooks");
    case Category::Packages:
        return QStringLiteral("packages");
    case Category::LargeFiles:
        return QStringLiteral("large-files");
    }
    return {};
}

QString FileTypePlaces::title(Category category)
{
    switch (category) {
    case Category::Documents:
        return i18nc("@item file-type category in Places", "Documents");
    case Category::Images:
        return i18nc("@item file-type category in Places", "Images");
    case Category::Audio:
        return i18nc("@item file-type category in Places", "Audio");
    case Category::Video:
        return i18nc("@item file-type category in Places", "Videos");
    case Category::Archives:
        return i18nc("@item file-type category in Places", "Archives");
    case Category::Ebooks:
        return i18nc("@item file-type category in Places", "Ebooks");
    case Category::Packages:
        return i18nc("@item file-type category in Places", "Application Packages");
    case Category::LargeFiles:
        return i18nc("@item file-type category in Places", "Large Files");
    }
    return {};
}

QString FileTypePlaces::iconName(Category category)
{
    switch (category) {
    case Category::Documents:
        return QStringLiteral("folder-text");
    case Category::Images:
        return QStringLiteral("folder-images");
    case Category::Audio:
        return QStringLiteral("folder-music");
    case Category::Video:
        return QStringLiteral("folder-videos");
    case Category::Archives:
        return QStringLiteral("application-x-archive");
    case Category::Ebooks:
        return QStringLiteral("application-epub+zip");
    case Category::Packages:
        return QStringLiteral("application-x-executable");
    case Category::LargeFiles:
        return QStringLiteral("drive-harddisk");
    }
    return {};
}

QUrl FileTypePlaces::placeUrl(Category category)
{
    QUrl url;
    url.setScheme(Scheme);
    url.setPath(QLatin1Char('/') + id(category));
    return url;
}

bool FileTypePlaces::isPlaceUrl(const QUrl &url)
{
    return fromPlaceUrl(url, nullptr);
}

bool FileTypePlaces::fromPlaceUrl(const QUrl &url, Category *category)
{
    if (url.scheme() != Scheme) {
        return false;
    }
    QString path = url.path();
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    for (const Category candidate : categories()) {
        if (id(candidate) == path) {
            if (category) {
                *category = candidate;
            }
            return true;
        }
    }
    return false;
}

bool FileTypePlaces::usesIndex(Category category)
{
    return category != Category::LargeFiles;
}

QString FileTypePlaces::balooType(Category category)
{
    switch (category) {
    case Category::Documents:
        return QStringLiteral("Document");
    case Category::Images:
        return QStringLiteral("Image");
    case Category::Audio:
        return QStringLiteral("Audio");
    case Category::Video:
        return QStringLiteral("Video");
    case Category::Archives:
        return QStringLiteral("Archive");
    default:
        return {};
    }
}

QStringList FileTypePlaces::mimeTypes(Category category)
{
    switch (category) {
    case Category::Ebooks:
        return {QStringLiteral("application/epub+zip"),
                QStringLiteral("application/x-mobipocket-ebook"),
                QStringLiteral("application/vnd.amazon.mobi8-ebook"),
                QStringLiteral("application/x-fictionbook+xml"),
                QStringLiteral("application/vnd.comicbook+zip"),
                QStringLiteral("application/vnd.comicbook-rar"),
                QStringLiteral("image/vnd.djvu")};
    case Category::Packages:
        return {QStringLiteral("application/vnd.flatpak"),
                QStringLiteral("application/vnd.flatpak.ref"),
                QStringLiteral("application/vnd.flatpak.repo"),
                QStringLiteral("application/vnd.appimage"),
                QStringLiteral("application/x-iso9660-appimage"),
                QStringLiteral("application/x-rpm"),
                QStringLiteral("application/vnd.debian.binary-package"),
                QStringLiteral("application/vnd.snap")};
    default:
        return {};
    }
}

QString FileTypePlaces::balooSearchString(Category category)
{
    QStringList alternatives;
    for (const QString &mimeType : mimeTypes(category)) {
        alternatives.append(QStringLiteral("mimetype:\"%1\"").arg(mimeType));
    }
    return alternatives.join(QStringLiteral(" OR "));
}

QUrl FileTypePlaces::balooUrl(Category category)
{
#ifdef OPUSFM_HAVE_BALOO
    if (!usesIndex(category)) {
        return {};
    }
    Baloo::Query query;
    if (const QString type = balooType(category); !type.isEmpty()) {
        query.addType(type);
    } else {
        query.setSearchString(balooSearchString(category));
    }
    return query.toSearchUrl(title(category));
#else
    Q_UNUSED(category)
    return {};
#endif
}

qint64 FileTypePlaces::largeFileSize()
{
    return qint64(100) * 1024 * 1024;
}

QString FileTypePlaces::liveQuery(Category category)
{
    switch (category) {
    case Category::Images:
        return QStringLiteral("type:image/*");
    case Category::Audio:
        return QStringLiteral("type:audio/*");
    case Category::Video:
        return QStringLiteral("type:video/*");
    case Category::LargeFiles:
        return QStringLiteral("size>100M");
    default: {
        QStringList alternatives;
        for (const QString &extension : extensions(category)) {
            alternatives.append(QStringLiteral("type:") + extension);
        }
        return alternatives.join(QStringLiteral(" OR "));
    }
    }
}
