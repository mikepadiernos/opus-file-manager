// SPDX-License-Identifier: GPL-3.0-or-later

#include "previewcontent.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QStringDecoder>

#include <algorithm>

namespace
{
constexpr qint64 ImageLimit = 8 * 1024 * 1024;
constexpr qint64 GalleryImageLimit = 128 * 1024 * 1024;
constexpr qint64 ImagePixelLimit = 40 * 1000 * 1000;
constexpr int ImagePreviewDimension = 2048;
PreviewContent load(const QUrl &url, bool image, bool text, int imageDimension, qint64 imageLimit, qint64 textLimit)
{
    if (!url.isLocalFile()) {
        return {PreviewContent::Kind::Remote, {}, {}, false};
    }

    if (!image && !text) {
        return {PreviewContent::Kind::Unsupported, {}, {}, false};
    }

    const QFileInfo info(url.toLocalFile());
    if (info.exists() && !info.isFile()) {
        return {PreviewContent::Kind::Unsupported, {}, {}, false};
    }
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        PreviewContent result;
        result.kind = PreviewContent::Kind::Error;
        result.text = file.errorString();
        return result;
    }

    if (image) {
        if (file.size() > imageLimit) {
            return {PreviewContent::Kind::TooLarge, {}, {}, false};
        }
        QImageReader reader(&file);
        reader.setAutoTransform(true);
        const bool animated = reader.supportsAnimation() && reader.imageCount() != 1;
        const QSize dimensions = reader.size();
        if (!dimensions.isValid()) {
            PreviewContent result;
            result.kind = PreviewContent::Kind::Error;
            result.text = reader.errorString();
            return result;
        }
        if (qint64(dimensions.width()) * dimensions.height() > ImagePixelLimit) {
            return {PreviewContent::Kind::TooLarge, {}, {}, false};
        }
        if (imageDimension > 0 && (dimensions.width() > imageDimension || dimensions.height() > imageDimension)) {
            reader.setScaledSize(dimensions.scaled(QSize(imageDimension, imageDimension), Qt::KeepAspectRatio));
        }
        QImage decoded = reader.read();
        if (decoded.isNull()) {
            PreviewContent result;
            result.kind = PreviewContent::Kind::Error;
            result.text = reader.errorString();
            return result;
        }
        PreviewContent result;
        result.kind = PreviewContent::Kind::Image;
        result.image = std::move(decoded);
        result.animated = animated;
        return result;
    }
    const QByteArray bytes = file.read(textLimit + 1);
    if (file.error() != QFileDevice::NoError) {
        PreviewContent result;
        result.kind = PreviewContent::Kind::Error;
        result.text = file.errorString();
        return result;
    }
    if (bytes.contains('\0')) {
        return {PreviewContent::Kind::Unsupported, {}, {}, false};
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    PreviewContent result;
    result.kind = PreviewContent::Kind::Text;
    result.truncated = bytes.size() > textLimit;
    result.text = decoder.decode(QByteArrayView(bytes.constData(), std::min<qsizetype>(bytes.size(), textLimit)));
    if (decoder.hasError()) {
        return {PreviewContent::Kind::Unsupported, {}, {}, false};
    }
    return result;
}
}

PreviewContent Preview::load(const QUrl &url, const QString &mimeType, qint64 textLimit)
{
    const QMimeType mime = QMimeDatabase().mimeTypeForName(mimeType);
    const bool text =
        mime.inherits(QStringLiteral("text/plain")) || mime.name() == QLatin1String("application/xhtml+xml") || mime.name() == QLatin1String("text/markdown");
    return ::load(url, mime.name().startsWith(QLatin1String("image/")), text, ImagePreviewDimension, ImageLimit, textLimit);
}

PreviewContent Preview::loadImage(const QUrl &url, int maximumDimension)
{
    return ::load(url, true, false, maximumDimension, GalleryImageLimit, 0);
}
