// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagedetails.h"

#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/SimpleExtractionResult>

#include <QMimeDatabase>

#include <algorithm>
#include <array>
#include <numeric>
#include <span>

ImageDetails::Data ImageDetails::analyze(const QImage &image, const QString &path)
{
    Data result;
    result.histogram.fill(0, 40);
    if (image.isNull()) {
        return result;
    }

    struct Bucket {
        int count = 0;
        int red = 0;
        int green = 0;
        int blue = 0;
    };
    std::array<Bucket, 4096> buckets{};
    const QImage sample = image.scaled(QSize(160, 160), Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < sample.height(); ++y) {
        const auto *pixels = reinterpret_cast<const QRgb *>(sample.constScanLine(y));
        for (int x = 0; x < sample.width(); ++x) {
            const QRgb pixel = pixels[x];
            if (qAlpha(pixel) == 0) {
                continue;
            }
            const int red = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue = qBlue(pixel);
            Bucket &bucket = buckets[(red >> 4) * 256 + (green >> 4) * 16 + (blue >> 4)];
            ++bucket.count;
            bucket.red += red;
            bucket.green += green;
            bucket.blue += blue;
            const int luminance = (2126 * red + 7152 * green + 722 * blue) / 10000;
            ++result.histogram[std::min(39, luminance * 40 / 256)];
        }
    }
    std::array<int, 4096> indices;
    std::iota(indices.begin(), indices.end(), 0);
    std::partial_sort(indices.begin(), indices.begin() + 5, indices.end(), [&buckets](int left, int right) {
        return buckets[left].count > buckets[right].count;
    });
    for (int index : std::span(indices).first<5>()) {
        const Bucket &bucket = buckets[index];
        if (bucket.count) {
            result.colours.append(QColor(bucket.red / bucket.count, bucket.green / bucket.count, bucket.blue / bucket.count));
        }
    }

    if (!path.isEmpty()) {
        const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
        result.metadata = extractMetadata(path, mime);
    }
    return result;
}

KFileMetaData::PropertyMultiMap ImageDetails::extractMetadata(const QString &path, const QString &mimeType)
{
    KFileMetaData::ExtractorCollection extractors;
    KFileMetaData::SimpleExtractionResult extraction(path, mimeType, KFileMetaData::ExtractionResult::ExtractMetaData);
    for (KFileMetaData::Extractor *extractor : extractors.fetchExtractors(mimeType)) {
        extractor->extract(&extraction);
    }
    return extraction.properties();
}
