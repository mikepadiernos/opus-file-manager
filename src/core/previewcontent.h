// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QString>
#include <QUrl>

struct PreviewContent {
    enum class Kind {
        Text,
        Image,
        Unsupported,
        Remote,
        TooLarge,
        Error,
    };

    Kind kind = Kind::Unsupported;
    QString text;
    QImage image;
    bool truncated = false;
    bool animated = false;
};

namespace Preview
{
PreviewContent load(const QUrl &url, const QString &mimeType, qint64 textLimit = 1024 * 1024);
PreviewContent loadImage(const QUrl &url, int maximumDimension);
}
